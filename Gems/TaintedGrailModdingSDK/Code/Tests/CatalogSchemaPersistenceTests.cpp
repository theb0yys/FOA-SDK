/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "CatalogSchemaPersistenceService.h"
#include "CatalogTransactionService.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* SourceId = "preview.source.catalog";
        constexpr const char* SourceFingerprint =
            "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return { utf8.constData(), static_cast<size_t>(utf8.size()) };
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
        }

        template<class T>
        void ReflectIfMissing(AZ::SerializeContext& context)
        {
            if (!context.FindClassData(azrtti_typeid<T>()))
            {
                T::Reflect(&context);
            }
        }

        bool EnsureCatalogReflection()
        {
            AZ::SerializeContext* context = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(
                context,
                &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            if (!context)
            {
                return false;
            }
            ReflectIfMissing<CatalogRecord>(*context);
            ReflectIfMissing<CatalogRelationship>(*context);
            ReflectIfMissing<CatalogValidationEvent>(*context);
            ReflectIfMissing<CatalogGovernanceEvent>(*context);
            ReflectIfMissing<EconomyItemProfile>(*context);
            ReflectIfMissing<EconomyRecipeProfile>(*context);
            ReflectIfMissing<EconomyRecipeIngredient>(*context);
            ReflectIfMissing<EconomyRecipeOutput>(*context);
            ReflectIfMissing<CatalogDocument>(*context);
            return true;
        }

        GameProfile MakeProfile()
        {
            GameProfile profile;
            profile.m_profileId = "preview.profile";
            profile.m_displayName = "Preview profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/BepInEx/plugins";
            return profile;
        }

        WorkspaceModel MakeWorkspace(const AZStd::string& root)
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "preview.workspace";
            workspace.m_displayName = "Preview workspace";
            workspace.m_rootPath = root;
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = "preview.profile";
            workspace.m_gameProfiles.push_back(MakeProfile());
            return workspace;
        }

        SourceRecord MakeSource(const GameProfile& profile)
        {
            SourceRecord source;
            source.m_sourceId = SourceId;
            source.m_title = "Synthetic catalog persistence source";
            source.m_sourceKind = "synthetic-fixture";
            source.m_locator = "Sources/preview.catalog.json";
            source.m_fingerprint = SourceFingerprint;
            source.m_profileId = profile.m_profileId;
            source.m_gameVersion = profile.m_gameVersion;
            source.m_branch = profile.m_branch;
            source.m_runtimeTarget = profile.m_runtimeTarget;
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "preview.importer";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T12:00:00Z";
            source.m_importedAt = "2026-07-20T12:01:00Z";
            source.m_limitations = "Project-owned synthetic persistence fixture.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 1024;
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence(
            const GameProfile& profile,
            AZStd::string evidenceId,
            AZStd::string subjectRef)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = SourceId;
            evidence.m_sourceFingerprint = SourceFingerprint;
            evidence.m_profileId = profile.m_profileId;
            evidence.m_gameVersion = profile.m_gameVersion;
            evidence.m_branch = profile.m_branch;
            evidence.m_subjectRef = AZStd::move(subjectRef);
            evidence.m_claim = "Synthetic exact-subject catalog evidence.";
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = "Sources/preview.catalog.json";
            evidence.m_recordPath = "/records/0";
            evidence.m_extractedAt = "2026-07-20T12:00:30Z";
            return evidence;
        }

        CatalogRecord MakeRecord(
            AZStd::string recordId,
            AZStd::string domain,
            AZStd::string kind,
            AZStd::string subjectRef,
            AZStd::string evidenceId,
            bool synthetic)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = synthetic ? "preview.pack" : "";
            record.m_domain = AZStd::move(domain);
            record.m_recordKind = AZStd::move(kind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_nativeRefExact = synthetic ? "" : "native:" + record.m_recordId;
            record.m_identityKind = synthetic ? "synthetic" : "native";
            record.m_displayName = record.m_recordId;
            record.m_researchStage = "reviewed";
            record.m_confidence = "documented";
            record.m_operationalRisk = "unknown";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            return record;
        }

        struct PopulationFixture
        {
            WorkspaceModel m_workspace;
            GameProfile m_profile;
            SourceEvidenceRegistry m_registry;
            CatalogDatabase m_catalog;
        };

        bool BuildPopulationFixture(
            const AZStd::string& root,
            PopulationFixture& fixture,
            AZStd::string& error)
        {
            fixture.m_workspace = MakeWorkspace(root);
            fixture.m_profile = fixture.m_workspace.m_gameProfiles.front();
            if (!fixture.m_registry.RegisterSource(MakeSource(fixture.m_profile), &error)
                || !fixture.m_registry.RegisterEvidence(
                    MakeEvidence(
                        fixture.m_profile,
                        "preview.evidence.actor",
                        "FoA/Actors/PreviewLeader"),
                    &error)
                || !fixture.m_registry.RegisterEvidence(
                    MakeEvidence(
                        fixture.m_profile,
                        "preview.evidence.troop",
                        "FoA/Troops/PreviewPatrol"),
                    &error))
            {
                return false;
            }
            if (!fixture.m_catalog.InsertNew(
                    MakeRecord(
                        "preview.actor.leader",
                        "population",
                        "actor",
                        "FoA/Actors/PreviewLeader",
                        "preview.evidence.actor",
                        true),
                    &error)
                || !fixture.m_catalog.InsertNew(
                    MakeRecord(
                        "preview.troop.patrol",
                        "population",
                        "troop",
                        "FoA/Troops/PreviewPatrol",
                        "preview.evidence.troop",
                        true),
                    &error))
            {
                return false;
            }

            PopulationActorProfile actor;
            actor.m_recordId = "preview.actor.leader";
            actor.m_actorKind = "npc";
            actor.m_archetype = "preview-leader";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 10;
            actor.m_tags = { "zeta", "alpha" };
            actor.m_evidenceIds = { "preview.evidence.actor" };

            PopulationTroopProfile troop;
            troop.m_recordId = "preview.troop.patrol";
            troop.m_troopKind = "patrol";
            troop.m_leaderActorRecordId = actor.m_recordId;
            troop.m_leaderActorSubjectRef = "FoA/Actors/PreviewLeader";
            troop.m_minimumSize = 1;
            troop.m_maximumSize = 1;
            troop.m_evidenceIds = { "preview.evidence.troop" };

            PopulationTroopMember member;
            member.m_linkId = "preview.member.patrol-leader";
            member.m_troopRecordId = troop.m_recordId;
            member.m_actorRecordId = actor.m_recordId;
            member.m_actorSubjectRef = troop.m_leaderActorSubjectRef;
            member.m_role = "leader";
            member.m_conditions = { "rain", "morning" };
            member.m_evidenceIds = { "preview.evidence.actor" };

            return fixture.m_catalog.UpsertPopulationActorProfile(actor, &error)
                && fixture.m_catalog.UpsertPopulationTroopProfile(troop, &error)
                && fixture.m_catalog.UpsertPopulationTroopMember(member, &error)
                && fixture.m_catalog.ValidateIntegrity(
                    fixture.m_workspace,
                    fixture.m_profile,
                    fixture.m_registry,
                    &error);
        }

        CatalogDocument MakeLegacyDocument()
        {
            CatalogDocument document;
            document.m_schemaVersion = LegacyCatalogSchemaVersion;
            document.m_workspaceId = "legacy.workspace";
            document.m_profileId = "legacy.profile";
            document.m_gameVersion = "1.0.0";
            document.m_branch = "mono";
            document.m_records.push_back(MakeRecord(
                "legacy.item",
                "economy",
                "item",
                "FoA/Items/LegacyBlade",
                "legacy.evidence.item",
                false));
            document.m_records.push_back(MakeRecord(
                "legacy.container",
                "test",
                "container",
                "FoA/Containers/Legacy",
                "legacy.evidence.container",
                false));

            CatalogRelationship relationship;
            relationship.m_relationshipId = "legacy.relationship.item-container";
            relationship.m_fromRecordId = "legacy.item";
            relationship.m_toRecordId = "legacy.container";
            relationship.m_relationshipKind = "contained_by";
            relationship.m_evidenceIds = { "legacy.evidence.relationship" };
            document.m_relationships.push_back(relationship);

            CatalogValidationEvent validation;
            validation.m_validationId = "legacy.validation.item";
            validation.m_subjectKind = "record";
            validation.m_subjectId = "legacy.item";
            validation.m_recordId = "legacy.item";
            validation.m_state = "unvalidated";
            validation.m_method = "legacy-review";
            validation.m_validator = "legacy.validator";
            validation.m_checkedAt = "2026-07-20T11:00:00Z";
            validation.m_profileId = document.m_profileId;
            validation.m_gameVersion = document.m_gameVersion;
            validation.m_branch = document.m_branch;
            validation.m_evidenceIds = { "legacy.evidence.item" };
            document.m_validationHistory.push_back(validation);

            CatalogGovernanceEvent governance;
            governance.m_eventId = "legacy.governance.item";
            governance.m_subjectKind = "record";
            governance.m_subjectId = "legacy.item";
            governance.m_axis = "maturity";
            governance.m_newValue = "reviewed";
            governance.m_evidenceIds = { "legacy.evidence.item" };
            governance.m_validationIds = { validation.m_validationId };
            governance.m_reviewer = "legacy.reviewer";
            governance.m_decidedAt = "2026-07-20T11:01:00Z";
            document.m_governanceHistory.push_back(governance);

            EconomyItemProfile item;
            item.m_recordId = "legacy.item";
            item.m_category = "weapon";
            item.m_subtype = "blade";
            item.m_stackLimit = 1;
            item.m_weight = 2.5;
            item.m_baseValue = 25.0;
            item.m_evidenceIds = { "legacy.evidence.item" };
            document.m_economyItems.push_back(item);
            return document;
        }

        bool WriteRawCatalog(
            const CatalogSchemaPersistenceService& persistence,
            const AZStd::string& root,
            const CatalogDocument& document,
            AZStd::string& error)
        {
            const AZStd::string path = persistence.GetCatalogPath(root);
            if (path.empty()
                || !QDir().mkpath(QFileInfo(ToQString(path)).absolutePath()))
            {
                error = "Unable to create the raw catalog fixture directory.";
                return false;
            }
            const auto saved = AZ::JsonSerializationUtils::SaveObjectToFile(
                &document,
                path);
            if (!saved.IsSuccess())
            {
                error = AZStd::string(saved.GetError());
                return false;
            }
            return true;
        }

        QByteArray ReadFile(const AZStd::string& path)
        {
            QFile file(ToQString(path));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        }
    } // namespace

    TEST(CatalogSchemaPersistenceTests, LegacyLoadPromotesAndPreservesExistingData)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const CatalogDocument legacy = MakeLegacyDocument();
        const AZStd::string rawRoot = ToAzString(
            QDir(temporary.path()).filePath("legacy"));
        const AZStd::string savedRoot = ToAzString(
            QDir(temporary.path()).filePath("saved"));
        CatalogSchemaPersistenceService persistence;
        AZStd::string error;
        ASSERT_TRUE(WriteRawCatalog(persistence, rawRoot, legacy, error))
            << error.c_str();

        auto loaded = persistence.Load(rawRoot);
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        const CatalogDocument& migrated = loaded.GetValue();
        EXPECT_EQ(migrated.m_schemaVersion, CurrentCatalogSchemaVersion);
        EXPECT_EQ(migrated.m_records.size(), legacy.m_records.size());
        EXPECT_EQ(migrated.m_relationships.size(), 1);
        EXPECT_EQ(migrated.m_validationHistory.size(), 1);
        EXPECT_EQ(migrated.m_governanceHistory.size(), 1);
        EXPECT_EQ(migrated.m_economyItems.size(), 1);
        EXPECT_TRUE(migrated.m_actorProfiles.empty());
        EXPECT_TRUE(migrated.m_troopProfiles.empty());
        EXPECT_TRUE(migrated.m_troopMembers.empty());
        EXPECT_EQ(migrated.m_records.front().m_recordId, "legacy.item");
        EXPECT_EQ(migrated.m_economyItems.front().m_category, "weapon");
        EXPECT_EQ(
            migrated.m_governanceHistory.front().m_eventId,
            "legacy.governance.item");

        auto saved = persistence.Save(migrated, savedRoot);
        ASSERT_TRUE(saved.IsSuccess()) << saved.GetError().c_str();
        const QByteArray bytes = ReadFile(saved.GetValue());
        ASSERT_FALSE(bytes.isEmpty());
        EXPECT_TRUE(bytes.contains("\"SchemaVersion\": 2"));
        EXPECT_EQ(legacy.m_schemaVersion, LegacyCatalogSchemaVersion);
        auto reopened = persistence.Load(savedRoot);
        ASSERT_TRUE(reopened.IsSuccess()) << reopened.GetError().c_str();
        EXPECT_EQ(reopened.GetValue().m_records.size(), legacy.m_records.size());
        EXPECT_EQ(reopened.GetValue().m_economyItems.size(), 1);
    }

    TEST(CatalogSchemaPersistenceTests, SchemaTwoSaveClearAndReopenIsCanonical)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const AZStd::string firstRoot = ToAzString(
            QDir(temporary.path()).filePath("first"));
        const AZStd::string secondRoot = ToAzString(
            QDir(temporary.path()).filePath("second"));
        PopulationFixture fixture;
        AZStd::string error;
        ASSERT_TRUE(BuildPopulationFixture(firstRoot, fixture, error))
            << error.c_str();

        const CatalogDocument original = fixture.m_catalog.BuildDocument(
            fixture.m_workspace,
            fixture.m_profile);
        ASSERT_EQ(original.m_schemaVersion, CurrentCatalogSchemaVersion);
        ASSERT_EQ(original.m_actorProfiles.size(), 1);
        ASSERT_EQ(original.m_troopProfiles.size(), 1);
        ASSERT_EQ(original.m_troopMembers.size(), 1);
        EXPECT_EQ(original.m_actorProfiles.front().m_tags.front(), "alpha");
        EXPECT_EQ(original.m_troopMembers.front().m_conditions.front(), "morning");

        CatalogSchemaPersistenceService persistence;
        auto firstSave = persistence.Save(original, firstRoot);
        ASSERT_TRUE(firstSave.IsSuccess()) << firstSave.GetError().c_str();
        const QByteArray firstBytes = ReadFile(firstSave.GetValue());
        ASSERT_FALSE(firstBytes.isEmpty());
        EXPECT_TRUE(firstBytes.contains("\"SchemaVersion\": 2"));

        fixture.m_catalog.Clear();
        EXPECT_TRUE(fixture.m_catalog.GetRecords().empty());
        EXPECT_TRUE(fixture.m_catalog.GetPopulationActorProfiles().empty());
        auto loaded = persistence.Load(firstRoot);
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        CatalogDatabase reopened;
        ASSERT_TRUE(reopened.ReplaceFromBoundDocument(
            loaded.GetValue(),
            fixture.m_workspace,
            fixture.m_profile,
            fixture.m_registry,
            &error)) << error.c_str();

        auto secondSave = persistence.Save(
            reopened.BuildDocument(fixture.m_workspace, fixture.m_profile),
            secondRoot);
        ASSERT_TRUE(secondSave.IsSuccess()) << secondSave.GetError().c_str();
        EXPECT_EQ(ReadFile(secondSave.GetValue()), firstBytes);
    }

    TEST(CatalogSchemaPersistenceTests, UnsupportedAndMalformedSchemasFailClosed)
    {
        ASSERT_TRUE(EnsureCatalogReflection());
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        CatalogSchemaPersistenceService persistence;
        AZStd::string error;

        CatalogDocument future = MakeLegacyDocument();
        future.m_schemaVersion = CurrentCatalogSchemaVersion + 1;
        const AZStd::string futureRoot = ToAzString(
            QDir(temporary.path()).filePath("future"));
        ASSERT_TRUE(WriteRawCatalog(persistence, futureRoot, future, error))
            << error.c_str();
        auto futureLoad = persistence.Load(futureRoot);
        EXPECT_FALSE(futureLoad.IsSuccess());
        EXPECT_NE(
            futureLoad.GetError().find("Unsupported canonical catalog schema"),
            AZStd::string::npos);

        CatalogDocument smuggled = MakeLegacyDocument();
        smuggled.m_actorProfiles.push_back(PopulationActorProfile{});
        const AZStd::string smuggledRoot = ToAzString(
            QDir(temporary.path()).filePath("smuggled"));
        ASSERT_TRUE(WriteRawCatalog(
            persistence,
            smuggledRoot,
            smuggled,
            error)) << error.c_str();
        auto smuggledLoad = persistence.Load(smuggledRoot);
        EXPECT_FALSE(smuggledLoad.IsSuccess());
        EXPECT_NE(
            smuggledLoad.GetError().find(
                "schema 1 cannot contain population collections"),
            AZStd::string::npos);
    }

    TEST(CatalogSchemaPersistenceTests, FailedWriteDoesNotPublishCandidateCatalog)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        PopulationFixture fixture;
        AZStd::string error;
        ASSERT_TRUE(BuildPopulationFixture(
            ToAzString(temporary.path()),
            fixture,
            error)) << error.c_str();

        CatalogDatabase published = fixture.m_catalog;
        CatalogDatabase candidate = published;
        ASSERT_TRUE(fixture.m_registry.RegisterEvidence(
            MakeEvidence(
                fixture.m_profile,
                "preview.evidence.actor-second",
                "FoA/Actors/PreviewSecond"),
            &error)) << error.c_str();
        ASSERT_TRUE(candidate.InsertNew(
            MakeRecord(
                "preview.actor.second",
                "population",
                "actor",
                "FoA/Actors/PreviewSecond",
                "preview.evidence.actor-second",
                true),
            &error)) << error.c_str();
        PopulationActorProfile actor;
        actor.m_recordId = "preview.actor.second";
        actor.m_actorKind = "npc";
        actor.m_archetype = "preview-second";
        actor.m_evidenceIds = { "preview.evidence.actor-second" };
        ASSERT_TRUE(candidate.UpsertPopulationActorProfile(actor, &error))
            << error.c_str();

        CatalogTransactionService transaction;
        const auto commit = transaction.Commit(
            candidate,
            fixture.m_workspace,
            fixture.m_profile,
            fixture.m_registry,
            [](const CatalogDocument&, const AZStd::string&)
                -> AZ::Outcome<AZStd::string, AZStd::string>
            {
                return AZ::Failure(AZStd::string(
                    "Synthetic catalog write failure."));
            });
        EXPECT_FALSE(commit.IsSuccess());
        EXPECT_NE(
            commit.GetError().find("Synthetic catalog write failure"),
            AZStd::string::npos);
        EXPECT_EQ(published.FindByRecordId("preview.actor.second"), nullptr);
        EXPECT_EQ(
            published.FindPopulationActorProfile("preview.actor.second"),
            nullptr);
        EXPECT_EQ(published.GetPopulationActorProfiles().size(), 1);
    }
} // namespace TaintedGrailModdingSDK
