/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "ExtensionAPI.h"
#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        WorkspaceModel MakeWorkspace()
        {
            GameProfile profile;
            profile.m_profileId = "profile.foa.mono";
            profile.m_displayName = "FoA Mono";
            profile.m_installPath = "C:/fixture/game";
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "2022.3.20f1";
            profile.m_bepInExVersion = "5.4.23";
            profile.m_managedAssembliesPath = "C:/fixture/game/Managed";
            profile.m_pluginPath = "C:/fixture/game/BepInEx/plugins";
            profile.m_diagnosticsPath = "C:/fixture/workspace/diagnostics";
            profile.m_extractedDataPath = "C:/fixture/workspace/extracted";
            profile.m_dlcScopes = { "dlc.z", "dlc.a" };

            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.extension-api";
            workspace.m_displayName = "Extension API fixture";
            workspace.m_rootPath = "C:/fixture/workspace";
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(profile);
            return workspace;
        }

        ExtensionAPI::ExtensionDeclaration MakeDeclaration(
            AZStd::string identity,
            AZStd::vector<ExtensionAPI::Capability> capabilities)
        {
            ExtensionAPI::ExtensionDeclaration declaration;
            declaration.m_extensionId = AZStd::move(identity);
            declaration.m_displayName = "Fixture Extension";
            declaration.m_version = "1.2.3";
            declaration.m_supportedGameVersions = { "1.0.0" };
            declaration.m_supportedBranches = { "mono" };
            declaration.m_capabilities = AZStd::move(capabilities);
            return declaration;
        }

        SourceRecord MakeSource()
        {
            SourceRecord source;
            source.m_sourceId = "source.extension.fixture";
            source.m_title = "Fixture source";
            source.m_sourceKind = "extension_candidate";
            source.m_locator = "extension://fixture/source";
            source.m_fingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            source.m_profileId = "profile.foa.mono";
            source.m_gameVersion = "1.0.0";
            source.m_branch = "mono";
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.2.3";
            source.m_importerId = "importer.fixture";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-21T12:00:00Z";
            source.m_importedAt = "2026-07-21T12:03:00Z";
            source.m_mediaType = "application/json";
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence()
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence.extension.fixture";
            evidence.m_sourceId = "source.extension.fixture";
            evidence.m_sourceFingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            evidence.m_profileId = "profile.foa.mono";
            evidence.m_gameVersion = "1.0.0";
            evidence.m_branch = "mono";
            evidence.m_subjectRef = "foa:actor:fixture";
            evidence.m_claim = "The fixture actor exists in the exact imported source.";
            evidence.m_evidenceKind = "extension_candidate";
            evidence.m_confidence = "documented";
            evidence.m_locator = "extension://fixture/source#actor";
            evidence.m_recordPath = "actors[0]";
            evidence.m_extractedAt = "2026-07-21T12:02:00Z";
            return evidence;
        }
    } // namespace

    class ExtensionAPITests : public ::testing::Test
    {
    protected:
        ExtensionAPITests()
            : m_foundation(FoundationWorkspaceLoadDependencies{})
        {
            m_foundation.SetWorkspace(MakeWorkspace());
        }

        ExtensionAPI::Client RegisterAndBind(
            const ExtensionAPI::ExtensionDeclaration& declaration)
        {
            AZStd::string error;
            EXPECT_TRUE(m_foundation.RegisterExtension(declaration, &error))
                << error.c_str();
            ExtensionAPI::Client client;
            EXPECT_TRUE(m_foundation.CreateExtensionClient(
                declaration.m_extensionId, client, &error)) << error.c_str();
            return client;
        }

        FoundationService m_foundation;
    };

    TEST_F(ExtensionAPITests, RegistrationIsDeterministicAndDuplicateIdentityFailsClosed)
    {
        AZStd::string error;
        EXPECT_TRUE(m_foundation.RegisterExtension(
            MakeDeclaration("extension.zeta", { ExtensionAPI::Capability::QueryCatalog }), &error));
        EXPECT_TRUE(m_foundation.RegisterExtension(
            MakeDeclaration("extension.alpha", { ExtensionAPI::Capability::ReadActiveProfile }), &error));
        EXPECT_FALSE(m_foundation.RegisterExtension(
            MakeDeclaration("extension.alpha", { ExtensionAPI::Capability::ReadActiveProfile }), &error));

        const auto declarations = m_foundation.GetRegisteredExtensions();
        ASSERT_EQ(declarations.size(), 2);
        EXPECT_EQ(declarations[0].m_extensionId, "extension.alpha");
        EXPECT_EQ(declarations[1].m_extensionId, "extension.zeta");
    }

    TEST_F(ExtensionAPITests, ClientIsIdentityBoundAndOmitsPrivateProfilePaths)
    {
        ExtensionAPI::Client client = RegisterAndBind(
            MakeDeclaration(
                "extension.profile",
                { ExtensionAPI::Capability::ReadActiveProfile }));
        EXPECT_TRUE(client.IsBound());
        EXPECT_EQ(client.GetExtensionId(), "extension.profile");

        ExtensionAPI::ProfileView profile;
        AZStd::string error;
        ASSERT_TRUE(client.GetActiveProfile(profile, &error));
        EXPECT_EQ(profile.m_profileId, "profile.foa.mono");
        EXPECT_EQ(profile.m_branch, "mono");
        EXPECT_EQ(profile.m_runtimeTarget, "Mono");
        ASSERT_EQ(profile.m_dlcScopes.size(), 2);
        EXPECT_EQ(profile.m_dlcScopes[0], "dlc.a");
        EXPECT_EQ(profile.m_dlcScopes[1], "dlc.z");

        AZStd::vector<CatalogRecord> records;
        EXPECT_FALSE(client.QueryCatalog(CatalogQuery{}, records, 16, &error));
    }

    TEST_F(ExtensionAPITests, UnboundOrUnknownClientFailsClosed)
    {
        ExtensionAPI::Client client;
        ExtensionAPI::ProfileView profile;
        AZStd::string error;
        EXPECT_FALSE(client.GetActiveProfile(profile, &error));
        EXPECT_FALSE(error.empty());

        EXPECT_FALSE(m_foundation.CreateExtensionClient(
            "extension.not-registered", client, &error));
        EXPECT_FALSE(client.IsBound());
    }

    TEST_F(ExtensionAPITests, ExactBranchAndVersionDeclarationsAreEnforced)
    {
        auto declaration = MakeDeclaration(
            "extension.wrong-branch", { ExtensionAPI::Capability::ReadActiveProfile });
        declaration.m_supportedBranches = { "il2cpp" };
        ExtensionAPI::Client client = RegisterAndBind(declaration);

        ExtensionAPI::ProfileView profile;
        AZStd::string error;
        EXPECT_FALSE(client.GetActiveProfile(profile, &error));
    }

    TEST_F(ExtensionAPITests, CatalogQueriesAreCopyOnlyAndBounded)
    {
        ExtensionAPI::Client client = RegisterAndBind(
            MakeDeclaration(
                "extension.catalog",
                { ExtensionAPI::Capability::QueryCatalog }));

        AZStd::string error;
        AZStd::vector<CatalogRecord> records;
        EXPECT_TRUE(client.QueryCatalog(CatalogQuery{}, records, 16, &error));
        EXPECT_TRUE(records.empty());
        EXPECT_FALSE(client.QueryCatalog(
            CatalogQuery{},
            records,
            ExtensionAPI::Service::MaximumCatalogQueryResults + 1,
            &error));
    }

    TEST_F(ExtensionAPITests, CandidateEvidenceRequiresReviewBeforePublication)
    {
        AZStd::string error;
        ASSERT_TRUE(m_foundation.RegisterSource(MakeSource(), &error));
        ExtensionAPI::Client client = RegisterAndBind(
            MakeDeclaration(
                "extension.evidence",
                { ExtensionAPI::Capability::SubmitCandidateEvidence }));

        EvidenceRecord evidence = MakeEvidence();
        EXPECT_TRUE(client.SubmitCandidateEvidence(evidence, &error));
        EXPECT_EQ(m_foundation.GetSourceRegistry().FindEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_NE(
            m_foundation.GetSourceRegistry().FindCandidateEvidence(evidence.m_evidenceId),
            nullptr);

        ASSERT_TRUE(m_foundation.PromoteCandidateEvidence(evidence.m_evidenceId, &error));
        EXPECT_NE(m_foundation.GetSourceRegistry().FindEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_EQ(
            m_foundation.GetSourceRegistry().FindCandidateEvidence(evidence.m_evidenceId),
            nullptr);

        evidence.m_evidenceId = "evidence.extension.wrong-branch";
        evidence.m_branch = "il2cpp";
        EXPECT_FALSE(client.SubmitCandidateEvidence(evidence, &error));
    }

    TEST_F(ExtensionAPITests, UnknownCapabilityValueFailsRegistration)
    {
        auto declaration = MakeDeclaration(
            "extension.invalid-capability",
            { static_cast<ExtensionAPI::Capability>(999) });
        AZStd::string error;
        EXPECT_FALSE(m_foundation.RegisterExtension(declaration, &error));
    }
} // namespace TaintedGrailModdingSDK
