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
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
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
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStringList>
#include <QStyle>
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
        auto* autoFindButton = new QPushButton(tr("Refresh assets"), actionRow);
        auto* loadButton = new QPushButton(tr("Load assets"), actionRow);
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

        const QString paneModel = FindEvidenceDocument(QStringLiteral("foa-asset-browser-pane-model"));
        const QString thumbnailEvidence = FindEvidenceDocument(QStringLiteral("foa-thumbnail-artifact-evidence"));
        const QString viewportEvidence = FindEvidenceDocument(QStringLiteral("foa-3d-preview-viewport-render"));
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
        QString status = tr("Loaded %1 asset entries.").arg(static_cast<qulonglong>(m_snapshot.m_entries.size()));
        if (!m_snapshot.m_issues.empty())
        {
            status += QStringLiteral(" ");
            status += ToQString(m_snapshot.m_issues.front());
        }
        SetStatus(status);
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
        const QDir root(m_extractedRootPath.trimmed());
        if (!root.exists())
        {
            return {};
        }
        const AssetBrowserPreviewLoadRequest request = BuildRequest();
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
        request.m_customAssetsPath = ToAzString(m_customAssetsEdit->text());
        request.m_paneModelPath = ToAzString(m_paneModelPath);
        request.m_thumbnailEvidencePath = ToAzString(m_thumbnailEvidencePath);
        request.m_viewportEvidencePath = ToAzString(m_viewportEvidencePath);
        return request;
    }
} // namespace TaintedGrailModdingSDK
