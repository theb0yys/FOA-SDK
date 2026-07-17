/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationValidationService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            for (const AZStd::string& candidate : values)
            {
                if (candidate == value)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsKnownSaveImpact(const AZStd::string& value)
        {
            return value == "none" || value == "compatible" || value == "migration"
                || value == "destructive" || value == "unknown";
        }
    } // namespace

    AZStd::vector<BlockerRecord> FoundationValidationService::Evaluate(
        const WorkspaceModel& workspace,
        const AZStd::vector<PackManifest>& packs,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::vector<BlockerRecord> blockers;

        if (workspace.m_workspaceId.empty() || workspace.m_rootPath.empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.root",
                "error",
                "workspace",
                "workspace:active",
                "Configure a stable workspace ID and writable workspace root before importing or authoring data."));
        }

        if (workspace.m_outputPath.empty() || workspace.m_stagingPath.empty() || workspace.m_deploymentPath.empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.pipeline-paths",
                "error",
                "workspace",
                "workspace:active",
                "Configure separate output, staging, and deployment directories before generating mod packages.",
                { "generate", "package", "deploy" }));
        }

        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile || !activeProfile->IsConfigured())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.game-profile",
                "error",
                "game-profile",
                "game-profile:active",
                "Configure an active FoA installation, exact game version, branch, runtime target, and managed assembly path before accepting evidence.",
                { "source_intake", "catalog_promotion", "runtime_handoff" }));
        }
        else
        {
            if (activeProfile->m_runtimeTarget != "Mono" && activeProfile->m_runtimeTarget != "IL2CPP")
            {
                blockers.push_back(MakeBlocker(
                    "foundation.workspace.runtime-target",
                    "error",
                    "game-profile",
                    activeProfile->m_profileId,
                    "Runtime target must be exactly Mono or IL2CPP.",
                    { "source_intake", "build", "runtime_handoff" }));
            }

            if (activeProfile->m_unityVersion.empty())
            {
                blockers.push_back(MakeBlocker(
                    "foundation.workspace.unity-version",
                    "error",
                    "game-profile",
                    activeProfile->m_profileId,
                    "Record the exact Unity version for asset and compatibility validation."));
            }

            if (activeProfile->m_runtimeTarget == "Mono"
                && (activeProfile->m_bepInExVersion.empty() || activeProfile->m_pluginPath.empty()))
            {
                blockers.push_back(MakeBlocker(
                    "foundation.workspace.mono-loader",
                    "error",
                    "game-profile",
                    activeProfile->m_profileId,
                    "Mono profiles require an exact BepInEx version and plugin path.",
                    { "build", "deploy", "runtime_handoff" }));
            }

            if (activeProfile->m_diagnosticsPath.empty() || activeProfile->m_extractedDataPath.empty())
            {
                blockers.push_back(MakeBlocker(
                    "foundation.workspace.research-paths",
                    "warning",
                    "game-profile",
                    activeProfile->m_profileId,
                    "Configure diagnostic and extracted-data locations before source intake.",
                    { "source_intake" }));
            }
        }

        if (packs.empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.pack.none",
                "error",
                "pack-manifest",
                "pack:active",
                "Create or open a pack manifest so every authored record has an owner."));
        }
        else
        {
            for (const PackManifest& pack : packs)
            {
                const AZStd::string subject = pack.m_packId.empty() ? AZStd::string("pack:unnamed") : pack.m_packId;
                if (!pack.HasStableIdentity())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.identity." + subject,
                        "error",
                        "pack-manifest",
                        subject,
                        "Pack ID must be namespaced and lowercase, owner ID is required, and version must use semantic versioning."));
                }

                if (pack.m_targetGameVersion.empty() || pack.m_targetBranch.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.game-target." + subject,
                        "error",
                        "pack-manifest",
                        subject,
                        "Declare the exact primary FoA game version and branch targeted by the pack.",
                        { "build", "package", "deploy" }));
                }
                else if (activeProfile)
                {
                    const bool versionCompatible = pack.m_targetGameVersion == activeProfile->m_gameVersion
                        || Contains(pack.m_compatibleGameVersions, activeProfile->m_gameVersion);
                    if (!versionCompatible || pack.m_targetBranch != activeProfile->m_branch)
                    {
                        blockers.push_back(MakeBlocker(
                            "foundation.pack.profile-mismatch." + subject,
                            "error",
                            "compatibility",
                            subject,
                            "The active game profile is outside the pack's declared game-version or branch compatibility.",
                            { "build", "package", "deploy", "runtime_handoff" }));
                    }
                }

                if (pack.m_requiredCoreVersion.empty() || pack.m_requiredAdapterVersion.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.sdk-compatibility." + subject,
                        "error",
                        "compatibility",
                        subject,
                        "Declare compatible Avalon Core and FoA adapter versions before release packaging.",
                        { "package", "release", "runtime_handoff" }));
                }

                if (!IsKnownSaveImpact(pack.m_saveImpact))
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.save-impact." + subject,
                        "error",
                        "save-impact",
                        subject,
                        "Save impact must be one of: none, compatible, migration, destructive, or unknown."));
                }
                else if (pack.m_saveImpact == "unknown")
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.save-impact-unknown." + subject,
                        "warning",
                        "save-impact",
                        subject,
                        "Save impact is explicitly unknown and must be resolved before a release is approved.",
                        { "release" }));
                }

                for (const AZStd::string& dependency : pack.m_dependencies)
                {
                    if (Contains(pack.m_incompatibilities, dependency))
                    {
                        blockers.push_back(MakeBlocker(
                            "foundation.pack.dependency-conflict." + subject + "." + dependency,
                            "error",
                            "dependencies",
                            subject,
                            "The same pack is declared as both a dependency and an incompatibility: " + dependency));
                    }
                }
                for (const AZStd::string& requiredMod : pack.m_requiredMods)
                {
                    if (Contains(pack.m_incompatibilities, requiredMod))
                    {
                        blockers.push_back(MakeBlocker(
                            "foundation.pack.required-mod-conflict." + subject + "." + requiredMod,
                            "error",
                            "dependencies",
                            subject,
                            "The same mod is declared as both required and incompatible: " + requiredMod));
                    }
                }

                if (pack.m_contentDefinitionPaths.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.content-empty." + subject,
                        "warning",
                        "content",
                        subject,
                        "No content-definition documents are declared for this pack."));
                }
                if (pack.m_assetPaths.empty() && pack.m_localisationPaths.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.resources-empty." + subject,
                        "warning",
                        "resources",
                        subject,
                        "The pack declares no asset or localisation resources."));
                }
                if (pack.m_buildConfiguration.empty() || pack.m_releaseChannel.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.release-config." + subject,
                        "warning",
                        "release",
                        subject,
                        "Declare a build configuration and release channel before packaging."));
                }
                if (pack.m_runtimeActionsEnabled)
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.runtime-disabled." + subject,
                        "error",
                        "runtime-boundary",
                        subject,
                        "Runtime actions are not available in the editor foundation milestone.",
                        { "all_runtime_actions" }));
                }
            }
        }

        if (sourceRegistry.GetSources().empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.sources.empty",
                "warning",
                "source-registry",
                "source-set:workspace",
                "No registered research, diagnostic, or extracted source artifacts are available."));
        }

        if (!sourceRegistry.GetSources().empty() && sourceRegistry.GetEvidence().empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.evidence.empty",
                "warning",
                "evidence-registry",
                "evidence-set:workspace",
                "Registered sources have not yet been decomposed into evidence records."));
        }

        if (catalog.GetRecords().empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.catalog.empty",
                "warning",
                "catalog",
                "catalog:workspace",
                "The catalog contains no reviewed or research-stage game-knowledge records."));
        }

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (record.m_identityKind == "native" && record.m_nativeRefExact.empty())
            {
                AZStd::string blockerId = "catalog.native-ref.";
                blockerId += record.m_recordId;
                blockers.push_back(MakeBlocker(
                    AZStd::move(blockerId),
                    "error",
                    record.m_domain,
                    record.m_subjectRef,
                    "A native record must preserve its exact game-facing reference."));
            }

            if (record.m_evidenceIds.empty())
            {
                AZStd::string blockerId = "catalog.evidence.";
                blockerId += record.m_recordId;
                blockers.push_back(MakeBlocker(
                    AZStd::move(blockerId),
                    "warning",
                    record.m_domain,
                    record.m_subjectRef,
                    "The record has no linked evidence and remains research-only."));
            }

            for (const AZStd::string& missingRef : record.m_missingRefs)
            {
                AZStd::string blockerId = "catalog.missing-ref.";
                blockerId += record.m_recordId;
                blockerId += ".";
                blockerId += missingRef;
                AZStd::string reason = "Required reference is unresolved: ";
                reason += missingRef;
                blockers.push_back(MakeBlocker(
                    AZStd::move(blockerId),
                    "error",
                    record.m_domain,
                    record.m_subjectRef,
                    AZStd::move(reason),
                    record.m_allowedUsages));
            }
        }

        return blockers;
    }

    BlockerRecord FoundationValidationService::MakeBlocker(
        AZStd::string blockerId,
        AZStd::string severity,
        AZStd::string area,
        AZStd::string subjectRef,
        AZStd::string reason,
        AZStd::vector<AZStd::string> affectedUsages)
    {
        BlockerRecord blocker;
        blocker.m_blockerId = AZStd::move(blockerId);
        blocker.m_severity = AZStd::move(severity);
        blocker.m_area = AZStd::move(area);
        blocker.m_subjectRef = AZStd::move(subjectRef);
        blocker.m_reason = AZStd::move(reason);
        blocker.m_status = "open";
        blocker.m_affectedUsages = AZStd::move(affectedUsages);
        return blocker;
    }
} // namespace TaintedGrailModdingSDK
