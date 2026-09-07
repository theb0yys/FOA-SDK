/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetBrowserPreviewWidget.h"

#include "FoundationModels.h"
#include "FoundationService.h"
#include "NativeItemPreviewService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/Utils/Utils.h>

#include <QAbstractItemView>
#include <QComboBox>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QFile>
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
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QVBoxLayout>
#include <chrono>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr int EntryIndexRole = Qt::UserRole + 1;
        constexpr int MaximumEvidenceScanFiles = 2000;
        constexpr qint64 MaximumEvidenceDocumentBytes = 16 * 1024 * 1024;

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

    AssetBrowserPreviewWidget::AssetBrowserPreviewWidget(QWidget* parent, bool itemPreviewOnly)
        : QWidget(parent)
        , m_itemPreviewOnly(itemPreviewOnly)
    {
        // This pane can be the first SDK window opened after an Editor restart.
        // Load the saved workspace before subscribing; setup publishes notifications.
        if (FoundationService::Get().GetWorkspaceFilePath().empty())
        {
            FoundationService::Get().RefreshLocalSetup();
        }
        FoundationNotificationBus::Handler::BusConnect();

        setMinimumSize(itemPreviewOnly ? QSize(320, 300) : QSize(640, 480));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_nativePreviewService = new NativeItemPreviewService(this);
        m_previewLoadPool.setMaxThreadCount(1);

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Asset Browser Preview"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* profileGroup = new QGroupBox(tr("Active source"), this);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_profileValue = new QLabel(profileGroup);
        ConfigureValueLabel(m_profileValue);
        profileLayout->addRow(tr("Profile"), m_profileValue);
        rootLayout->addWidget(profileGroup);

        auto* sourceGroup = new QGroupBox(tr("Asset roots"), this);
        auto* sourceLayout = new QFormLayout(sourceGroup);
        m_gameInstallEdit = CreateEvidencePathEdit(
            sourceGroup,
            tr("Detected FoA installation"));
        m_gameInstallEdit->setReadOnly(true);
        m_gameInstallEdit->setClearButtonEnabled(false);
        m_customAssetsEdit = CreateEvidencePathEdit(
            sourceGroup,
            tr("Workspace Assets folder"));
        m_customAssetsEdit->setReadOnly(true);
        m_customAssetsEdit->setClearButtonEnabled(false);

        sourceLayout->addRow(tr("Game install"), m_gameInstallEdit);
        sourceLayout->addRow(tr("Custom Assets"), m_customAssetsEdit);
        rootLayout->addWidget(sourceGroup);

        auto* actionRow = new QWidget(this);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        m_refreshButton = new QPushButton(tr("Refresh assets"), actionRow);
        m_refreshButton->setObjectName(QStringLiteral("NativeItemRefreshButton"));
        m_loadButton = new QPushButton(tr("Load assets"), actionRow);
        actionLayout->addWidget(m_refreshButton);
        actionLayout->addWidget(m_loadButton);
        actionLayout->addStretch(1);
        rootLayout->addWidget(actionRow);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setObjectName(QStringLiteral("AssetPreviewStatus"));
        m_statusLabel->setWordWrap(true);
        ConfigureValueLabel(m_statusLabel);
        rootLayout->addWidget(m_statusLabel);

        auto* filterRow = new QHBoxLayout();
        m_categoryFilter = new QComboBox(this);
        m_categoryFilter->setObjectName(QStringLiteral("AssetPreviewCategory"));
        m_categoryFilter->setAccessibleName(tr("Item category"));
        m_categoryFilter->addItem(tr("All categories"), QString());
        m_subcategoryFilter = new QComboBox(this);
        m_subcategoryFilter->setObjectName(QStringLiteral("AssetPreviewSubcategory"));
        m_subcategoryFilter->setAccessibleName(tr("Item subcategory"));
        m_subcategoryFilter->addItem(tr("All subcategories"), QString());
        m_subcategoryFilter->setEnabled(false);
        filterRow->addWidget(new QLabel(tr("Category"), this));
        filterRow->addWidget(m_categoryFilter, 1);
        filterRow->addWidget(new QLabel(tr("Subcategory"), this));
        filterRow->addWidget(m_subcategoryFilter, 1);
        rootLayout->addLayout(filterRow);
        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setObjectName(QStringLiteral("AssetPreviewSearch"));
        m_searchEdit->setPlaceholderText(tr("Search items and assets"));
        m_searchEdit->setClearButtonEnabled(true);
        rootLayout->addWidget(m_searchEdit);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        m_assetTree = new QTreeWidget(splitter);
        m_assetTree->setObjectName(QStringLiteral("AssetPreviewTree"));
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
        m_thumbnailLabel->setObjectName(QStringLiteral("AssetPreviewImage"));
        m_thumbnailLabel->setMinimumSize(220, 220);
        m_thumbnailLabel->setAlignment(Qt::AlignCenter);
        m_thumbnailLabel->setFrameShape(QFrame::StyledPanel);
        m_thumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        inspectorLayout->addWidget(m_thumbnailLabel);
        m_itemCaption = new QLabel(inspector);
        m_itemCaption->setAlignment(Qt::AlignCenter);
        m_itemCaption->setWordWrap(true);
        m_itemCaption->setVisible(m_itemPreviewOnly);
        inspectorLayout->addWidget(m_itemCaption);

        auto* detailsGroup = new QGroupBox(tr("Selected item or asset"), inspector);
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

        if (m_itemPreviewOnly)
        {
            setObjectName(QStringLiteral("economyNativePreview"));
            m_thumbnailLabel->setObjectName(QStringLiteral("economyNativePreviewImage"));
            m_thumbnailLabel->setAccessibleName(tr("Selected item's game icon"));
            m_thumbnailLabel->setMinimumSize(320, 320);
            m_thumbnailLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            inspectorLayout->setStretch(0, 1);
            inspectorLayout->setStretch(inspectorLayout->count() - 1, 0);
            m_itemCaption->setObjectName(QStringLiteral("economyNativePreviewCaption"));
            m_statusLabel->setObjectName(QStringLiteral("economyNativePreviewStatus"));
            m_refreshButton->setObjectName(QStringLiteral("economyNativePreviewRefresh"));
            for (QWidget* widget : QList<QWidget*>{heading, profileGroup, sourceGroup,
                     m_loadButton, m_searchEdit, m_assetTree, detailsGroup, m_routeButton, m_routeStatus})
            {
                widget->hide();
            }
            for (int index = 0; index < filterRow->count(); ++index)
            {
                filterRow->itemAt(index)->widget()->hide();
            }
        }

        connect(m_refreshButton, &QPushButton::clicked, this, [this]() { RefreshAssets(); });
        connect(m_loadButton, &QPushButton::clicked, this, [this]() { AutoFindEvidence(); LoadPreviewEvidence(); });
        connect(m_categoryFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]()
        {
            RefreshSubcategoryFilter();
            PopulateTree();
        });
        connect(m_subcategoryFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { PopulateTree(); });
        auto* searchTimer = new QTimer(this);
        searchTimer->setSingleShot(true);
        searchTimer->setInterval(150);
        connect(m_searchEdit, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));
        connect(searchTimer, &QTimer::timeout, this, [this]() { PopulateTree(); });
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
        if (m_loadCancelled)
        {
            *m_loadCancelled = true;
        }
        m_previewLoadPool.clear();
        m_previewLoadPool.waitForDone();
        delete m_nativePreviewService;
    }

    void AssetBrowserPreviewWidget::OnFoundationChanged()
    {
        RefreshProfileContext();
    }

    void AssetBrowserPreviewWidget::SetItemTarget(const QString& recordId, const QString& nativeRefExact)
    {
        if (!m_itemPreviewOnly || (m_itemRecordId == recordId && m_itemNativeRef == nativeRefExact))
        {
            return;
        }
        m_itemRecordId = recordId;
        m_itemNativeRef = nativeRefExact;
        ShowItemThumbnail();
    }

    void AssetBrowserPreviewWidget::ShowItemThumbnail()
    {
        m_itemPixmap = {};
        m_thumbnailLabel->clear();
        m_thumbnailLabel->setProperty("nativeRefExact", QString());
        m_thumbnailLabel->setProperty("previewPath", QString());
        m_thumbnailLabel->setProperty("itemRecordId", m_itemRecordId);
        m_itemCaption->clear();
        const auto* entry = AssetBrowserPreviewService::FindItemThumbnail(m_snapshot, ToAzString(m_itemNativeRef));
        if (entry)
        {
            m_itemPixmap.load(ToQString(entry->m_thumbnailPath));
            if (!m_itemPixmap.isNull())
            {
                m_thumbnailLabel->setProperty("nativeRefExact", ToQString(entry->m_nativeAssetRef));
                m_thumbnailLabel->setProperty("previewPath", ToQString(entry->m_thumbnailPath));
                m_itemCaption->setText(ToQString(entry->m_displayName));
                ScaleItemThumbnail();
                return;
            }
        }
        m_thumbnailLabel->setText(m_itemRecordId.isEmpty() ? tr("Select an item or a recipe output to see its game icon.")
            : m_loading ? tr("Loading game icons...")
            : m_itemNativeRef.isEmpty() ? tr("This local item has no game icon. Use Custom visuals to choose an asset.")
            : tr("No supported game icon is available for this item. Refresh assets to check again."));
        m_thumbnailLabel->setWordWrap(true);
    }

    void AssetBrowserPreviewWidget::ScaleItemThumbnail()
    {
        if (!m_itemPixmap.isNull())
        {
            m_thumbnailLabel->setPixmap(m_itemPixmap.scaled(
                m_thumbnailLabel->contentsRect().size().boundedTo(QSize(512, 512)),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    void AssetBrowserPreviewWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        if (m_itemPreviewOnly)
        {
            ScaleItemThumbnail();
        }
    }

    void AssetBrowserPreviewWidget::RefreshAssets()
    {
        m_autoRefreshIfEmpty = false;
        if (m_loading)
        {
            *m_loadCancelled = true;
            ++m_loadGeneration;
            m_previewLoadPool.clear();
            m_loading = false;
            m_refreshButton->setText(tr("Refresh assets"));
            m_loadButton->setEnabled(true);
            SetStatus(tr("Item preview refresh cancelled. Previous previews are preserved."));
            return;
        }
        if (m_nativePreviewService->IsRunning())
        {
            m_nativePreviewService->Cancel();
            return;
        }
        const QString workspacePath = ToQString(FoundationService::Get().GetWorkspaceFilePath());
        m_refreshButton->setText(tr("Cancel refresh"));
        m_loadButton->setEnabled(false);
        SetStatus(tr("Reading installed items and generating icon previews..."));
        const QStringList context = m_profileContext;
        m_nativePreviewService->Start(workspacePath,
            [this, context](const QString& progress)
            {
                if (context == m_profileContext)
                {
                    SetStatus(progress);
                }
            },
            [this, context](const QString& manifest, const QString& error)
        {
            if (context != m_profileContext)
            {
                return;
            }
            m_refreshButton->setText(tr("Refresh assets"));
            m_loadButton->setEnabled(true);
            if (!error.isEmpty())
            {
                SetStatus(error, true);
                return;
            }
            m_thumbnailEvidencePath = manifest;
            LoadPreviewEvidence();
        });
    }

    void AssetBrowserPreviewWidget::RefreshProfileContext()
    {
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        QStringList context{ToQString(FoundationService::Get().GetWorkspaceFilePath())};
        if (profile)
        {
            context << ToQString(profile->m_profileId) << ToQString(profile->m_gameVersion)
                << ToQString(profile->m_branch) << ToQString(profile->m_runtimeTarget)
                << ToQString(profile->m_installPath) << ToQString(profile->m_extractedDataPath);
        }
        // Item saves publish Foundation notifications too. Keep the validated snapshot
        // until its source context changes, rather than scanning on every field edit.
        if (m_itemPreviewOnly && context == m_profileContext)
        {
            return;
        }
        m_profileContext = context;
        m_nativePreviewService->Cancel();
        if (m_loadCancelled)
        {
            *m_loadCancelled = true;
        }
        ++m_loadGeneration;
        m_previewLoadPool.clear();
        m_loading = false;
        m_refreshButton->setText(tr("Refresh assets"));
        m_loadButton->setEnabled(true);
        m_autoRefreshIfEmpty = true;
        m_snapshot = {};
        PopulateTree();
        if (!profile)
        {
            m_gameInstallEdit->clear();
            m_extractedRootPath.clear();
            m_paneModelPath.clear();
            m_thumbnailEvidencePath.clear();
            m_viewportEvidencePath.clear();
            m_profileValue->setText(tr("No active FoA profile"));
        }
        else
        {
            m_profileValue->setText(
                tr("%1 [%2] / %3 / %4 / %5")
                    .arg(ToQString(profile->m_displayName))
                    .arg(ToQString(profile->m_profileId))
                    .arg(ToQString(profile->m_gameVersion))
                    .arg(ToQString(profile->m_branch))
                    .arg(ToQString(profile->m_runtimeTarget)));
            m_gameInstallEdit->setText(ToQString(profile->m_installPath));
            m_extractedRootPath = ToQString(profile->m_extractedDataPath);
        }

        m_customAssetsEdit->setText(ResolveCustomAssetsRoot());
        if (m_itemPreviewOnly)
        {
            m_paneModelPath.clear();
            m_thumbnailEvidencePath.clear();
            m_viewportEvidencePath.clear();
            LoadPreviewEvidence();
            return;
        }
        AutoFindEvidence();
        if (!m_paneModelPath.isEmpty()
            || !m_thumbnailEvidencePath.isEmpty()
            || !m_customAssetsEdit->text().trimmed().isEmpty())
        {
            LoadPreviewEvidence();
        }
    }

    void AssetBrowserPreviewWidget::AutoFindEvidence()
    {
        m_paneModelPath.clear();
        m_thumbnailEvidencePath.clear();
        m_viewportEvidencePath.clear();

        const QString customAssetsRoot = m_customAssetsEdit->text().trimmed();
        const bool customAssetsAvailable = !customAssetsRoot.isEmpty()
            && QFileInfo(customAssetsRoot).isDir();
        if (m_extractedRootPath.trimmed().isEmpty())
        {
            SetStatus(customAssetsAvailable
                ? tr("Custom Assets folder found.")
                : tr("No game install evidence or custom Assets folder found."));
            return;
        }

        const QString paneModel = m_itemPreviewOnly ? QString()
            : FindEvidenceDocument(QStringLiteral("foa-asset-browser-pane-model"));
        const QString thumbnailEvidence = FindEvidenceDocument(QStringLiteral("foa-thumbnail-artifact-evidence"));
        const QString viewportEvidence = m_itemPreviewOnly ? QString()
            : FindEvidenceDocument(QStringLiteral("foa-3d-preview-viewport-render"));
        m_paneModelPath = paneModel;
        m_thumbnailEvidencePath = thumbnailEvidence;
        m_viewportEvidencePath = viewportEvidence;

        if (m_paneModelPath.isEmpty() && m_thumbnailEvidencePath.isEmpty() && !customAssetsAvailable)
        {
            SetStatus(tr("No in-game asset evidence or custom Assets files found."));
            return;
        }

        SetStatus(m_viewportEvidencePath.isEmpty()
            ? tr("Asset sources resolved.")
            : tr("Asset sources and viewport routes resolved."));
    }

    void AssetBrowserPreviewWidget::LoadPreviewEvidence()
    {
        if (m_loadCancelled)
        {
            *m_loadCancelled = true;
        }
        m_previewLoadPool.clear();
        m_loadCancelled = std::make_shared<std::atomic_bool>(false);
        const auto cancellation = m_loadCancelled;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        auto request = BuildRequest();
        request.m_isCancelled = [cancellation, deadline]()
        {
            return cancellation->load() || std::chrono::steady_clock::now() >= deadline;
        };
        const auto generation = ++m_loadGeneration;
        m_loading = true;
        if (m_itemPreviewOnly && m_itemPixmap.isNull())
        {
            ShowItemThumbnail();
        }
        m_refreshButton->setText(tr("Cancel refresh"));
        m_loadButton->setEnabled(false);
        SetStatus(tr("Checking and loading item previews..."));
        m_previewLoadPool.start([this, request, generation, itemPreviewOnly = m_itemPreviewOnly]() mutable
        {
            if (itemPreviewOnly && request.m_thumbnailEvidencePath.empty())
            {
                // Cache discovery can inspect large observation documents. It belongs
                // on the same bounded worker as validation, never in pane construction.
                request.m_thumbnailEvidencePath = ToAzString(
                    FindEvidenceDocument(request, QStringLiteral("foa-thumbnail-artifact-evidence")));
            }
            const QString thumbnailEvidencePath = ToQString(request.m_thumbnailEvidencePath);
            auto result = AssetBrowserPreviewService().LoadPreview(request);
            QMetaObject::invokeMethod(this, [this, generation, thumbnailEvidencePath, result = AZStd::move(result)]() mutable
            {
                if (generation != m_loadGeneration)
                {
                    return;
                }
                m_thumbnailEvidencePath = thumbnailEvidencePath;
                ApplyPreviewResult(AZStd::move(result));
            }, Qt::QueuedConnection);
        });
    }

    void AssetBrowserPreviewWidget::ApplyPreviewResult(AZ::Outcome<AssetBrowserPreviewSnapshot, AZStd::string> result)
    {
        m_loading = false;
        m_refreshButton->setText(tr("Refresh assets"));
        m_loadButton->setEnabled(true);
        const bool autoRefresh = m_autoRefreshIfEmpty;
        m_autoRefreshIfEmpty = false;
        if (!result.IsSuccess())
        {
            m_snapshot = {};
            PopulateTree();
            SetStatus(ToQString(result.GetError()), true);
            if (autoRefresh && !m_gameInstallEdit->text().isEmpty())
            {
                QTimer::singleShot(0, this, [this]() { RefreshAssets(); });
            }
            return;
        }

        m_snapshot = result.TakeValue();
        if (!m_itemPreviewOnly)
        {
            RebuildCategoryFilters();
        }
        PopulateTree();
        QString status = tr("Loaded %1 asset entries.").arg(static_cast<qulonglong>(m_snapshot.m_entries.size()));
        if (!m_snapshot.m_issues.empty())
        {
            status += QStringLiteral(" ");
            status += ToQString(m_snapshot.m_issues.front());
        }
        SetStatus(status);
        const bool missingIcons = m_itemPreviewOnly ? m_thumbnailEvidencePath.isEmpty() : m_snapshot.m_entries.empty();
        if (autoRefresh && missingIcons && !m_gameInstallEdit->text().isEmpty())
        {
            QTimer::singleShot(0, this, [this]() { RefreshAssets(); });
        }
    }

    void AssetBrowserPreviewWidget::RebuildCategoryFilters()
    {
        const QString previous = m_categoryFilter->currentData().toString();
        QMap<QString, int> counts;
        for (const auto& entry : m_snapshot.m_entries)
        {
            ++counts[ToQString(entry.m_category).section(QStringLiteral(" / "), 0, 0)];
        }
        m_categoryFilter->blockSignals(true);
        m_categoryFilter->clear();
        m_categoryFilter->addItem(tr("All categories (%1)").arg(m_snapshot.m_entries.size()), QString());
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        {
            m_categoryFilter->addItem(tr("%1 (%2)").arg(it.key()).arg(it.value()), it.key());
        }
        m_categoryFilter->setCurrentIndex(qMax(0, m_categoryFilter->findData(previous)));
        m_categoryFilter->blockSignals(false);
        RefreshSubcategoryFilter(true);
    }

    void AssetBrowserPreviewWidget::RefreshSubcategoryFilter(bool preserveSelection)
    {
        const QString previous = preserveSelection ? m_subcategoryFilter->currentData().toString() : QString();
        const QString group = m_categoryFilter->currentData().toString();
        QMap<QString, int> counts;
        int total = 0;
        if (!group.isEmpty())
        {
            for (const auto& entry : m_snapshot.m_entries)
            {
                const QString category = ToQString(entry.m_category);
                if (category.section(QStringLiteral(" / "), 0, 0) == group)
                {
                    ++counts[category];
                    ++total;
                }
            }
        }
        m_subcategoryFilter->blockSignals(true);
        m_subcategoryFilter->clear();
        m_subcategoryFilter->addItem(group.isEmpty() ? tr("All subcategories")
            : tr("All subcategories (%1)").arg(total), QString());
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        {
            const QString detail = it.key().section(QStringLiteral(" / "), 1);
            m_subcategoryFilter->addItem(tr("%1 (%2)").arg(detail.isEmpty() ? tr("General") : detail).arg(it.value()), it.key());
        }
        m_subcategoryFilter->setCurrentIndex(qMax(0, m_subcategoryFilter->findData(previous)));
        m_subcategoryFilter->setEnabled(!counts.isEmpty());
        m_subcategoryFilter->blockSignals(false);
    }

    void AssetBrowserPreviewWidget::PopulateTree()
    {
        if (m_itemPreviewOnly)
        {
            ShowItemThumbnail();
            return;
        }
        m_assetTree->clear();
        m_selectedEntryId.clear();
        m_routeButton->setEnabled(false);
        m_thumbnailLabel->clear();

        const QString selectedCategory = m_categoryFilter->currentData().toString();
        const QString selectedSubcategory = m_subcategoryFilter->currentData().toString();
        const QString search = m_searchEdit->text().trimmed();
        const bool hasModels = AZStd::any_of(m_snapshot.m_entries.begin(), m_snapshot.m_entries.end(),
            [](const AssetBrowserPreviewEntry& entry) { return entry.m_canRouteToViewport; });
        m_assetTree->setColumnHidden(3, !hasModels);
        m_assetTree->setColumnHidden(4, !hasModels);
        QHash<QString, QTreeWidgetItem*> categories;
        for (int index = 0; index < static_cast<int>(m_snapshot.m_entries.size()); ++index)
        {
            const AssetBrowserPreviewEntry& entry = m_snapshot.m_entries[static_cast<size_t>(index)];
            const QString category = ToQString(entry.m_category);
            if (!search.isEmpty() && !ToQString(entry.m_displayName).contains(search, Qt::CaseInsensitive)
                && !ToQString(entry.m_nativeAssetRef).contains(search, Qt::CaseInsensitive))
            {
                continue;
            }
            if ((!selectedCategory.isEmpty() && selectedCategory != category.section(QStringLiteral(" / "), 0, 0))
                || (!selectedSubcategory.isEmpty() && selectedSubcategory != category))
            {
                continue;
            }

            QTreeWidgetItem* categoryItem = EnsureCategoryItem(m_assetTree, categories, category);
            auto* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, ToQString(entry.m_displayName));
            item->setToolTip(0, ToQString(entry.m_displayName));
            item->setSizeHint(0, QSize(400, 80));
            item->setText(1, ToQString(entry.m_fidelityState));
            item->setText(2, ToQString(entry.m_thumbnailStatus));
            item->setText(3, ToQString(entry.m_viewportRouteState));
            item->setText(4, ToQString(entry.m_productAssetId));
            item->setData(0, EntryIndexRole, index);
            if (!entry.m_thumbnailPath.empty())
            {
                // QIcon loads visible rows lazily; filtering must not decode thousands of PNGs.
                item->setIcon(0, QIcon(ToQString(entry.m_thumbnailPath)));
            }
            if (item->icon(0).isNull())
            {
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            }
        }

        m_assetTree->expandAll();
        for (int column = 0; column < m_assetTree->columnCount(); ++column)
        {
            m_assetTree->resizeColumnToContents(column);
        }
        if (m_assetTree->topLevelItemCount() > 0 && m_assetTree->topLevelItem(0)->childCount() > 0)
        {
            m_assetTree->setCurrentItem(m_assetTree->topLevelItem(0)->child(0));
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
            QString());
        m_thumbnailLabel->setPixmap(
            style()->standardIcon(QStyle::SP_FileIcon).pixmap(QSize(160, 160)));
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

    void AssetBrowserPreviewWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }

    QString AssetBrowserPreviewWidget::ResolveCustomAssetsRoot() const
    {
        const auto activeProject = AZ::Utils::GetProjectPath();
        if (!activeProject.empty())
        {
            return QDir(QString::fromUtf8(activeProject.c_str())).filePath(QStringLiteral("Assets"));
        }
        const FoundationService& service = FoundationService::Get();
        const WorkspaceModel& workspace = service.GetWorkspace();

        QStringList projectRoots;
        const auto addProjectRoot = [&projectRoots](const QString& path)
        {
            const QString trimmed = path.trimmed();
            if (trimmed.isEmpty())
            {
                return;
            }
            const QString clean = QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
            if (!projectRoots.contains(clean, Qt::CaseInsensitive))
            {
                projectRoots.push_back(clean);
            }
        };

        addProjectRoot(ToQString(service.GetWorkspaceRootPath()));
        addProjectRoot(ToQString(workspace.m_rootPath));
        if (!service.GetWorkspaceFilePath().empty())
        {
            addProjectRoot(QFileInfo(ToQString(service.GetWorkspaceFilePath())).absolutePath());
        }

        const QDir appDir(QCoreApplication::applicationDirPath());
        addProjectRoot(appDir.filePath(QStringLiteral("../TaintedGrailModdingEditor")));
        addProjectRoot(appDir.filePath(QStringLiteral("../../TaintedGrailModdingEditor")));
        addProjectRoot(appDir.filePath(QStringLiteral("../../../TaintedGrailModdingEditor")));
        addProjectRoot(appDir.filePath(QStringLiteral("../../../../TaintedGrailModdingEditor")));
        addProjectRoot(QDir::current().filePath(QStringLiteral("TaintedGrailModdingEditor")));

        const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
        if (!localAppData.isEmpty())
        {
            addProjectRoot(
                QDir(localAppData).filePath(QStringLiteral("O3DE/TGEditor/installed/project")));
        }

        for (const QString& projectRoot : projectRoots)
        {
            if (QFileInfo(QDir(projectRoot).filePath(QStringLiteral("project.json"))).isFile())
            {
                return QFileInfo(QDir(projectRoot).filePath(QStringLiteral("Assets"))).absoluteFilePath();
            }
        }

        for (const QString& projectRoot : projectRoots)
        {
            const QFileInfo assets(QDir(projectRoot).filePath(QStringLiteral("Assets")));
            if (assets.isDir())
            {
                return assets.absoluteFilePath();
            }
        }

        return {};
    }

    QString AssetBrowserPreviewWidget::FindEvidenceDocument(const QString& documentKind) const
    {
        return FindEvidenceDocument(BuildRequest(), documentKind);
    }

    QString AssetBrowserPreviewWidget::FindEvidenceDocument(
        const AssetBrowserPreviewLoadRequest& request, const QString& documentKind)
    {
        const QDir root(ToQString(request.m_extractedDataPath));
        if (!root.exists())
        {
            return {};
        }
        if (request.m_profileId.empty()
            || request.m_gameVersion.empty()
            || request.m_branch.empty()
            || request.m_runtimeTarget.empty())
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
            if (request.m_isCancelled && request.m_isCancelled())
            {
                return {};
            }
            const QString path = iterator.next();
            ++scanned;
            const QFileInfo info(path);
            if (info.size() <= MaximumEvidenceDocumentBytes)
            {
                candidates.push_back(path);
            }
        }
        AZStd::sort(candidates.begin(), candidates.end(), [](const QString& left, const QString& right)
        {
            const auto leftTime = QFileInfo(left).lastModified();
            const auto rightTime = QFileInfo(right).lastModified();
            return leftTime != rightTime ? leftTime > rightTime : left > right;
        });

        for (const QString& path : candidates)
        {
            if (request.m_isCancelled && request.m_isCancelled())
            {
                return {};
            }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                continue;
            }

            QJsonParseError error;
            const QByteArray payload = file.read(MaximumEvidenceDocumentBytes + 1);
            if (payload.size() > MaximumEvidenceDocumentBytes)
            {
                continue;
            }
            const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                continue;
            }

            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("DocumentKind")).toString() != documentKind)
            {
                continue;
            }
            if (object.value(QStringLiteral("ProfileId")).toString() != ToQString(request.m_profileId)
                || object.value(QStringLiteral("GameVersion")).toString() != ToQString(request.m_gameVersion)
                || object.value(QStringLiteral("Branch")).toString() != ToQString(request.m_branch))
            {
                continue;
            }
            const QJsonValue runtimeTarget = object.value(QStringLiteral("RuntimeTarget"));
            if (runtimeTarget.isString()
                && runtimeTarget.toString() != ToQString(request.m_runtimeTarget))
            {
                continue;
            }
            return QFileInfo(path).absoluteFilePath();
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
            request.m_installPath = profile->m_installPath;
            request.m_extractedDataPath = profile->m_extractedDataPath;
        }
        if (request.m_extractedDataPath.empty())
        {
            request.m_extractedDataPath = ToAzString(m_extractedRootPath);
        }
        request.m_customAssetsPath = m_itemPreviewOnly ? AZStd::string() : ToAzString(m_customAssetsEdit->text());
        request.m_paneModelPath = ToAzString(m_paneModelPath);
        request.m_thumbnailEvidencePath = ToAzString(m_thumbnailEvidencePath);
        request.m_viewportEvidencePath = ToAzString(m_viewportEvidencePath);
        return request;
    }
} // namespace TaintedGrailModdingSDK
