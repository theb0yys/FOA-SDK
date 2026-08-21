/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetBrowserPreviewWidget.h"

#include "FoundationModels.h"
#include "FoundationService.h"

#include <AzCore/std/algorithm.h>

#include <QAbstractItemView>
#include <QComboBox>
#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr int EntryIndexRole = Qt::UserRole + 1;
        constexpr int MaximumEvidenceScanFiles = 2000;
        constexpr qint64 MaximumEvidenceDocumentBytes = 4 * 1024 * 1024;

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList output;
            for (const AZStd::string& value : values)
            {
                output.push_back(ToQString(value));
            }
            return output.join(QStringLiteral(", "));
        }

        QWidget* CreateFileRow(
            QWidget* parent,
            QLineEdit* edit,
            QPushButton* browseButton)
        {
            auto* row = new QWidget(parent);
            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(edit, 1);
            layout->addWidget(browseButton);
            return row;
        }

        QLineEdit* CreateEvidencePathEdit(QWidget* parent, const QString& placeholder)
        {
            auto* edit = new QLineEdit(parent);
            edit->setPlaceholderText(placeholder);
            edit->setClearButtonEnabled(true);
            return edit;
        }

        void ConfigureValueLabel(QLabel* label)
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        }

        QTreeWidgetItem* EnsureCategoryItem(
            QTreeWidget* tree,
            QHash<QString, QTreeWidgetItem*>& categories,
            const QString& category)
        {
            if (categories.contains(category))
            {
                return categories.value(category);
            }

            auto* item = new QTreeWidgetItem(tree);
            item->setText(0, category);
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            categories.insert(category, item);
            return item;
        }
    } // namespace

    AssetBrowserPreviewWidget::AssetBrowserPreviewWidget(QWidget* parent)
        : QWidget(parent)
    {
        FoundationNotificationBus::Handler::BusConnect();

        setMinimumSize(640, 720);
        setMaximumWidth(1080);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Asset Browser Preview"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* profileGroup = new QGroupBox(tr("Active evidence binding"), this);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_profileValue = new QLabel(profileGroup);
        ConfigureValueLabel(m_profileValue);
        profileLayout->addRow(tr("Profile"), m_profileValue);
        rootLayout->addWidget(profileGroup);

        auto* evidenceGroup = new QGroupBox(tr("Preview evidence inputs"), this);
        auto* evidenceLayout = new QFormLayout(evidenceGroup);
        m_extractedRootEdit = CreateEvidencePathEdit(
            evidenceGroup,
            tr("Configured extracted-data root"));
        m_paneModelEdit = CreateEvidencePathEdit(
            evidenceGroup,
            tr("foa-asset-browser-pane-model JSON"));
        m_thumbnailEvidenceEdit = CreateEvidencePathEdit(
            evidenceGroup,
            tr("foa-thumbnail-artifact-evidence JSON"));
        m_thumbnailRootEdit = CreateEvidencePathEdit(
            evidenceGroup,
            tr("Preview thumbnail root"));
        m_viewportEvidenceEdit = CreateEvidencePathEdit(
            evidenceGroup,
            tr("foa-3d-preview-viewport-render JSON"));

        auto* browseExtracted = new QPushButton(tr("Browse..."), evidenceGroup);
        auto* browsePaneModel = new QPushButton(tr("Browse..."), evidenceGroup);
        auto* browseThumbnailEvidence = new QPushButton(tr("Browse..."), evidenceGroup);
        auto* browseThumbnailRoot = new QPushButton(tr("Browse..."), evidenceGroup);
        auto* browseViewportEvidence = new QPushButton(tr("Browse..."), evidenceGroup);

        evidenceLayout->addRow(
            tr("Extracted data root"),
            CreateFileRow(evidenceGroup, m_extractedRootEdit, browseExtracted));
        evidenceLayout->addRow(
            tr("Asset Browser model"),
            CreateFileRow(evidenceGroup, m_paneModelEdit, browsePaneModel));
        evidenceLayout->addRow(
            tr("Thumbnail evidence"),
            CreateFileRow(evidenceGroup, m_thumbnailEvidenceEdit, browseThumbnailEvidence));
        evidenceLayout->addRow(
            tr("Thumbnail root"),
            CreateFileRow(evidenceGroup, m_thumbnailRootEdit, browseThumbnailRoot));
        evidenceLayout->addRow(
            tr("Viewport evidence"),
            CreateFileRow(evidenceGroup, m_viewportEvidenceEdit, browseViewportEvidence));
        rootLayout->addWidget(evidenceGroup);

        auto* actionRow = new QWidget(this);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        auto* autoFindButton = new QPushButton(tr("Auto-find evidence"), actionRow);
        auto* loadButton = new QPushButton(tr("Load preview"), actionRow);
        actionLayout->addWidget(autoFindButton);
        actionLayout->addWidget(loadButton);
        actionLayout->addStretch(1);
        rootLayout->addWidget(actionRow);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        ConfigureValueLabel(m_statusLabel);
        rootLayout->addWidget(m_statusLabel);

        m_categoryFilter = new QComboBox(this);
        m_categoryFilter->addItem(tr("All categories"));
        rootLayout->addWidget(m_categoryFilter);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        m_assetTree = new QTreeWidget(splitter);
        m_assetTree->setColumnCount(5);
        m_assetTree->setHeaderLabels({
            tr("Asset"),
            tr("Fidelity"),
            tr("Preview"),
            tr("Route"),
            tr("Product")
        });
        m_assetTree->setIconSize(QSize(72, 72));
        m_assetTree->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_assetTree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_assetTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_assetTree->header()->setStretchLastSection(true);

        auto* inspectorScroll = new QScrollArea(splitter);
        inspectorScroll->setWidgetResizable(true);
        auto* inspector = new QWidget(inspectorScroll);
        auto* inspectorLayout = new QVBoxLayout(inspector);
        inspectorScroll->setWidget(inspector);

        m_thumbnailLabel = new QLabel(inspector);
        m_thumbnailLabel->setMinimumSize(220, 220);
        m_thumbnailLabel->setAlignment(Qt::AlignCenter);
        m_thumbnailLabel->setFrameShape(QFrame::StyledPanel);
        m_thumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        inspectorLayout->addWidget(m_thumbnailLabel);

        auto* detailsGroup = new QGroupBox(tr("Selected preview product"), inspector);
        auto* detailsLayout = new QFormLayout(detailsGroup);
        m_identityValue = new QLabel(detailsGroup);
        m_categoryValue = new QLabel(detailsGroup);
        m_fidelityValue = new QLabel(detailsGroup);
        m_routeValue = new QLabel(detailsGroup);
        m_productValue = new QLabel(detailsGroup);
        m_evidenceValue = new QLabel(detailsGroup);
        m_blockerValue = new QLabel(detailsGroup);
        for (QLabel* label : {
                 m_identityValue,
                 m_categoryValue,
                 m_fidelityValue,
                 m_routeValue,
                 m_productValue,
                 m_evidenceValue,
                 m_blockerValue })
        {
            ConfigureValueLabel(label);
        }
        detailsLayout->addRow(tr("Identity"), m_identityValue);
        detailsLayout->addRow(tr("Category"), m_categoryValue);
        detailsLayout->addRow(tr("Fidelity"), m_fidelityValue);
        detailsLayout->addRow(tr("Viewport route"), m_routeValue);
        detailsLayout->addRow(tr("Product path"), m_productValue);
        detailsLayout->addRow(tr("Evidence"), m_evidenceValue);
        detailsLayout->addRow(tr("Blocker"), m_blockerValue);
        inspectorLayout->addWidget(detailsGroup);

        m_routeButton = new QPushButton(tr("Route to central viewport"), inspector);
        m_routeStatus = new QLabel(inspector);
        ConfigureValueLabel(m_routeStatus);
        inspectorLayout->addWidget(m_routeButton);
        inspectorLayout->addWidget(m_routeStatus);
        inspectorLayout->addStretch(1);

        splitter->addWidget(m_assetTree);
        splitter->addWidget(inspectorScroll);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        rootLayout->addWidget(splitter, 1);

        connect(browseExtracted, &QPushButton::clicked, this, [this]()
        {
            BrowseForDirectory(m_extractedRootEdit, tr("Select extracted-data root"));
        });
        connect(browsePaneModel, &QPushButton::clicked, this, [this]()
        {
            BrowseForFile(m_paneModelEdit, tr("Select Asset Browser pane-model evidence"));
        });
        connect(browseThumbnailEvidence, &QPushButton::clicked, this, [this]()
        {
            BrowseForFile(m_thumbnailEvidenceEdit, tr("Select thumbnail evidence"));
        });
        connect(browseThumbnailRoot, &QPushButton::clicked, this, [this]()
        {
            BrowseForDirectory(m_thumbnailRootEdit, tr("Select thumbnail preview root"));
        });
        connect(browseViewportEvidence, &QPushButton::clicked, this, [this]()
        {
            BrowseForFile(m_viewportEvidenceEdit, tr("Select viewport evidence"));
        });
        connect(autoFindButton, &QPushButton::clicked, this, [this]() { AutoFindEvidence(); });
        connect(loadButton, &QPushButton::clicked, this, [this]() { LoadPreviewEvidence(); });
        connect(m_categoryFilter, &QComboBox::currentTextChanged, this, [this]() { PopulateTree(); });
        connect(m_assetTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current)
        {
            ShowSelectedEntry(current);
        });
        connect(m_routeButton, &QPushButton::clicked, this, [this]() { RouteSelectedEntry(); });

        RefreshProfileContext();
    }

    AssetBrowserPreviewWidget::~AssetBrowserPreviewWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void AssetBrowserPreviewWidget::OnFoundationChanged()
    {
        RefreshProfileContext();
    }

    void AssetBrowserPreviewWidget::RefreshProfileContext()
    {
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile)
        {
            m_profileValue->setText(tr("Not configured"));
            SetStatus(tr("Configure an exact active FoA game profile before loading preview evidence."), true);
            return;
        }

        m_profileValue->setText(
            tr("%1 [%2] / %3 / %4 / %5")
                .arg(ToQString(profile->m_displayName))
                .arg(ToQString(profile->m_profileId))
                .arg(ToQString(profile->m_gameVersion))
                .arg(ToQString(profile->m_branch))
                .arg(ToQString(profile->m_runtimeTarget)));
        if (m_extractedRootEdit->text().trimmed().isEmpty())
        {
            m_extractedRootEdit->setText(ToQString(profile->m_extractedDataPath));
        }
        AutoFindEvidence();
        if (!m_paneModelEdit->text().trimmed().isEmpty())
        {
            LoadPreviewEvidence();
        }
    }

    void AssetBrowserPreviewWidget::AutoFindEvidence()
    {
        if (m_extractedRootEdit->text().trimmed().isEmpty())
        {
            SetStatus(tr("No extracted-data root is configured for evidence scanning."), true);
            return;
        }

        const QString paneModel = FindEvidenceDocument(QStringLiteral("foa-asset-browser-pane-model"));
        const QString thumbnailEvidence = FindEvidenceDocument(QStringLiteral("foa-thumbnail-artifact-evidence"));
        const QString viewportEvidence = FindEvidenceDocument(QStringLiteral("foa-3d-preview-viewport-render"));
        if (!paneModel.isEmpty())
        {
            m_paneModelEdit->setText(paneModel);
        }
        if (!thumbnailEvidence.isEmpty())
        {
            m_thumbnailEvidenceEdit->setText(thumbnailEvidence);
            if (m_thumbnailRootEdit->text().trimmed().isEmpty())
            {
                m_thumbnailRootEdit->setText(QFileInfo(thumbnailEvidence).absolutePath());
            }
        }
        if (!viewportEvidence.isEmpty())
        {
            m_viewportEvidenceEdit->setText(viewportEvidence);
        }

        const QStringList found = {
            paneModel.isEmpty() ? tr("Asset Browser model: missing") : tr("Asset Browser model: found"),
            thumbnailEvidence.isEmpty() ? tr("Thumbnail evidence: missing") : tr("Thumbnail evidence: found"),
            viewportEvidence.isEmpty() ? tr("Viewport evidence: missing") : tr("Viewport evidence: found")
        };
        SetStatus(found.join(QStringLiteral("; ")), paneModel.isEmpty());
    }

    void AssetBrowserPreviewWidget::LoadPreviewEvidence()
    {
        auto result = m_service.LoadPreview(BuildRequest());
        if (!result.IsSuccess())
        {
            m_snapshot = {};
            PopulateTree();
            SetStatus(ToQString(result.GetError()), true);
            return;
        }

        m_snapshot = result.TakeValue();
        m_categoryFilter->blockSignals(true);
        m_categoryFilter->clear();
        m_categoryFilter->addItem(tr("All categories"));
        for (const AZStd::string& category : m_snapshot.m_categories)
        {
            m_categoryFilter->addItem(ToQString(category));
        }
        m_categoryFilter->blockSignals(false);
        PopulateTree();
        SetStatus(tr("Loaded %1 preview asset entries.").arg(static_cast<qulonglong>(m_snapshot.m_entries.size())));
    }

    void AssetBrowserPreviewWidget::PopulateTree()
    {
        m_assetTree->clear();
        m_selectedEntryId.clear();
        m_routeButton->setEnabled(false);
        m_thumbnailLabel->clear();

        const QString selectedCategory = m_categoryFilter->currentText();
        QHash<QString, QTreeWidgetItem*> categories;
        for (int index = 0; index < static_cast<int>(m_snapshot.m_entries.size()); ++index)
        {
            const AssetBrowserPreviewEntry& entry = m_snapshot.m_entries[static_cast<size_t>(index)];
            const QString category = ToQString(entry.m_category);
            if (selectedCategory != tr("All categories") && selectedCategory != category)
            {
                continue;
            }

            QTreeWidgetItem* categoryItem = EnsureCategoryItem(m_assetTree, categories, category);
            auto* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, ToQString(entry.m_displayName));
            item->setText(1, ToQString(entry.m_fidelityState));
            item->setText(2, ToQString(entry.m_thumbnailStatus));
            item->setText(3, ToQString(entry.m_viewportRouteState));
            item->setText(4, ToQString(entry.m_productAssetId));
            item->setData(0, EntryIndexRole, index);
            if (!entry.m_thumbnailPath.empty())
            {
                QPixmap thumbnail(ToQString(entry.m_thumbnailPath));
                if (!thumbnail.isNull())
                {
                    item->setIcon(0, QIcon(thumbnail.scaled(
                        QSize(72, 72),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation)));
                }
            }
        }

        m_assetTree->expandAll();
        for (int column = 0; column < m_assetTree->columnCount(); ++column)
        {
            m_assetTree->resizeColumnToContents(column);
        }
    }

    void AssetBrowserPreviewWidget::ShowSelectedEntry(QTreeWidgetItem* current)
    {
        if (!current || !current->data(0, EntryIndexRole).isValid())
        {
            return;
        }

        const int index = current->data(0, EntryIndexRole).toInt();
        if (index < 0 || index >= static_cast<int>(m_snapshot.m_entries.size()))
        {
            return;
        }

        const AssetBrowserPreviewEntry& entry = m_snapshot.m_entries[static_cast<size_t>(index)];
        m_selectedEntryId = entry.m_entryId;
        m_identityValue->setText(ToQString(entry.m_displayName) + QStringLiteral(" [") + ToQString(entry.m_entryId) + ']');
        m_categoryValue->setText(ToQString(entry.m_category));
        m_fidelityValue->setText(ToQString(entry.m_fidelityState));
        m_routeValue->setText(ToQString(entry.m_viewportRouteState));
        m_productValue->setText(ToQString(entry.m_productCachePath));
        m_evidenceValue->setText(JoinValues(entry.m_evidenceRefs));
        m_blockerValue->setText(entry.m_blocker.empty() ? tr("None") : ToQString(entry.m_blocker));
        m_routeStatus->clear();
        m_routeButton->setEnabled(entry.m_canRouteToViewport);

        if (!entry.m_thumbnailPath.empty())
        {
            QPixmap thumbnail(ToQString(entry.m_thumbnailPath));
            if (!thumbnail.isNull())
            {
                m_thumbnailLabel->setPixmap(thumbnail.scaled(
                    m_thumbnailLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
                return;
            }
        }

        m_thumbnailLabel->setText(
            tr("%1\n%2")
                .arg(ToQString(entry.m_fidelityState))
                .arg(entry.m_thumbnailStatus.empty()
                    ? tr("No thumbnail evidence")
                    : ToQString(entry.m_thumbnailStatus)));
    }

    void AssetBrowserPreviewWidget::RouteSelectedEntry()
    {
        const auto iterator = AZStd::find_if(
            m_snapshot.m_entries.begin(),
            m_snapshot.m_entries.end(),
            [this](const AssetBrowserPreviewEntry& entry)
            {
                return entry.m_entryId == m_selectedEntryId;
            });
        if (iterator == m_snapshot.m_entries.end())
        {
            SetStatus(tr("Select a preview product before routing."), true);
            return;
        }
        if (!iterator->m_canRouteToViewport)
        {
            SetStatus(tr("The selected preview product is not routable to the central viewport."), true);
            return;
        }

        auto routeResult = m_service.PrepareViewportRoute(BuildRequest(), *iterator);
        if (!routeResult.IsSuccess())
        {
            SetStatus(ToQString(routeResult.GetError()), true);
            return;
        }

        const AssetBrowserPreviewViewportRoute route = routeResult.TakeValue();
        AZStd::string error;
        if (!AssetBrowserPreviewRouteRegistry::Get().RegisterRoute(route, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }

        m_routeStatus->setText(
            tr("Central viewport route %1 prepared for %2 with %3 fidelity. Typed binding and scene mutation remain disabled.")
                .arg(ToQString(route.m_routeId))
                .arg(ToQString(route.m_productAssetId))
                .arg(ToQString(route.m_fidelityState)));
    }

    void AssetBrowserPreviewWidget::BrowseForFile(QLineEdit* target, const QString& title)
    {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            title,
            m_extractedRootEdit->text(),
            tr("Evidence JSON (*.json);;All files (*)"));
        if (!filePath.isEmpty())
        {
            target->setText(filePath);
        }
    }

    void AssetBrowserPreviewWidget::BrowseForDirectory(QLineEdit* target, const QString& title)
    {
        const QString directory = QFileDialog::getExistingDirectory(
            this,
            title,
            target->text().trimmed().isEmpty() ? m_extractedRootEdit->text() : target->text());
        if (!directory.isEmpty())
        {
            target->setText(directory);
        }
    }

    void AssetBrowserPreviewWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }

    QString AssetBrowserPreviewWidget::FindEvidenceDocument(const QString& documentKind) const
    {
        const QDir root(m_extractedRootEdit->text().trimmed());
        if (!root.exists())
        {
            return {};
        }

        QStringList candidates;
        QDirIterator iterator(
            root.absolutePath(),
            QStringList({ QStringLiteral("*.json") }),
            QDir::Files,
            QDirIterator::Subdirectories);
        int scanned = 0;
        while (iterator.hasNext() && scanned < MaximumEvidenceScanFiles)
        {
            const QString path = iterator.next();
            ++scanned;
            const QFileInfo info(path);
            if (info.size() <= MaximumEvidenceDocumentBytes)
            {
                candidates.push_back(path);
            }
        }
        candidates.sort(Qt::CaseInsensitive);

        for (const QString& path : candidates)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                continue;
            }

            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError
                && document.isObject()
                && document.object().value(QStringLiteral("DocumentKind")).toString() == documentKind)
            {
                return QFileInfo(path).absoluteFilePath();
            }
        }
        return {};
    }

    AssetBrowserPreviewLoadRequest AssetBrowserPreviewWidget::BuildRequest() const
    {
        AssetBrowserPreviewLoadRequest request;
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        if (const GameProfile* profile = workspace.FindActiveGameProfile())
        {
            request.m_profileId = profile->m_profileId;
            request.m_gameVersion = profile->m_gameVersion;
            request.m_branch = profile->m_branch;
            request.m_runtimeTarget = profile->m_runtimeTarget;
        }
        request.m_extractedDataPath = ToAzString(m_extractedRootEdit->text());
        request.m_paneModelPath = ToAzString(m_paneModelEdit->text());
        request.m_thumbnailEvidencePath = ToAzString(m_thumbnailEvidenceEdit->text());
        request.m_thumbnailPreviewRootPath = ToAzString(m_thumbnailRootEdit->text());
        request.m_viewportEvidencePath = ToAzString(m_viewportEvidenceEdit->text());
        return request;
    }
} // namespace TaintedGrailModdingSDK
