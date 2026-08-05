/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorAppearancePreviewWidget.h"

#include "ActorAppearanceBindingService.h"
#include "ActorTroopEditorWidget.h"
#include "CatalogDatabase.h"
#include "FoundationModels.h"
#include "FoundationService.h"
#include "PathPolicyService.h"

#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/AssetBrowser/Entries/ProductAssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/Previewer/PreviewerFrame.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr qint64 MaximumModelBytes = 16 * 1024 * 1024;
        constexpr int MaximumEntries = 10000;
        constexpr int EntryIndexRole = Qt::UserRole;

        AZStd::string ToAz(const QString& value)
        {
            const QByteArray bytes = value.trimmed().toUtf8();
            return AZStd::string(bytes.constData(), static_cast<size_t>(bytes.size()));
        }

        QString ToQt(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString StringValue(const QJsonObject& object, const char* key)
        {
            const QJsonValue value = object.value(QLatin1String(key));
            return value.isString() ? value.toString() : QString();
        }

        QString FirstString(const QJsonArray& values)
        {
            for (const QJsonValue& value : values)
            {
                if (value.isString() && !value.toString().trimmed().isEmpty())
                {
                    return value.toString().trimmed();
                }
            }
            return {};
        }

        QString Sha256(const QByteArray& payload)
        {
            return QString::fromLatin1(
                QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
        }

        bool RequireBool(const QJsonObject& object, const char* key, bool expected)
        {
            const QJsonValue value = object.value(QLatin1String(key));
            return value.isBool() && value.toBool() == expected;
        }

        bool AllFalse(const QJsonObject& object)
        {
            if (object.isEmpty())
            {
                return false;
            }
            for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator)
            {
                if (!iterator.value().isBool() || iterator.value().toBool())
                {
                    return false;
                }
            }
            return true;
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        bool PortraitCandidate(const QString& kind, const QString& path)
        {
            const QString lowerKind = kind.toLower();
            const QString lowerPath = path.toLower();
            return lowerKind.contains(QStringLiteral("texture"))
                || lowerKind.contains(QStringLiteral("image"))
                || lowerPath.endsWith(QStringLiteral(".streamingimage"));
        }

        bool ModelCandidate(const QString& kind)
        {
            const QString lower = kind.toLower();
            return lower.contains(QStringLiteral("model"))
                || lower.contains(QStringLiteral("mesh"))
                || lower.contains(QStringLiteral("prefab"));
        }
    } // namespace

    ActorAppearancePreviewWidget::ActorAppearancePreviewWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("TaintedGrailActorAppearancePreview"));
        auto* root = new QVBoxLayout(this);

        auto* boundary = new QLabel(
            tr("Editor-preview boundary: this tab previews exact O3DE products and existing equipment references only. It does not reconstruct an in-game actor, attach equipment to sockets, promote evidence, grant runtime permission, change saves, deploy, sign, or publish."),
            this);
        boundary->setObjectName(QStringLiteral("ActorAppearanceBoundaryWarning"));
        boundary->setWordWrap(true);
        root->addWidget(boundary);

        auto* target = new QGroupBox(tr("Actor and bounded preview model"), this);
        auto* form = new QFormLayout(target);
        m_actor = new QComboBox(target);
        m_actor->setObjectName(QStringLiteral("ActorAppearanceActorSelector"));
        m_actorSummary = new QLabel(target);
        m_actorSummary->setWordWrap(true);
        m_fidelity = new QLabel(target);
        m_fidelity->setObjectName(QStringLiteral("ActorAppearanceFidelity"));
        m_fidelity->setWordWrap(true);
        m_modelPath = new QLineEdit(target);
        m_modelPath->setObjectName(QStringLiteral("ActorAppearancePaneModelPath"));
        m_modelPath->setReadOnly(true);
        auto* buttons = new QWidget(target);
        auto* buttonLayout = new QHBoxLayout(buttons);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        auto* choose = new QPushButton(tr("Choose Model..."), buttons);
        m_reload = new QPushButton(tr("Reload"), buttons);
        m_reload->setEnabled(false);
        buttonLayout->addWidget(choose);
        buttonLayout->addWidget(m_reload);
        buttonLayout->addStretch(1);
        form->addRow(tr("Canonical actor"), m_actor);
        form->addRow(tr("Actor state"), m_actorSummary);
        form->addRow(tr("Preview fidelity"), m_fidelity);
        form->addRow(tr("Pane model"), m_modelPath);
        form->addRow(QString(), buttons);
        root->addWidget(target);

        m_search = new QLineEdit(this);
        m_search->setObjectName(QStringLiteral("ActorAppearanceProductFilter"));
        m_search->setClearButtonEnabled(true);
        m_search->setPlaceholderText(tr("Filter preview products by name, type, source, AssetId, or blocker"));
        root->addWidget(m_search);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        m_products = new QTableWidget(0, 6, splitter);
        m_products->setObjectName(QStringLiteral("ActorAppearanceProductTable"));
        m_products->setHorizontalHeaderLabels({
            tr("Name"), tr("Kind"), tr("Source"), tr("AssetId"), tr("Cache"), tr("Blockers") });
        ConfigureTable(m_products);
        m_previewer = new AzToolsFramework::AssetBrowser::PreviewerFrame(splitter);
        m_previewer->setObjectName(QStringLiteral("ActorAppearanceLivePreview"));
        m_previewer->setMinimumSize(360, 320);
        splitter->addWidget(m_products);
        splitter->addWidget(m_previewer);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        root->addWidget(splitter, 1);

        auto* equipmentGroup = new QGroupBox(tr("Read-only equipment reference preview"), this);
        auto* equipmentLayout = new QVBoxLayout(equipmentGroup);
        m_equipment = new QTableWidget(0, 6, equipmentGroup);
        m_equipment->setObjectName(QStringLiteral("ActorAppearanceEquipmentTable"));
        m_equipment->setHorizontalHeaderLabels({
            tr("Slot"), tr("Item"), tr("AssetId"), tr("Relationship"), tr("Current"), tr("Blockers") });
        ConfigureTable(m_equipment);
        equipmentLayout->addWidget(m_equipment);
        root->addWidget(equipmentGroup);

        auto* bindingGroup = new QGroupBox(tr("Explicit appearance binding"), this);
        auto* bindingLayout = new QHBoxLayout(bindingGroup);
        m_bindPortrait = new QPushButton(tr("Use Selected as Portrait Reference"), bindingGroup);
        m_bindPortrait->setObjectName(QStringLiteral("ActorAppearanceBindPortrait"));
        m_bindModel = new QPushButton(tr("Use Selected as Model Reference"), bindingGroup);
        m_bindModel->setObjectName(QStringLiteral("ActorAppearanceBindModel"));
        m_bindPortrait->setEnabled(false);
        m_bindModel->setEnabled(false);
        bindingLayout->addWidget(m_bindPortrait);
        bindingLayout->addWidget(m_bindModel);
        bindingLayout->addStretch(1);
        root->addWidget(bindingGroup);

        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("ActorAppearanceStatus"));
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        root->addWidget(m_status);

        connect(choose, &QPushButton::clicked, this, [this]() { ChooseModel(); });
        connect(m_reload, &QPushButton::clicked, this, [this]() { ReloadModel(); });
        connect(m_actor, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { RefreshActorView(); });
        connect(m_search, &QLineEdit::textChanged, this, [this]() { FilterEntries(); });
        connect(m_products, &QTableWidget::itemSelectionChanged, this, [this]() { RefreshSelection(); });
        connect(m_equipment, &QTableWidget::itemSelectionChanged, this, [this]()
        {
            const auto ranges = m_equipment->selectedRanges();
            if (ranges.isEmpty())
            {
                return;
            }
            const QString assetId = m_equipment->item(ranges.first().topRow(), 2)->text();
            if (const PreviewEntry* entry = FindEntryByAssetId(ToAz(assetId)))
            {
                for (int row = 0; row < m_products->rowCount(); ++row)
                {
                    const QTableWidgetItem* item = m_products->item(row, 3);
                    if (item && item->text() == entry->m_assetId)
                    {
                        m_products->selectRow(row);
                        break;
                    }
                }
            }
        });
        connect(m_bindPortrait, &QPushButton::clicked, this, [this]() { BindSelection(true); });
        connect(m_bindModel, &QPushButton::clicked, this, [this]() { BindSelection(false); });

        FoundationNotificationBus::Handler::BusConnect();
        RefreshActors();
        SetStatus(tr("Choose a pane model produced for the exact active FoA profile."));
    }

    ActorAppearancePreviewWidget::~ActorAppearancePreviewWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    ActorTroopEditorWithAppearanceWidget::ActorTroopEditorWithAppearanceWidget(QWidget* parent)
        : ActorTroopEditorWidget(parent)
    {
        AddFeatureTab(new ActorAppearancePreviewWidget(this), tr("Appearance Preview"));
    }

    void ActorAppearancePreviewWidget::OnFoundationChanged()
    {
        if (!ModelStillValid())
        {
            ClearModel(tr("The active profile or loaded pane model changed; the preview model was cleared."));
        }
        RefreshActors();
    }

    void ActorAppearancePreviewWidget::RefreshActors()
    {
        if (m_refreshing)
        {
            return;
        }
        m_refreshing = true;
        const QString previous = m_actor->currentData().toString();
        QSignalBlocker blocker(m_actor);
        m_actor->clear();
        m_actor->addItem(tr("Select canonical actor..."), QString());
        for (const CatalogRecord& record : FoundationService::Get().GetCatalog().GetRecords())
        {
            if (record.m_domain == "population" && record.m_recordKind == "actor")
            {
                QString label = ToQt(record.m_recordId);
                if (!record.m_displayName.empty())
                {
                    label += QStringLiteral(" - ") + ToQt(record.m_displayName);
                }
                m_actor->addItem(label, ToQt(record.m_recordId));
            }
        }
        const int restored = m_actor->findData(previous);
        m_actor->setCurrentIndex(restored >= 0 ? restored : 0);
        m_refreshing = false;
        RefreshActorView();
    }

    void ActorAppearancePreviewWidget::RefreshActorView()
    {
        const AZStd::string actorId = ToAz(m_actor->currentData().toString());
        const ActorAppearancePreviewView view = ActorAppearancePreviewService::BuildView(
            FoundationService::Get().GetCatalog(), actorId);
        m_actorSummary->setText(actorId.empty()
            ? tr("No actor selected.")
            : tr("Actor: %1 | subject: %2 | portrait: %3 | model: %4")
                .arg(ToQt(view.m_actorRecordId), ToQt(view.m_actorSubjectRef),
                    ToQt(view.m_portraitAssetRef), ToQt(view.m_modelAssetRef)));
        const QString state = view.m_state == ActorAppearancePreviewState::Blocked
            ? tr("blocked")
            : view.m_state == ActorAppearancePreviewState::Ready ? tr("ready") : tr("partial");
        m_fidelity->setText(
            tr("State: %1. Equipment remains reference-only; no skeletal/socket composition is claimed. Blockers: %2")
                .arg(state, view.m_blockers.empty() ? tr("none") : ToQt(view.m_blockers.front())));

        m_equipment->setRowCount(static_cast<int>(view.m_equipment.size()));
        for (int row = 0; row < static_cast<int>(view.m_equipment.size()); ++row)
        {
            const ActorEquipmentPreviewEntry& entry = view.m_equipment[static_cast<size_t>(row)];
            const QString blockers = entry.m_blockers.empty() ? QString() : ToQt(entry.m_blockers.front());
            m_equipment->setItem(row, 0, new QTableWidgetItem(ToQt(entry.m_slot)));
            m_equipment->setItem(row, 1, new QTableWidgetItem(ToQt(entry.m_itemRecordId)));
            m_equipment->setItem(row, 2, new QTableWidgetItem(ToQt(entry.m_itemAssetRef)));
            m_equipment->setItem(row, 3, new QTableWidgetItem(ToQt(entry.m_relationshipId)));
            m_equipment->setItem(row, 4, new QTableWidgetItem(entry.m_current ? tr("yes") : tr("no")));
            m_equipment->setItem(row, 5, new QTableWidgetItem(blockers));
        }
        RefreshSelection();
    }

    QString ActorAppearancePreviewWidget::ExtractedRoot() const
    {
        const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        return profile ? QFileInfo(ToQt(profile->m_extractedDataPath)).canonicalFilePath() : QString();
    }

    void ActorAppearancePreviewWidget::ChooseModel()
    {
        const QString root = ExtractedRoot();
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Choose FOA Asset Browser pane model"), root,
            tr("FOA pane model (foa-asset-browser-pane-model.json)"));
        if (!path.isEmpty())
        {
            LoadModel(path);
        }
    }

    void ActorAppearancePreviewWidget::ReloadModel()
    {
        LoadModel(m_modelPath->text());
    }

    bool ActorAppearancePreviewWidget::LoadModel(const QString& path)
    {
        ClearModel();
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const QString rootPath = ExtractedRoot();
        if (canonical.isEmpty() || rootPath.isEmpty()
            || QFileInfo(canonical).fileName() != QStringLiteral("foa-asset-browser-pane-model.json")
            || !PathPolicyService::IsCanonicalPathContained(ToAz(rootPath), ToAz(canonical), true))
        {
            SetStatus(tr("Pane model must be the canonical file below the active ExtractedDataPath."), true);
            return false;
        }
        QFile file(canonical);
        if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > MaximumModelBytes)
        {
            SetStatus(tr("Pane model must be readable and no larger than 16 MiB."), true);
            return false;
        }
        const QByteArray payload = file.readAll();
        file.close();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            SetStatus(tr("Pane model is not valid UTF-8 JSON."), true);
            return false;
        }
        const QJsonObject root = document.object();
        const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        if (!profile || root.value(QStringLiteral("SchemaVersion")).toInt(-1) != 1
            || StringValue(root, "DocumentKind") != QStringLiteral("foa-asset-browser-pane-model")
            || StringValue(root, "ProfileId") != ToQt(profile->m_profileId)
            || StringValue(root, "GameVersion") != ToQt(profile->m_gameVersion)
            || StringValue(root, "Branch") != ToQt(profile->m_branch)
            || StringValue(root, "RuntimeTarget") != ToQt(profile->m_runtimeTarget))
        {
            SetStatus(tr("Pane model schema or exact active-profile binding is invalid."), true);
            return false;
        }
        const QJsonObject contract = root.value(QStringLiteral("InputContract")).toObject();
        const QJsonObject stage = root.value(QStringLiteral("PreviewStageStatus")).toObject();
        if (!RequireBool(contract, "ImportProofEvidenceConsumed", true)
            || !RequireBool(contract, "RawConversionFileConsumed", false)
            || !RequireBool(contract, "RawO3dePreviewSourceConsumed", false)
            || !RequireBool(stage, "TypedAuthoringBindingCreated", false)
            || !RequireBool(stage, "FunctionCompleteAllowed", false)
            || !AllFalse(root.value(QStringLiteral("OperationalAuthority")).toObject()))
        {
            SetStatus(tr("Pane model input or operational-authority boundary is invalid."), true);
            return false;
        }
        const QJsonArray entries = root.value(QStringLiteral("PaneEntries")).toArray();
        if (entries.isEmpty() || entries.size() > MaximumEntries)
        {
            SetStatus(tr("Pane model must contain 1 to 10,000 entries."), true);
            return false;
        }

        QSet<QString> seen;
        AZStd::vector<PreviewEntry> parsed;
        parsed.reserve(static_cast<size_t>(entries.size()));
        for (const QJsonValue& value : entries)
        {
            if (!value.isObject())
            {
                SetStatus(tr("Pane entries must be objects."), true);
                return false;
            }
            const QJsonObject object = value.toObject();
            PreviewEntry entry;
            entry.m_entryId = StringValue(object, "PaneEntryId");
            entry.m_displayName = StringValue(object, "DisplayName");
            entry.m_entryKind = StringValue(object, "EntryKind");
            entry.m_productKind = StringValue(object, "ProductKind");
            entry.m_assetId = FirstString(object.value(QStringLiteral("ProductAssetIds")).toArray());
            entry.m_cachePath = FirstString(object.value(QStringLiteral("ProductCachePaths")).toArray());
            entry.m_sourceSubject = StringValue(object, "PrimarySourceAssetRecordId");
            entry.m_evidenceId = StringValue(object, "ProductEvidenceId");
            entry.m_issues = StringValue(object, "IssueText");
            const QJsonObject policy = object.value(QStringLiteral("SelectionPolicy")).toObject();
            if (entry.m_entryId.isEmpty() || seen.contains(entry.m_entryId)
                || !RequireBool(policy, "CanCreateTypedAuthoringBinding", false)
                || !RequireBool(policy, "RequiresExplicitBindingStep", true)
                || !RequireBool(policy, "CatalogPromotionAllowed", false)
                || !RequireBool(policy, "RuntimePermissionGranted", false))
            {
                SetStatus(tr("Pane entry identity or explicit-binding policy is invalid."), true);
                return false;
            }
            seen.insert(entry.m_entryId);
            if (entry.m_entryKind == QStringLiteral("o3de-preview-product"))
            {
                if (!AZ::Data::AssetId::CreateString(ToAz(entry.m_assetId)).IsValid()
                    || !entry.m_cachePath.startsWith(QStringLiteral("$assetcache/"))
                    || entry.m_sourceSubject.isEmpty()
                    || entry.m_evidenceId.isEmpty())
                {
                    SetStatus(tr("Preview products require AssetId, cache token, source identity, and evidence."), true);
                    return false;
                }
            }
            else if (entry.m_entryKind != QStringLiteral("o3de-import-failure"))
            {
                SetStatus(tr("Pane entry kind is unsupported."), true);
                return false;
            }
            parsed.push_back(AZStd::move(entry));
        }

        m_entries = AZStd::move(parsed);
        m_products->setRowCount(static_cast<int>(m_entries.size()));
        for (int row = 0; row < static_cast<int>(m_entries.size()); ++row)
        {
            const PreviewEntry& entry = m_entries[static_cast<size_t>(row)];
            auto* first = new QTableWidgetItem(entry.m_displayName);
            first->setData(EntryIndexRole, row);
            m_products->setItem(row, 0, first);
            m_products->setItem(row, 1, new QTableWidgetItem(entry.m_productKind));
            m_products->setItem(row, 2, new QTableWidgetItem(entry.m_sourceSubject));
            m_products->setItem(row, 3, new QTableWidgetItem(entry.m_assetId));
            m_products->setItem(row, 4, new QTableWidgetItem(entry.m_cachePath));
            m_products->setItem(row, 5, new QTableWidgetItem(entry.m_issues));
        }
        m_modelPath->setText(canonical);
        m_reload->setEnabled(true);
        m_profileId = StringValue(root, "ProfileId");
        m_gameVersion = StringValue(root, "GameVersion");
        m_branch = StringValue(root, "Branch");
        m_runtimeTarget = StringValue(root, "RuntimeTarget");
        m_modelSha256 = Sha256(payload);
        FilterEntries();
        RefreshActorView();
        SetStatus(tr("Loaded %1 bounded preview entries.").arg(static_cast<qulonglong>(m_entries.size())));
        return true;
    }

    void ActorAppearancePreviewWidget::ClearModel(const QString& reason)
    {
        m_entries.clear();
        m_products->clearContents();
        m_products->setRowCount(0);
        m_previewer->Clear();
        m_modelPath->clear();
        m_reload->setEnabled(false);
        m_profileId.clear();
        m_gameVersion.clear();
        m_branch.clear();
        m_runtimeTarget.clear();
        m_modelSha256.clear();
        m_bindPortrait->setEnabled(false);
        m_bindModel->setEnabled(false);
        if (!reason.isEmpty())
        {
            SetStatus(reason, true);
        }
    }

    bool ActorAppearancePreviewWidget::ModelStillValid() const
    {
        if (m_modelPath->text().isEmpty())
        {
            return true;
        }
        const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        QFile file(m_modelPath->text());
        if (!profile || m_profileId != ToQt(profile->m_profileId)
            || m_gameVersion != ToQt(profile->m_gameVersion)
            || m_branch != ToQt(profile->m_branch)
            || m_runtimeTarget != ToQt(profile->m_runtimeTarget)
            || !file.open(QIODevice::ReadOnly)
            || file.size() <= 0 || file.size() > MaximumModelBytes)
        {
            return false;
        }
        const QByteArray payload = file.readAll();
        return Sha256(payload) == m_modelSha256;
    }

    void ActorAppearancePreviewWidget::FilterEntries()
    {
        const QString filter = m_search->text().trimmed();
        for (int row = 0; row < m_products->rowCount(); ++row)
        {
            QString text;
            for (int column = 0; column < m_products->columnCount(); ++column)
            {
                if (const QTableWidgetItem* item = m_products->item(row, column))
                {
                    text += item->text() + QLatin1Char(' ');
                }
            }
            m_products->setRowHidden(row, !filter.isEmpty() && !text.contains(filter, Qt::CaseInsensitive));
        }
    }

    const ActorAppearancePreviewWidget::PreviewEntry* ActorAppearancePreviewWidget::SelectedEntry() const
    {
        const auto ranges = m_products->selectedRanges();
        if (ranges.isEmpty())
        {
            return nullptr;
        }
        const QTableWidgetItem* first = m_products->item(ranges.first().topRow(), 0);
        const int index = first ? first->data(EntryIndexRole).toInt() : -1;
        return index >= 0 && index < static_cast<int>(m_entries.size())
            ? &m_entries[static_cast<size_t>(index)] : nullptr;
    }

    const ActorAppearancePreviewWidget::PreviewEntry* ActorAppearancePreviewWidget::FindEntryByAssetId(
        const AZStd::string& assetId) const
    {
        for (const PreviewEntry& entry : m_entries)
        {
            if (ToAz(entry.m_assetId) == assetId)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    void ActorAppearancePreviewWidget::RefreshSelection()
    {
        m_previewer->Clear();
        m_bindPortrait->setEnabled(false);
        m_bindModel->setEnabled(false);
        if (!ModelStillValid())
        {
            ClearModel(tr("The pane model changed or no longer matches the active profile."));
            return;
        }
        const PreviewEntry* entry = SelectedEntry();
        if (!entry || entry->m_entryKind != QStringLiteral("o3de-preview-product"))
        {
            return;
        }
        const AZ::Data::AssetId assetId = AZ::Data::AssetId::CreateString(ToAz(entry->m_assetId));
        auto* product = AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry::GetProductByAssetId(assetId);
        if (!product)
        {
            SetStatus(tr("The product is not registered in the live O3DE Asset Browser."), true);
            return;
        }
        m_previewer->Display(product);
        m_bindPortrait->setEnabled(!m_actor->currentData().toString().isEmpty()
            && PortraitCandidate(entry->m_productKind, entry->m_cachePath));
        m_bindModel->setEnabled(!m_actor->currentData().toString().isEmpty()
            && ModelCandidate(entry->m_productKind));
        SetStatus(tr("Displaying the selected product through O3DE's registered previewer."));
    }

    void ActorAppearancePreviewWidget::BindSelection(bool portrait)
    {
        if (!ModelStillValid())
        {
            ClearModel(tr("The pane model changed before binding."));
            return;
        }
        const PreviewEntry* entry = SelectedEntry();
        if (!entry || m_actor->currentData().toString().isEmpty())
        {
            SetStatus(tr("Select an actor and an imported preview product."), true);
            return;
        }
        if ((portrait && !PortraitCandidate(entry->m_productKind, entry->m_cachePath))
            || (!portrait && !ModelCandidate(entry->m_productKind)))
        {
            SetStatus(tr("The selected product kind is incompatible with this appearance role."), true);
            return;
        }
        ActorAppearanceBindingRequest request;
        request.m_actorRecordId = ToAz(m_actor->currentData().toString());
        request.m_role = portrait ? ActorAppearanceBindingRole::Portrait : ActorAppearanceBindingRole::Model;
        request.m_productAssetId = ToAz(entry->m_assetId);
        request.m_sourceAssetSubjectRef = ToAz(entry->m_sourceSubject);
        request.m_productEvidenceIds = { ToAz(entry->m_evidenceId) };
        AZStd::string error;
        if (!FoundationService::Get().BindActorAppearancePreview(request, &error))
        {
            SetStatus(tr("Appearance binding failed: %1").arg(ToQt(error)), true);
            return;
        }
        SetStatus(tr("Appearance reference and provenance relationship were saved atomically."));
    }

    void ActorAppearancePreviewWidget::SetStatus(const QString& text, bool error)
    {
        m_status->setText((error ? tr("Error: ") : QString()) + text);
    }
} // namespace TaintedGrailModdingSDK
