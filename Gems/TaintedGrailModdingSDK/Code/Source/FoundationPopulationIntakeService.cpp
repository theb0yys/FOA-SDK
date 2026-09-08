/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "PathPolicyService.h"
#include <AzCore/Utils/Utils.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
        AZStd::string Field(const QJsonObject& value, const char* key) { return A(value.value(key).toString()); }
        bool Fail(AZStd::string* error, const QString& message)
        {
            if (error) { *error = A(message); }
            return false;
        }
        QString Id(const char* prefix) { return QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces); }
        QString Hash(const QByteArray& bytes)
        {
            return "sha256:" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        }
        bool Inside(const QString& file, const QString& root)
        {
            const QString base = QFileInfo(root).canonicalFilePath();
            const QString target = QFileInfo(file).canonicalFilePath();
            const auto sensitivity = QDir::separator() == QChar('\\') ? Qt::CaseInsensitive : Qt::CaseSensitive;
            return !base.isEmpty() && !target.isEmpty() && target.startsWith(base + '/', sensitivity);
        }
        QJsonArray Strings(const AZStd::vector<AZStd::string>& values)
        {
            QJsonArray result;
            for (const auto& value : values) { result.append(Q(value)); }
            return result;
        }
        QJsonObject Evidence(const QString& subject, const QString& kind, const QString& claim)
        {
            return {{"evidence_id", Id("evidence.authored.population.")}, {"subject_ref", subject},
                {"kind", kind}, {"confidence", "documented"}, {"claim", claim}};
        }
        QJsonObject MemberEvidence(PopulationTroopMember& member)
        {
            QJsonObject data{{"link_id", Q(member.m_linkId)}, {"troop_record_id", Q(member.m_troopRecordId)},
                {"actor_record_id", Q(member.m_actorRecordId)}, {"actor_subject_ref", Q(member.m_actorSubjectRef)},
                {"role", Q(member.m_role)}, {"minimum_count", static_cast<int>(member.m_minimumCount)},
                {"maximum_count", static_cast<int>(member.m_maximumCount)}, {"weight", member.m_weight},
                {"required", member.m_required}, {"conditions", Strings(member.m_conditions)}};
            auto row = Evidence("population-troop-member:" + Q(member.m_linkId), "troop-member",
                "User-authored troop member; local authoring intent only: "
                + QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
            // Fresh intent replaces stale link claims when its actor or values change.
            // Core independently adds the current troop/actor canonical evidence.
            member.m_evidenceIds = {Field(row, "evidence_id")};
            return row;
        }
        bool ImportEvidence(SourceImportService& service, const QString& path, const QJsonObject& document,
            const QByteArray& bytes, const GameProfile& profile, SourceEvidenceRegistry& registry,
            SourceImportResult& imported, AZStd::string* error)
        {
            SourceImportRequest request;
            request.m_inputPath = A(path);
            request.m_sourceKind = "template-diagnostics";
            request.m_title = "Actor and troop authoring observations";
            request.m_toolName = "FOA-SDK population intake";
            request.m_toolVersion = "1.0.0";
            request.m_capturedAt = Field(document, "CapturedAt");
            request.m_limitations = "Serialized observations or explicit local authoring intent; no runtime or deployment authority.";
            auto result = service.Import(request, profile);
            if (!result.IsSuccess()) { return Fail(error, Q(result.GetError())); }
            imported = result.TakeValue();
            if (imported.HasErrors() || imported.m_sourceDocument.m_source.m_fingerprint != A(Hash(bytes)))
            {
                return Fail(error, "Population evidence is invalid or changed during intake.");
            }
            if (!registry.RegisterSource(imported.m_sourceDocument.m_source, error)) { return false; }
            for (const auto& evidence : imported.m_evidenceDocument.m_evidence)
            {
                if (!registry.RegisterEvidence(evidence, error)) { return false; }
            }
            return true;
        }
    }

    bool FoundationService::ImportNativePopulation(const AZStd::string& path, AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured() || m_workspaceRootPath.empty()
            || !Inside(Q(path), Q(m_workspaceRootPath)))
        {
            return Fail(error, "Open a saved workspace with a configured game and local actor observations.");
        }
        QFile file(Q(path));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        {
            return Fail(error, "Cannot read actor observations within the 16 MiB document limit.");
        }
        const auto bytes = file.read(16 * 1024 * 1024 + 1);
        if (bytes.size() > 16 * 1024 * 1024) { return Fail(error, "Actor observations grew beyond the document limit."); }
        const auto document = QJsonDocument::fromJson(bytes).object();
        if (document.value("SchemaVersion").toInt() != 1
            || document.value("DocumentKind").toString() != "foa-native-population-observations"
            || Field(document, "ProfileId") != profile->m_profileId || Field(document, "GameVersion") != profile->m_gameVersion
            || Field(document, "Branch") != profile->m_branch || Field(document, "RuntimeTarget") != profile->m_runtimeTarget)
        {
            return Fail(error, "Actor observations have an unsupported schema or a different game profile.");
        }
        const auto authority = document.value("OperationalAuthority").toObject();
        if (authority.isEmpty()) { return Fail(error, "Actor observations have no authority boundary."); }
        for (auto value = authority.begin(); value != authority.end(); ++value)
        {
            if (!value.value().isBool() || value.value().toBool()) { return Fail(error, "Actor observations cannot grant authority."); }
        }
        const QString locator = "$install/Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.npc_assets_all.bundle";
        const auto sources = document.value("SourceFiles").toArray();
        if (sources.size() != 1 || sources[0].toObject().value("Locator").toString() != locator)
        {
            return Fail(error, "Actor observations must identify the exact supported NPC bundle.");
        }
        const auto source = sources[0].toObject();
        const auto sourcePath = QDir(Q(profile->m_installPath)).filePath(locator.mid(9));
        QFile sourceFile(sourcePath);
        if (!Inside(sourcePath, Q(profile->m_installPath)) || !sourceFile.open(QIODevice::ReadOnly)
            || sourceFile.size() > 64 * 1024 * 1024 || sourceFile.size() != source.value("ByteSize").toInteger()
            || Hash(sourceFile.read(64 * 1024 * 1024 + 1)) != source.value("Sha256").toString())
        {
            return Fail(error, "The NPC source changed or is outside the registered game. Load actors again.");
        }
        const auto rows = document.value("definitions").toArray();
        if (rows.isEmpty() || rows.size() > 10000) { return Fail(error, "Actor intake requires 1 to 10,000 definitions."); }
        SourceEvidenceRegistry registry = m_sourceRegistry;
        SourceImportResult imported;
        if (!ImportEvidence(m_sourceImportService, Q(path), document, bytes, *profile, registry, imported, error)) { return false; }
        CatalogDatabase candidate = m_catalog;
        QSet<QString> seenIds, seenSubjects;
        bool changed = false;
        for (const auto& value : rows)
        {
            const auto row = value.toObject();
            const auto id = Field(row, "record_id");
            const auto subject = Field(row, "subject_ref");
            const auto native = Field(row, "native_ref");
            const auto data = row.value("actor").toObject();
            const int minimum = data.value("minimum_level").toInt(-1);
            const int maximum = data.value("maximum_level").toInt(-1);
            if (id.empty() || seenIds.contains(Q(id)) || seenSubjects.contains(Q(subject))
                || Field(row, "kind") != "actor" || Field(row, "display_name").empty()
                || subject != native || !Q(native).startsWith(locator + "#/Assets/")
                || !Q(native).endsWith(".prefab") || Q(native).contains("..") || Q(native).contains('\\')
                || Field(data, "actor_kind") != "other" || Field(data, "archetype") != "native-npc-template"
                || minimum < 1 || maximum != minimum || maximum > 1000
                || data.value("minimum_level").toDouble() != minimum || data.value("maximum_level").toDouble() != maximum
                || !data.value("tags").isArray())
            {
                return Fail(error, "An actor observation has invalid, duplicate or unsupported fields.");
            }
            seenIds.insert(Q(id)); seenSubjects.insert(Q(subject));
            const CatalogRecord* existing = candidate.FindByRecordId(id);
            if (!existing) { existing = candidate.FindByExactNativeRef(native); }
            if (existing)
            {
                if (existing->m_domain != "population" || existing->m_recordKind != "actor"
                    || existing->m_nativeRefExact != native || existing->m_subjectRef != subject)
                {
                    return Fail(error, "An actor identity conflicts with an existing catalog record.");
                }
                continue; // Preserve authored fields and evidence on refresh, including stable canonical IDs.
            }
            CatalogPromotionRequest promotion;
            promotion.m_recordId = id; promotion.m_domain = "population"; promotion.m_recordKind = "actor";
            promotion.m_subjectRef = subject; promotion.m_nativeRefExact = native; promotion.m_identityKind = "native";
            promotion.m_displayName = Field(row, "display_name"); promotion.m_evidenceId = Field(row, "evidence_id");
            promotion.m_confidence = "documented"; promotion.m_researchStage = "S1";
            auto record = m_catalogPromotion.BuildReviewedRecord(promotion, m_workspace, m_packs, registry);
            if (!record.IsSuccess()) { return Fail(error, Q(record.GetError())); }
            if (!candidate.InsertNew(record.GetValue(), error)) { return false; }
            PopulationActorProfile actor;
            actor.m_recordId = id; actor.m_actorKind = "other"; actor.m_archetype = "native-npc-template";
            actor.m_minimumLevel = static_cast<AZ::u32>(minimum); actor.m_maximumLevel = static_cast<AZ::u32>(maximum);
            actor.m_evidenceIds = {promotion.m_evidenceId};
            for (const auto& tag : data.value("tags").toArray())
            {
                if (!tag.isString()) { return Fail(error, "Actor tags must be strings."); }
                actor.m_tags.push_back(A(tag.toString()));
            }
            if (!candidate.UpsertPopulationActorProfile(actor, error)) { return false; }
            changed = true;
        }
        if (!changed) { return candidate.ValidateIntegrity(m_workspace, *profile, registry, error); }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }

    bool FoundationService::PrepareAuthoredPopulationEvidence(const AZStd::string& rowsJson, SourceEvidenceRegistry& registry,
        SourceImportResult& imported, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile();
        const auto* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty()
            || pack->m_runtimeActionsEnabled || !pack->HasStableIdentity() || !pack->UsesSupportedSchema())
        {
            return Fail(error, "Save a compatible authoring mod before creating actors, troops or members.");
        }
        PathPolicyService policy;
        const auto validation = policy.ValidateWorkspacePaths(m_workspace, m_workspaceRootPath);
        if (!validation.IsSuccess()) { return Fail(error, Q(validation.GetError())); }
        const QString root = Q(m_workspaceRootPath);
        const QString directory = QDir(root).filePath("Sources/AuthoredPopulation");
        const auto engine = AZ::Utils::GetEnginePath();
        const QString enginePath = QString::fromUtf8(engine.c_str());
        if (root.isEmpty() || Inside(root, Q(profile->m_installPath))
            || QFileInfo(root).canonicalFilePath() == QFileInfo(Q(profile->m_installPath)).canonicalFilePath()
            || (!enginePath.isEmpty() && (Inside(root, enginePath)
                || QFileInfo(root).canonicalFilePath() == QFileInfo(enginePath).canonicalFilePath())))
        {
            return Fail(error, "The authoring workspace must be outside the game and engine.");
        }
        QString ancestor = directory;
        while (!QFileInfo::exists(ancestor))
        {
            const auto parent = QFileInfo(ancestor).absolutePath();
            if (ancestor == parent) { return Fail(error, "Cannot resolve the authoring source folder."); }
            ancestor = parent;
        }
        if (QFileInfo(ancestor).canonicalFilePath() != QFileInfo(root).canonicalFilePath() && !Inside(ancestor, root))
        {
            return Fail(error, "The authoring source folder escapes the workspace.");
        }
        if (!QDir().mkpath(directory) || !Inside(directory, root)) { return Fail(error, "Cannot create the authoring source folder."); }
        const auto rows = QJsonDocument::fromJson(Q(rowsJson).toUtf8()).array();
        if (rows.isEmpty() || rows.size() > 1001) { return Fail(error, "Invalid authoring evidence count."); }
        const QJsonObject document{{"SchemaVersion", 1}, {"DocumentKind", "foa-authored-population-definition"},
            {"ProfileId", Q(profile->m_profileId)}, {"GameVersion", Q(profile->m_gameVersion)},
            {"Branch", Q(profile->m_branch)}, {"RuntimeTarget", Q(profile->m_runtimeTarget)},
            {"CapturedAt", QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ssZ")}, {"evidence", rows}};
        const auto bytes = QJsonDocument(document).toJson(QJsonDocument::Compact);
        if (bytes.size() > 16 * 1024 * 1024) { return Fail(error, "Authoring evidence exceeds 16 MiB."); }
        const auto path = QDir(directory).filePath(Id("") + ".json");
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            return Fail(error, "Cannot save local population authoring evidence.");
        }
        return ImportEvidence(m_sourceImportService, path, document, bytes, *profile, registry, imported, error);
    }

    bool FoundationService::CreatePopulationRecord(const AZStd::string& kind, const AZStd::string& name,
        const AZStd::string& leaderActorId, AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear();
        const auto* profile = m_workspace.FindActiveGameProfile();
        const auto* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty())
        {
            return Fail(error, "Save a mod before creating an actor or troop.");
        }
        if ((kind != "actor" && kind != "troop") || Q(name).trimmed().isEmpty() || name.size() > 512)
        {
            return Fail(error, "Choose an actor or troop and enter a name up to 512 bytes.");
        }
        if (kind == "troop" && !m_catalog.FindPopulationActorProfile(leaderActorId))
        {
            return Fail(error, "Choose a saved actor as the new troop's leader.");
        }
        const auto id = A(Id(kind == "actor" ? "custom.actor." : "custom.troop."));
        const auto subject = "pack:" + Q(pack->m_packId) + "/" + Q(id);
        const auto row = Evidence(subject, Q(kind), "User-authored local " + Q(kind) + " named " + Q(name).trimmed()
            + ". Establishes authoring intent only.");
        QJsonArray rows{row};
        PopulationTroopDefinition troop;
        if (kind == "troop")
        {
            troop.m_profile.m_recordId = id; troop.m_profile.m_troopKind = "party";
            troop.m_profile.m_leaderActorRecordId = leaderActorId;
            troop.m_profile.m_minimumSize = 1; troop.m_profile.m_maximumSize = 1;
            troop.m_profile.m_formation = "unspecified"; troop.m_profile.m_evidenceIds = {Field(row, "evidence_id")};
            PopulationTroopMember member;
            member.m_linkId = A(Id("custom.member.")); member.m_troopRecordId = id; member.m_actorRecordId = leaderActorId;
            member.m_role = "leader"; member.m_minimumCount = 1; member.m_maximumCount = 1;
            member.m_required = true; member.m_weight = 1;
            rows.append(MemberEvidence(member)); troop.m_members.push_back(member);
        }
        SourceEvidenceRegistry registry = m_sourceRegistry;
        SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(A(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))),
                registry, imported, error)) { return false; }
        CatalogPromotionRequest promotion;
        promotion.m_recordId = id; promotion.m_domain = "population"; promotion.m_recordKind = kind;
        promotion.m_subjectRef = A(subject); promotion.m_identityKind = "synthetic"; promotion.m_ownerPackId = pack->m_packId;
        promotion.m_displayName = A(Q(name).trimmed()); promotion.m_evidenceId = Field(row, "evidence_id");
        promotion.m_confidence = "documented"; promotion.m_researchStage = "S1";
        auto record = m_catalogPromotion.BuildReviewedRecord(promotion, m_workspace, m_packs, registry);
        if (!record.IsSuccess()) { return Fail(error, Q(record.GetError())); }
        CatalogDatabase candidate = m_catalog;
        if (!candidate.InsertNew(record.GetValue(), error)) { return false; }
        if (kind == "actor")
        {
            PopulationActorProfile actor;
            actor.m_recordId = id; actor.m_actorKind = "npc"; actor.m_archetype = "custom";
            actor.m_minimumLevel = 1; actor.m_maximumLevel = 1; actor.m_evidenceIds = {promotion.m_evidenceId};
            auto authored = m_populationAuthoring.BuildActorProfileCandidate(actor, m_workspaceRootPath, m_workspace,
                *profile, *pack, registry, candidate);
            if (!authored.IsSuccess()) { return Fail(error, Q(authored.GetError())); }
            candidate = authored.TakeValue();
        }
        else
        {
            auto authored = m_populationAuthoring.BuildTroopDefinitionCandidate(troop, m_workspaceRootPath, m_workspace,
                *profile, *pack, registry, candidate);
            if (!authored.IsSuccess()) { return Fail(error, Q(authored.GetError())); }
            candidate = authored.TakeValue();
        }
        if (!CommitPopulationIntake(candidate, AZStd::move(registry), imported, error)) { return false; }
        recordId = id;
        return true;
    }

    bool FoundationService::SaveAuthoredPopulationTroop(const PopulationTroopDefinition& definition, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile();
        const auto* pack = GetActivePack();
        if (!profile || !pack) { return Fail(error, "Choose a saved authoring mod first."); }
        PopulationTroopDefinition authored = definition;
        if (authored.m_members.empty() || authored.m_members.size() > 1000)
        {
            return Fail(error, "A troop needs between 1 and 1,000 member rows.");
        }
        QJsonArray rows;
        for (auto& member : authored.m_members)
        {
            const auto previousEvidence = member.m_evidenceIds;
            rows.append(MemberEvidence(member));
            // Preserve independently supplied evidence for an unresolved actor.
            // The new link intent does not establish that external subject.
            for (const auto& id : previousEvidence)
            {
                const auto* evidence = m_sourceRegistry.FindEvidence(id);
                if (evidence && !member.m_actorSubjectRef.empty() && evidence->m_subjectRef == member.m_actorSubjectRef)
                {
                    member.m_evidenceIds.push_back(id);
                }
            }
        }
        SourceEvidenceRegistry registry = m_sourceRegistry;
        SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(A(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))),
                registry, imported, error)) { return false; }
        auto candidate = m_populationAuthoring.BuildTroopDefinitionCandidate(authored, m_workspaceRootPath, m_workspace,
            *profile, *pack, registry, m_catalog);
        if (!candidate.IsSuccess()) { return Fail(error, Q(candidate.GetError())); }
        return CommitPopulationIntake(candidate.GetValue(), AZStd::move(registry), imported, error);
    }

    bool FoundationService::CommitPopulationIntake(const CatalogDatabase& candidate, SourceEvidenceRegistry registry,
        const SourceImportResult& imported, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile();
        if (!profile) { return Fail(error, "The game profile changed before saving."); }
        WorkspaceModel workspace = m_workspace;
        workspace.m_rootPath = m_workspaceRootPath;
        auto committed = m_catalogTransaction.Commit(candidate, workspace, *profile, registry,
            [this, &imported](const CatalogDocument& document, const AZStd::string& root)
                -> AZ::Outcome<AZStd::string, AZStd::string>
            {
                auto evidence = m_sourceEvidencePersistence.SaveDocuments(imported.m_sourceDocument, imported.m_evidenceDocument, root);
                if (!evidence.IsSuccess()) { return AZ::Failure(AZStd::string(evidence.GetError())); }
                return m_catalogPersistence.Save(document, root);
            });
        if (!committed.IsSuccess()) { return Fail(error, Q(committed.GetError())); }
        auto result = committed.TakeValue();
        m_catalog = AZStd::move(result.m_catalog); m_catalogFilePath = AZStd::move(result.m_filePath);
        m_sourceRegistry = AZStd::move(registry);
        RefreshSnapshot();
        return true;
    }
} // namespace TaintedGrailModdingSDK
