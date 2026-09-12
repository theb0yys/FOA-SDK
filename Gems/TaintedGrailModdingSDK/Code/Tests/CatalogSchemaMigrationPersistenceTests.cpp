/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPersistenceService.h"
#include "CatalogTransactionService.h"
#include "FoundationService.h"
#include "PersistenceJsonUtils.h"
#include "SourceEvidencePersistenceService.h"
#include "WorkspacePersistenceService.h"

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/Json/RegistrationContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzTest/AzTest.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QTemporaryDir>

#ifndef TG_SDK_PREVIEW_TEMPLATE_ROOT
#   error TG_SDK_PREVIEW_TEMPLATE_ROOT must identify the Developer Preview fixture template directory.
#endif

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ActorRecordId = "population.actor.guard";
        constexpr const char* ActorSubject = "subject:population:actor:guard";
        constexpr const char* ActorEvidenceId = "evidence.population.actor";
        constexpr const char* TroopRecordId = "population.troop.patrol";
        constexpr const char* TroopSubject = "subject:population:troop:patrol";
        constexpr const char* TroopEvidenceId = "evidence.population.troop";
        constexpr const char* MemberLinkId = "population.member.patrol.guard";
        constexpr const char* MemberSubject = "population-troop-member:population.member.patrol.guard";
        constexpr const char* MemberEvidenceId = "evidence.population.member";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string CatalogPath(const AZStd::string& workspaceRoot)
        {
            return ToAzString(QDir(ToQString(workspaceRoot)).filePath(
                QStringLiteral("Catalog/catalog.tgcatalog.json")));
        }

        AZStd::string PreviewPath(const char* relativePath)
        {
            return ToAzString(QDir(QString::fromUtf8(TG_SDK_PREVIEW_TEMPLATE_ROOT)).filePath(
                    QString::fromUtf8(relativePath)));
        }

        void ReflectCatalogPersistenceTypes(AZ::SerializeContext& context)
        {
            GameProfile::Reflect(&context);
            WorkspaceModel::Reflect(&context);
            PackManifest::Reflect(&context);
            SourceRecord::Reflect(&context);
            EvidenceRecord::Reflect(&context);
            ImportIssue::Reflect(&context);
            SourceDocument::Reflect(&context);
            EvidenceDocument::Reflect(&context);
            CatalogRecord::Reflect(&context);
            CatalogRelationship::Reflect(&context);
            CatalogValidationEvent::Reflect(&context);
            CatalogGovernanceEvent::Reflect(&context);
            EconomyItemProfile::Reflect(&context);
            EconomyRecipeProfile::Reflect(&context);
            EconomyRecipeIngredient::Reflect(&context);
            EconomyRecipeOutput::Reflect(&context);
            CatalogDocument::Reflect(&context);
        }

        template<class ObjectType>
        AZ::Outcome<AZStd::string, AZStd::string> SerializeObject(
            const ObjectType& object)
        {
            AZStd::string output;
            AZ::IO::ByteContainerStream<AZStd::string> stream(&output);
            const AZ::Outcome<void, AZStd::string> result =
                AZ::JsonSerializationUtils::SaveObjectToStream(
                    &object,
                    stream);
            if (!result.IsSuccess())
            {
                return AZ::Failure(AZStd::string(result.GetError()));
            }
            return AZ::Success(AZStd::move(output));
        }

        AZ::Outcome<QByteArray, AZStd::string> ReadFile(
            const AZStd::string& path)
        {
            QFile file(ToQString(path));
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(
                    AZStd::string("Unable to read test catalog: ") + path);
            }
            return AZ::Success(file.readAll());
        }

        AZ::Outcome<void, AZStd::string> WriteCatalogBytes(
            const AZStd::string& workspaceRoot,
            const QByteArray& bytes)
        {
            const AZStd::string path = CatalogPath(workspaceRoot);
            if (!QDir().mkpath(QFileInfo(ToQString(path)).absolutePath()))
            {
                return AZ::Failure(AZStd::string(
                    "Unable to create the test catalog directory."));
            }

            QSaveFile file(ToQString(path));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return AZ::Failure(AZStd::string(
                    "Unable to open the test catalog for writing."));
            }
            if (file.write(bytes) != bytes.size() || !file.commit())
            {
                return AZ::Failure(AZStd::string(
                    "Unable to commit the test catalog."));
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> WriteCatalogJson(
            const AZStd::string& workspaceRoot,
            const QJsonDocument& document)
        {
            return WriteCatalogBytes(
                workspaceRoot,
                document.toJson(QJsonDocument::Indented));
        }

        QJsonObject MakePlainEmptyCatalogObject()
        {
            QJsonObject object;
            object.insert(QStringLiteral("SchemaVersion"), static_cast<int>(PopulationCatalogSchemaVersion));
            object.insert(QStringLiteral("WorkspaceId"), QStringLiteral("test.workspace"));
            object.insert(QStringLiteral("ProfileId"), QStringLiteral("foa.population.test"));
            object.insert(QStringLiteral("GameVersion"), QStringLiteral("1.0-test"));
            object.insert(QStringLiteral("Branch"), QStringLiteral("preview"));
            object.insert(QStringLiteral("Records"), QJsonArray{});
            object.insert(QStringLiteral("Relationships"), QJsonArray{});
            object.insert(QStringLiteral("ValidationHistory"), QJsonArray{});
            object.insert(QStringLiteral("GovernanceHistory"), QJsonArray{});
            object.insert(QStringLiteral("EconomyItems"), QJsonArray{});
            object.insert(QStringLiteral("EconomyRecipes"), QJsonArray{});
            object.insert(QStringLiteral("RecipeIngredients"), QJsonArray{});
            object.insert(QStringLiteral("RecipeOutputs"), QJsonArray{});
            object.insert(QStringLiteral("ActorProfiles"), QJsonArray{});
            object.insert(QStringLiteral("TroopProfiles"), QJsonArray{});
            object.insert(QStringLiteral("TroopMembers"), QJsonArray{});
            return object;
        }

        WorkspaceModel MakePopulationWorkspace(const AZStd::string& rootPath)
        {
            GameProfile profile;
            profile.m_profileId = "foa.population.test";
            profile.m_displayName = "Population schema test profile";
            profile.m_installPath = "/test/game";
            profile.m_gameVersion = "1.0-test";
            profile.m_branch = "preview";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.22f1";
            profile.m_bepInExVersion = "5.4.23.2";
            profile.m_managedAssembliesPath = "/test/game/Data/Managed";
            profile.m_pluginPath = "/test/game/BepInEx/plugins";

            WorkspaceModel workspace;
            workspace.m_workspaceId = "test.workspace";
            workspace.m_displayName = "Population schema test workspace";
            workspace.m_rootPath = rootPath;
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(AZStd::move(profile));
            return workspace;
        }

        SourceEvidenceRegistry MakePopulationRegistry()
        {
            SourceRecord source;
            source.m_sourceId = "source.population.test";
            source.m_title = "Population schema synthetic fixture";
            source.m_sourceKind = "test-fixture";
            source.m_locator = "Tests/Fixtures/population-schema.json";
            source.m_fingerprint = "sha256:" + AZStd::string(64, 'a');
            source.m_profileId = "foa.population.test";
            source.m_gameVersion = "1.0-test";
            source.m_branch = "preview";
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "catalog-schema-tests";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "test.population.importer";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-20T00:00:00Z";
            source.m_importedAt = "2026-07-20T00:00:02Z";
            source.m_limitations = "Project-owned synthetic test fixture.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 512;
            source.m_importStatus = "imported";

            SourceEvidenceRegistry registry;
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(source, &error))
                << error.c_str();

            const auto registerEvidence =
                [&registry, &source, &error](
                    const char* evidenceId,
                    const char* subject,
                    const char* recordPath)
                {
                    EvidenceRecord evidence;
                    evidence.m_evidenceId = evidenceId;
                    evidence.m_sourceId = source.m_sourceId;
                    evidence.m_sourceFingerprint = source.m_fingerprint;
                    evidence.m_profileId = source.m_profileId;
                    evidence.m_gameVersion = source.m_gameVersion;
                    evidence.m_branch = source.m_branch;
                    evidence.m_subjectRef = subject;
                    evidence.m_claim = "Synthetic population schema claim.";
                    evidence.m_evidenceKind = "structured_record";
                    evidence.m_confidence = "documented";
                    evidence.m_locator = source.m_locator;
                    evidence.m_recordPath = recordPath;
                    evidence.m_extractedAt = "2026-07-20T00:00:01Z";
                    EXPECT_TRUE(registry.RegisterEvidence(evidence, &error))
                        << error.c_str();
                };

            registerEvidence(ActorEvidenceId, ActorSubject, "/actors/0");
            registerEvidence(TroopEvidenceId, TroopSubject, "/troops/0");
            registerEvidence(MemberEvidenceId, MemberSubject, "/troops/0/members/0");
            return registry;
        }

        CatalogRecord MakePopulationRecord(
            const char* recordId,
            const char* subject,
            const char* recordKind,
            const char* evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = recordId;
            record.m_ownerPackId = "pack.population.test";
            record.m_domain = "population";
            record.m_recordKind = recordKind;
            record.m_subjectRef = subject;
            record.m_identityKind = "synthetic";
            record.m_displayName = recordId;
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { evidenceId };
            record.m_createdAt = "2026-07-20T00:00:00Z";
            record.m_updatedAt = "2026-07-20T00:00:00Z";
            return record;
        }

        CatalogDatabase MakePopulationCatalog()
        {
            CatalogDatabase catalog;
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(
                MakePopulationRecord(
                    ActorRecordId,
                    ActorSubject,
                    "actor",
                    ActorEvidenceId),
                &error)) << error.c_str();
            EXPECT_TRUE(catalog.InsertNew(
                MakePopulationRecord(
                    TroopRecordId,
                    TroopSubject,
                    "troop",
                    TroopEvidenceId),
                &error)) << error.c_str();

            PopulationActorProfile actor;
            actor.m_recordId = ActorRecordId;
            actor.m_actorKind = "npc";
            actor.m_archetype = "synthetic_guard";
            actor.m_minimumLevel = 1;
            actor.m_maximumLevel = 20;
            actor.m_localisationNameRef = "loc.population.guard.name";
            actor.m_tags = { "guard", "synthetic" };
            actor.m_evidenceIds = { ActorEvidenceId };
            EXPECT_TRUE(catalog.UpsertPopulationActorProfile(actor, &error))
                << error.c_str();

            PopulationTroopProfile troop;
            troop.m_recordId = TroopRecordId;
            troop.m_troopKind = "patrol";
            troop.m_leaderActorRecordId = ActorRecordId;
            troop.m_leaderActorSubjectRef = ActorSubject;
            troop.m_minimumSize = 1;
            troop.m_maximumSize = 3;
            troop.m_formation = "line";
            troop.m_tags = { "patrol", "synthetic" };
            troop.m_evidenceIds = { TroopEvidenceId };
            EXPECT_TRUE(catalog.UpsertPopulationTroopProfile(troop, &error))
                << error.c_str();

            PopulationTroopMember member;
            member.m_linkId = MemberLinkId;
            member.m_troopRecordId = TroopRecordId;
            member.m_actorRecordId = ActorRecordId;
            member.m_actorSubjectRef = ActorSubject;
            member.m_role = "leader";
            member.m_minimumCount = 1;
            member.m_maximumCount = 1;
            member.m_weight = 1.0;
            member.m_required = true;
            member.m_conditions = { "preview_only" };
            member.m_evidenceIds = { MemberEvidenceId };
            EXPECT_TRUE(catalog.UpsertPopulationTroopMember(member, &error))
                << error.c_str();
            return catalog;
        }

        AZ::Outcome<SourceEvidenceRegistry, AZStd::string>
        LoadPreviewRegistry()
        {
            SourceEvidencePersistenceService persistence;
            AZStd::vector<SourceDocument> sources;
            AZStd::vector<EvidenceDocument> evidenceDocuments;
            AZStd::vector<ImportIssue> issues;
            const auto load = persistence.LoadWorkspaceDocuments(
                TG_SDK_PREVIEW_TEMPLATE_ROOT,
                sources,
                evidenceDocuments,
                issues);
            if (!load.IsSuccess())
            {
                return AZ::Failure(AZStd::string(load.GetError()));
            }
            for (const ImportIssue& issue : issues)
            {
                if (issue.m_severity == "error")
                {
                    return AZ::Failure(
                        AZStd::string("Preview registry fixture error: ")
                        + issue.m_message);
                }
            }

            SourceEvidenceRegistry registry;
            AZStd::string error;
            for (const SourceDocument& source : sources)
            {
                if (!registry.RegisterSource(source.m_source, &error))
                {
                    return AZ::Failure(error);
                }
            }
            for (const EvidenceDocument& document : evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    if (!registry.RegisterEvidence(evidence, &error))
                    {
                        return AZ::Failure(error);
                    }
                }
            }
            return AZ::Success(AZStd::move(registry));
        }

        class CatalogSchemaMigrationPersistenceTests
            : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                AZ::ComponentApplication::Descriptor descriptor;
                descriptor.m_useExistingAllocator = true;
                AZ::ComponentApplication::StartupParameters startup;
                startup.m_loadStaticModules = false;
                startup.m_loadDynamicModules = false;
                startup.m_loadAssetCatalog = false;
                startup.m_loadSettingsRegistry = false;
                ASSERT_NE(m_application.Create(descriptor, startup), nullptr);
                m_created = true;

                if (!AZ::IO::FileIOBase::GetInstance())
                {
                    AZ::IO::FileIOBase::SetInstance(&m_fileIO);
                    m_installedFileIo = true;
                }

                AZ::SerializeContext* serializeContext =
                    m_application.GetSerializeContext();
                AZ::JsonRegistrationContext* jsonRegistrationContext =
                    m_application.GetJsonRegistrationContext();
                ASSERT_NE(serializeContext, nullptr);
                ASSERT_NE(jsonRegistrationContext, nullptr);
                AZ::JsonSystemComponent::Reflect(serializeContext);
                AZ::JsonSystemComponent::Reflect(jsonRegistrationContext);
                ReflectCatalogPersistenceTypes(*serializeContext);
            }

            void TearDown() override
            {
                if (AZ::SerializeContext* serializeContext =
                        m_application.GetSerializeContext())
                {
                    serializeContext->EnableRemoveReflection();
                    ReflectCatalogPersistenceTypes(*serializeContext);
                    AZ::JsonSystemComponent::Reflect(serializeContext);
                    serializeContext->DisableRemoveReflection();
                }
                if (AZ::JsonRegistrationContext* jsonRegistrationContext =
                        m_application.GetJsonRegistrationContext())
                {
                    jsonRegistrationContext->EnableRemoveReflection();
                    AZ::JsonSystemComponent::Reflect(jsonRegistrationContext);
                    jsonRegistrationContext->DisableRemoveReflection();
                }
                if (m_created)
                {
                    m_application.Destroy();
                }
                if (m_installedFileIo)
                {
                    AZ::IO::FileIOBase::SetInstance(nullptr);
                }
            }

            AZ::ComponentApplication m_application;
            AZ::IO::LocalFileIO m_fileIO;
            bool m_created = false;
            bool m_installedFileIo = false;
        };
    } // namespace

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        SchemaOnePreviewMigratesToCurrentWithEmptyPopulationAndPreservesLegacyProjection)
    {
        EXPECT_EQ(CurrentCatalogSchemaVersion, AssetLocalisationCatalogSchemaVersion);
        WorkspacePersistenceService workspacePersistence;
        auto workspaceResult = workspacePersistence.Load(
            PreviewPath("preview.tgworkspace.json"));
        ASSERT_TRUE(workspaceResult.IsSuccess())
            << workspaceResult.GetError().c_str();
        WorkspaceModel workspace = workspaceResult.TakeValue();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);

        auto registryResult = LoadPreviewRegistry();
        ASSERT_TRUE(registryResult.IsSuccess())
            << registryResult.GetError().c_str();
        const SourceEvidenceRegistry registry = registryResult.TakeValue();

        CatalogPersistenceService persistence;
        auto rawFixture = AZ::JsonSerializationUtils::ReadJsonFile(
            CatalogPath(TG_SDK_PREVIEW_TEMPLATE_ROOT));
        ASSERT_TRUE(rawFixture.IsSuccess()) << rawFixture.GetError().c_str();
        ASSERT_TRUE(rawFixture.GetValue().IsObject());
        ASSERT_TRUE(rawFixture.GetValue().HasMember("SchemaVersion"));
        ASSERT_TRUE(rawFixture.GetValue()["SchemaVersion"].IsUint());
        EXPECT_EQ(rawFixture.GetValue()["SchemaVersion"].GetUint(), LegacyCatalogSchemaVersion);
        auto loadedResult = persistence.Load(TG_SDK_PREVIEW_TEMPLATE_ROOT);
        ASSERT_TRUE(loadedResult.IsSuccess())
            << loadedResult.GetError().c_str();
        CatalogDocument loaded = loadedResult.TakeValue();
        ASSERT_EQ(loaded.m_schemaVersion, LegacyCatalogSchemaVersion);
        EXPECT_TRUE(loaded.m_actorProfiles.empty());
        EXPECT_TRUE(loaded.m_troopProfiles.empty());
        EXPECT_TRUE(loaded.m_troopMembers.empty());

        QTemporaryDir unvalidatedSaveDirectory;
        ASSERT_TRUE(unvalidatedSaveDirectory.isValid());
        auto unvalidatedSave = persistence.Save(
            loaded,
            ToAzString(unvalidatedSaveDirectory.path()));
        EXPECT_FALSE(unvalidatedSave.IsSuccess());
        EXPECT_FALSE(QFile::exists(ToQString(
            CatalogPath(ToAzString(unvalidatedSaveDirectory.path())))));

        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.ReplaceFromBoundDocument(
            loaded,
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();

        CatalogDocument expected = loaded;
        expected.m_schemaVersion = CurrentCatalogSchemaVersion;
        const CatalogDocument migrated = catalog.BuildDocument(workspace, *profile);
        EXPECT_EQ(migrated.m_schemaVersion, CurrentCatalogSchemaVersion);
        EXPECT_TRUE(migrated.m_actorProfiles.empty());
        EXPECT_TRUE(migrated.m_troopProfiles.empty());
        EXPECT_TRUE(migrated.m_troopMembers.empty());

        auto expectedJson = SerializeObject(expected);
        auto migratedJson = SerializeObject(migrated);
        ASSERT_TRUE(expectedJson.IsSuccess()) << expectedJson.GetError().c_str();
        ASSERT_TRUE(migratedJson.IsSuccess()) << migratedJson.GetError().c_str();
        EXPECT_EQ(expectedJson.GetValue(), migratedJson.GetValue());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        PlainCatalogRejectsMissingMalformedAndFutureSchemaVersions)
    {
        CatalogPersistenceService persistence;

        QTemporaryDir missingDirectory;
        ASSERT_TRUE(missingDirectory.isValid());
        QJsonObject missing = MakePlainEmptyCatalogObject();
        missing.remove(QStringLiteral("SchemaVersion"));
        auto write = WriteCatalogJson(
            ToAzString(missingDirectory.path()),
            QJsonDocument(missing));
        ASSERT_TRUE(write.IsSuccess()) << write.GetError().c_str();
        auto missingResult = persistence.Load(
            ToAzString(missingDirectory.path()));
        ASSERT_FALSE(missingResult.IsSuccess());
        EXPECT_NE(
            missingResult.GetError().find("SchemaVersion"),
            AZStd::string::npos);

        QTemporaryDir malformedDirectory;
        ASSERT_TRUE(malformedDirectory.isValid());
        QJsonObject malformed = MakePlainEmptyCatalogObject();
        malformed.insert(QStringLiteral("SchemaVersion"), QStringLiteral("2"));
        write = WriteCatalogJson(
            ToAzString(malformedDirectory.path()),
            QJsonDocument(malformed));
        ASSERT_TRUE(write.IsSuccess()) << write.GetError().c_str();
        auto malformedResult = persistence.Load(
            ToAzString(malformedDirectory.path()));
        ASSERT_FALSE(malformedResult.IsSuccess());
        EXPECT_NE(
            malformedResult.GetError().find("SchemaVersion"),
            AZStd::string::npos);

        QTemporaryDir futureDirectory;
        ASSERT_TRUE(futureDirectory.isValid());
        QJsonObject future = MakePlainEmptyCatalogObject();
        future.insert(QStringLiteral("SchemaVersion"), static_cast<int>(CurrentCatalogSchemaVersion + 1));
        future.insert(QStringLiteral("FuturePopulationData"), QJsonArray{});
        write = WriteCatalogJson(
            ToAzString(futureDirectory.path()),
            QJsonDocument(future));
        ASSERT_TRUE(write.IsSuccess()) << write.GetError().c_str();
        auto futureResult = persistence.Load(ToAzString(futureDirectory.path()));
        ASSERT_FALSE(futureResult.IsSuccess());
        EXPECT_NE(futureResult.GetError().find("5"), AZStd::string::npos);
        EXPECT_NE(
            futureResult.GetError().find("unsupported"),
            AZStd::string::npos);
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        LegacySerializationEnvelopeWithoutSchemaVersionMigratesAsSchemaOne)
    {
        CatalogPersistenceService persistence;
        CatalogDocument legacyDocument;
        auto previewResult = PersistenceJsonUtils::LoadObjectFromFile(
            legacyDocument,
            CatalogPath(TG_SDK_PREVIEW_TEMPLATE_ROOT));
        ASSERT_TRUE(previewResult.IsSuccess())
            << previewResult.GetError().c_str();
        ASSERT_EQ(legacyDocument.m_schemaVersion, LegacyCatalogSchemaVersion);
        auto serialized = SerializeObject(legacyDocument);
        ASSERT_TRUE(serialized.IsSuccess()) << serialized.GetError().c_str();

        QJsonParseError parseError;
        QJsonDocument envelope = QJsonDocument::fromJson(
            QByteArray(
                serialized.GetValue().data(),
                static_cast<qsizetype>(serialized.GetValue().size())),
            &parseError);
        ASSERT_EQ(parseError.error, QJsonParseError::NoError);
        ASSERT_TRUE(envelope.isObject());
        QJsonObject root = envelope.object();
        ASSERT_TRUE(root.value(QStringLiteral("ClassData")).isObject());
        QJsonObject classData = root.value(QStringLiteral("ClassData")).toObject();
        ASSERT_TRUE(classData.contains(QStringLiteral("SchemaVersion")));
        classData.remove(QStringLiteral("SchemaVersion"));
        root.insert(QStringLiteral("ClassData"), classData);

        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        auto write = WriteCatalogJson(
            ToAzString(directory.path()),
            QJsonDocument(root));
        ASSERT_TRUE(write.IsSuccess()) << write.GetError().c_str();
        auto loaded = persistence.Load(ToAzString(directory.path()));
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        CatalogDocument normalizedLegacy = loaded.TakeValue();
        EXPECT_EQ(normalizedLegacy.m_schemaVersion, LegacyCatalogSchemaVersion);
        EXPECT_TRUE(normalizedLegacy.m_actorProfiles.empty());
        EXPECT_TRUE(normalizedLegacy.m_troopProfiles.empty());
        EXPECT_TRUE(normalizedLegacy.m_troopMembers.empty());

        WorkspacePersistenceService workspacePersistence;
        auto workspaceResult = workspacePersistence.Load(
            PreviewPath("preview.tgworkspace.json"));
        ASSERT_TRUE(workspaceResult.IsSuccess())
            << workspaceResult.GetError().c_str();
        WorkspaceModel workspace = workspaceResult.TakeValue();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        auto registryResult = LoadPreviewRegistry();
        ASSERT_TRUE(registryResult.IsSuccess()) << registryResult.GetError().c_str();

        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.ReplaceFromBoundDocument(
            normalizedLegacy,
            workspace,
            *profile,
            registryResult.GetValue(),
            &error)) << error.c_str();
        const CatalogDocument promoted = catalog.BuildDocument(workspace, *profile);
        EXPECT_EQ(promoted.m_schemaVersion, CurrentCatalogSchemaVersion);
        EXPECT_TRUE(promoted.m_actorProfiles.empty());
        EXPECT_TRUE(promoted.m_troopProfiles.empty());
        EXPECT_TRUE(promoted.m_troopMembers.empty());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        SchemaOnePopulationCollectionsAreRejectedWithoutReplacement)
    {
        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(directory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakePopulationRegistry();
        CatalogDatabase published = MakePopulationCatalog();
        AZStd::string error;
        ASSERT_TRUE(published.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();

        CatalogDocument illegal = published.BuildDocument(workspace, *profile);
        ASSERT_FALSE(illegal.m_actorProfiles.empty());
        illegal.m_schemaVersion = LegacyCatalogSchemaVersion;
        const auto before = SerializeObject(
            published.BuildDocument(workspace, *profile));
        ASSERT_TRUE(before.IsSuccess()) << before.GetError().c_str();

        auto serializedIllegal = SerializeObject(illegal);
        ASSERT_TRUE(serializedIllegal.IsSuccess())
            << serializedIllegal.GetError().c_str();
        auto written = WriteCatalogBytes(
            ToAzString(directory.path()),
            QByteArray(
                serializedIllegal.GetValue().data(),
                static_cast<qsizetype>(
                    serializedIllegal.GetValue().size())));
        ASSERT_TRUE(written.IsSuccess()) << written.GetError().c_str();
        CatalogPersistenceService persistence;
        auto durableLoad = persistence.Load(ToAzString(directory.path()));
        ASSERT_FALSE(durableLoad.IsSuccess());
        EXPECT_NE(
            durableLoad.GetError().find("schema 1"),
            AZStd::string::npos);
        EXPECT_NE(
            durableLoad.GetError().find("population"),
            AZStd::string::npos);

        EXPECT_FALSE(published.ReplaceFromBoundDocument(
            illegal,
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("schema 1"), AZStd::string::npos);
        const auto after = SerializeObject(
            published.BuildDocument(workspace, *profile));
        ASSERT_TRUE(after.IsSuccess()) << after.GetError().c_str();
        EXPECT_EQ(before.GetValue(), after.GetValue());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        WriterRejectsSchemaOneAndWritesPlainCurrentWithExplicitPopulationArrays)
    {
        CatalogPersistenceService persistence;
        auto legacy = persistence.Load(TG_SDK_PREVIEW_TEMPLATE_ROOT);
        ASSERT_TRUE(legacy.IsSuccess()) << legacy.GetError().c_str();
        CatalogDocument legacyDocument = legacy.TakeValue();
        ASSERT_EQ(legacyDocument.m_schemaVersion, LegacyCatalogSchemaVersion);

        QTemporaryDir legacyDirectory;
        ASSERT_TRUE(legacyDirectory.isValid());
        auto legacySave = persistence.Save(
            legacyDocument,
            ToAzString(legacyDirectory.path()));
        EXPECT_FALSE(legacySave.IsSuccess());
        EXPECT_FALSE(QFile::exists(ToQString(CatalogPath(
            ToAzString(legacyDirectory.path())))));

        QTemporaryDir schemaTwoDirectory;
        ASSERT_TRUE(schemaTwoDirectory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(schemaTwoDirectory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        CatalogDatabase catalog = MakePopulationCatalog();
        const CatalogDocument schemaTwo = catalog.BuildDocument(
            workspace,
            *profile);
        ASSERT_EQ(schemaTwo.m_schemaVersion, CurrentCatalogSchemaVersion);

        auto schemaTwoSave = persistence.Save(
            schemaTwo,
            ToAzString(schemaTwoDirectory.path()));
        ASSERT_TRUE(schemaTwoSave.IsSuccess())
            << schemaTwoSave.GetError().c_str();
        auto bytes = ReadFile(schemaTwoSave.GetValue());
        ASSERT_TRUE(bytes.IsSuccess()) << bytes.GetError().c_str();

        QJsonParseError parseError;
        const QJsonDocument written = QJsonDocument::fromJson(
            bytes.GetValue(),
            &parseError);
        ASSERT_EQ(parseError.error, QJsonParseError::NoError);
        ASSERT_TRUE(written.isObject());
        const QJsonObject object = written.object();
        EXPECT_FALSE(object.contains(QStringLiteral("Type")));
        EXPECT_FALSE(object.contains(QStringLiteral("ClassData")));
        EXPECT_EQ(
            object.value(QStringLiteral("SchemaVersion")).toInt(),
            static_cast<int>(CurrentCatalogSchemaVersion));
        EXPECT_TRUE(object.value(QStringLiteral("ActorProfiles")).isArray());
        EXPECT_TRUE(object.value(QStringLiteral("TroopProfiles")).isArray());
        EXPECT_TRUE(object.value(QStringLiteral("TroopMembers")).isArray());
        EXPECT_EQ(
            object.value(QStringLiteral("ActorProfiles")).toArray().size(),
            1);
        EXPECT_EQ(
            object.value(QStringLiteral("TroopProfiles")).toArray().size(),
            1);
        EXPECT_EQ(
            object.value(QStringLiteral("TroopMembers")).toArray().size(),
            1);
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        CurrentSaveLoadSaveIsByteStable)
    {
        QTemporaryDir firstDirectory;
        QTemporaryDir secondDirectory;
        ASSERT_TRUE(firstDirectory.isValid());
        ASSERT_TRUE(secondDirectory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(firstDirectory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakePopulationRegistry();
        CatalogDatabase catalog = MakePopulationCatalog();
        AZStd::string error;
        ASSERT_TRUE(catalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();

        CatalogPersistenceService persistence;
        auto firstSave = persistence.Save(
            catalog.BuildDocument(workspace, *profile),
            ToAzString(firstDirectory.path()));
        ASSERT_TRUE(firstSave.IsSuccess()) << firstSave.GetError().c_str();
        auto firstBytes = ReadFile(firstSave.GetValue());
        ASSERT_TRUE(firstBytes.IsSuccess()) << firstBytes.GetError().c_str();

        auto loaded = persistence.Load(ToAzString(firstDirectory.path()));
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        CatalogDatabase reopened;
        ASSERT_TRUE(reopened.ReplaceFromBoundDocument(
            loaded.GetValue(),
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
        auto secondSave = persistence.Save(
            reopened.BuildDocument(workspace, *profile),
            ToAzString(secondDirectory.path()));
        ASSERT_TRUE(secondSave.IsSuccess()) << secondSave.GetError().c_str();
        auto secondBytes = ReadFile(secondSave.GetValue());
        ASSERT_TRUE(secondBytes.IsSuccess()) << secondBytes.GetError().c_str();
        EXPECT_EQ(firstBytes.GetValue(), secondBytes.GetValue());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        CurrentSaveClearLoadAndReplacePreservesCanonicalState)
    {
        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(directory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakePopulationRegistry();
        CatalogDatabase catalog = MakePopulationCatalog();
        AZStd::string error;
        ASSERT_TRUE(catalog.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
        auto expected = SerializeObject(catalog.BuildDocument(workspace, *profile));
        ASSERT_TRUE(expected.IsSuccess()) << expected.GetError().c_str();

        CatalogPersistenceService persistence;
        auto saved = persistence.Save(
            catalog.BuildDocument(workspace, *profile),
            ToAzString(directory.path()));
        ASSERT_TRUE(saved.IsSuccess()) << saved.GetError().c_str();

        catalog.Clear();
        EXPECT_TRUE(catalog.GetRecords().empty());
        EXPECT_TRUE(catalog.GetRelationships().empty());
        EXPECT_TRUE(catalog.GetValidationHistory().empty());
        EXPECT_TRUE(catalog.GetGovernanceHistory().empty());
        EXPECT_TRUE(catalog.GetEconomyItems().empty());
        EXPECT_TRUE(catalog.GetEconomyRecipes().empty());
        EXPECT_TRUE(catalog.GetRecipeIngredients().empty());
        EXPECT_TRUE(catalog.GetRecipeOutputs().empty());
        EXPECT_TRUE(catalog.GetPopulationActorProfiles().empty());
        EXPECT_TRUE(catalog.GetPopulationTroopProfiles().empty());
        EXPECT_TRUE(catalog.GetPopulationTroopMembers().empty());

        auto loaded = persistence.Load(ToAzString(directory.path()));
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        ASSERT_TRUE(catalog.ReplaceFromBoundDocument(
            loaded.GetValue(),
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
        auto actual = SerializeObject(catalog.BuildDocument(workspace, *profile));
        ASSERT_TRUE(actual.IsSuccess()) << actual.GetError().c_str();
        EXPECT_EQ(expected.GetValue(), actual.GetValue());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        MalformedCurrentDocumentDoesNotReplacePublishedCatalog)
    {
        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(directory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakePopulationRegistry();
        CatalogDatabase published = MakePopulationCatalog();
        AZStd::string error;
        ASSERT_TRUE(published.ValidateIntegrity(
            workspace,
            *profile,
            registry,
            &error)) << error.c_str();
        auto before = SerializeObject(published.BuildDocument(workspace, *profile));
        ASSERT_TRUE(before.IsSuccess()) << before.GetError().c_str();

        CatalogDocument malformed = published.BuildDocument(workspace, *profile);
        ASSERT_EQ(malformed.m_troopMembers.size(), 1);
        malformed.m_troopMembers.front().m_role = "invented_role";
        CatalogPersistenceService persistence;
        auto saved = persistence.Save(
            malformed,
            ToAzString(directory.path()));
        ASSERT_TRUE(saved.IsSuccess()) << saved.GetError().c_str();
        auto loaded = persistence.Load(ToAzString(directory.path()));
        ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        EXPECT_FALSE(published.ReplaceFromBoundDocument(
            loaded.GetValue(),
            workspace,
            *profile,
            registry,
            &error));
        EXPECT_NE(error.find("role"), AZStd::string::npos);

        auto after = SerializeObject(published.BuildDocument(workspace, *profile));
        ASSERT_TRUE(after.IsSuccess()) << after.GetError().c_str();
        EXPECT_EQ(before.GetValue(), after.GetValue());
    }

    TEST_F(
        CatalogSchemaMigrationPersistenceTests,
        PopulationCandidateSaveFailureDoesNotPublish)
    {
        QTemporaryDir directory;
        ASSERT_TRUE(directory.isValid());
        WorkspaceModel workspace = MakePopulationWorkspace(
            ToAzString(directory.path()));
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        const SourceEvidenceRegistry registry = MakePopulationRegistry();
        CatalogDatabase published = MakePopulationCatalog();
        CatalogDatabase candidate = published;

        const PopulationActorProfile* actor =
            candidate.FindPopulationActorProfile(ActorRecordId);
        ASSERT_NE(actor, nullptr);
        PopulationActorProfile changed = *actor;
        changed.m_archetype = "synthetic_veteran_guard";
        AZStd::string error;
        ASSERT_TRUE(candidate.UpsertPopulationActorProfile(changed, &error))
            << error.c_str();

        bool saveCalled = false;
        const CatalogTransactionService::SaveCallback failSave =
            [&saveCalled](
                const CatalogDocument& document,
                const AZStd::string&)
                -> AZ::Outcome<AZStd::string, AZStd::string>
            {
                saveCalled = true;
                EXPECT_EQ(
                    document.m_schemaVersion,
                    CurrentCatalogSchemaVersion);
                EXPECT_EQ(document.m_actorProfiles.size(), 1);
                EXPECT_EQ(document.m_troopProfiles.size(), 1);
                EXPECT_EQ(document.m_troopMembers.size(), 1);
                return AZ::Failure(AZStd::string(
                    "injected population catalog persistence failure"));
            };

        const CatalogTransactionService transaction;
        auto result = transaction.Commit(
            candidate,
            workspace,
            *profile,
            registry,
            failSave);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_TRUE(saveCalled);
        ASSERT_NE(published.FindPopulationActorProfile(ActorRecordId), nullptr);
        ASSERT_NE(candidate.FindPopulationActorProfile(ActorRecordId), nullptr);
        EXPECT_EQ(
            published.FindPopulationActorProfile(ActorRecordId)->m_archetype,
            "synthetic_guard");
        EXPECT_EQ(
            candidate.FindPopulationActorProfile(ActorRecordId)->m_archetype,
            "synthetic_veteran_guard");
    }

    TEST_F(CatalogSchemaMigrationPersistenceTests, SchemaTwoMigrationPreservesPopulationAndExactOriginalBytes)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        const auto root = ToAzString(directory.path());
        const auto workspace = MakePopulationWorkspace(root);
        const auto& profile = *workspace.FindActiveGameProfile();
        auto source = MakePopulationCatalog().BuildDocument(workspace, profile);
        source.m_schemaVersion = PopulationCatalogSchemaVersion;
        const auto envelope = SerializeObject(source); ASSERT_TRUE(envelope.IsSuccess());
        const QByteArray original(envelope.GetValue().data(), static_cast<qsizetype>(envelope.GetValue().size()));
        ASSERT_TRUE(WriteCatalogBytes(root, original).IsSuccess());
        CatalogPersistenceService persistence;
        const auto loaded = persistence.Load(root); ASSERT_TRUE(loaded.IsSuccess()) << loaded.GetError().c_str();
        EXPECT_EQ(loaded.GetValue().m_schemaVersion, PopulationCatalogSchemaVersion);
        CatalogDatabase migrated; AZStd::string error;
        ASSERT_TRUE(migrated.ReplaceFromBoundDocument(loaded.GetValue(), workspace, profile, MakePopulationRegistry(), &error)) << error.c_str();
        const auto document = migrated.BuildDocument(workspace, profile);
        EXPECT_EQ(document.m_schemaVersion, CurrentCatalogSchemaVersion);
        EXPECT_EQ(document.m_records.size(), source.m_records.size());
        EXPECT_EQ(document.m_actorProfiles.size(), source.m_actorProfiles.size());
        EXPECT_EQ(document.m_troopProfiles.size(), source.m_troopProfiles.size());
        EXPECT_EQ(document.m_troopMembers.size(), source.m_troopMembers.size());
        auto expected = source; expected.m_schemaVersion = CurrentCatalogSchemaVersion;
        EXPECT_EQ(SerializeObject(expected).GetValue(), SerializeObject(document).GetValue());
        ASSERT_TRUE(persistence.Save(document, root).IsSuccess());
        const auto backups = QDir(directory.path() + "/Catalog").entryList({"*.schema-2.*.backup.json"}, QDir::Files);
        ASSERT_EQ(backups.size(), 1);
        QFile backup(directory.path() + "/Catalog/" + backups.front());
        ASSERT_TRUE(backup.open(QIODevice::ReadOnly)); EXPECT_EQ(backup.readAll(), original); backup.close();
        ASSERT_TRUE(persistence.Save(document, root).IsSuccess());
        EXPECT_EQ(QDir(directory.path() + "/Catalog").entryList({"*.backup.json"}, QDir::Files).size(), 1);
    }

    TEST_F(CatalogSchemaMigrationPersistenceTests, MigrationBackupFailureLeavesOriginalCatalogUnchanged)
    {
        QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
        const auto root = ToAzString(directory.path());
        const auto workspace = MakePopulationWorkspace(root);
        const auto& profile = *workspace.FindActiveGameProfile();
        auto document = MakePopulationCatalog().BuildDocument(workspace, profile);
        auto old = document; old.m_schemaVersion = PopulationCatalogSchemaVersion;
        const auto serialized = SerializeObject(old); ASSERT_TRUE(serialized.IsSuccess());
        const QByteArray bytes(serialized.GetValue().data(), static_cast<qsizetype>(serialized.GetValue().size()));
        ASSERT_TRUE(WriteCatalogBytes(root, bytes).IsSuccess());
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        const QString backupPath = ToQString(CatalogPath(root)) + ".schema-2." + hash + ".backup.json";
        ASSERT_TRUE(QDir().mkdir(backupPath)); // a path collision prevents making the exact recovery copy
        const auto saved = CatalogPersistenceService().Save(document, root);
        ASSERT_FALSE(saved.IsSuccess()); EXPECT_NE(saved.GetError().find("backup"), AZStd::string::npos);
        EXPECT_EQ(ReadFile(CatalogPath(root)).GetValue(), bytes);
    }

    TEST_F(CatalogSchemaMigrationPersistenceTests, LegacyVersionsCannotSmuggleEncounterDefinitions)
    {
        for (const auto version : {LegacyCatalogSchemaVersion, PopulationCatalogSchemaVersion})
        {
            QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
            auto object = MakePlainEmptyCatalogObject();
            object["SchemaVersion"] = static_cast<int>(version);
            object["EncounterDefinitions"] = QJsonArray{QJsonObject{{"RecordId", "encounter.illegal"}}};
            ASSERT_TRUE(WriteCatalogJson(ToAzString(directory.path()), QJsonDocument(object)).IsSuccess());
            const auto loaded = CatalogPersistenceService().Load(ToAzString(directory.path()));
            ASSERT_FALSE(loaded.IsSuccess()); EXPECT_NE(loaded.GetError().find("encounter"), AZStd::string::npos);
        }
    }

    TEST_F(CatalogSchemaMigrationPersistenceTests, EconomyDefinitionsAndJoinsPersistAndRejectWrongRecipeRemoval)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString root = temporary.path() + "/Workspace";
        ASSERT_TRUE(QDir().mkpath(root + "/Packs/test.economy"));
        for (const QString& folder : {QString("Build"), QString("Staging"), QString("Deployment"), QString("Diagnostics"), QString("Extracted")})
        {
            ASSERT_TRUE(QDir().mkpath(root + '/' + folder));
        }
        ASSERT_TRUE(QDir().mkpath(temporary.path() + "/Game/Managed"));
        ASSERT_TRUE(QDir().mkpath(temporary.path() + "/Game/BepInEx/plugins"));
        QFile syntheticAssembly(temporary.path() + "/Game/Managed/Synthetic.dll");
        ASSERT_TRUE(syntheticAssembly.open(QIODevice::WriteOnly));
        ASSERT_GT(syntheticAssembly.write("Synthetic path-validation fixture; never executed."), 0);
        syntheticAssembly.close();
        WorkspaceModel workspace;
        workspace.m_workspaceId = "test.economy.workspace";
        workspace.m_displayName = "Synthetic economy authoring";
        workspace.m_rootPath = ToAzString(root);
        workspace.m_outputPath = ToAzString(root + "/Build");
        workspace.m_stagingPath = ToAzString(root + "/Staging");
        workspace.m_deploymentPath = ToAzString(root + "/Deployment");
        workspace.m_activeGameProfileId = "test.economy";
        GameProfile profile;
        profile.m_profileId = workspace.m_activeGameProfileId;
        profile.m_displayName = "Synthetic";
        profile.m_installPath = ToAzString(temporary.path() + "/Game");
        profile.m_gameVersion = "1.0.0";
        profile.m_branch = "mono";
        profile.m_runtimeTarget = "Mono";
        profile.m_unityVersion = "6000.0.64f1";
        profile.m_bepInExVersion = "5.4.23";
        profile.m_managedAssembliesPath = profile.m_installPath + "/Managed";
        profile.m_pluginPath = profile.m_installPath + "/BepInEx/plugins";
        profile.m_diagnosticsPath = ToAzString(root + "/Diagnostics");
        profile.m_extractedDataPath = ToAzString(root + "/Extracted");
        workspace.m_gameProfiles.push_back(profile);
        FoundationService service(FoundationWorkspaceLoadDependencies{});
        service.SetWorkspace(workspace);
        AZStd::string error;
        ASSERT_TRUE(service.SaveWorkspace(ToAzString(root + "/economy.tgworkspace.json"), &error)) << error.c_str();
        AZStd::string itemId;
        EXPECT_FALSE(service.CreateEconomyRecord("item", "Requires a saved mod", itemId, &error));
        EXPECT_TRUE(service.GetCatalog().GetRecords().empty());
        PackManifest pack;
        pack.m_packId = "test.economy";
        pack.m_ownerId = "test";
        pack.m_displayName = "Synthetic economy mod";
        pack.m_version = "1.0.0";
        ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
        ASSERT_TRUE(service.SaveActivePack(ToAzString(root + "/Packs/test.economy/pack.tgpack.json"), &error)) << error.c_str();
        ASSERT_TRUE(service.CreateEconomyRecord("item", "Synthetic ingredient", itemId, &error)) << error.c_str();
        AZStd::string recipeId;
        ASSERT_TRUE(service.CreateEconomyRecord("recipe", "Synthetic recipe", recipeId, &error)) << error.c_str();
        const CatalogRecord* recipeRecord = service.GetCatalog().FindByRecordId(recipeId);
        ASSERT_NE(recipeRecord, nullptr);
        EXPECT_TRUE(recipeRecord->m_nativeRefExact.empty());
        EXPECT_TRUE(recipeRecord->m_allowedUsages.empty());
        EXPECT_EQ(recipeRecord->m_ownerPackId, pack.m_packId);
        EconomyRecipeIngredient ingredient;
        ingredient.m_linkId = "test.ingredient";
        ingredient.m_recipeRecordId = recipeId;
        ingredient.m_itemRecordId = itemId;
        ingredient.m_quantity = 3;
        ingredient.m_evidenceIds = recipeRecord->m_evidenceIds;
        EconomyRecipeOutput output;
        output.m_linkId = "test.output";
        output.m_recipeRecordId = recipeId;
        output.m_itemRecordId = itemId;
        output.m_quantity = 2;
        output.m_evidenceIds = ingredient.m_evidenceIds;
        EXPECT_FALSE(service.UpsertEconomyRecipeIngredient(ingredient, &error));
        ASSERT_TRUE(service.SaveAuthoredRecipeIngredient(ingredient, &error)) << error.c_str();
        ASSERT_TRUE(service.SaveAuthoredRecipeOutput(output, &error)) << error.c_str();
        ingredient.m_quantity = 7;
        ASSERT_TRUE(service.SaveAuthoredRecipeIngredient(ingredient, &error)) << error.c_str();
        ingredient.m_quantity = 0;
        EXPECT_FALSE(service.SaveAuthoredRecipeIngredient(ingredient, &error));
        EXPECT_EQ(service.GetCatalog().FindIngredientsForRecipe(recipeId).front().m_quantity, 7);
        EXPECT_FALSE(service.RemoveEconomyRecipeJoin(itemId, output.m_linkId, true, &error));
        EXPECT_EQ(service.GetCatalog().FindOutputsForRecipe(recipeId).size(), 1);

        FoundationService reopened(FoundationWorkspaceLoadDependencies{});
        reopened.SetWorkspace(workspace);
        ASSERT_TRUE(reopened.SaveWorkspace(ToAzString(root + "/economy.tgworkspace.json"), &error)) << error.c_str();
        ASSERT_TRUE(reopened.ReloadSourceEvidence(&error)) << error.c_str();
        ASSERT_TRUE(reopened.ReloadCatalog(&error)) << error.c_str();
        ASSERT_EQ(reopened.GetCatalog().GetRecords().size(), 2);
        ASSERT_EQ(reopened.GetCatalog().FindIngredientsForRecipe(recipeId).size(), 1);
        EXPECT_EQ(reopened.GetCatalog().FindIngredientsForRecipe(recipeId).front().m_quantity, 7);
        ASSERT_TRUE(reopened.RemoveEconomyRecipeJoin(recipeId, output.m_linkId, true, &error)) << error.c_str();
        ASSERT_TRUE(reopened.ReloadCatalog(&error)) << error.c_str();
        EXPECT_TRUE(reopened.GetCatalog().FindOutputsForRecipe(recipeId).empty());
        EXPECT_FALSE(reopened.ImportNativeEconomy(ToAzString(temporary.path() + "/escape.json"), &error));
        EXPECT_EQ(reopened.GetCatalog().GetRecords().size(), 2);

        // Synthetic source bytes exercise intake boundaries without parsing or copying game data.
        const QByteArray sourceBytes("Synthetic serialized-source fixture; never executed.");
        const QString sourceHash = "sha256:" + QString::fromLatin1(
            QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256).toHex());
        QJsonArray sources;
        for (const QString& relative : {
            QString("Fall of Avalon_Data/StreamingAssets/aa/catalog.json"),
            QString("Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.items_assets_all.bundle"),
            QString("Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.crafting_assets_all.bundle")})
        {
            const QString sourcePath = ToQString(profile.m_installPath) + '/' + relative;
            ASSERT_TRUE(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
            QFile source(sourcePath);
            ASSERT_TRUE(source.open(QIODevice::WriteOnly));
            ASSERT_EQ(source.write(sourceBytes), sourceBytes.size());
            sources.append(QJsonObject{{"Locator", "$install/" + relative}, {"Sha256", sourceHash},
                {"ByteSize", sourceBytes.size()}});
        }
        QJsonObject nativeRow{{"record_id", "native.item.synthetic"}, {"kind", "item"},
            {"native_ref", "$install/synthetic#/Assets/Item.prefab"},
            {"subject_ref", "$install/synthetic#/Assets/Item.prefab"},
            {"display_name", "Synthetic native item"}, {"evidence_id", "evidence.native.synthetic"},
            {"confidence", "documented"}, {"claim", "Synthetic fixture observation only."},
            {"item", QJsonObject{{"weight", 1.0}, {"base_value", 2.0}, {"hidden", false}, {"tags", QJsonArray{}}}}};
        QJsonObject observation{{"SchemaVersion", 1}, {"DocumentKind", "foa-native-economy-observations"},
            {"ProfileId", ToQString(profile.m_profileId)}, {"GameVersion", ToQString(profile.m_gameVersion)},
            {"Branch", ToQString(profile.m_branch)}, {"RuntimeTarget", ToQString(profile.m_runtimeTarget)},
            {"CapturedAt", "2026-09-07T00:00:00Z"}, {"SourceFiles", sources},
            {"OperationalAuthority", QJsonObject{{"Runtime", false}}},
            {"definitions", QJsonArray{nativeRow}}, {"evidence", QJsonArray{nativeRow}}};
        const QString observationPath = root + "/native-observation.json";
        const auto importObservation = [&](const QJsonObject& document)
        {
            QFile observationFile(observationPath);
            if (!observationFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) { return false; }
            const QByteArray payload = QJsonDocument(document).toJson();
            if (observationFile.write(payload) != payload.size()) { return false; }
            observationFile.close();
            return reopened.ImportNativeEconomy(ToAzString(observationPath), &error);
        };
        QJsonObject invalid = observation;
        invalid.insert("ProfileId", "different.profile");
        EXPECT_FALSE(importObservation(invalid));
        invalid = observation;
        invalid.insert("OperationalAuthority", QJsonObject{{"Runtime", true}});
        EXPECT_FALSE(importObservation(invalid));
        invalid = observation;
        QJsonArray staleSources = sources;
        QJsonObject staleSource = staleSources[0].toObject();
        staleSource.insert("Sha256", "sha256:changed");
        staleSources[0] = staleSource;
        invalid.insert("SourceFiles", staleSources);
        EXPECT_FALSE(importObservation(invalid));
        QJsonObject invalidRow = nativeRow;
        invalidRow.insert("record_id", "native.item.invalid");
        invalidRow.insert("native_ref", "$install/synthetic#/Assets/Invalid.prefab");
        invalidRow.insert("subject_ref", "$install/synthetic#/Assets/Invalid.prefab");
        invalidRow.insert("evidence_id", "evidence.native.invalid");
        invalidRow.insert("item", QJsonObject{{"weight", -1.0}, {"base_value", 2.0}, {"hidden", false}, {"tags", QJsonArray{}}});
        invalid = observation;
        invalid.insert("definitions", QJsonArray{nativeRow, invalidRow});
        invalid.insert("evidence", QJsonArray{nativeRow, invalidRow});
        EXPECT_FALSE(importObservation(invalid));
        EXPECT_EQ(reopened.GetCatalog().GetRecords().size(), 2);

        // Make the exact catalog destination unwritable as a file, then restore it.
        const QString catalogFile = ToQString(CatalogPath(workspace.m_rootPath));
        ASSERT_TRUE(QFile::rename(catalogFile, catalogFile + ".baseline"));
        ASSERT_TRUE(QDir().mkpath(catalogFile));
        EXPECT_FALSE(importObservation(observation));
        EXPECT_EQ(reopened.GetCatalog().GetRecords().size(), 2);
        ASSERT_TRUE(QDir().rmdir(catalogFile));
        ASSERT_TRUE(QFile::rename(catalogFile + ".baseline", catalogFile));
        // A new reader run emits a fresh observation and evidence identity.
        nativeRow.insert("evidence_id", "evidence.native.synthetic.retry");
        observation.insert("evidence", QJsonArray{nativeRow});
        observation.insert("definitions", QJsonArray{nativeRow});
        observation.insert("CapturedAt", "2026-09-07T00:00:01Z");
        ASSERT_TRUE(importObservation(observation)) << error.c_str();
        ASSERT_EQ(reopened.GetCatalog().GetRecords().size(), 3);
        EconomyItemProfile authored = *reopened.GetCatalog().FindEconomyItem("native.item.synthetic");
        authored.m_weight = 9.0;
        ASSERT_TRUE(reopened.UpsertEconomyItemProfile(authored, &error)) << error.c_str();
        nativeRow.insert("evidence_id", "evidence.native.synthetic.refresh");
        observation.insert("evidence", QJsonArray{nativeRow});
        observation.insert("definitions", QJsonArray{nativeRow});
        observation.insert("CapturedAt", "2026-09-07T00:00:02Z");
        ASSERT_TRUE(importObservation(observation)) << error.c_str();
        EXPECT_EQ(reopened.GetCatalog().FindEconomyItem("native.item.synthetic")->m_weight, 9.0);
    }
} // namespace TaintedGrailModdingSDK
