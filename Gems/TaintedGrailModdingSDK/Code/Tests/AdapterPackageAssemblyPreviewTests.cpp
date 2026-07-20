/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPackageAssemblyPreviewService.h"
#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Fingerprint(char digit)
        {
            return AZStd::string("sha256:") + AZStd::string(64, digit);
        }

        AdapterBuildExpectedOutput Output(
            AZStd::string path,
            AZStd::string role,
            AZStd::string mediaType)
        {
            AdapterBuildExpectedOutput output;
            output.m_relativePath = AZStd::move(path);
            output.m_role = AZStd::move(role);
            output.m_mediaType = AZStd::move(mediaType);
            output.m_redistributable = true;
            return output;
        }

        AdapterStagingInventoryEntry Entry(
            AZStd::string id,
            const AdapterBuildExpectedOutput& output,
            char fingerprintDigit)
        {
            AdapterStagingInventoryEntry entry;
            entry.m_entryId = AZStd::move(id);
            entry.m_stagingPath = AZStd::string("Staging/") + output.m_relativePath;
            entry.m_packagePath = output.m_relativePath;
            entry.m_role = output.m_role;
            entry.m_mediaType = output.m_mediaType;
            entry.m_outputFingerprint = Fingerprint(fingerprintDigit);
            entry.m_byteSize = 1024;
            entry.m_projectOwned = true;
            entry.m_redistributable = true;
            entry.m_includeInPackage = true;
            return entry;
        }

        AdapterPackageAssemblyPreviewRequest MakeReadyRequest()
        {
            AdapterPackageAssemblyPreviewRequest request;
            request.m_manifest.m_manifestId =
                "buildmanifest:owner.preview-pack:owner.foa-adapter";
            request.m_manifest.m_packId = "owner.preview-pack";
            request.m_manifest.m_packVersion = "1.2.3";
            request.m_manifest.m_adapterId = "owner.foa-adapter";
            request.m_manifest.m_adapterVersion = "0.4.1";
            request.m_manifest.m_planId =
                "workorder.plan:owner.preview-pack:owner.foa-adapter";
            request.m_manifest.m_packageRoot =
                "BepInEx/plugins/owner.preview-pack";
            request.m_manifest.m_status = AdapterBuildManifestStatus::Ready;
            request.m_manifest.m_canonicalJson = "{\"BuildAllowed\":false}";
            request.m_manifest.m_buildAllowed = false;
            request.m_manifest.m_expectedOutputs = {
                Output(
                    "BepInEx/plugins/owner.preview-pack/owner.preview-pack.dll",
                    "plugin_binary",
                    "application/vnd.microsoft.portable-executable"),
                Output(
                    "BepInEx/plugins/owner.preview-pack/README.md",
                    "readme",
                    "text/markdown"),
                Output(
                    "BepInEx/plugins/owner.preview-pack/CHANGELOG.md",
                    "changelog",
                    "text/markdown"),
                Output(
                    "BepInEx/plugins/owner.preview-pack/MANIFEST.md",
                    "manifest",
                    "text/markdown"),
                Output(
                    "BepInEx/plugins/owner.preview-pack/LICENSE",
                    "license",
                    "text/plain"),
            };

            request.m_review.m_reviewId = "owner.manifest-review";
            request.m_review.m_manifestId = request.m_manifest.m_manifestId;
            const AZStd::string manifestFingerprint =
                CalculateCanonicalSha256(request.m_manifest.m_canonicalJson);
            request.m_review.m_manifestFingerprint = manifestFingerprint;
            request.m_review.m_decision =
                AdapterBuildManifestReviewDecision::Accepted;
            request.m_review.m_reviewer = "package-reviewer";
            request.m_review.m_evidenceIds = { "evidence.manifest.review" };

            request.m_inventory.m_inventoryId = "owner.staging-inventory";
            request.m_inventory.m_manifestId = request.m_manifest.m_manifestId;
            request.m_inventory.m_manifestFingerprint = manifestFingerprint;
            request.m_inventory.m_packId = request.m_manifest.m_packId;
            request.m_inventory.m_packageRoot = request.m_manifest.m_packageRoot;
            char digit = '1';
            for (const AdapterBuildExpectedOutput& output :
                request.m_manifest.m_expectedOutputs)
            {
                const char currentDigit = digit;
                request.m_inventory.m_entries.push_back(Entry(
                    AZStd::string("entry.output-") + currentDigit,
                    output,
                    currentDigit));
                ++digit;
            }
            return request;
        }
    } // namespace

    TEST(AdapterPackageAssemblyPreviewTests, TypedStatusAndReviewVocabulariesAreStrict)
    {
        AdapterBuildManifestReviewDecision decision =
            AdapterBuildManifestReviewDecision::Unknown;
        EXPECT_TRUE(TryParseAdapterBuildManifestReviewDecision("accepted", decision));
        EXPECT_EQ(decision, AdapterBuildManifestReviewDecision::Accepted);
        EXPECT_FALSE(TryParseAdapterBuildManifestReviewDecision("approved", decision));

        AdapterPackageAssemblyPreviewStatus status =
            AdapterPackageAssemblyPreviewStatus::ManifestNotReady;
        EXPECT_TRUE(TryParseAdapterPackageAssemblyPreviewStatus("collision", status));
        EXPECT_EQ(status, AdapterPackageAssemblyPreviewStatus::Collision);
        EXPECT_FALSE(TryParseAdapterPackageAssemblyPreviewStatus("assembled", status));
    }

    TEST(AdapterPackageAssemblyPreviewTests, RegistryRejectsDuplicateInventoryIdentities)
    {
        AdapterPackageAssemblyPreviewRegistry& registry =
            AdapterPackageAssemblyPreviewRegistry::Get();
        registry.Clear();
        const AdapterPackageAssemblyPreviewRequest request = MakeReadyRequest();
        AZStd::string error;
        EXPECT_TRUE(registry.RegisterRequest(request, &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterRequest(request, &error));
        EXPECT_NE(error.find("already exists"), AZStd::string::npos);
        registry.Clear();
    }

    TEST(AdapterPackageAssemblyPreviewTests, ReviewedCompleteInventoryProducesReadyPreviewOnly)
    {
        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview preview =
            service.BuildPreview(MakeReadyRequest());
        EXPECT_EQ(preview.m_status, AdapterPackageAssemblyPreviewStatus::Ready);
        EXPECT_EQ(preview.m_layout.size(), 5);
        EXPECT_TRUE(preview.m_omissions.empty());
        EXPECT_TRUE(preview.m_collisions.empty());
        EXPECT_FALSE(preview.m_assemblyAllowed);
        EXPECT_FALSE(preview.m_archiveAllowed);
        EXPECT_FALSE(preview.m_deploymentAllowed);
        EXPECT_NE(
            preview.m_canonicalJson.find("\"AssemblyAllowed\":false"),
            AZStd::string::npos);
        EXPECT_NE(
            preview.m_canonicalJson.find("\"OutputDigest\":\"sha256:"),
            AZStd::string::npos);
    }

    TEST(AdapterPackageAssemblyPreviewTests, ManifestNotReadyPrecedesInventoryFailures)
    {
        AdapterPackageAssemblyPreviewRequest request = MakeReadyRequest();
        request.m_manifest.m_status = AdapterBuildManifestStatus::ToolchainUnresolved;
        request.m_inventory.m_entries.clear();
        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview preview = service.BuildPreview(request);
        EXPECT_EQ(
            preview.m_status,
            AdapterPackageAssemblyPreviewStatus::ManifestNotReady);
    }

    TEST(AdapterPackageAssemblyPreviewTests, ReviewAndInventoryBindingsFailClosed)
    {
        AdapterPackageAssemblyPreviewService service;
        AdapterPackageAssemblyPreviewRequest rejected = MakeReadyRequest();
        rejected.m_review.m_decision = AdapterBuildManifestReviewDecision::Rejected;
        EXPECT_EQ(
            service.BuildPreview(rejected).m_status,
            AdapterPackageAssemblyPreviewStatus::ManifestUnreviewed);

        AdapterPackageAssemblyPreviewRequest drifted = MakeReadyRequest();
        drifted.m_inventory.m_manifestFingerprint = Fingerprint('f');
        EXPECT_EQ(
            service.BuildPreview(drifted).m_status,
            AdapterPackageAssemblyPreviewStatus::InventoryBindingMismatch);
    }

    TEST(AdapterPackageAssemblyPreviewTests, MissingOutputProducesExplicitOmission)
    {
        AdapterPackageAssemblyPreviewRequest request = MakeReadyRequest();
        request.m_inventory.m_entries.pop_back();
        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview preview = service.BuildPreview(request);
        EXPECT_EQ(
            preview.m_status,
            AdapterPackageAssemblyPreviewStatus::OutputMissing);
        ASSERT_EQ(preview.m_omissions.size(), 1);
        EXPECT_EQ(preview.m_omissions.front().m_role, "license");
    }

    TEST(AdapterPackageAssemblyPreviewTests, MissingOutputDigestIsDistinct)
    {
        AdapterPackageAssemblyPreviewRequest request = MakeReadyRequest();
        request.m_inventory.m_entries.front().m_outputFingerprint.clear();
        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview preview = service.BuildPreview(request);
        EXPECT_EQ(
            preview.m_status,
            AdapterPackageAssemblyPreviewStatus::FingerprintMissing);
    }

    TEST(AdapterPackageAssemblyPreviewTests, UnsafePathAndTargetCollisionAreRejected)
    {
        AdapterPackageAssemblyPreviewService service;
        AdapterPackageAssemblyPreviewRequest unsafe = MakeReadyRequest();
        unsafe.m_inventory.m_entries.front().m_stagingPath = "../escape.dll";
        EXPECT_EQ(
            service.BuildPreview(unsafe).m_status,
            AdapterPackageAssemblyPreviewStatus::PathInvalid);

        AdapterPackageAssemblyPreviewRequest duplicate = MakeReadyRequest();
        AdapterStagingInventoryEntry copy = duplicate.m_inventory.m_entries.front();
        copy.m_entryId = "entry.duplicate-target";
        duplicate.m_inventory.m_entries.push_back(copy);
        const AdapterPackageAssemblyPreview collision =
            service.BuildPreview(duplicate);
        EXPECT_EQ(
            collision.m_status,
            AdapterPackageAssemblyPreviewStatus::Collision);
        ASSERT_EQ(collision.m_collisions.size(), 1);
        EXPECT_EQ(collision.m_collisions.front().m_inventoryEntryIds.size(), 2);
    }

    TEST(AdapterPackageAssemblyPreviewTests, OwnershipAndRedistributionFailClosed)
    {
        AdapterPackageAssemblyPreviewService service;
        AdapterPackageAssemblyPreviewRequest unowned = MakeReadyRequest();
        unowned.m_inventory.m_entries.front().m_projectOwned = false;
        EXPECT_EQ(
            service.BuildPreview(unowned).m_status,
            AdapterPackageAssemblyPreviewStatus::InventoryUntrusted);

        AdapterPackageAssemblyPreviewRequest restricted = MakeReadyRequest();
        restricted.m_inventory.m_entries.front().m_redistributable = false;
        EXPECT_EQ(
            service.BuildPreview(restricted).m_status,
            AdapterPackageAssemblyPreviewStatus::RedistributionBlocked);
    }

    TEST(AdapterPackageAssemblyPreviewTests, CanonicalLayoutAndDigestsAreDeterministic)
    {
        AdapterPackageAssemblyPreviewRequest first = MakeReadyRequest();
        AdapterPackageAssemblyPreviewRequest second = first;
        AZStd::reverse(
            second.m_inventory.m_entries.begin(),
            second.m_inventory.m_entries.end());
        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview firstPreview = service.BuildPreview(first);
        const AdapterPackageAssemblyPreview secondPreview = service.BuildPreview(second);
        EXPECT_EQ(firstPreview.m_canonicalJson, secondPreview.m_canonicalJson);
        EXPECT_EQ(
            firstPreview.m_layout.front().m_outputDigest,
            secondPreview.m_layout.front().m_outputDigest);
    }

    TEST(AdapterPackageAssemblyPreviewTests, PreviewGenerationDoesNotMutateInputs)
    {
        AdapterPackageAssemblyPreviewRequest request = MakeReadyRequest();
        const size_t expectedOutputCount = request.m_manifest.m_expectedOutputs.size();
        const size_t inventoryCount = request.m_inventory.m_entries.size();
        const AZStd::string firstInventoryId =
            request.m_inventory.m_entries.front().m_entryId;
        const AZStd::string manifestJson = request.m_manifest.m_canonicalJson;

        AdapterPackageAssemblyPreviewService service;
        const AdapterPackageAssemblyPreview preview = service.BuildPreview(request);
        EXPECT_EQ(preview.m_status, AdapterPackageAssemblyPreviewStatus::Ready);
        EXPECT_EQ(request.m_manifest.m_expectedOutputs.size(), expectedOutputCount);
        EXPECT_EQ(request.m_inventory.m_entries.size(), inventoryCount);
        EXPECT_EQ(request.m_inventory.m_entries.front().m_entryId, firstInventoryId);
        EXPECT_EQ(request.m_manifest.m_canonicalJson, manifestJson);
    }
} // namespace TaintedGrailModdingSDK
