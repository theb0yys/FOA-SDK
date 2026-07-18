/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        enum class FailureStage
        {
            None,
            WorkspaceDocument,
            ActiveProfile,
            WorkspacePaths,
            SourceLoad,
            ImportIssues,
            RegistryBinding,
            EvidenceBinding,
            CatalogLoad,
            CatalogBinding,
            CatalogValidation,
        };

        struct Scenario
        {
            FailureStage m_failure = FailureStage::None;
            AZStd::string m_tag = "old";
        };

        WorkspaceModel MakeWorkspace(const AZStd::string& tag, bool invalidProfile = false)
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = tag + ".workspace";
            workspace.m_displayName = tag + " workspace";
            workspace.m_rootPath = ".";
            workspace.m_outputPath = "Build";
            workspace.m_stagingPath = "Staging";
            workspace.m_deploymentPath = "Deployment";
            workspace.m_activeGameProfileId = invalidProfile ? "missing.profile" : tag + ".profile";

            GameProfile profile;
            profile.m_profileId = tag + ".profile";
            profile.m_displayName = tag + " profile";
            profile.m_installPath = "Game";
            profile.m_gameVersion = tag + "-version";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3";
            profile.m_bepInExVersion = "5.4";
            profile.m_managedAssembliesPath = "Game/Managed";
            profile.m_pluginPath = "Game/Plugins";
            profile.m_diagnosticsPath = "Diagnostics";
            profile.m_extractedDataPath = "Extracted";
            workspace.m_gameProfiles.push_back(profile);
            return workspace;
        }

        SourceDocument MakeSourceDocument(const WorkspaceModel& workspace, bool mismatched)
        {
            const GameProfile& profile = workspace.m_gameProfiles.front();
            SourceDocument document;
            document.m_source.m_sourceId = "source." + workspace.m_workspaceId;
            document.m_source.m_title = "Synthetic source";
            document.m_source.m_sourceKind = "test";
            document.m_source.m_locator = "fixture";
            document.m_source.m_fingerprint = "sha256:fixture-" + workspace.m_workspaceId;
            document.m_source.m_profileId = mismatched ? "wrong.profile" : profile.m_profileId;
            document.m_source.m_gameVersion = profile.m_gameVersion;
            document.m_source.m_branch = profile.m_branch;
            document.m_source.m_runtimeTarget = profile.m_runtimeTarget;
            document.m_source.m_importerId = "test.importer";
            document.m_source.m_importerVersion = "1.0.0";
            return document;
        }

        EvidenceDocument MakeEvidenceDocument(
            const WorkspaceModel& workspace,
            const SourceDocument& source,
            bool mismatched)
        {
            const GameProfile& profile = workspace.m_gameProfiles.front();
            EvidenceDocument document;
            document.m_sourceId = source.m_source.m_sourceId;
            document.m_sourceFingerprint = mismatched
                ? "sha256:wrong"
                : source.m_source.m_fingerprint;
            document.m_profileId = profile.m_profileId;
            document.m_gameVersion = profile.m_gameVersion;
            document.m_branch = profile.m_branch;

            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence." + workspace.m_workspaceId;
            evidence.m_sourceId = source.m_source.m_sourceId;
            evidence.m_sourceFingerprint = document.m_sourceFingerprint;
            evidence.m_profileId = profile.m_profileId;
            evidence.m_gameVersion = profile.m_gameVersion;
            evidence.m_branch = profile.m_branch;
            evidence.m_subjectRef = "subject:test";
            evidence.m_claim = "Synthetic claim";
            document.m_evidence.push_back(evidence);
            return document;
        }

        CatalogDocument MakeCatalogDocument(
            const WorkspaceModel& workspace,
            bool mismatched,
            bool invalid)
        {
            const GameProfile& profile = workspace.m_gameProfiles.front();
            CatalogDocument document;
            document.m_workspaceId = mismatched ? "wrong.workspace" : workspace.m_workspaceId;
            document.m_profileId = profile.m_profileId;
            document.m_gameVersion = profile.m_gameVersion;
            document.m_branch = profile.m_branch;
            if (invalid)
            {
                CatalogRecord first;
                first.m_recordId = "record.duplicate";
                first.m_domain = "test";
                first.m_recordKind = "test";
                first.m_subjectRef = "subject:first";
                first.m_identityKind = "synthetic";
                first.m_ownerPackId = "owner.pack";
                CatalogRecord second = first;
                second.m_subjectRef = "subject:second";
                document.m_records.push_back(first);
                document.m_records.push_back(second);
            }
            return document;
        }

        FoundationWorkspaceLoadDependencies MakeDependencies(Scenario& scenario)
        {
            FoundationWorkspaceLoadDependencies dependencies;
            dependencies.m_loadWorkspace = [&scenario](const AZStd::string& filePath)
                -> AZ::Outcome<WorkspaceDocumentLoadResult, AZStd::string>
            {
                scenario.m_tag = filePath;
                if (scenario.m_failure == FailureStage::WorkspaceDocument && filePath == "new")
                {
                    return AZ::Failure(AZStd::string("injected workspace document failure"));
                }
                WorkspaceDocumentLoadResult result;
                result.m_workspace = MakeWorkspace(
                    filePath,
                    scenario.m_failure == FailureStage::ActiveProfile && filePath == "new");
                result.m_filePath = "/documents/" + filePath + ".tgworkspace.json";
                return AZ::Success(AZStd::move(result));
            };
            dependencies.m_resolveWorkspaceRoot = [&scenario](
                const WorkspaceModel& workspace,
                const AZStd::string&)
                -> AZ::Outcome<AZStd::string, AZStd::string>
            {
                if (scenario.m_failure == FailureStage::WorkspacePaths && scenario.m_tag == "new")
                {
                    return AZ::Failure(AZStd::string("injected workspace path failure"));
                }
                return AZ::Success(AZStd::string("/canonical/") + workspace.m_workspaceId);
            };
            dependencies.m_loadSourceEvidence = [&scenario](
                const AZStd::string&,
                AZStd::vector<SourceDocument>& sources,
                AZStd::vector<EvidenceDocument>& evidence,
                AZStd::vector<ImportIssue>& issues)
                -> AZ::Outcome<void, AZStd::string>
            {
                if (scenario.m_failure == FailureStage::SourceLoad && scenario.m_tag == "new")
                {
                    return AZ::Failure(AZStd::string("injected source load failure"));
                }
                if (scenario.m_failure == FailureStage::ImportIssues && scenario.m_tag == "new")
                {
                    ImportIssue issue;
                    issue.m_issueId = "issue.injected";
                    issue.m_severity = "error";
                    issue.m_code = "injected.import-issue";
                    issue.m_message = "injected import issue failure";
                    issues.push_back(issue);
                }
                const WorkspaceModel workspace = MakeWorkspace(scenario.m_tag);
                SourceDocument source = MakeSourceDocument(
                    workspace,
                    scenario.m_failure == FailureStage::RegistryBinding && scenario.m_tag == "new");
                evidence.push_back(MakeEvidenceDocument(
                    workspace,
                    source,
                    scenario.m_failure == FailureStage::EvidenceBinding && scenario.m_tag == "new"));
                sources.push_back(AZStd::move(source));
                return AZ::Success();
            };
            dependencies.m_catalogExists = [](const AZStd::string&)
            {
                return true;
            };
            dependencies.m_loadCatalog = [&scenario](const AZStd::string&)
                -> AZ::Outcome<CatalogDocument, AZStd::string>
            {
                if (scenario.m_failure == FailureStage::CatalogLoad && scenario.m_tag == "new")
                {
                    return AZ::Failure(AZStd::string("injected catalog load failure"));
                }
                return AZ::Success(MakeCatalogDocument(
                    MakeWorkspace(scenario.m_tag),
                    scenario.m_failure == FailureStage::CatalogBinding && scenario.m_tag == "new",
                    scenario.m_failure == FailureStage::CatalogValidation && scenario.m_tag == "new"));
            };
            dependencies.m_getCatalogPath = [](const AZStd::string& root)
            {
                return root + "/Catalog/catalog.tgcatalog.json";
            };
            return dependencies;
        }

        AZStd::string Count(size_t value)
        {
            return AZStd::string::format("%zu", value);
        }

        AZStd::string StateSignature(const FoundationService& service)
        {
            AZStd::string signature = service.GetWorkspace().m_workspaceId;
            signature += "|" + service.GetWorkspaceFilePath();
            signature += "|" + service.GetWorkspaceRootPath();
            signature += "|" + service.GetCatalogFilePath();
            signature += "|" + service.GetSnapshot().m_workspaceName;
            signature += "|" + service.GetSnapshot().m_workspaceFilePath;
            signature += "|" + Count(service.GetPacks().size());
            signature += "|" + (service.GetActivePack() ? service.GetActivePack()->m_packId : "none");
            signature += "|" + Count(service.GetSourceRegistry().GetSources().size());
            signature += "|" + Count(service.GetSourceRegistry().GetEvidence().size());
            signature += "|" + Count(service.GetImportIssues().size());
            signature += "|" + Count(service.GetCatalog().GetRecords().size());
            return signature;
        }

        void ExpectFailurePreservesState(FailureStage stage)
        {
            Scenario scenario;
            FoundationService service(MakeDependencies(scenario));
            AZStd::string error;
            ASSERT_TRUE(service.LoadWorkspace("old", &error)) << error.c_str();

            PackManifest pack;
            pack.m_packId = "owner.pack";
            pack.m_ownerId = "owner";
            pack.m_version = "1.0.0";
            ASSERT_TRUE(service.SetActivePack(pack, &error)) << error.c_str();
            const AZStd::string before = StateSignature(service);

            scenario.m_failure = stage;
            EXPECT_FALSE(service.LoadWorkspace("new", &error));
            EXPECT_FALSE(error.empty());
            EXPECT_EQ(StateSignature(service), before);
        }
    } // namespace

    TEST(FoundationServiceWorkspaceLoadTests, SuccessfulCandidatePublishesEveryWorkspaceObject)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        AZStd::string error;
        ASSERT_TRUE(service.LoadWorkspace("new", &error)) << error.c_str();
        EXPECT_EQ(service.GetWorkspace().m_workspaceId, "new.workspace");
        EXPECT_EQ(service.GetWorkspaceFilePath(), "/documents/new.tgworkspace.json");
        EXPECT_EQ(service.GetWorkspaceRootPath(), "/canonical/new.workspace");
        EXPECT_EQ(service.GetSourceRegistry().GetSources().size(), 1);
        EXPECT_EQ(service.GetSourceRegistry().GetEvidence().size(), 1);
        EXPECT_EQ(service.GetCatalogFilePath(), "/canonical/new.workspace/Catalog/catalog.tgcatalog.json");
        EXPECT_EQ(service.GetSnapshot().m_workspaceFilePath, service.GetWorkspaceFilePath());
    }

    TEST(FoundationServiceWorkspaceLoadTests, WorkspaceDocumentFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::WorkspaceDocument);
    }

    TEST(FoundationServiceWorkspaceLoadTests, ActiveProfileFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::ActiveProfile);
    }

    TEST(FoundationServiceWorkspaceLoadTests, WorkspacePathFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::WorkspacePaths);
    }

    TEST(FoundationServiceWorkspaceLoadTests, SourceLoadFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::SourceLoad);
    }

    TEST(FoundationServiceWorkspaceLoadTests, ImportIssueFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::ImportIssues);
    }

    TEST(FoundationServiceWorkspaceLoadTests, RegistryBindingFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::RegistryBinding);
    }

    TEST(FoundationServiceWorkspaceLoadTests, EvidenceBindingFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::EvidenceBinding);
    }

    TEST(FoundationServiceWorkspaceLoadTests, CatalogLoadFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::CatalogLoad);
    }

    TEST(FoundationServiceWorkspaceLoadTests, CatalogBindingFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::CatalogBinding);
    }

    TEST(FoundationServiceWorkspaceLoadTests, CatalogValidationFailurePreservesAllLiveState)
    {
        ExpectFailurePreservesState(FailureStage::CatalogValidation);
    }
} // namespace TaintedGrailModdingSDK
