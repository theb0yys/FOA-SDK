/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemVisualSelectorWidget.h"

#include "CatalogDatabase.h"
#include "FoundationModels.h"
#include "FoundationService.h"
#include "PathPolicyService.h"

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>
#include <AzToolsFramework/AssetBrowser/Entries/ProductAssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/Previewer/PreviewerFrame.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QList>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr qint64 MaximumModelBytes = 16 * 1024 * 1024;
        constexpr int MaximumPreviewEntries = 10000;
        constexpr int EntryIndexRole = Qt::UserRole;

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->setSortingEnabled(false);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        QTableWidgetItem* SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            auto* item = new QTableWidgetItem(value);
            table->setItem(row, column, item);
            return item;
        }

        QString JsonString(const QJsonObject& object, const char* key)
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

        QString IssueText(const QJsonArray& issues)
        {
            QStringList result;
            for (const QJsonValue& value : issues)
            {
                if (!value.isObject())
                {
                    continue;
                }
                const QJsonObject issue = value.toObject();
                const QString code = JsonString(issue, "Code");
                const QString message = JsonString(issue, "Message");
                if (!code.isEmpty() && !message.isEmpty())
                {
                    result.push_back(code + QStringLiteral(": ") + message);
                }
                else if (!message.isEmpty())
                {
                    result.push_back(message);
                }
                else if (!code.isEmpty())
                {
                    result.push_back(code);
                }
            }
            return result.join(QStringLiteral(" | "));
        }

        bool RequireBoolean(
            const QJsonObject& object,
            const char* key,
            bool expected,
            QString& error)
        {
            const QJsonValue value = object.value(QLatin1String(key));
            if (!value.isBool() || value.toBool() != expected)
            {
                error = QStringLiteral("%1 must be %2.")
                    .arg(QString::fromLatin1(key))
                    .arg(expected ? QStringLiteral("true") : QStringLiteral("false"));
                return false;
            }
            return true;
        }

        bool RequireAllFalse(const QJsonObject& object, QString& error)
        {
            if (object.isEmpty())
            {
                error = QStringLiteral("OperationalAuthority is required.");
                return false;
            }
            for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator)
            {
                if (!iterator.value().isBool() || iterator.value().toBool())
                {
                    error = QStringLiteral("Operational authority escalation: %1.")
                        .arg(iterator.key());
                    return false;
                }
            }
            return true;
        }

        bool IsIconCandidate(
            const QString& productKind,
            const QString& cachePath)
        {
            const QString normalizedKind = productKind.toLower();
            const QString normalizedPath = cachePath.toLower();
            return normalizedKind.contains(QStringLiteral("texture"))
                || normalizedKind.contains(QStringLiteral("image"))
                || normalizedPath.endsWith(QStringLiteral(".streamingimage"))
                || normalizedPath.endsWith(QStringLiteral(".png"))
                || normalizedPath.endsWith(QStringLiteral(".jpg"))
                || normalizedPath.endsWith(QStringLiteral(".jpeg"))
                || normalizedPath.endsWith(QStringLiteral(".webp"));
        }

        QString RecordLabel(const CatalogRecord& record)
        {
            QString label = ToQString(record.m_recordId);
            if (!record.m_displayName.empty())
            {
                label += QStringLiteral(" - ") + ToQString(record.m_displayName);
            }
            label += record.m_recordKind == "recipe"
                ? QStringLiteral(" [recipe]")
                : QStringLiteral(" [item]");
            return label;
        }
    } // namespace

    ItemVisualSelectorWidget::ItemVisualSelectorWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("TaintedGrailItemVisualSelector"));
        auto* rootLayout = new QVBoxLayout(this);

        auto* boundary = new QLabel(
            tr("Editor-preview boundary: this tab consumes an explicit, profile-matched Asset Browser pane model and uses O3DE's registered previewers. Selecting or previewing a product never grants runtime permission, changes FoA, promotes catalog evidence, deploys files, or creates a binding until an explicit button is pressed."),
            this);
        boundary->setWordWrap(true);
        boundary->setProperty("class", QStringLiteral("Card"));
        rootLayout->addWidget(boundary);

        auto* targetGroup = new QGroupBox(tr("Authoring target and bounded preview model"), this);
        auto* targetLayout = new QFormLayout(targetGroup);
        m_targetRecord = new QComboBox(targetGroup);
        m_targetRecord->setAccessibleName(tr("Canonical item or recipe target"));
        m_recipeItemLabel = new QLabel(tr("Recipe-linked item"), targetGroup);
        m_recipeItemRecord = new QComboBox(targetGroup);
        m_recipeItemRecord->setAccessibleName(tr("Recipe-linked item receiving the visual binding"));
        m_modelPath = new QLineEdit(targetGroup);
        m_modelPath->setReadOnly(true);
        m_modelPath->setAccessibleName(tr("Loaded Asset Browser pane model path"));
        m_modelPath->setPlaceholderText(tr("Choose foa-asset-browser-pane-model.json"));
        auto* modelButtons = new QWidget(targetGroup);
        auto* modelButtonLayout = new QHBoxLayout(modelButtons);
        modelButtonLayout->setContentsMargins(0, 0, 0, 0);
        auto* chooseModel = new QPushButton(tr("Choose Model..."), modelButtons);
        chooseModel->setAccessibleDescription(
            tr("Choose one bounded Asset Browser pane model below the active profile ExtractedDataPath."));
        m_reloadModel = new QPushButton(tr("Reload"), modelButtons);
        m_reloadModel->setEnabled(false);
        modelButtonLayout->addWidget(chooseModel);
        modelButtonLayout->addWidget(m_reloadModel);
        modelButtonLayout->addStretch(1);
        targetLayout->addRow(tr("Canonical target"), m_targetRecord);
        targetLayout->addRow(m_recipeItemLabel, m_recipeItemRecord);
        targetLayout->addRow(tr("Pane model"), m_modelPath);
        targetLayout->addRow(QString(), modelButtons);
        rootLayout->addWidget(targetGroup);

        m_search = new QLineEdit(this);
        m_search->setClearButtonEnabled(true);
        m_search->setPlaceholderText(tr("Filter by name, type, source identity, AssetId, cache path, or blocker"));
        m_search->setAccessibleName(tr("Preview entry filter"));
        rootLayout->addWidget(m_search);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        m_entryTable = new QTableWidget(0, 6, splitter);
        m_entryTable->setHorizontalHeaderLabels({
            tr("Name"),
            tr("Preview state"),
            tr("Product kind"),
            tr("Source record"),
            tr("O3DE AssetId"),
            tr("Blockers") });
        ConfigureTable(m_entryTable);
        m_entryTable->setAccessibleName(tr("Evidence-backed preview products"));
        m_previewer = new AzToolsFramework::AssetBrowser::PreviewerFrame(splitter);
        m_previewer->setMinimumSize(360, 320);
        m_previewer->setAccessibleName(tr("Live O3DE item preview"));
        splitter->addWidget(m_entryTable);
        splitter->addWidget(m_previewer);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);
        rootLayout->addWidget(splitter, 1);

        auto* informationGroup = new QGroupBox(tr("Selection and explicit binding"), this);
        auto* informationLayout = new QVBoxLayout(informationGroup);
        m_modelInfo = new QLabel(tr("No preview model loaded."), informationGroup);
        m_selectionInfo = new QLabel(tr("No preview product selected."), informationGroup);
        m_bindingInfo = new QLabel(tr("Select a canonical target."), informationGroup);
        m_modelInfo->setWordWrap(true);
        m_selectionInfo->setWordWrap(true);
        m_bindingInfo->setWordWrap(true);
        m_modelInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_selectionInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_bindingInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        informationLayout->addWidget(m_modelInfo);
        informationLayout->addWidget(m_selectionInfo);
        informationLayout->addWidget(m_bindingInfo);
        auto* bindingButtons = new QWidget(informationGroup);
        auto* bindingButtonLayout = new QHBoxLayout(bindingButtons);
        bindingButtonLayout->setContentsMargins(0, 0, 0, 0);
        m_applyIcon = new QPushButton(tr("Use Selected as Icon Reference"), bindingButtons);
        m_applyAsset = new QPushButton(tr("Use Selected as Asset Reference"), bindingButtons);
        m_applyIcon->setEnabled(false);
        m_applyAsset->setEnabled(false);
        m_applyIcon->setAccessibleDescription(
            tr("Explicitly writes the selected preview source identity into the existing typed item icon reference."));
        m_applyAsset->setAccessibleDescription(
            tr("Explicitly writes the selected O3DE preview AssetId into the existing typed item asset reference."));
        bindingButtonLayout->addWidget(m_applyIcon);
        bindingButtonLayout->addWidget(m_applyAsset);
        bindingButtonLayout->addStretch(1);
        informationLayout->addWidget(bindingButtons);
        rootLayout->addWidget(informationGroup);

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rootLayout->addWidget(m_status);

        connect(chooseModel, &QPushButton::clicked, this, [this]() { ChoosePreviewModel(); });
        connect(m_reloadModel, &QPushButton::clicked, this, [this]() { ReloadPreviewModel(); });
        connect(m_search, &QLineEdit::textChanged, this, [this]() { ApplySearchFilter(); });
        connect(m_entryTable, &QTableWidget::itemSelectionChanged, this, [this]() { RefreshSelection(); });
        connect(m_targetRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int)
        {
            RefreshRecipeItemChoices();
            RefreshBindingSummary();
        });
        connect(m_recipeItemRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int)
        {
            RefreshBindingSummary();
        });
        connect(m_applyIcon, &QPushButton::clicked, this, [this]() { ApplySelectionAsIcon(); });
        connect(m_applyAsset, &QPushButton::clicked, this, [this]() { ApplySelectionAsAsset(); });

        FoundationNotificationBus::Handler::BusConnect();
        RefreshTargetChoices();
        SetStatus(tr("Choose an Asset Browser pane model produced for the exact active FoA profile."));
    }

    ItemVisualSelectorWidget::~ItemVisualSelectorWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void ItemVisualSelectorWidget::OnFoundationChanged()
    {
        if (!LoadedModelMatchesActiveProfile())
        {
            ClearLoadedModel(
                tr("The active FoA profile or ExtractedDataPath changed. The previously loaded preview model was cleared and must be reselected."));
        }
        RefreshTargetChoices();
    }

    void ItemVisualSelectorWidget::RefreshTargetChoices()
    {
        if (m_refreshing)
        {
            return;
        }
        m_refreshing = true;
        const QString previous = m_targetRecord->currentData().toString();
        QSignalBlocker blocker(m_targetRecord);
        m_targetRecord->clear();
        m_targetRecord->addItem(tr("Select canonical item or recipe..."), QString());
        for (const CatalogRecord& record : FoundationService::Get().GetCatalog().GetRecords())
        {
            if (record.m_domain == "economy"
                && (record.m_recordKind == "item" || record.m_recordKind == "recipe"))
            {
                m_targetRecord->addItem(RecordLabel(record), ToQString(record.m_recordId));
            }
        }
        const int restored = m_targetRecord->findData(previous);
        m_targetRecord->setCurrentIndex(restored >= 0 ? restored : 0);
        m_refreshing = false;
        RefreshRecipeItemChoices();
        RefreshBindingSummary();
    }

    void ItemVisualSelectorWidget::RefreshRecipeItemChoices()
    {
        const QString previous = m_recipeItemRecord->currentData().toString();
        QSignalBlocker blocker(m_recipeItemRecord);
        m_recipeItemRecord->clear();
        m_recipeItemRecord->addItem(tr("Select recipe-linked item..."), QString());

        const AZStd::string targetId = ToAzString(m_targetRecord->currentData().toString());
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const CatalogRecord* target = catalog.FindByRecordId(targetId);
        const bool recipeContext = target && target->m_domain == "economy" && target->m_recordKind == "recipe";
        m_recipeItemLabel->setVisible(recipeContext);
        m_recipeItemRecord->setVisible(recipeContext);
        if (!recipeContext)
        {
            return;
        }

        QSet<QString> linkedItems;
        for (const EconomyRecipeIngredient& ingredient : catalog.FindIngredientsForRecipe(targetId))
        {
            if (!ingredient.m_itemRecordId.empty())
            {
                linkedItems.insert(ToQString(ingredient.m_itemRecordId));
            }
        }
        for (const EconomyRecipeOutput& output : catalog.FindOutputsForRecipe(targetId))
        {
            if (!output.m_itemRecordId.empty())
            {
                linkedItems.insert(ToQString(output.m_itemRecordId));
            }
        }

        QStringList sorted = linkedItems.values();
        sorted.sort(Qt::CaseInsensitive);
        for (const QString& itemId : sorted)
        {
            const CatalogRecord* item = catalog.FindByRecordId(ToAzString(itemId));
            if (item && item->m_domain == "economy" && item->m_recordKind == "item")
            {
                m_recipeItemRecord->addItem(RecordLabel(*item), itemId);
            }
        }
        const int restored = m_recipeItemRecord->findData(previous);
        m_recipeItemRecord->setCurrentIndex(restored >= 0 ? restored : 0);
    }

    void ItemVisualSelectorWidget::RefreshBindingSummary()
    {
        const QString itemId = ResolveBindingItemRecordId();
        const PreviewEntry* selected = GetSelectedEntry();
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const EconomyItemProfile* profile = itemId.isEmpty()
            ? nullptr
            : catalog.FindEconomyItem(ToAzString(itemId));

        if (itemId.isEmpty())
        {
            const CatalogRecord* target = catalog.FindByRecordId(
                ToAzString(m_targetRecord->currentData().toString()));
            m_bindingInfo->setText(
                target && target->m_recordKind == "recipe"
                    ? tr("Recipe profiles do not own visual fields in the current schema. Select one resolved ingredient or output item; the explicit binding will update that item's existing typed profile without changing recipe identity.")
                    : tr("Select a canonical item target."));
        }
        else if (!profile)
        {
            m_bindingInfo->setText(
                tr("%1 has no evidence-backed typed item profile. Create and save that profile first; this selector will not invent a profile or evidence.")
                    .arg(itemId));
        }
        else
        {
            m_bindingInfo->setText(
                tr("Binding target: %1 | current icon ref: %2 | current asset ref: %3")
                    .arg(itemId)
                    .arg(profile->m_iconRef.empty() ? tr("unset") : ToQString(profile->m_iconRef))
                    .arg(profile->m_assetRef.empty() ? tr("unset") : ToQString(profile->m_assetRef)));
        }

        const bool productSelected = selected
            && selected->m_entryKind == QStringLiteral("o3de-preview-product")
            && !selected->m_productAssetId.isEmpty();
        const bool canBind = productSelected
            && profile
            && LoadedModelMatchesActiveProfile();
        m_applyAsset->setEnabled(canBind);
        m_applyIcon->setEnabled(
            canBind
            && IsIconCandidate(selected->m_productKind, selected->m_productCachePath));
    }

    void ItemVisualSelectorWidget::ChoosePreviewModel()
    {
        const QString extractedRoot = ResolveExtractedDataRoot();
        if (extractedRoot.isEmpty())
        {
            SetStatus(
                tr("Save a workspace and configure an exact active FoA profile with a valid ExtractedDataPath first."),
                true);
            return;
        }
        QString initial = QDir(extractedRoot).filePath(QStringLiteral("PreviewArtifacts/AssetBrowser"));
        if (!QFileInfo(initial).isDir())
        {
            initial = extractedRoot;
        }
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Choose FOA Asset Browser Pane Model"),
            initial,
            tr("FOA pane model (foa-asset-browser-pane-model.json);;JSON files (*.json)"));
        if (!path.isEmpty())
        {
            LoadPreviewModel(path);
        }
    }

    void ItemVisualSelectorWidget::ReloadPreviewModel()
    {
        if (m_modelPath->text().trimmed().isEmpty())
        {
            SetStatus(tr("Choose a preview model before reloading."), true);
            return;
        }
        const QString path = m_modelPath->text();
        LoadPreviewModel(path);
    }

    bool ItemVisualSelectorWidget::LoadPreviewModel(const QString& path)
    {
        ClearLoadedModel();

        const QString extractedRoot = ResolveExtractedDataRoot();
        const QFileInfo requested(path);
        const QString canonicalPath = requested.canonicalFilePath();
        if (extractedRoot.isEmpty() || canonicalPath.isEmpty() || !QFileInfo(canonicalPath).isFile())
        {
            SetStatus(tr("The selected pane model is unavailable."), true);
            return false;
        }
        if (QFileInfo(canonicalPath).fileName() != QStringLiteral("foa-asset-browser-pane-model.json"))
        {
            SetStatus(tr("The selected file is not the canonical FOA Asset Browser pane-model filename."), true);
            return false;
        }
        if (!PathPolicyService::IsCanonicalPathContained(
                ToAzString(extractedRoot),
                ToAzString(canonicalPath),
                true))
        {
            SetStatus(tr("The selected pane model is outside the active profile ExtractedDataPath."), true);
            return false;
        }

        QFile file(canonicalPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            SetStatus(tr("Unable to open the selected pane model."), true);
            return false;
        }
        if (file.size() <= 0 || file.size() > MaximumModelBytes)
        {
            SetStatus(
                tr("Pane model size must be between 1 byte and %1 bytes.")
                    .arg(MaximumModelBytes),
                true);
            return false;
        }
        const QByteArray payload = file.readAll();
        file.close();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            SetStatus(
                tr("Pane model is not valid UTF-8 JSON: %1")
                    .arg(parseError.errorString()),
                true);
            return false;
        }
        const QJsonObject root = document.object();
        if (root.value(QStringLiteral("SchemaVersion")).toInt(-1) != 1
            || JsonString(root, "DocumentKind") != QStringLiteral("foa-asset-browser-pane-model"))
        {
            SetStatus(tr("Input must be a SchemaVersion 1 FOA Asset Browser pane model."), true);
            return false;
        }

        const GameProfile* activeProfile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        if (!activeProfile
            || JsonString(root, "ProfileId") != ToQString(activeProfile->m_profileId)
            || JsonString(root, "GameVersion") != ToQString(activeProfile->m_gameVersion)
            || JsonString(root, "Branch") != ToQString(activeProfile->m_branch)
            || JsonString(root, "RuntimeTarget") != ToQString(activeProfile->m_runtimeTarget))
        {
            SetStatus(tr("Pane model does not match the exact active profile, version, branch, and runtime target."), true);
            return false;
        }

        QString contractError;
        const QJsonObject contract = root.value(QStringLiteral("InputContract")).toObject();
        if (!RequireBoolean(contract, "ImportProofEvidenceConsumed", true, contractError)
            || !RequireBoolean(contract, "RawConversionFileConsumed", false, contractError)
            || !RequireBoolean(contract, "RawO3dePreviewSourceConsumed", false, contractError))
        {
            SetStatus(tr("Pane-model input contract failed: %1").arg(contractError), true);
            return false;
        }
        const QJsonObject stage = root.value(QStringLiteral("PreviewStageStatus")).toObject();
        if (!RequireBoolean(stage, "O3deAssetBrowserEntryCreated", false, contractError)
            || !RequireBoolean(stage, "TypedAuthoringBindingCreated", false, contractError)
            || !RequireBoolean(stage, "FunctionCompleteAllowed", false, contractError))
        {
            SetStatus(tr("Pane-model stage boundary failed: %1").arg(contractError), true);
            return false;
        }
        if (!RequireAllFalse(
                root.value(QStringLiteral("OperationalAuthority")).toObject(),
                contractError))
        {
            SetStatus(tr("Pane-model authority boundary failed: %1").arg(contractError), true);
            return false;
        }

        const QJsonArray entries = root.value(QStringLiteral("PaneEntries")).toArray();
        if (entries.isEmpty() || entries.size() > MaximumPreviewEntries)
        {
            SetStatus(
                tr("Pane model must contain between 1 and %1 preview entries.")
                    .arg(MaximumPreviewEntries),
                true);
            return false;
        }

        QSet<QString> seenEntryIds;
        AZStd::vector<PreviewEntry> parsedEntries;
        parsedEntries.reserve(static_cast<size_t>(entries.size()));
        for (const QJsonValue& value : entries)
        {
            if (!value.isObject())
            {
                SetStatus(tr("PaneEntries must contain JSON objects only."), true);
                return false;
            }
            const QJsonObject object = value.toObject();
            PreviewEntry entry;
            entry.m_paneEntryId = JsonString(object, "PaneEntryId");
            entry.m_displayName = JsonString(object, "DisplayName");
            entry.m_entryKind = JsonString(object, "EntryKind");
            entry.m_previewAvailability = JsonString(object, "PreviewAvailability");
            entry.m_productKind = JsonString(object, "ProductKind");
            entry.m_productAssetId = FirstString(
                object.value(QStringLiteral("ProductAssetIds")).toArray());
            entry.m_productCachePath = FirstString(
                object.value(QStringLiteral("ProductCachePaths")).toArray());
            entry.m_primarySourceAssetRecordId = JsonString(
                object,
                "PrimarySourceAssetRecordId");
            entry.m_productEvidenceId = JsonString(object, "ProductEvidenceId");
            entry.m_issueText = IssueText(object.value(QStringLiteral("Issues")).toArray());
            entry.m_previewRenderVerified = object.value(
                QStringLiteral("PreviewRenderVerified")).toBool(false);

            if (entry.m_paneEntryId.isEmpty() || seenEntryIds.contains(entry.m_paneEntryId))
            {
                SetStatus(tr("Every pane entry requires one unique stable PaneEntryId."), true);
                return false;
            }
            seenEntryIds.insert(entry.m_paneEntryId);

            const QJsonObject policy = object.value(QStringLiteral("SelectionPolicy")).toObject();
            if (!RequireBoolean(policy, "CanCreateTypedAuthoringBinding", false, contractError)
                || !RequireBoolean(policy, "RequiresExplicitBindingStep", true, contractError)
                || !RequireBoolean(policy, "CatalogPromotionAllowed", false, contractError)
                || !RequireBoolean(policy, "RuntimePermissionGranted", false, contractError)
                || !RequireBoolean(policy, "RepositoryCommitAllowed", false, contractError)
                || !RequireBoolean(policy, "RedistributionAllowed", false, contractError))
            {
                SetStatus(
                    tr("Pane entry %1 violates the explicit-selection boundary: %2")
                        .arg(entry.m_paneEntryId, contractError),
                    true);
                return false;
            }

            if (entry.m_entryKind == QStringLiteral("o3de-preview-product"))
            {
                const AZ::Data::AssetId assetId = AZ::Data::AssetId::CreateString(
                    ToAzString(entry.m_productAssetId));
                if (!assetId.IsValid()
                    || !entry.m_productCachePath.startsWith(QStringLiteral("$assetcache/")))
                {
                    SetStatus(
                        tr("Pane entry %1 requires one valid O3DE AssetId and a tokenized product cache path.")
                            .arg(entry.m_paneEntryId),
                        true);
                    return false;
                }
            }
            else if (entry.m_entryKind != QStringLiteral("o3de-import-failure"))
            {
                SetStatus(
                    tr("Pane entry %1 uses unsupported kind %2.")
                        .arg(entry.m_paneEntryId, entry.m_entryKind),
                    true);
                return false;
            }
            parsedEntries.push_back(AZStd::move(entry));
        }

        m_entries = AZStd::move(parsedEntries);
        m_entryTable->setRowCount(static_cast<int>(m_entries.size()));
        for (int row = 0; row < static_cast<int>(m_entries.size()); ++row)
        {
            const PreviewEntry& entry = m_entries[static_cast<size_t>(row)];
            QTableWidgetItem* first = SetCell(m_entryTable, row, 0, entry.m_displayName);
            first->setData(EntryIndexRole, row);
            SetCell(m_entryTable, row, 1, entry.m_previewAvailability);
            SetCell(m_entryTable, row, 2, entry.m_productKind);
            SetCell(m_entryTable, row, 3, entry.m_primarySourceAssetRecordId);
            SetCell(m_entryTable, row, 4, entry.m_productAssetId);
            SetCell(m_entryTable, row, 5, entry.m_issueText);
        }
        m_entryTable->resizeColumnsToContents();
        m_modelPath->setText(canonicalPath);
        m_reloadModel->setEnabled(true);
        m_loadedProfileId = JsonString(root, "ProfileId");
        m_loadedGameVersion = JsonString(root, "GameVersion");
        m_loadedBranch = JsonString(root, "Branch");
        m_loadedRuntimeTarget = JsonString(root, "RuntimeTarget");
        m_modelInfo->setText(
            tr("Model: %1 | profile: %2 | entries: %3 | captured: %4")
                .arg(JsonString(root, "AssetBrowserModelId"))
                .arg(m_loadedProfileId)
                .arg(static_cast<qulonglong>(m_entries.size()))
                .arg(JsonString(root, "CapturedAt")));
        ApplySearchFilter();
        SetStatus(
            tr("Loaded %1 bounded preview entries. Selection remains non-authoritative until an explicit binding action succeeds.")
                .arg(static_cast<qulonglong>(m_entries.size())));
        return true;
    }

    void ItemVisualSelectorWidget::ClearLoadedModel(const QString& reason)
    {
        m_entries.clear();
        m_entryTable->clearContents();
        m_entryTable->setRowCount(0);
        m_previewer->Clear();
        m_modelPath->clear();
        m_reloadModel->setEnabled(false);
        m_loadedProfileId.clear();
        m_loadedGameVersion.clear();
        m_loadedBranch.clear();
        m_loadedRuntimeTarget.clear();
        m_modelInfo->setText(tr("No preview model loaded."));
        m_selectionInfo->setText(tr("No preview product selected."));
        RefreshBindingSummary();
        if (!reason.isEmpty())
        {
            SetStatus(reason, true);
        }
    }

    bool ItemVisualSelectorWidget::LoadedModelMatchesActiveProfile() const
    {
        if (m_modelPath->text().isEmpty())
        {
            return true;
        }

        const GameProfile* profile = FoundationService::Get().GetWorkspace().FindActiveGameProfile();
        if (!profile
            || m_loadedProfileId != ToQString(profile->m_profileId)
            || m_loadedGameVersion != ToQString(profile->m_gameVersion)
            || m_loadedBranch != ToQString(profile->m_branch)
            || m_loadedRuntimeTarget != ToQString(profile->m_runtimeTarget))
        {
            return false;
        }

        const QString extractedRoot = ResolveExtractedDataRoot();
        const QString modelPath = QFileInfo(m_modelPath->text()).canonicalFilePath();
        return !extractedRoot.isEmpty()
            && !modelPath.isEmpty()
            && PathPolicyService::IsCanonicalPathContained(
                ToAzString(extractedRoot),
                ToAzString(modelPath),
                true);
    }

    void ItemVisualSelectorWidget::ApplySearchFilter()
    {
        const QString filter = m_search->text().trimmed();
        for (int row = 0; row < m_entryTable->rowCount(); ++row)
        {
            QStringList searchable;
            for (int column = 0; column < m_entryTable->columnCount(); ++column)
            {
                const QTableWidgetItem* item = m_entryTable->item(row, column);
                if (item)
                {
                    searchable.push_back(item->text());
                }
            }
            m_entryTable->setRowHidden(
                row,
                !filter.isEmpty()
                    && !searchable.join(QStringLiteral(" ")).contains(
                        filter,
                        Qt::CaseInsensitive));
        }
    }

    void ItemVisualSelectorWidget::RefreshSelection()
    {
        if (!LoadedModelMatchesActiveProfile())
        {
            ClearLoadedModel(
                tr("The loaded preview model no longer matches the active FoA profile or ExtractedDataPath."));
            return;
        }

        const PreviewEntry* selected = GetSelectedEntry();
        m_previewer->Clear();
        if (!selected)
        {
            m_selectionInfo->setText(tr("No preview product selected."));
            RefreshBindingSummary();
            return;
        }

        QString information = tr("%1 | source: %2 | AssetId: %3 | cache: %4 | render proof: %5")
            .arg(selected->m_displayName)
            .arg(selected->m_primarySourceAssetRecordId.isEmpty()
                    ? tr("unresolved")
                    : selected->m_primarySourceAssetRecordId)
            .arg(selected->m_productAssetId.isEmpty()
                    ? tr("none")
                    : selected->m_productAssetId)
            .arg(selected->m_productCachePath.isEmpty()
                    ? tr("none")
                    : selected->m_productCachePath)
            .arg(selected->m_previewRenderVerified ? tr("verified") : tr("not verified"));
        if (!selected->m_issueText.isEmpty())
        {
            information += tr(" | blockers: %1").arg(selected->m_issueText);
        }
        m_selectionInfo->setText(information);

        if (selected->m_entryKind != QStringLiteral("o3de-preview-product")
            || selected->m_productAssetId.isEmpty())
        {
            SetStatus(tr("The selected row is failure evidence and has no preview product."), true);
            RefreshBindingSummary();
            return;
        }

        const AZ::Data::AssetId assetId = AZ::Data::AssetId::CreateString(
            ToAzString(selected->m_productAssetId));
        AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry* productEntry =
            AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry::GetProductByAssetId(assetId);
        if (!productEntry)
        {
            SetStatus(
                tr("The product is evidence-backed but is not registered in this live O3DE Asset Browser. Run or refresh Asset Processor for the active project, then reload the model."),
                true);
            RefreshBindingSummary();
            return;
        }

        m_previewer->Display(productEntry);
        SetStatus(
            tr("Displaying the selected product through O3DE's registered live previewer. This is editor-preview authority only."));
        RefreshBindingSummary();
    }

    void ItemVisualSelectorWidget::ApplySelectionAsIcon()
    {
        ApplySelection(true);
    }

    void ItemVisualSelectorWidget::ApplySelectionAsAsset()
    {
        ApplySelection(false);
    }

    void ItemVisualSelectorWidget::ApplySelection(bool iconBinding)
    {
        if (!LoadedModelMatchesActiveProfile())
        {
            ClearLoadedModel(
                tr("The loaded preview model no longer matches the active FoA profile or ExtractedDataPath."));
            return;
        }

        const PreviewEntry* selected = GetSelectedEntry();
        const QString itemId = ResolveBindingItemRecordId();
        if (!selected
            || selected->m_entryKind != QStringLiteral("o3de-preview-product")
            || selected->m_productAssetId.isEmpty()
            || itemId.isEmpty())
        {
            SetStatus(tr("Select one imported preview product and one resolved item target first."), true);
            return;
        }
        if (iconBinding
            && !IsIconCandidate(selected->m_productKind, selected->m_productCachePath))
        {
            SetStatus(tr("Only image or texture preview products can be used as item icon references."), true);
            return;
        }

        FoundationService& foundation = FoundationService::Get();
        const EconomyItemProfile* current = foundation.GetCatalog().FindEconomyItem(
            ToAzString(itemId));
        if (!current)
        {
            SetStatus(
                tr("The target item has no evidence-backed typed profile. Save that profile before adding a visual binding."),
                true);
            return;
        }

        EconomyItemProfile candidate = *current;
        if (iconBinding)
        {
            const QString sourceIdentity = selected->m_primarySourceAssetRecordId.isEmpty()
                ? selected->m_paneEntryId
                : selected->m_primarySourceAssetRecordId;
            candidate.m_iconRef = ToAzString(sourceIdentity);
        }
        else
        {
            candidate.m_assetRef = ToAzString(selected->m_productAssetId);
        }

        AZStd::string error;
        if (!foundation.UpsertEconomyItemProfile(candidate, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(
            iconBinding
                ? tr("Saved the explicit editor-preview icon reference for %1. Runtime permission remains unchanged.").arg(itemId)
                : tr("Saved the explicit O3DE preview AssetId for %1. Runtime permission remains unchanged.").arg(itemId));
    }

    QString ItemVisualSelectorWidget::ResolveExtractedDataRoot() const
    {
        const FoundationService& foundation = FoundationService::Get();
        const WorkspaceModel& workspace = foundation.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || profile->m_extractedDataPath.empty())
        {
            return {};
        }

        QString baseDirectory;
        if (!foundation.GetWorkspaceFilePath().empty())
        {
            baseDirectory = QFileInfo(ToQString(foundation.GetWorkspaceFilePath())).absolutePath();
        }
        else if (!foundation.GetWorkspaceRootPath().empty())
        {
            baseDirectory = ToQString(foundation.GetWorkspaceRootPath());
        }
        else
        {
            return {};
        }

        const QString configured = ToQString(profile->m_extractedDataPath);
        const QString absolute = QFileInfo(configured).isAbsolute()
            ? QDir::cleanPath(configured)
            : QDir(baseDirectory).absoluteFilePath(configured);
        const QFileInfo resolved(absolute);
        if (!resolved.isDir())
        {
            return {};
        }
        const QString canonical = resolved.canonicalFilePath();
        return canonical.isEmpty() ? resolved.absoluteFilePath() : canonical;
    }

    QString ItemVisualSelectorWidget::ResolveBindingItemRecordId() const
    {
        const AZStd::string targetId = ToAzString(m_targetRecord->currentData().toString());
        const CatalogRecord* target = FoundationService::Get().GetCatalog().FindByRecordId(targetId);
        if (!target || target->m_domain != "economy")
        {
            return {};
        }
        if (target->m_recordKind == "item")
        {
            return ToQString(target->m_recordId);
        }
        if (target->m_recordKind == "recipe")
        {
            return m_recipeItemRecord->currentData().toString();
        }
        return {};
    }

    const ItemVisualSelectorWidget::PreviewEntry* ItemVisualSelectorWidget::GetSelectedEntry() const
    {
        const int row = m_entryTable->currentRow();
        const QTableWidgetItem* first = row >= 0 ? m_entryTable->item(row, 0) : nullptr;
        if (!first)
        {
            return nullptr;
        }
        const int index = first->data(EntryIndexRole).toInt();
        if (index < 0 || index >= static_cast<int>(m_entries.size()))
        {
            return nullptr;
        }
        return &m_entries[static_cast<size_t>(index)];
    }

    void ItemVisualSelectorWidget::SetStatus(const QString& message, bool error)
    {
        m_status->setText(message);
        m_status->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }
} // namespace TaintedGrailModdingSDK
