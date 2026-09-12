/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "FoundationNotificationBus.h"

#include <AzCore/std/function/function_template.h>

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
            document.m_source.m_fingerprint =
                "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
            document.m_source.m_profileId = mismatched ? "wrong.profile" : profile.m_profileId;
            document.m_source.m_gameVersion = profile.m_gameVersion;
            document.m_source.m_branch = profile.m_branch;
            document.m_source.m_runtimeTarget = profile.m_runtimeTarget;
            document.m_source.m_toolName = "foundation-workspace-load-tests";
            document.m_source.m_toolVersion = "1.0.0";
            document.m_source.m_importerId = "test.importer";
            document.m_source.m_importerVersion = "1.0.0";
            document.m_source.m_capturedAt = "2026-01-01T00:00:00Z";
            document.m_source.m_importedAt = "2026-01-01T00:00:01Z";
            document.m_source.m_mediaType = "application/json";
            document.m_source.m_byteSize = 512;
            document.m_source.m_importStatus = "imported";
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
                ? "sha256:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
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
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = "fixture";
            evidence.m_recordPath = "/records/0";
            evidence.m_extractedAt = "2026-01-01T00:00:00Z";
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

        class WorkspaceObserver : public FoundationNotificationBus::Handler
        {
        public:
            explicit WorkspaceObserver(const FoundationService& service)
                : m_service(service)
            {
                BusConnect();
            }
            ~WorkspaceObserver() override { BusDisconnect(); }

            bool CanChangeWorkspace(const FoundationService& service) override
            {
                if (&service != &m_service)
                {
                    return true;
                }
                ++m_requests;
                m_before = StateSignature(service);
                return m_admit ? m_admit() : m_allow;
            }
            void OnWorkspaceChanged(const FoundationService& service) override
            {
                if (&service == &m_service)
                {
                    ++m_changes;
                    m_after = StateSignature(service);
                    if (m_committed)
                    {
                        m_committed();
                    }
                }
            }

            const FoundationService& m_service;
            AZStd::function<bool()> m_admit;
            AZStd::function<void()> m_committed;
            AZStd::string m_before;
            AZStd::string m_after;
            int m_requests = 0;
            int m_changes = 0;
            bool m_allow = true;
        };

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

            WorkspaceObserver observer(service);
            scenario.m_failure = stage;
            bool cancelled = true;
            EXPECT_FALSE(service.LoadWorkspace("new", &error, &cancelled));
            EXPECT_FALSE(cancelled);
            EXPECT_EQ(observer.m_requests, 0);
            EXPECT_EQ(observer.m_changes, 0);
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

    TEST(FoundationServiceWorkspaceLoadTests, VetoPreservesStateAndRetryPublishesOnce)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        PackManifest pack;
        pack.m_packId = "owner.pack";
        pack.m_ownerId = "owner";
        pack.m_version = "1.0.0";
        ASSERT_TRUE(service.SetActivePack(pack));
        const AZStd::string before = StateSignature(service);
        WorkspaceObserver observer(service);
        observer.m_allow = false;
        AZStd::string error;
        bool cancelled = false;
        EXPECT_FALSE(service.LoadWorkspace("new", &error, &cancelled));
        EXPECT_TRUE(cancelled);
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(StateSignature(service), before);
        EXPECT_EQ(observer.m_before, before);
        EXPECT_EQ(observer.m_requests, 1);
        EXPECT_EQ(observer.m_changes, 0);

        observer.m_allow = true;
        EXPECT_TRUE(service.LoadWorkspace("new", &error, &cancelled));
        EXPECT_FALSE(cancelled);
        EXPECT_EQ(observer.m_requests, 2);
        EXPECT_EQ(observer.m_changes, 1);
        EXPECT_EQ(observer.m_after, StateSignature(service));
        EXPECT_EQ(service.GetWorkspace().m_workspaceId, "new.workspace");
        EXPECT_TRUE(service.GetPacks().empty());
    }

    TEST(FoundationServiceWorkspaceLoadTests, AnyVetoWinsWithMultipleHandlers)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        WorkspaceObserver allowFirst(service);
        WorkspaceObserver veto(service);
        WorkspaceObserver allowLast(service);
        veto.m_allow = false;
        const AZStd::string before = StateSignature(service);
        EXPECT_FALSE(service.LoadWorkspace("new"));
        EXPECT_EQ(StateSignature(service), before);
        EXPECT_EQ(veto.m_requests, 1);
        EXPECT_EQ(allowFirst.m_changes + veto.m_changes + allowLast.m_changes, 0);
    }

    TEST(FoundationServiceWorkspaceLoadTests, SameWorkspaceReloadStillRequestsAdmission)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        WorkspaceObserver observer(service);
        observer.m_allow = false;
        EXPECT_FALSE(service.LoadWorkspace("old"));
        EXPECT_EQ(observer.m_requests, 1);
        EXPECT_EQ(observer.m_changes, 0);
        observer.m_allow = true;
        EXPECT_TRUE(service.LoadWorkspace("old"));
        EXPECT_EQ(observer.m_changes, 1);
    }

    TEST(FoundationServiceWorkspaceLoadTests, AdmissionSeesOldWorkspaceAndAllowsPackSaveNotifications)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        WorkspaceObserver observer(service);
        observer.m_admit = [&service]()
        {
            EXPECT_EQ(service.GetWorkspaceRootPath(), "/canonical/old.workspace");
            // A successful draft save publishes an active pack and a Foundation notification.
            PackManifest pack;
            pack.m_packId = "owner.saved";
            pack.m_ownerId = "owner";
            pack.m_version = "1.0.0";
            EXPECT_TRUE(service.SetActivePack(pack));
            return true;
        };
        ASSERT_TRUE(service.LoadWorkspace("new"));
        EXPECT_EQ(observer.m_requests, 1);
        EXPECT_EQ(observer.m_changes, 1);
        EXPECT_EQ(service.GetActivePack(), nullptr);
        EXPECT_EQ(service.GetWorkspaceRootPath(), "/canonical/new.workspace");
    }

    TEST(FoundationServiceWorkspaceLoadTests, NestedReplacementIsRejectedDuringAdmissionAndPublication)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        WorkspaceObserver observer(service);
        auto nested = [&service]()
        {
            bool cancelled = false;
            const AZStd::string before = StateSignature(service);
            EXPECT_FALSE(service.LoadWorkspace("old", nullptr, &cancelled));
            EXPECT_TRUE(cancelled);
            EXPECT_FALSE(service.SetWorkspace(MakeWorkspace("nested")));
            EXPECT_EQ(StateSignature(service), before);
        };
        observer.m_admit = [&nested]() { nested(); return true; };
        observer.m_committed = nested;
        EXPECT_TRUE(service.LoadWorkspace("new"));
        EXPECT_EQ(observer.m_requests, 1);
        EXPECT_EQ(observer.m_changes, 1);
        observer.m_admit = {};
        observer.m_committed = {};
        EXPECT_TRUE(service.LoadWorkspace("old"));
    }

    TEST(FoundationServiceWorkspaceLoadTests, SetWorkspaceHonorsVetoAndNotifiesAfterSuccess)
    {
        Scenario scenario;
        FoundationService service(MakeDependencies(scenario));
        ASSERT_TRUE(service.LoadWorkspace("old"));
        WorkspaceObserver observer(service);
        observer.m_allow = false;
        const AZStd::string before = StateSignature(service);
        EXPECT_FALSE(service.SetWorkspace(MakeWorkspace("new")));
        EXPECT_EQ(StateSignature(service), before);
        EXPECT_EQ(observer.m_changes, 0);
        observer.m_allow = true;
        EXPECT_TRUE(service.SetWorkspace(MakeWorkspace("new")));
        EXPECT_EQ(observer.m_before, before);
        EXPECT_EQ(observer.m_changes, 1);
        EXPECT_EQ(observer.m_after, StateSignature(service));
        EXPECT_EQ(service.GetWorkspace().m_workspaceId, "new.workspace");
    }

    TEST(FoundationServiceWorkspaceLoadTests, AdmissionAndPublicationIdentifyTheOwningService)
    {
        Scenario scenario;
        FoundationService observed(MakeDependencies(scenario));
        FoundationService other(MakeDependencies(scenario));
        WorkspaceObserver observer(observed);
        observer.m_allow = false;
        EXPECT_TRUE(other.LoadWorkspace("new"));
        EXPECT_EQ(observer.m_requests, 0);
        EXPECT_EQ(observer.m_changes, 0);
        EXPECT_FALSE(observed.LoadWorkspace("old"));
        EXPECT_EQ(observer.m_requests, 1);
    }
} // namespace TaintedGrailModdingSDK
