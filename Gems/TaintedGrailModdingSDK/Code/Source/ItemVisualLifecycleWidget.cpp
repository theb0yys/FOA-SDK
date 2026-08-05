/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ItemVisualLifecycleEnhancer.h"

#include "FoundationModels.h"
#include "FoundationService.h"

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzToolsFramework/AssetBrowser/Thumbnails/ProductThumbnail.h>
#include <AzToolsFramework/Thumbnails/ThumbnailWidget.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr int EntryRowRole = Qt::UserRole;
        constexpr int AssetIdColumn = 4;

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        class ProductTile final : public QWidget
        {
        public:
            ProductTile(const AZ::Data::AssetId& assetId, const QString& name, int extent, QWidget* parent)
                : QWidget(parent)
            {
                setAttribute(Qt::WA_TransparentForMouseEvents);
                auto* layout = new QVBoxLayout(this);
                layout->setContentsMargins(4, 4, 4, 4);
                layout->setSpacing(3);
                auto* thumbnail = new AzToolsFramework::Thumbnailer::ThumbnailWidget(this);
                thumbnail->setAttribute(Qt::WA_TransparentForMouseEvents);
                thumbnail->setFixedSize(extent, extent);
                thumbnail->SetThumbnailKey(
                    AZStd::make_shared<AzToolsFramework::AssetBrowser::ProductThumbnailKey>(assetId));
                auto* label = new QLabel(name, this);
                label->setAttribute(Qt::WA_TransparentForMouseEvents);
                label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
                label->setWordWrap(true);
                label->setMaximumWidth(extent + 20);
                layout->addWidget(thumbnail, 0, Qt::AlignHCenter);
                layout->addWidget(label);
                setFixedSize(extent + 28, extent + 58);
            }
        };
    }

    ItemVisualLifecycleEnhancer::ItemVisualLifecycleEnhancer(QWidget* selector)
        : QObject(selector)
        , m_selector(selector)
    {
        InstallGrid();
    }

    void ItemVisualLifecycleEnhancer::InstallGrid()
    {
        if (!m_selector)
        {
            return;
        }

        for (QTableWidget* table : m_selector->findChildren<QTableWidget*>())
        {
            if (table->accessibleName() == tr("Evidence-backed preview products"))
            {
                m_table = table;
                break;
            }
        }
        for (QLineEdit* edit : m_selector->findChildren<QLineEdit*>())
        {
            if (edit->accessibleName() == tr("Preview entry filter"))
            {
                m_search = edit;
            }
            else if (edit->accessibleName() == tr("Loaded Asset Browser pane model path"))
            {
                m_modelPath = edit;
            }
        }
        for (QComboBox* combo : m_selector->findChildren<QComboBox*>())
        {
            if (combo->accessibleName() == tr("Canonical item or recipe target"))
            {
                m_target = combo;
                break;
            }
        }
        for (QPushButton* button : m_selector->findChildren<QPushButton*>())
        {
            if (button->text() == tr("Reload"))
            {
                m_reload = button;
                break;
            }
        }
        if (!m_table)
        {
            return;
        }

        auto* splitter = qobject_cast<QSplitter*>(m_table->parentWidget());
        if (!splitter)
        {
            return;
        }
        auto* browserPanel = new QWidget(splitter);
        auto* browserLayout = new QVBoxLayout(browserPanel);
        browserLayout->setContentsMargins(0, 0, 0, 0);
        auto* controls = new QHBoxLayout();
        controls->addWidget(new QLabel(tr("Visual assets"), browserPanel));
        controls->addStretch();
        controls->addWidget(new QLabel(tr("Size"), browserPanel));
        m_sizeSlider = new QSlider(Qt::Horizontal, browserPanel);
        m_sizeSlider->setRange(64, 192);
        m_sizeSlider->setValue(112);
        m_sizeSlider->setFixedWidth(150);
        m_sizeSlider->setAccessibleName(tr("Thumbnail size"));
        controls->addWidget(m_sizeSlider);
        browserLayout->addLayout(controls);

        m_grid = new QListWidget(browserPanel);
        m_grid->setObjectName(QStringLiteral("FOAItemViewerThumbnailGrid"));
        m_grid->setAccessibleName(tr("Visual asset thumbnail grid"));
        m_grid->setViewMode(QListView::IconMode);
        m_grid->setResizeMode(QListView::Adjust);
        m_grid->setMovement(QListView::Static);
        m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
        m_grid->setSpacing(6);
        m_grid->setUniformItemSizes(true);
        browserLayout->addWidget(m_grid, 1);

        m_table->setParent(browserPanel);
        m_table->setMaximumHeight(150);
        m_table->setAccessibleName(tr("Selected asset details"));
        browserLayout->addWidget(m_table);
        splitter->insertWidget(0, browserPanel);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        connect(m_table->model(), &QAbstractItemModel::rowsInserted, this, [this]() { ScheduleRebuild(); });
        connect(m_table->model(), &QAbstractItemModel::rowsRemoved, this, [this]() { ScheduleRebuild(); });
        connect(m_table->model(), &QAbstractItemModel::modelReset, this, [this]() { ScheduleRebuild(); });
        connect(m_table->model(), &QAbstractItemModel::dataChanged, this, [this]() { ScheduleRebuild(); });
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { SelectGridItem(); });
        connect(m_grid, &QListWidget::itemSelectionChanged, this, [this]() { SelectTableRow(); });
        connect(m_sizeSlider, &QSlider::valueChanged, this, [this]() { RebuildGrid(); SaveState(); });
        if (m_search)
        {
            connect(m_search, &QLineEdit::textChanged, this, [this]() { ApplyFilter(); });
        }
        if (m_modelPath)
        {
            connect(m_modelPath, &QLineEdit::textChanged, this, [this]() { SaveState(); ScheduleRebuild(); });
        }
        if (m_target)
        {
            connect(m_target, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { SaveState(); });
        }
        QTimer::singleShot(0, this, [this]() { RestoreState(); ScheduleRebuild(); });
    }

    void ItemVisualLifecycleEnhancer::ScheduleRebuild()
    {
        if (m_rebuildPending)
        {
            return;
        }
        m_rebuildPending = true;
        QTimer::singleShot(0, this, [this]()
        {
            m_rebuildPending = false;
            RebuildGrid();
        });
    }

    void ItemVisualLifecycleEnhancer::RebuildGrid()
    {
        if (!m_grid || !m_table)
        {
            return;
        }
        const QString selectedAsset = m_table->currentRow() >= 0 && m_table->item(m_table->currentRow(), AssetIdColumn)
            ? m_table->item(m_table->currentRow(), AssetIdColumn)->text() : m_pendingAssetId;
        QSignalBlocker blocker(m_grid);
        m_grid->clear();
        const int extent = m_sizeSlider ? m_sizeSlider->value() : 112;
        m_grid->setGridSize(QSize(extent + 40, extent + 72));
        for (int row = 0; row < m_table->rowCount(); ++row)
        {
            const QTableWidgetItem* nameItem = m_table->item(row, 0);
            const QTableWidgetItem* assetItem = m_table->item(row, AssetIdColumn);
            if (!nameItem || !assetItem)
            {
                continue;
            }
            const AZ::Data::AssetId assetId = AZ::Data::AssetId::CreateString(ToAzString(assetItem->text()));
            if (!assetId.IsValid())
            {
                continue;
            }
            auto* item = new QListWidgetItem(m_grid);
            item->setData(EntryRowRole, row);
            item->setData(Qt::UserRole + 1, assetItem->text());
            item->setSizeHint(QSize(extent + 28, extent + 58));
            m_grid->setItemWidget(item, new ProductTile(assetId, nameItem->text(), extent, m_grid));
            if (assetItem->text() == selectedAsset)
            {
                m_grid->setCurrentItem(item);
            }
        }
        ApplyFilter();
        SelectGridItem();
    }

    void ItemVisualLifecycleEnhancer::ApplyFilter()
    {
        if (!m_grid || !m_table)
        {
            return;
        }
        for (int index = 0; index < m_grid->count(); ++index)
        {
            QListWidgetItem* gridItem = m_grid->item(index);
            const int row = gridItem->data(EntryRowRole).toInt();
            gridItem->setHidden(row < 0 || row >= m_table->rowCount() || m_table->isRowHidden(row));
        }
    }

    void ItemVisualLifecycleEnhancer::SelectTableRow()
    {
        if (m_syncing || !m_grid || !m_table || !m_grid->currentItem())
        {
            return;
        }
        m_syncing = true;
        const int row = m_grid->currentItem()->data(EntryRowRole).toInt();
        if (row >= 0 && row < m_table->rowCount())
        {
            m_table->selectRow(row);
            m_pendingAssetId = m_grid->currentItem()->data(Qt::UserRole + 1).toString();
        }
        m_syncing = false;
        SaveState();
    }

    void ItemVisualLifecycleEnhancer::SelectGridItem()
    {
        if (m_syncing || !m_grid || !m_table || m_table->currentRow() < 0)
        {
            return;
        }
        m_syncing = true;
        for (int index = 0; index < m_grid->count(); ++index)
        {
            QListWidgetItem* item = m_grid->item(index);
            if (item->data(EntryRowRole).toInt() == m_table->currentRow())
            {
                m_grid->setCurrentItem(item);
                m_grid->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                m_pendingAssetId = item->data(Qt::UserRole + 1).toString();
                break;
            }
        }
        m_syncing = false;
        SaveState();
    }

    QString ItemVisualLifecycleEnhancer::SettingsPrefix() const
    {
        const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        return profile ? QStringLiteral("profiles/%1/workingItemViewer/").arg(ToQString(profile->m_profileId)) : QString();
    }

    void ItemVisualLifecycleEnhancer::SaveState()
    {
        if (m_restoring || SettingsPrefix().isEmpty())
        {
            return;
        }
        QSettings settings(QStringLiteral("FOA-SDK"), QStringLiteral("ItemViewer"));
        const QString prefix = SettingsPrefix();
        settings.setValue(prefix + QStringLiteral("modelPath"), m_modelPath ? m_modelPath->text() : QString());
        settings.setValue(prefix + QStringLiteral("target"), m_target ? m_target->currentData().toString() : QString());
        settings.setValue(prefix + QStringLiteral("assetId"), m_pendingAssetId);
        settings.setValue(prefix + QStringLiteral("thumbnailSize"), m_sizeSlider ? m_sizeSlider->value() : 112);
        settings.sync();
    }

    void ItemVisualLifecycleEnhancer::RestoreState()
    {
        const QString prefix = SettingsPrefix();
        if (prefix.isEmpty())
        {
            return;
        }
        m_restoring = true;
        QSettings settings(QStringLiteral("FOA-SDK"), QStringLiteral("ItemViewer"));
        if (m_sizeSlider)
        {
            m_sizeSlider->setValue(settings.value(prefix + QStringLiteral("thumbnailSize"), 112).toInt());
        }
        if (m_target)
        {
            const int index = m_target->findData(settings.value(prefix + QStringLiteral("target")).toString());
            if (index >= 0)
            {
                m_target->setCurrentIndex(index);
            }
        }
        m_pendingAssetId = settings.value(prefix + QStringLiteral("assetId")).toString();
        const QString path = settings.value(prefix + QStringLiteral("modelPath")).toString();
        if (m_modelPath && m_reload && !path.isEmpty())
        {
            m_modelPath->setText(path);
            QTimer::singleShot(0, m_reload, [reload = m_reload]()
            {
                if (reload)
                {
                    reload->click();
                }
            });
        }
        m_restoring = false;
    }
}
