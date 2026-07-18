/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPersistenceService.h"
#include "EconomyAuthoringService.h"
#include "PackPersistenceService.h"
#include "SourceEvidencePersistenceService.h"
#include "WorkspacePersistenceService.h"

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Json/JsonRegistrationContext.h>
#include <AzCore/Serialization/Json/JsonSystemComponent.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#ifndef TG_SDK_PREVIEW_TEMPLATE_ROOT
#   error TG_SDK_PREVIEW_TEMPLATE_ROOT must identify the Developer Preview fixture template directory.
#endif

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* WorkspaceRelativePath = "preview.tgworkspace.json";
        constexpr const char* PackRelativePath = "Packs/preview.developer-preview-0.tgpack.json";
        constexpr const char* RecipeRecordId = "preview.recipe.synthesis";
        constexpr const char* ComponentRecordId = "preview.item.component";
        constexpr const char* ResultRecordId = "preview.item.result";
        constexpr const char* StationRecordId = "preview.station.workbench";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string FilePath(const AZStd::string& root, const char* relativePath)
        {
            return ToAzString(QDir(ToQString(root)).filePath(QString::fromUtf8(relativePath)));
        }

        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void ReflectPreviewTypes(AZ::SerializeContext& context)
        {
            GameProfile::Reflect(&context);
            WorkspaceModel::Reflect(&context);
            PackManifest::Reflect(&context);
            SourceImporterContract::Reflect(&context);
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
            BlockerRecord::Reflect(&context);
            DomainCoverage::Reflect(&context);
            FoundationSnapshot::Reflect(&context);
        }

        template<class ObjectType>
        AZ::Outcome<AZStd::string, AZStd::string> SerializeObject(const ObjectType& object)
        {
            AZStd::string output;
            AZ::IO::ByteContainerStream<AZStd::string> stream(&output);
            const AZ::Outcome<void, AZStd::string> result =
                AZ::JsonSerializationUtils::SaveObjectToStream(&object, stream);
            if (!result.IsSuccess())
            {
                return AZ::Failure(AZStd::string(result.GetError()));
            }
            return AZ::Success(AZStd::move(output));
        }

        void AppendValues(AZStd::string& output, const AZStd::vector<AZStd::string>& values)
        {
            output += "[";
            for (const AZStd::string& value : values)
            {
                output += AZStd::to_string(value.size());
                output += ":";
                output += value;
                output += ";";
            }
            output += "]";
        }

        AZStd::string EvidenceRowsSignature(const AZStd::vector<EconomyRecipeStationEvidenceRow>& rows)
        {
            AZStd::string signature;
            for (const EconomyRecipeStationEvidenceRow& row : rows)
            {
                signature += row.m_recipeRecordId + "|" + row.m_stationRecordId + "|";
                signature += row.m_stationSubjectRef + "|" + row.m_stationIdentity + "|";
                signature += row.m_unlockMode + "|" + row.m_status + "|";
                signature += row.m_validationState + "|" + row.m_stalenessState;
                AppendValues(signature, row.m_associationSources);
                AppendValues(signature, row.m_unlockSubjectRefs);
                AppendValues(signature, row.m_learnedFromRelationshipIds);
                AppendValues(signature, row.m_evidenceIds);
                AppendValues(signature, row.m_reasons);
                AppendValues(signature, row.m_blockerIds);
                for (const EconomyActionLaneStatus& lane : row.m_actionLanes)
                {
                    signature += lane.m_lane + "=" + lane.m_status + ";";
                }
                signature += "\n";
            }
            return signature;
        }

        struct PreviewState
        {
            WorkspaceModel m_workspace;
            PackManifest m_pack;
            AZStd::vector<SourceDocument> m_sourceDocuments;
            AZStd::vector<EvidenceDocument> m_evidenceDocuments;
            SourceEvidenceRegistry m_registry;
            CatalogDatabase m_catalog;
        };

        AZ::Outcome<PreviewState, AZStd::string> LoadPreviewState(
            const AZStd::string& documentRoot,
            const AZStd::string& relocatedWorkspaceRoot = {})
        {
            WorkspacePersistenceService workspacePersistence;
            PackPersistenceService packPersistence;
            SourceEvidencePersistenceService sourcePersistence;
            CatalogPersistenceService catalogPersistence;

            auto workspaceResult = workspacePersistence.Load(FilePath(documentRoot, WorkspaceRelativePath));
            if (!workspaceResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(workspaceResult.GetError()));
            }
            auto packResult = packPersistence.Load(FilePath(documentRoot, PackRelativePath));
            if (!packResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(packResult.GetError()));
            }

            PreviewState state;
            state.m_workspace = workspaceResult.TakeValue();
            if (!relocatedWorkspaceRoot.empty())
            {
                state.m_workspace.m_rootPath = relocatedWorkspaceRoot;
            }
            state.m_pack = packResult.TakeValue();

            AZStd::vector<ImportIssue> loadIssues;
            const auto sourceResult = sourcePersistence.LoadWorkspaceDocuments(
                documentRoot,
                state.m_sourceDocuments,
                state.m_evidenceDocuments,
                loadIssues);
            if (!sourceResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(sourceResult.GetError()));
            }
            if (!loadIssues.empty())
            {
                return AZ::Failure(AZStd::string::format(
                    "Developer Preview source/evidence load produced %zu issue(s).",
                    loadIssues.size()));
            }

            AZStd::string error;
            for (const SourceDocument& document : state.m_sourceDocuments)
            {
                if (!state.m_registry.RegisterSource(document.m_source, &error))
                {
                    return AZ::Failure(error);
                }
            }
            for (const EvidenceDocument& document : state.m_evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    if (!state.m_registry.RegisterEvidence(evidence, &error))
                    {
                        return AZ::Failure(error);
                    }
                }
            }

            auto catalogResult = catalogPersistence.Load(documentRoot);
            if (!catalogResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(catalogResult.GetError()));
            }
            CatalogDocument catalogDocument = catalogResult.TakeValue();
            const GameProfile* profile = state.m_workspace.FindActiveGameProfile();
            if (!profile)
            {
                return AZ::Failure(AZStd::string("Developer Preview workspace has no active profile."));
            }
            if (catalogDocument.m_workspaceId != state.m_workspace.m_workspaceId
                || catalogDocument.m_profileId != profile->m_profileId
                || catalogDocument.m_gameVersion != profile->m_gameVersion
                || catalogDocument.m_branch != profile->m_branch)
            {
                return AZ::Failure(AZStd::string(
                    "Developer Preview catalog binding does not match the workspace profile."));
            }
            if (!state.m_catalog.ReplaceFromDocument(catalogDocument, &error))
            {
                return AZ::Failure(error);
            }
            return AZ::Success(AZStd::move(state));
        }

        AZ::Outcome<void, AZStd::string> SavePreviewState(
            const PreviewState& state,
            const AZStd::string& outputRoot)
        {
            WorkspacePersistenceService workspacePersistence;
            PackPersistenceService packPersistence;
            SourceEvidencePersistenceService sourcePersistence;
            CatalogPersistenceService catalogPersistence;

            const auto workspaceSave = workspacePersistence.Save(
                state.m_workspace,
                FilePath(outputRoot, WorkspaceRelativePath));
            if (!workspaceSave.IsSuccess())
            {
                return AZ::Failure(AZStd::string(workspaceSave.GetError()));
            }
            const auto packSave = packPersistence.Save(
                state.m_pack,
                FilePath(outputRoot, PackRelativePath));
            if (!packSave.IsSuccess())
            {
                return AZ::Failure(AZStd::string(packSave.GetError()));
            }

            for (const SourceDocument& sourceDocument : state.m_sourceDocuments)
            {
                const EvidenceDocument* matchingEvidence = nullptr;
                for (const EvidenceDocument& candidate : state.m_evidenceDocuments)
                {
                    if (candidate.m_sourceId == sourceDocument.m_source.m_sourceId)
                    {
                        matchingEvidence = &candidate;
                        break;
                    }
                }
                if (!matchingEvidence)
                {
                    return AZ::Failure(AZStd::string(
                        "Developer Preview source has no matching evidence document."));
                }
                const auto sourceSave = sourcePersistence.SaveDocuments(
                    sourceDocument,
                    *matchingEvidence,
                    outputRoot);
                if (!sourceSave.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(sourceSave.GetError()));
                }
            }

            const GameProfile* profile = state.m_workspace.FindActiveGameProfile();
            if (!profile)
            {
                return AZ::Failure(AZStd::string("Developer Preview workspace has no active profile."));
            }
            const CatalogDocument catalogDocument = state.m_catalog.BuildDocument(state.m_workspace, *profile);
            const auto catalogSave = catalogPersistence.Save(catalogDocument, outputRoot);
            if (!catalogSave.IsSuccess())
            {
                return AZ::Failure(AZStd::string(catalogSave.GetError()));
            }
            return AZ::Success();
        }

        AZ::Outcome<AZStd::string, AZStd::string> BuildCanonicalSnapshot(const PreviewState& state)
        {
            const GameProfile* profile = state.m_workspace.FindActiveGameProfile();
            if (!profile)
            {
                return AZ::Failure(AZStd::string("Developer Preview workspace has no active profile."));
            }

            AZStd::string snapshot;
            auto appendObject = [&snapshot](const auto& object) -> AZ::Outcome<void, AZStd::string>
            {
                auto serialized = SerializeObject(object);
                if (!serialized.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(serialized.GetError()));
                }
                snapshot += serialized.TakeValue();
                snapshot += "\n---\n";
                return AZ::Success();
            };

            auto appendResult = appendObject(state.m_workspace);
            if (!appendResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(appendResult.GetError()));
            }
            appendResult = appendObject(state.m_pack);
            if (!appendResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(appendResult.GetError()));
            }

            AZStd::vector<SourceDocument> sources = state.m_sourceDocuments;
            AZStd::sort(sources.begin(), sources.end(), [](const SourceDocument& left, const SourceDocument& right)
            {
                return left.m_source.m_sourceId < right.m_source.m_sourceId;
            });
            for (const SourceDocument& source : sources)
            {
                appendResult = appendObject(source);
                if (!appendResult.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(appendResult.GetError()));
                }
            }

            AZStd::vector<EvidenceDocument> evidenceDocuments = state.m_evidenceDocuments;
            AZStd::sort(
                evidenceDocuments.begin(),
                evidenceDocuments.end(),
                [](const EvidenceDocument& left, const EvidenceDocument& right)
                {
                    return left.m_sourceId < right.m_sourceId;
                });
            for (const EvidenceDocument& evidence : evidenceDocuments)
            {
                appendResult = appendObject(evidence);
                if (!appendResult.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(appendResult.GetError()));
                }
            }

            appendResult = appendObject(state.m_catalog.BuildDocument(state.m_workspace, *profile));
            if (!appendResult.IsSuccess())
            {
                return AZ::Failure(AZStd::string(appendResult.GetError()));
            }

            const EconomyAuthoringService economy;
            snapshot += EvidenceRowsSignature(economy.BuildRecipeStationEvidence(
                RecipeRecordId,
                state.m_registry,
                state.m_catalog,
                {}));
            return AZ::Success(AZStd::move(snapshot));
        }

        class DeveloperPreviewSmokeTests
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

                AZ::SerializeContext* serializeContext = m_application.GetSerializeContext();
                AZ::JsonRegistrationContext* jsonRegistrationContext =
                    m_application.GetJsonRegistrationContext();
                ASSERT_NE(serializeContext, nullptr);
                ASSERT_NE(jsonRegistrationContext, nullptr);
                AZ::JsonSystemComponent::Reflect(serializeContext);
                AZ::JsonSystemComponent::Reflect(jsonRegistrationContext);
                ReflectPreviewTypes(*serializeContext);
            }

            void TearDown() override
            {
                if (m_created)
                {
                    m_application.Destroy();
                }
            }

            AZ::ComponentApplication m_application;
            bool m_created = false;
        };
    } // namespace

    TEST_F(DeveloperPreviewSmokeTests, DeveloperPreviewFixtureLoadSaveReopenPreservesCanonicalState)
    {
        const AZStd::string templateRoot = TG_SDK_PREVIEW_TEMPLATE_ROOT;
        QTemporaryDir outputDirectory;
        ASSERT_TRUE(outputDirectory.isValid());
        const AZStd::string outputRoot = ToAzString(outputDirectory.path());

        auto loadedResult = LoadPreviewState(templateRoot, outputRoot);
        ASSERT_TRUE(loadedResult.IsSuccess()) << loadedResult.GetError().c_str();
        PreviewState loaded = loadedResult.TakeValue();

        ASSERT_EQ(loaded.m_sourceDocuments.size(), 1);
        ASSERT_EQ(loaded.m_registry.GetSources().size(), 1);
        ASSERT_EQ(loaded.m_registry.GetEvidence().size(), 8);
        ASSERT_EQ(loaded.m_catalog.GetRecords().size(), 5);
        ASSERT_EQ(loaded.m_catalog.GetRelationships().size(), 3);
        ASSERT_EQ(loaded.m_catalog.GetGovernanceHistory().size(), 6);

        const CatalogRecord* component = loaded.m_catalog.FindByRecordId(ComponentRecordId);
        const CatalogRecord* result = loaded.m_catalog.FindByRecordId(ResultRecordId);
        const CatalogRecord* station = loaded.m_catalog.FindByRecordId(StationRecordId);
        ASSERT_NE(component, nullptr);
        ASSERT_NE(result, nullptr);
        ASSERT_NE(station, nullptr);
        EXPECT_TRUE(Contains(component->m_allowedUsages, "existing_item_consume"));
        EXPECT_TRUE(Contains(result->m_forbiddenUsages, "existing_item_grant"));
        EXPECT_EQ(station->m_stalenessState, "stale");

        const EconomyAuthoringService economy;
        const auto rowsBefore = economy.BuildRecipeStationEvidence(
            RecipeRecordId,
            loaded.m_registry,
            loaded.m_catalog,
            {});
        ASSERT_EQ(rowsBefore.size(), 1);
        EXPECT_EQ(rowsBefore.front().m_status, "blocked");
        EXPECT_EQ(rowsBefore.front().m_stationRecordId, StationRecordId);
        EXPECT_EQ(rowsBefore.front().m_learnedFromRelationshipIds.size(), 2);

        auto snapshotBeforeResult = BuildCanonicalSnapshot(loaded);
        ASSERT_TRUE(snapshotBeforeResult.IsSuccess()) << snapshotBeforeResult.GetError().c_str();
        const AZStd::string snapshotBefore = snapshotBeforeResult.TakeValue();

        const auto saveResult = SavePreviewState(loaded, outputRoot);
        ASSERT_TRUE(saveResult.IsSuccess()) << saveResult.GetError().c_str();
        loaded = {};

        auto reopenedResult = LoadPreviewState(outputRoot);
        ASSERT_TRUE(reopenedResult.IsSuccess()) << reopenedResult.GetError().c_str();
        const PreviewState reopened = reopenedResult.TakeValue();
        auto snapshotAfterResult = BuildCanonicalSnapshot(reopened);
        ASSERT_TRUE(snapshotAfterResult.IsSuccess()) << snapshotAfterResult.GetError().c_str();
        EXPECT_EQ(snapshotBefore, snapshotAfterResult.GetValue());

        EXPECT_TRUE(QFileInfo::exists(ToQString(FilePath(outputRoot, WorkspaceRelativePath))));
        EXPECT_TRUE(QFileInfo::exists(ToQString(FilePath(outputRoot, PackRelativePath))));
        EXPECT_TRUE(QFileInfo::exists(ToQString(FilePath(
            outputRoot,
            "Sources/preview.source.synthetic/source.tgsource.json"))));
        EXPECT_TRUE(QFileInfo::exists(ToQString(FilePath(
            outputRoot,
            "Sources/preview.source.synthetic/evidence.tgevidence.json"))));
        EXPECT_TRUE(QFileInfo::exists(ToQString(FilePath(
            outputRoot,
            "Catalog/catalog.tgcatalog.json"))));
    }

    TEST_F(DeveloperPreviewSmokeTests, ProofBackedAllowedUsageSurvivesCatalogLoad)
    {
        CatalogPersistenceService persistence;
        auto result = persistence.Load(TG_SDK_PREVIEW_TEMPLATE_ROOT);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const CatalogDocument& document = result.GetValue();
        const auto component = AZStd::find_if(
            document.m_records.begin(),
            document.m_records.end(),
            [](const CatalogRecord& record)
            {
                return record.m_recordId == ComponentRecordId;
            });
        ASSERT_NE(component, document.m_records.end());
        EXPECT_TRUE(Contains(component->m_allowedUsages, "existing_item_consume"));
        EXPECT_FALSE(Contains(
            component->m_forbiddenUsages,
            "legacy_permission_review_required"));
    }

    TEST_F(DeveloperPreviewSmokeTests, LegacyUnprovenAllowanceStillFailsClosed)
    {
        CatalogPersistenceService persistence;
        auto fixture = persistence.Load(TG_SDK_PREVIEW_TEMPLATE_ROOT);
        ASSERT_TRUE(fixture.IsSuccess()) << fixture.GetError().c_str();
        CatalogDocument document = fixture.TakeValue();
        auto resultRecord = AZStd::find_if(
            document.m_records.begin(),
            document.m_records.end(),
            [](const CatalogRecord& record)
            {
                return record.m_recordId == ResultRecordId;
            });
        ASSERT_NE(resultRecord, document.m_records.end());
        resultRecord->m_allowedUsages.push_back("preview.unproven_usage");

        QTemporaryDir outputDirectory;
        ASSERT_TRUE(outputDirectory.isValid());
        const AZStd::string outputRoot = ToAzString(outputDirectory.path());
        auto saved = persistence.Save(document, outputRoot);
        ASSERT_TRUE(saved.IsSuccess()) << saved.GetError().c_str();

        auto reloaded = persistence.Load(outputRoot);
        ASSERT_TRUE(reloaded.IsSuccess()) << reloaded.GetError().c_str();
        const auto reloadedRecord = AZStd::find_if(
            reloaded.GetValue().m_records.begin(),
            reloaded.GetValue().m_records.end(),
            [](const CatalogRecord& record)
            {
                return record.m_recordId == ResultRecordId;
            });
        ASSERT_NE(reloadedRecord, reloaded.GetValue().m_records.end());
        EXPECT_FALSE(Contains(reloadedRecord->m_allowedUsages, "preview.unproven_usage"));
        EXPECT_TRUE(Contains(
            reloadedRecord->m_forbiddenUsages,
            "legacy_permission_review_required"));
    }
} // namespace TaintedGrailModdingSDK
