/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterBuildManifestService.h"
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

        AdapterBuildMaterial Material(
            AZStd::string id,
            AZStd::string role,
            AZStd::string locator,
            char fingerprintDigit,
            bool includeInPackage = false,
            bool redistributable = true)
        {
            AdapterBuildMaterial material;
            material.m_materialId = AZStd::move(id);
            material.m_role = AZStd::move(role);
            material.m_locator = AZStd::move(locator);
            material.m_mediaType = "application/octet-stream";
            material.m_fingerprint = Fingerprint(fingerprintDigit);
            material.m_required = true;
            material.m_includeInPackage = includeInPackage;
            material.m_redistributable = redistributable;
            return material;
        }

        AdapterBuildExpectedOutput Output(
            AZStd::string path,
            AZStd::string role,
            AZStd::string mediaType = "application/octet-stream")
        {
            AdapterBuildExpectedOutput output;
            output.m_relativePath = AZStd::move(path);
            output.m_role = AZStd::move(role);
            output.m_mediaType = AZStd::move(mediaType);
            output.m_redistributable = true;
            return output;
        }

        AdapterBuildManifestRequest MakeReadyRequest()
        {
            AdapterBuildManifestRequest request;
            request.m_pack.m_packId = "owner.preview-pack";
            request.m_pack.m_displayName = "Preview Pack";
            request.m_pack.m_version = "1.2.3";
            request.m_pack.m_requiredAdapterVersion = "0.4.0";
            request.m_pack.m_buildConfiguration = "Profile";

            request.m_profile.m_profileId = "foa.mono.preview";
            request.m_profile.m_gameVersion = "1.0.9";
            request.m_profile.m_branch = "mono";
            request.m_profile.m_runtimeTarget = "Mono";
            request.m_profile.m_unityVersion = "2022.3.22f1";
            request.m_profile.m_bepInExVersion = "5.4.23.3";

            request.m_declaration.m_adapterId = "owner.foa-adapter";
            request.m_declaration.m_displayName = "FoA Adapter";
            request.m_declaration.m_version = "0.4.1";
            request.m_declaration.m_runtimeTargets = { "Mono" };

            request.m_plan.m_planId =
                "workorder.plan:owner.preview-pack:owner.foa-adapter:foa.mono.preview";
            request.m_plan.m_packId = request.m_pack.m_packId;
            request.m_plan.m_packVersion = request.m_pack.m_version;
            request.m_plan.m_adapterId = request.m_declaration.m_adapterId;
            request.m_plan.m_adapterVersion = request.m_declaration.m_version;
            request.m_plan.m_requiredAdapterVersion = request.m_pack.m_requiredAdapterVersion;
            request.m_plan.m_profileId = request.m_profile.m_profileId;
            request.m_plan.m_gameVersion = request.m_profile.m_gameVersion;
            request.m_plan.m_branch = request.m_profile.m_branch;
            request.m_plan.m_runtimeTarget = request.m_profile.m_runtimeTarget;
            request.m_plan.m_executionAllowed = false;

            AdapterWorkOrderStep step;
            step.m_stepId = request.m_plan.m_planId + ":step:item_grant:record:item.preview";
            step.m_sequence = 1;
            step.m_capability = "item_grant";
            step.m_subjectKind = "record";
            step.m_subjectId = "item.preview";
            step.m_executionAllowed = false;
            request.m_plan.m_steps.push_back(step);
            AdapterWorkOrderPlanningService planningService;
            request.m_plan.m_canonicalJson =
                planningService.SerializeCanonicalPlan(request.m_plan);

            request.m_environment.m_builderId = "tg.builder.dotnet";
            request.m_environment.m_builderVersion = "1.0.0";
            request.m_environment.m_sourceCommit = AZStd::string(40, 'a');
            request.m_environment.m_o3deRevision = AZStd::string(40, 'b');
            request.m_environment.m_configuration = "Profile";
            request.m_environment.m_targetFramework = "net472";
            request.m_environment.m_compilerId = "dotnet.csharp";
            request.m_environment.m_compilerVersion = "8.0.0";
            request.m_environment.m_deterministicBuild = true;
            request.m_environment.m_continuousIntegrationBuild = true;
            request.m_environment.m_pathMapEnabled = true;

            request.m_planFingerprint =
                CalculateCanonicalSha256(request.m_plan.m_canonicalJson);
            request.m_pluginGuid = "owner.preview-plugin";
            request.m_pluginName = "Preview Plugin";
            request.m_pluginVersion = "1.2.3";
            request.m_packageRoot = "BepInEx/plugins/owner.preview-pack";

            AdapterBuildDependency dependency;
            dependency.m_pluginId = "bepinex.core";
            dependency.m_version = "5.4.23";
            dependency.m_kind = AdapterBuildDependencyKind::Hard;
            request.m_dependencies.push_back(dependency);

            request.m_materials = {
                Material(
                    "material.plan",
                    "work_order_plan",
                    "Reports/WorkOrders/preview.json",
                    '1',
                    false,
                    false),
                Material("material.source", "source_tree", "Source", '2', false, false),
                Material(
                    "material.dependencies",
                    "dependency_lock",
                    "Build/dependencies.lock.json",
                    '3'),
                Material(
                    "material.toolchain",
                    "toolchain_lock",
                    "Build/toolchain.lock.json",
                    '4'),
                Material("material.license", "license", "LICENSE", '5', true, true),
            };
            request.m_materials.front().m_fingerprint = request.m_planFingerprint;

            const AZStd::string root = request.m_packageRoot + "/";
            request.m_expectedOutputs = {
                Output(root + "owner.preview-pack.dll", "plugin_binary"),
                Output(root + "README.md", "readme", "text/markdown"),
                Output(root + "CHANGELOG.md", "changelog", "text/markdown"),
                Output(root + "MANIFEST.md", "manifest", "text/markdown"),
                Output(root + "LICENSE", "license", "text/plain"),
            };
            return request;
        }
    } // namespace

    TEST(AdapterBuildManifestTests, TypedStatusAndDependencyVocabulariesAreStrict)
    {
        AdapterBuildDependencyKind dependencyKind = AdapterBuildDependencyKind::Soft;
        EXPECT_TRUE(TryParseAdapterBuildDependencyKind("hard", dependencyKind));
        EXPECT_EQ(dependencyKind, AdapterBuildDependencyKind::Hard);
        EXPECT_FALSE(TryParseAdapterBuildDependencyKind("required", dependencyKind));

        AdapterBuildManifestStatus status = AdapterBuildManifestStatus::Ready;
        EXPECT_TRUE(TryParseAdapterBuildManifestStatus("toolchain_unresolved", status));
        EXPECT_EQ(status, AdapterBuildManifestStatus::ToolchainUnresolved);
        EXPECT_FALSE(TryParseAdapterBuildManifestStatus("built", status));
    }

    TEST(AdapterBuildManifestTests, CompleteDefinitionIsReadyButBuildProhibited)
    {
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(MakeReadyRequest());
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::Ready);
        EXPECT_FALSE(manifest.m_buildAllowed);
        EXPECT_NE(manifest.m_canonicalJson.find("\"BuildAllowed\":false"), AZStd::string::npos);
        EXPECT_NE(manifest.m_canonicalJson.find("\"BuildType\":\"foa.adapter.plugin\""), AZStd::string::npos);
        EXPECT_NE(manifest.m_canonicalJson.find("BepInEx/plugins/owner.preview-pack"), AZStd::string::npos);
    }

    TEST(AdapterBuildManifestTests, CallerSelectedPlanFingerprintIsRejected)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        request.m_planFingerprint = Fingerprint('1');
        request.m_materials.front().m_fingerprint = request.m_planFingerprint;
        AdapterBuildManifestService service;
        EXPECT_EQ(
            service.BuildManifest(request).m_status,
            AdapterBuildManifestStatus::FingerprintMissing);
    }

    TEST(AdapterBuildManifestTests, PlanMismatchPrecedesOtherFailures)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        request.m_plan.m_profileId = "foa.mono.other";
        request.m_environment = {};
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(request);
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::PlanMismatch);
    }

    TEST(AdapterBuildManifestTests, MissingToolchainFailsClosed)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        request.m_environment.m_sourceCommit.clear();
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(request);
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::ToolchainUnresolved);
        EXPECT_FALSE(manifest.m_buildAllowed);
    }

    TEST(AdapterBuildManifestTests, MissingRequiredInputAndFingerprintRemainDistinct)
    {
        AdapterBuildManifestService service;
        AdapterBuildManifestRequest missingInput = MakeReadyRequest();
        missingInput.m_materials.erase(missingInput.m_materials.begin() + 2);
        EXPECT_EQ(
            service.BuildManifest(missingInput).m_status,
            AdapterBuildManifestStatus::InputMissing);

        AdapterBuildManifestRequest missingFingerprint = MakeReadyRequest();
        missingFingerprint.m_materials[1].m_fingerprint.clear();
        EXPECT_EQ(
            service.BuildManifest(missingFingerprint).m_status,
            AdapterBuildManifestStatus::FingerprintMissing);
    }

    TEST(AdapterBuildManifestTests, RequiredRoleCannotBeOptionalOrUnlocated)
    {
        AdapterBuildManifestService service;
        AdapterBuildManifestRequest optional = MakeReadyRequest();
        optional.m_materials.front().m_required = false;
        EXPECT_EQ(
            service.BuildManifest(optional).m_status,
            AdapterBuildManifestStatus::InputMissing);

        AdapterBuildManifestRequest unlocated = MakeReadyRequest();
        unlocated.m_materials.front().m_locator.clear();
        EXPECT_EQ(
            service.BuildManifest(unlocated).m_status,
            AdapterBuildManifestStatus::InputMissing);
    }

    TEST(AdapterBuildManifestTests, PathTraversalAndDuplicateOutputsAreRejected)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        request.m_expectedOutputs[0].m_relativePath = "BepInEx/plugins/owner.preview-pack/../escape.dll";
        request.m_expectedOutputs[1].m_relativePath = request.m_expectedOutputs[0].m_relativePath;
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(request);
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::PathInvalid);
    }

    TEST(AdapterBuildManifestTests, NonRedistributablePackageMaterialIsBlocked)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        request.m_materials[1].m_includeInPackage = true;
        request.m_materials[1].m_redistributable = false;
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(request);
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::RedistributionBlocked);
        EXPECT_FALSE(manifest.m_buildAllowed);
    }

    TEST(AdapterBuildManifestTests, CanonicalSerializationIsDeterministic)
    {
        AdapterBuildManifestRequest first = MakeReadyRequest();
        AdapterBuildManifestRequest second = first;
        AZStd::reverse(second.m_materials.begin(), second.m_materials.end());
        AZStd::reverse(second.m_expectedOutputs.begin(), second.m_expectedOutputs.end());
        AZStd::reverse(second.m_dependencies.begin(), second.m_dependencies.end());

        AdapterBuildManifestService service;
        const AdapterBuildManifest firstManifest = service.BuildManifest(first);
        const AdapterBuildManifest secondManifest = service.BuildManifest(second);
        EXPECT_EQ(firstManifest.m_canonicalJson, secondManifest.m_canonicalJson);
    }

    TEST(AdapterBuildManifestTests, ManifestGenerationDoesNotMutateInputs)
    {
        AdapterBuildManifestRequest request = MakeReadyRequest();
        const size_t materialCount = request.m_materials.size();
        const size_t outputCount = request.m_expectedOutputs.size();
        const AZStd::string firstMaterial = request.m_materials.front().m_materialId;
        const AZStd::string firstOutput = request.m_expectedOutputs.front().m_relativePath;
        const AZStd::string planJson = request.m_plan.m_canonicalJson;

        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(request);
        EXPECT_EQ(manifest.m_status, AdapterBuildManifestStatus::Ready);
        EXPECT_EQ(request.m_materials.size(), materialCount);
        EXPECT_EQ(request.m_expectedOutputs.size(), outputCount);
        EXPECT_EQ(request.m_materials.front().m_materialId, firstMaterial);
        EXPECT_EQ(request.m_expectedOutputs.front().m_relativePath, firstOutput);
        EXPECT_EQ(request.m_plan.m_canonicalJson, planJson);
    }

    TEST(AdapterBuildManifestTests, BepInExMetadataAndResolvedInputsArePreserved)
    {
        AdapterBuildManifestService service;
        const AdapterBuildManifest manifest = service.BuildManifest(MakeReadyRequest());
        ASSERT_EQ(manifest.m_dependencies.size(), 1);
        EXPECT_EQ(manifest.m_dependencies.front().m_pluginId, "bepinex.core");
        EXPECT_EQ(manifest.m_dependencies.front().m_kind, AdapterBuildDependencyKind::Hard);
        EXPECT_EQ(manifest.m_pluginGuid, "owner.preview-plugin");
        EXPECT_EQ(manifest.m_pluginVersion, "1.2.3");
        EXPECT_EQ(manifest.m_environment.m_targetFramework, "net472");
        EXPECT_EQ(manifest.m_materials.size(), 5);
        EXPECT_EQ(manifest.m_expectedOutputs.size(), 5);
    }
} // namespace TaintedGrailModdingSDK
