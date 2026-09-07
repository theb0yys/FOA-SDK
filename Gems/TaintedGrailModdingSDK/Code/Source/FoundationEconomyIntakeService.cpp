/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/Utils/Utils.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string A(const QString& value)
        {
            const QByteArray bytes = value.toUtf8();
            return AZStd::string(bytes.constData(), static_cast<size_t>(bytes.size()));
        }
        AZStd::string Field(const QJsonObject& object, const char* key) { return A(object.value(key).toString()); }
        QJsonArray Strings(const AZStd::vector<AZStd::string>& values)
        {
            QJsonArray result;
            for (const AZStd::string& value : values) { result.append(Q(value)); }
            return result;
        }
        QJsonObject AuthoredDocument(const GameProfile& profile, const QJsonObject& row)
        {
            return {{"SchemaVersion", 1}, {"DocumentKind", "foa-authored-economy-definition"},
                {"ProfileId", Q(profile.m_profileId)}, {"GameVersion", Q(profile.m_gameVersion)},
                {"Branch", Q(profile.m_branch)}, {"RuntimeTarget", Q(profile.m_runtimeTarget)},
                {"CapturedAt", QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ")},
                {"evidence", QJsonArray{row}}};
        }
        bool Fail(AZStd::string* error, const QString& message)
        {
            if (error) { *error = A(message); }
            return false;
        }
        QString Hash(const QByteArray& bytes)
        {
            return "sha256:" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        }
        bool Inside(const QString& file, const QString& root)
        {
            const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
            const QString canonicalFile = QFileInfo(file).canonicalFilePath();
            const Qt::CaseSensitivity sensitivity = QDir::separator() == QChar('\\') ? Qt::CaseInsensitive : Qt::CaseSensitive;
            return !canonicalRoot.isEmpty() && !canonicalFile.isEmpty()
                && canonicalFile.startsWith(canonicalRoot + '/', sensitivity);
        }
    }

    bool FoundationService::ImportNativeEconomy(const AZStd::string& path, AZStd::string* error)
    {
        return ImportEconomyDocument(path, false, error);
    }

    bool FoundationService::CreateEconomyRecord(const AZStd::string& kind, const AZStd::string& name,
        AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear();
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        const PackManifest* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty())
        {
            return Fail(error, "Save a mod in Mod Manager before creating an item or recipe.");
        }
        if ((kind != "item" && kind != "recipe") || Q(name).trimmed().isEmpty() || name.size() > 512)
        {
            return Fail(error, "Choose an item or recipe and enter a name (up to 512 bytes).");
        }
        // A fresh pack-owned identity is explicitly authored here, never inferred from a game name.
        const QString id = "custom." + Q(kind) + "." + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject row{ {"record_id", id}, {"subject_ref", "pack:" + Q(pack->m_packId) + "/" + id},
            {"evidence_id", "evidence." + id}, {"kind", Q(kind)}, {"display_name", Q(name).trimmed()},
            {"claim", "User-authored local definition. This source establishes authoring intent only."},
            {"confidence", "documented"}, {"owner_pack_id", Q(pack->m_packId)} };
        row.insert(Q(kind), QJsonObject{});
        QJsonObject document{ {"SchemaVersion", 1}, {"DocumentKind", "foa-authored-economy-definition"},
            {"ProfileId", Q(profile->m_profileId)}, {"GameVersion", Q(profile->m_gameVersion)},
            {"Branch", Q(profile->m_branch)}, {"RuntimeTarget", Q(profile->m_runtimeTarget)},
            {"CapturedAt", QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ")},
            {"evidence", QJsonArray{row}} };
        if (!WriteAuthoredEconomyDocument(A(QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact))), error)) { return false; }
        recordId = A(id);
        return true;
    }

    bool FoundationService::WriteAuthoredEconomyDocument(const AZStd::string& json, AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured()) { return Fail(error, "Choose a configured game profile first."); }
        const QString root = Q(m_workspaceRootPath);
        const QString directory = QDir(root).filePath("Sources/AuthoredEconomy");
        const auto enginePath = AZ::Utils::GetEnginePath();
        const QString engine = QString::fromUtf8(enginePath.c_str());
        if (root.isEmpty() || Inside(root, Q(profile->m_installPath)) || root == Q(profile->m_installPath)
            || (!engine.isEmpty() && (Inside(root, engine) || root == engine)))
        {
            return Fail(error, "The authoring workspace must be outside the game and engine.");
        }
        QString ancestor = directory;
        while (!QFileInfo::exists(ancestor))
        {
            const QString parent = QFileInfo(ancestor).absolutePath();
            if (parent == ancestor) { return Fail(error, "Cannot resolve the authoring folder."); }
            ancestor = parent;
        }
        if (QFileInfo(ancestor).canonicalFilePath() != QFileInfo(root).canonicalFilePath() && !Inside(ancestor, root))
        {
            return Fail(error, "The authoring source folder resolves outside the workspace.");
        }
        if (!QDir().mkpath(directory) || !Inside(directory, root))
        {
            return Fail(error, "Cannot create an authoring source folder inside the workspace.");
        }
        const QString path = QDir(directory).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json");
        QSaveFile file(path);
        const QByteArray bytes = Q(json).toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            return Fail(error, "Cannot save the new definition's authoring source.");
        }
        return ImportEconomyDocument(A(path), true, error);
    }

    bool FoundationService::ImportEconomyDocument(const AZStd::string& path, bool custom, AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured() || m_workspaceRootPath.empty())
        {
            return Fail(error, "Open a saved workspace with a configured game before loading definitions.");
        }
        if (!Inside(Q(path), Q(m_workspaceRootPath)))
        {
            return Fail(error, "Economy observations must be a local file inside this workspace.");
        }
        QFile file(Q(path));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        {
            return Fail(error, "Cannot read economy observations within the 16 MiB document limit.");
        }
        const QByteArray bytes = file.read(16 * 1024 * 1024 + 1);
        if (bytes.size() > 16 * 1024 * 1024) { return Fail(error, "Economy observations grew beyond the document limit."); }
        const QJsonObject document = QJsonDocument::fromJson(bytes).object();
        const QString expectedKind = custom ? "foa-authored-economy-definition" : "foa-native-economy-observations";
        if (document.value("SchemaVersion").toInt() != 1 || document.value("DocumentKind").toString() != expectedKind
            || Field(document, "ProfileId") != profile->m_profileId || Field(document, "GameVersion") != profile->m_gameVersion
            || Field(document, "Branch") != profile->m_branch || Field(document, "RuntimeTarget") != profile->m_runtimeTarget)
        {
            return Fail(error, "Economy observations have an unsupported schema or belong to a different game profile.");
        }
        if (!custom)
        {
            const QJsonObject authority = document.value("OperationalAuthority").toObject();
            if (authority.isEmpty()) { return Fail(error, "The reader result has no authority boundary."); }
            for (auto value = authority.begin(); value != authority.end(); ++value)
            {
                if (!value.value().isBool() || value.value().toBool()) { return Fail(error, "Reader observations cannot grant authority."); }
            }
            const QJsonArray sources = document.value("SourceFiles").toArray();
            if (sources.size() != 3) { return Fail(error, "Expected the item bundle, recipe bundle and Addressables catalog."); }
            QSet<QString> locators;
            const QSet<QString> expectedSources{
                "$install/Fall of Avalon_Data/StreamingAssets/aa/catalog.json",
                "$install/Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.items_assets_all.bundle",
                "$install/Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.crafting_assets_all.bundle"};
            for (const QJsonValue& value : sources)
            {
                const QJsonObject source = value.toObject();
                const QString locator = source.value("Locator").toString();
                const QString sourcePath = QDir(Q(profile->m_installPath)).filePath(locator.mid(9));
                if (!expectedSources.contains(locator) || locators.contains(locator) || !Inside(sourcePath, Q(profile->m_installPath)))
                {
                    return Fail(error, "The reader source identity is duplicate or escapes the registered installation.");
                }
                locators.insert(locator);
                QFile sourceFile(sourcePath);
                if (!sourceFile.open(QIODevice::ReadOnly) || sourceFile.size() > 64 * 1024 * 1024
                    || sourceFile.size() != source.value("ByteSize").toInteger())
                {
                    return Fail(error, "A game source changed or exceeds the input limit. Read game definitions again.");
                }
                if (Hash(sourceFile.read(64 * 1024 * 1024 + 1)) != source.value("Sha256").toString())
                {
                    return Fail(error, "A game source changed. Read game definitions again.");
                }
            }
        }
        const QJsonArray rows = document.value(custom ? "evidence" : "definitions").toArray();
        if (rows.isEmpty() || rows.size() > 10000 || (custom && rows.size() != 1))
        {
            return Fail(error, "Economy intake requires 1 to 10,000 supported definitions.");
        }
        SourceImportRequest request;
        request.m_inputPath = path;
        request.m_sourceKind = "item-recipe-dump";
        request.m_title = custom ? "Local authored economy definition" : "Installed item and recipe observations";
        request.m_toolName = "FOA-SDK economy intake";
        request.m_toolVersion = "1.0.0";
        request.m_capturedAt = Field(document, "CapturedAt");
        request.m_limitations = "Local authoring source only; no runtime, deployment, station or unlock authority.";
        auto importedResult = m_sourceImportService.Import(request, *profile);
        if (!importedResult.IsSuccess()) { return Fail(error, Q(importedResult.GetError())); }
        SourceImportResult imported = importedResult.TakeValue();
        if (imported.HasErrors() || imported.m_sourceDocument.m_source.m_fingerprint != A(Hash(bytes)))
        {
            return Fail(error, "Source import failed or the observations changed during intake.");
        }
        SourceEvidenceRegistry registry = m_sourceRegistry;
        if (!registry.RegisterSource(imported.m_sourceDocument.m_source, error)) { return false; }
        for (const EvidenceRecord& evidence : imported.m_evidenceDocument.m_evidence)
        {
            if (!registry.RegisterEvidence(evidence, error)) { return false; }
        }
        CatalogDatabase candidate = m_catalog;
        QHash<QString, QString> identities;
        QSet<QString> newIds;
        QSet<QString> seen;
        for (const QJsonValue& value : rows)
        {
            const QJsonObject row = value.toObject();
            const QString id = row.value("record_id").toString();
            const AZStd::string kind = Field(row, "kind");
            const AZStd::string nativeRef = Field(row, "native_ref");
            if (custom && (kind == "recipe-ingredient" || kind == "recipe-output"))
            {
                if (id.isEmpty() || Field(row, "subject_ref") != "economy-" + kind + ":" + A(id))
                {
                    return Fail(error, "Authored recipe link evidence must bind its exact link identity.");
                }
                newIds.insert(id);
                continue;
            }
            if (id.isEmpty() || seen.contains(id) || (kind != "item" && kind != "recipe")
                || Field(row, "display_name").empty() || (!custom && (nativeRef.empty() || nativeRef != Field(row, "subject_ref"))))
            {
                return Fail(error, "An economy definition has missing, duplicate or contradictory identity fields.");
            }
            seen.insert(id);
            const CatalogRecord* existing = candidate.FindByRecordId(A(id));
            if (!existing && !custom) { existing = candidate.FindByExactNativeRef(nativeRef); }
            if (existing)
            {
                if (custom || existing->m_domain != "economy" || existing->m_recordKind != kind
                    || existing->m_nativeRefExact != nativeRef || existing->m_subjectRef != Field(row, "subject_ref"))
                {
                    return Fail(error, "An imported identity conflicts with an existing canonical record.");
                }
                identities.insert(id, Q(existing->m_recordId));
                continue; // Keep authored profiles, joins and governance on every refresh.
            }
            CatalogPromotionRequest promotion;
            promotion.m_recordId = A(id);
            promotion.m_domain = "economy";
            promotion.m_recordKind = kind;
            promotion.m_subjectRef = Field(row, "subject_ref");
            promotion.m_nativeRefExact = nativeRef;
            promotion.m_identityKind = custom ? "synthetic" : "native";
            promotion.m_ownerPackId = custom ? Field(row, "owner_pack_id") : AZStd::string{};
            promotion.m_displayName = Field(row, "display_name");
            promotion.m_evidenceId = Field(row, "evidence_id");
            promotion.m_confidence = "documented";
            promotion.m_researchStage = "S1";
            auto promoted = m_catalogPromotion.BuildReviewedRecord(promotion, m_workspace, m_packs, registry);
            if (!promoted.IsSuccess()) { return Fail(error, Q(promoted.GetError())); }
            if (!candidate.InsertNew(promoted.GetValue(), error)) { return false; }
            identities.insert(id, id);
            newIds.insert(id);
        }
        for (const QJsonValue& value : rows)
        {
            const QJsonObject row = value.toObject();
            const QString id = row.value("record_id").toString();
            if (!newIds.contains(id)) { continue; }
            const AZStd::vector<AZStd::string> evidence{Field(row, "evidence_id")};
            if (custom && (Field(row, "kind") == "recipe-ingredient" || Field(row, "kind") == "recipe-output"))
            {
                const QJsonObject data = row.value("link").toObject();
                if (Field(row, "kind") == "recipe-ingredient")
                {
                    EconomyRecipeIngredient link;
                    link.m_linkId = A(id);
                    link.m_recipeRecordId = Field(data, "recipe_record_id");
                    link.m_itemRecordId = Field(data, "item_record_id");
                    link.m_itemSubjectRef = Field(data, "item_subject_ref");
                    link.m_quantity = static_cast<AZ::u32>(data.value("quantity").toInt());
                    link.m_alternativeGroup = Field(data, "alternative_group");
                    link.m_consumed = data.value("consumed").toBool();
                    for (const auto& condition : data.value("conditions").toArray()) { link.m_conditions.push_back(A(condition.toString())); }
                    link.m_evidenceIds = evidence;
                    if (!candidate.UpsertRecipeIngredient(link, error)) { return false; }
                }
                else
                {
                    EconomyRecipeOutput link;
                    link.m_linkId = A(id);
                    link.m_recipeRecordId = Field(data, "recipe_record_id");
                    link.m_itemRecordId = Field(data, "item_record_id");
                    link.m_itemSubjectRef = Field(data, "item_subject_ref");
                    link.m_quantity = static_cast<AZ::u32>(data.value("quantity").toInt());
                    link.m_chance = data.value("chance").toDouble();
                    link.m_byProduct = data.value("by_product").toBool();
                    for (const auto& condition : data.value("conditions").toArray()) { link.m_conditions.push_back(A(condition.toString())); }
                    link.m_evidenceIds = evidence;
                    if (!candidate.UpsertRecipeOutput(link, error)) { return false; }
                }
                continue;
            }
            if (Field(row, "kind") == "item")
            {
                const QJsonObject data = row.value("item").toObject();
                if (!row.value("item").isObject()) { return Fail(error, "Missing typed item fields."); }
                if (!custom && (!data.value("weight").isDouble() || !data.value("base_value").isDouble()
                    || !data.value("tags").isArray() || !data.value("hidden").isBool()))
                {
                    return Fail(error, "Item values have missing or invalid numeric, tag or visibility fields.");
                }
                EconomyItemProfile item;
                item.m_recordId = A(id);
                item.m_category = Field(data, "category");
                item.m_subtype = Field(data, "subtype");
                item.m_weight = data.value("weight").toDouble();
                item.m_baseValue = data.value("base_value").toDouble();
                item.m_quality = Field(data, "quality");
                item.m_hiddenItem = data.value("hidden").toBool();
                item.m_localisationNameRef = Field(data, "name_ref");
                item.m_localisationDescriptionRef = Field(data, "description_ref");
                item.m_iconRef = Field(data, "icon_ref");
                for (const QJsonValue& tag : data.value("tags").toArray()) { item.m_tags.push_back(A(tag.toString())); }
                item.m_evidenceIds = evidence;
                if (!candidate.UpsertEconomyItem(item, error)) { return false; }
                continue;
            }
            const QJsonObject data = row.value("recipe").toObject();
            if (!row.value("recipe").isObject()) { return Fail(error, "Missing typed recipe fields."); }
            EconomyRecipeProfile recipe;
            recipe.m_recordId = A(id);
            recipe.m_recipeType = "unknown";
            recipe.m_unlockMode = "unknown";
            recipe.m_duplicateKey = A(id);
            recipe.m_persistenceMode = custom ? "custom_template" : "native_template";
            recipe.m_hiddenRecipe = data.value("hidden").toBool();
            recipe.m_evidenceIds = evidence;
            if (!candidate.UpsertEconomyRecipe(recipe, error)) { return false; }
            for (const QString& joinKind : {QString("ingredients"), QString("outputs")})
            {
                const QJsonArray links = data.value(joinKind).toArray();
                if (links.size() > 256 || (!custom && (!data.value(joinKind).isArray()
                    || (joinKind == "outputs" && links.isEmpty()))))
                {
                    return Fail(error, "A recipe has missing or excessive ingredient/output fields.");
                }
                for (qsizetype index = 0; index < links.size(); ++index)
                {
                    const QJsonObject link = links[index].toObject();
                    const int quantity = link.value("quantity").toInt(-1);
                    if (quantity < 1 || quantity > 1000000 || link.value("quantity").toDouble() != quantity)
                    {
                        return Fail(error, "Recipe quantities must be positive whole numbers up to 1,000,000.");
                    }
                    const QString inputId = link.value("item_record_id").toString();
                    const AZStd::string targetId = A(identities.value(inputId));
                    const AZStd::string targetSubject = Field(link, "item_subject_ref");
                    if ((!inputId.isEmpty() && targetId.empty()) || (targetId.empty() == targetSubject.empty()))
                    {
                        return Fail(error, "A recipe link needs one exact item identity or one unresolved subject.");
                    }
                    const AZStd::string linkId = Field(link, "link_id");
                    const AZStd::vector<AZStd::string> linkEvidence{Field(link, "evidence_id")};
                    if (joinKind == "ingredients")
                    {
                        EconomyRecipeIngredient linkRecord;
                        linkRecord.m_linkId = linkId;
                        linkRecord.m_recipeRecordId = A(id);
                        linkRecord.m_itemRecordId = targetId;
                        linkRecord.m_itemSubjectRef = targetSubject;
                        linkRecord.m_quantity = static_cast<AZ::u32>(quantity);
                        linkRecord.m_evidenceIds = linkEvidence;
                        if (!candidate.UpsertRecipeIngredient(linkRecord, error)) { return false; }
                    }
                    else
                    {
                        EconomyRecipeOutput linkRecord;
                        linkRecord.m_linkId = linkId;
                        linkRecord.m_recipeRecordId = A(id);
                        linkRecord.m_itemRecordId = targetId;
                        linkRecord.m_itemSubjectRef = targetSubject;
                        linkRecord.m_quantity = static_cast<AZ::u32>(quantity);
                        linkRecord.m_evidenceIds = linkEvidence;
                        if (!candidate.UpsertRecipeOutput(linkRecord, error)) { return false; }
                    }
                }
            }
        }
        WorkspaceModel workspace = m_workspace;
        workspace.m_rootPath = m_workspaceRootPath;
        if (newIds.isEmpty()) { return candidate.ValidateIntegrity(workspace, *profile, registry, error); }
        // Commit validates the entire candidate before invoking this writer. Persist
        // evidence first inside that transaction, avoiding a second full validation.
        // A failed catalog write can leave unused evidence, never a partial catalog.
        auto committed = m_catalogTransaction.Commit(candidate, workspace, *profile, registry,
            [this, &imported](const CatalogDocument& catalog, const AZStd::string& root)
                -> AZ::Outcome<AZStd::string, AZStd::string>
            {
                auto savedEvidence = m_sourceEvidencePersistence.SaveDocuments(
                    imported.m_sourceDocument, imported.m_evidenceDocument, root);
                if (!savedEvidence.IsSuccess()) { return AZ::Failure(AZStd::string(savedEvidence.GetError())); }
                return m_catalogPersistence.Save(catalog, root);
            });
        if (!committed.IsSuccess()) { return Fail(error, Q(committed.GetError())); }
        auto result = committed.TakeValue();
        m_catalog = AZStd::move(result.m_catalog);
        m_catalogFilePath = AZStd::move(result.m_filePath);
        m_sourceRegistry = AZStd::move(registry);
        RefreshSnapshot();
        return true;
    }

    bool FoundationService::SaveAuthoredRecipeIngredient(const EconomyRecipeIngredient& ingredient, AZStd::string* error)
    {
        for (const auto& existing : m_catalog.GetRecipeIngredients())
        {
            if (existing.m_linkId == ingredient.m_linkId && existing.m_recipeRecordId != ingredient.m_recipeRecordId)
            {
                return Fail(error, "This ingredient link belongs to another recipe.");
            }
        }
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || ingredient.m_quantity > 1000000) { return Fail(error, "Invalid ingredient profile or quantity."); }
        QJsonObject data{{"recipe_record_id", Q(ingredient.m_recipeRecordId)}, {"item_record_id", Q(ingredient.m_itemRecordId)},
            {"item_subject_ref", Q(ingredient.m_itemSubjectRef)}, {"quantity", static_cast<int>(ingredient.m_quantity)},
            {"alternative_group", Q(ingredient.m_alternativeGroup)}, {"consumed", ingredient.m_consumed},
            {"conditions", Strings(ingredient.m_conditions)}};
        QJsonObject row{{"record_id", Q(ingredient.m_linkId)}, {"kind", "recipe-ingredient"},
            {"subject_ref", "economy-recipe-ingredient:" + Q(ingredient.m_linkId)},
            {"evidence_id", "evidence.authored." + QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"confidence", "documented"}, {"link", data},
            {"claim", "User-authored ingredient definition; local authoring intent only: "
                + QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}};
        return WriteAuthoredEconomyDocument(A(QString::fromUtf8(QJsonDocument(AuthoredDocument(*profile, row)).toJson(QJsonDocument::Compact))), error);
    }

    bool FoundationService::SaveAuthoredRecipeOutput(const EconomyRecipeOutput& output, AZStd::string* error)
    {
        for (const auto& existing : m_catalog.GetRecipeOutputs())
        {
            if (existing.m_linkId == output.m_linkId && existing.m_recipeRecordId != output.m_recipeRecordId)
            {
                return Fail(error, "This output link belongs to another recipe.");
            }
        }
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || output.m_quantity > 1000000) { return Fail(error, "Invalid output profile or quantity."); }
        QJsonObject data{{"recipe_record_id", Q(output.m_recipeRecordId)}, {"item_record_id", Q(output.m_itemRecordId)},
            {"item_subject_ref", Q(output.m_itemSubjectRef)}, {"quantity", static_cast<int>(output.m_quantity)},
            {"chance", output.m_chance}, {"by_product", output.m_byProduct}, {"conditions", Strings(output.m_conditions)}};
        QJsonObject row{{"record_id", Q(output.m_linkId)}, {"kind", "recipe-output"},
            {"subject_ref", "economy-recipe-output:" + Q(output.m_linkId)},
            {"evidence_id", "evidence.authored." + QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"confidence", "documented"}, {"link", data},
            {"claim", "User-authored output definition; local authoring intent only: "
                + QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}};
        return WriteAuthoredEconomyDocument(A(QString::fromUtf8(QJsonDocument(AuthoredDocument(*profile, row)).toJson(QJsonDocument::Compact))), error);
    }

    bool FoundationService::RemoveEconomyRecipeJoin(const AZStd::string& recipeId, const AZStd::string& linkId,
        bool output, AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !m_catalog.FindEconomyRecipe(recipeId)) { return Fail(error, "Select a saved recipe first."); }
        CatalogDocument document = m_catalog.BuildDocument(m_workspace, *profile);
        const auto remove = [&recipeId, &linkId](auto& links)
        {
            for (auto link = links.begin(); link != links.end(); ++link)
            {
                if (link->m_linkId == linkId && link->m_recipeRecordId == recipeId) { links.erase(link); return true; }
            }
            return false;
        };
        if (!(output ? remove(document.m_recipeOutputs) : remove(document.m_recipeIngredients)))
        {
            return Fail(error, "The selected link does not belong to this recipe.");
        }
        CatalogDatabase candidate;
        if (!candidate.ReplaceFromBoundDocument(document, m_workspace, *profile, m_sourceRegistry, error)) { return false; }
        return PersistCatalogCandidate(candidate, error);
    }
} // namespace TaintedGrailModdingSDK
