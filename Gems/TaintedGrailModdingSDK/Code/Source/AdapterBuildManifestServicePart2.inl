/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    AdapterBuildManifest AdapterBuildManifestService::BuildManifest(
        const AdapterBuildManifestRequest& request) const
    {
        AdapterBuildManifest manifest;
        manifest.m_packId = request.m_pack.m_packId;
        manifest.m_packVersion = request.m_pack.m_version;
        manifest.m_adapterId = request.m_declaration.m_adapterId;
        manifest.m_adapterVersion = request.m_declaration.m_version;
        manifest.m_requiredAdapterVersion = request.m_pack.m_requiredAdapterVersion;
        manifest.m_profileId = request.m_profile.m_profileId;
        manifest.m_gameVersion = request.m_profile.m_gameVersion;
        manifest.m_branch = request.m_profile.m_branch;
        manifest.m_runtimeTarget = request.m_profile.m_runtimeTarget;
        manifest.m_unityVersion = request.m_profile.m_unityVersion;
        manifest.m_bepInExVersion = request.m_profile.m_bepInExVersion;
        manifest.m_planId = request.m_plan.m_planId;
        manifest.m_planFingerprint = request.m_planFingerprint;
        manifest.m_planCanonicalJson = request.m_plan.m_canonicalJson;
        manifest.m_pluginGuid = request.m_pluginGuid;
        manifest.m_pluginName = request.m_pluginName;
        manifest.m_pluginVersion = request.m_pluginVersion;
        manifest.m_packageRoot = request.m_packageRoot;
        manifest.m_environment = request.m_environment;
        manifest.m_dependencies = request.m_dependencies;
        manifest.m_materials = request.m_materials;
        manifest.m_expectedOutputs = request.m_expectedOutputs;
        manifest.m_buildAllowed = false;

        manifest.m_manifestId = AZStd::string::format(
            "buildmanifest:%s:%s:%s:%s:%s:%s:%s:%s:%s",
            manifest.m_packId.c_str(),
            manifest.m_packVersion.c_str(),
            manifest.m_adapterId.c_str(),
            manifest.m_adapterVersion.c_str(),
            manifest.m_profileId.c_str(),
            manifest.m_gameVersion.c_str(),
            manifest.m_branch.c_str(),
            manifest.m_runtimeTarget.c_str(),
            manifest.m_planId.c_str());

        SortDependencies(manifest.m_dependencies);
        SortMaterials(manifest.m_materials);
        SortOutputs(manifest.m_expectedOutputs);

        AZStd::vector<AZStd::string> planReasons;
        const bool planValid = PlanBindingMatches(request, planReasons);

        AZStd::vector<AZStd::string> toolchainReasons;
        AdapterSemanticVersion builderVersion;
        if (!IsStableNamespacedId(manifest.m_environment.m_builderId))
        {
            AddReason(toolchainReasons, "BuilderId must be a lowercase namespaced stable ID.");
        }
        if (!TryParseAdapterSemanticVersion(
                manifest.m_environment.m_builderVersion,
                builderVersion))
        {
            AddReason(toolchainReasons, "BuilderVersion must be a strict semantic version.");
        }
        if (!IsCommitSha(manifest.m_environment.m_sourceCommit))
        {
            AddReason(toolchainReasons, "SourceCommit must be an exact lowercase 40-character commit SHA.");
        }
        if (!IsCommitSha(manifest.m_environment.m_o3deRevision))
        {
            AddReason(toolchainReasons, "O3deRevision must be an exact lowercase 40-character commit SHA.");
        }
        if (manifest.m_environment.m_configuration.empty()
            || manifest.m_environment.m_targetFramework.empty()
            || manifest.m_environment.m_compilerId.empty()
            || manifest.m_environment.m_compilerVersion.empty())
        {
            AddReason(
                toolchainReasons,
                "Configuration, target framework, compiler ID, and compiler version are required.");
        }
        if (!manifest.m_environment.m_deterministicBuild)
        {
            AddReason(toolchainReasons, "Deterministic build settings must be declared.");
        }
        if (!manifest.m_environment.m_continuousIntegrationBuild)
        {
            AddReason(toolchainReasons, "Continuous-integration build normalization must be declared.");
        }
        if (!manifest.m_environment.m_pathMapEnabled)
        {
            AddReason(toolchainReasons, "Source path mapping must be enabled for reproducible output.");
        }

        AZStd::vector<AZStd::string> inputReasons;
        AdapterSemanticVersion pluginVersion;
        if (!IsStableNamespacedId(manifest.m_pluginGuid))
        {
            AddReason(inputReasons, "PluginGuid must be a lowercase namespaced stable ID.");
        }
        if (manifest.m_pluginName.empty())
        {
            AddReason(inputReasons, "PluginName is required.");
        }
        if (!TryParseAdapterSemanticVersion(manifest.m_pluginVersion, pluginVersion))
        {
            AddReason(inputReasons, "PluginVersion must be a strict semantic version.");
        }
        if (manifest.m_packageRoot.empty())
        {
            AddReason(inputReasons, "A BepInEx package root is required.");
        }
        for (const char* role : {
                 "work_order_plan",
                 "source_tree",
                 "dependency_lock",
                 "toolchain_lock",
                 "license" })
        {
            bool requiredRoleSatisfied = false;
            for (const AdapterBuildMaterial& material : manifest.m_materials)
            {
                if (material.m_role == role
                    && material.m_required
                    && !material.m_locator.empty())
                {
                    requiredRoleSatisfied = true;
                    break;
                }
            }
            if (!requiredRoleSatisfied)
            {
                AddReason(
                    inputReasons,
                    AZStd::string("Required build material must be marked required and have a locator: ")
                        + role);
            }
        }
        for (const char* role : {
                 "plugin_binary",
                 "readme",
                 "changelog",
                 "manifest",
                 "license" })
        {
            if (!HasOutputRole(manifest.m_expectedOutputs, role))
            {
                AddReason(
                    inputReasons,
                    AZStd::string("Required expected package output is missing: ") + role);
            }
        }
        for (const AdapterBuildDependency& dependency : manifest.m_dependencies)
        {
            AdapterSemanticVersion dependencyVersion;
            if (!IsStableNamespacedId(dependency.m_pluginId)
                || !TryParseAdapterSemanticVersion(
                    dependency.m_version,
                    dependencyVersion))
            {
                AddReason(
                    inputReasons,
                    "Dependencies require exact stable plugin IDs and strict semantic versions.");
            }
        }
        for (const AdapterBuildMaterial& material : manifest.m_materials)
        {
            if (material.m_materialId.empty()
                || material.m_role.empty()
                || material.m_mediaType.empty())
            {
                AddReason(
                    inputReasons,
                    "Every build material requires an ID, role, and media type.");
            }
            if (material.m_required && material.m_locator.empty())
            {
                AddReason(
                    inputReasons,
                    AZStd::string("Required material has no locator: ")
                        + material.m_materialId);
            }
        }
        for (const AdapterBuildExpectedOutput& output : manifest.m_expectedOutputs)
        {
            if (output.m_relativePath.empty()
                || output.m_role.empty()
                || output.m_mediaType.empty())
            {
                AddReason(
                    inputReasons,
                    "Every expected output requires a path, role, and media type.");
            }
        }

        const AZStd::string canonicalPlanJson =
            AdapterWorkOrderPlanningService{}.SerializeCanonicalPlan(request.m_plan);
        AZStd::vector<AZStd::string> fingerprintReasons;
        if (!IsSha256Fingerprint(manifest.m_planFingerprint))
        {
            AddReason(
                fingerprintReasons,
                "PlanFingerprint must use lowercase sha256:<64 hex>.");
        }
        if (manifest.m_planCanonicalJson.empty()
            || manifest.m_planCanonicalJson != canonicalPlanJson
            || !CanonicalSha256Matches(
                canonicalPlanJson,
                manifest.m_planFingerprint))
        {
            AddReason(
                fingerprintReasons,
                "PlanCanonicalJson must equal the canonical serialization of the supplied plan object and PlanFingerprint must equal SHA-256 over those exact bytes.");
        }
        bool planMaterialMatches = false;
        for (const AdapterBuildMaterial& material : manifest.m_materials)
        {
            if (!IsSha256Fingerprint(material.m_fingerprint))
            {
                AddReason(
                    fingerprintReasons,
                    AZStd::string("Material fingerprint is missing or malformed: ")
                        + material.m_materialId);
            }
            if (material.m_role == "work_order_plan"
                && material.m_required
                && material.m_fingerprint == manifest.m_planFingerprint)
            {
                planMaterialMatches = true;
            }
        }
        if (!planMaterialMatches)
        {
            AddReason(
                fingerprintReasons,
                "The required work_order_plan material must carry the derived PlanFingerprint.");
        }

        AZStd::vector<AZStd::string> pathReasons;
        if (!IsSafeRelativePath(manifest.m_packageRoot)
            || manifest.m_packageRoot.find("BepInEx/plugins/") != 0)
        {
            AddReason(
                pathReasons,
                "PackageRoot must be a safe relative path beneath BepInEx/plugins/.");
        }
        if (HasDuplicateDependencyIds(manifest.m_dependencies))
        {
            AddReason(pathReasons, "Dependency plugin IDs must be unique.");
        }
        if (HasDuplicateMaterialIds(manifest.m_materials))
        {
            AddReason(pathReasons, "Build material IDs must be unique.");
        }
        if (HasDuplicateOutputPaths(manifest.m_expectedOutputs))
        {
            AddReason(pathReasons, "Expected output paths must be unique.");
        }
        for (const AdapterBuildMaterial& material : manifest.m_materials)
        {
            if (!material.m_locator.empty()
                && !IsSafeRelativePath(material.m_locator))
            {
                AddReason(
                    pathReasons,
                    AZStd::string("Material locator is not safe and relative: ")
                        + material.m_materialId);
            }
        }
        for (const AdapterBuildExpectedOutput& output : manifest.m_expectedOutputs)
        {
            if (!PathIsInsideRoot(
                    manifest.m_packageRoot,
                    output.m_relativePath))
            {
                AddReason(
                    pathReasons,
                    AZStd::string("Expected output must remain beneath PackageRoot: ")
                        + output.m_relativePath);
            }
        }

        AZStd::vector<AZStd::string> redistributionReasons;
        for (const AdapterBuildMaterial& material : manifest.m_materials)
        {
            if (material.m_includeInPackage && !material.m_redistributable)
            {
                AddReason(
                    redistributionReasons,
                    AZStd::string("Non-redistributable material cannot enter package output: ")
                        + material.m_materialId);
            }
        }
        for (const AdapterBuildExpectedOutput& output : manifest.m_expectedOutputs)
        {
            if (!output.m_redistributable)
            {
                AddReason(
                    redistributionReasons,
                    AZStd::string("Expected package output is not redistributable: ")
                        + output.m_relativePath);
            }
        }

        if (!planValid)
        {
            manifest.m_status = AdapterBuildManifestStatus::PlanMismatch;
            manifest.m_reasons = AZStd::move(planReasons);
        }
        else if (!toolchainReasons.empty())
        {
            manifest.m_status = AdapterBuildManifestStatus::ToolchainUnresolved;
            manifest.m_reasons = AZStd::move(toolchainReasons);
        }
        else if (!inputReasons.empty())
        {
            manifest.m_status = AdapterBuildManifestStatus::InputMissing;
            manifest.m_reasons = AZStd::move(inputReasons);
        }
        else if (!fingerprintReasons.empty())
        {
            manifest.m_status = AdapterBuildManifestStatus::FingerprintMissing;
            manifest.m_reasons = AZStd::move(fingerprintReasons);
        }
        else if (!pathReasons.empty())
        {
            manifest.m_status = AdapterBuildManifestStatus::PathInvalid;
            manifest.m_reasons = AZStd::move(pathReasons);
        }
        else if (!redistributionReasons.empty())
        {
            manifest.m_status = AdapterBuildManifestStatus::RedistributionBlocked;
            manifest.m_reasons = AZStd::move(redistributionReasons);
        }
        else
        {
            manifest.m_status = AdapterBuildManifestStatus::Ready;
            manifest.m_reasons.push_back(
                "The reproducible build definition is complete; build invocation remains prohibited in this slice.");
        }

        SortUnique(manifest.m_reasons);
        manifest.m_canonicalJson = SerializeCanonicalManifest(manifest);
        return manifest;
    }
} // namespace TaintedGrailModdingSDK
