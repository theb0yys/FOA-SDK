/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationValidationService.h"

#include "PathPolicyService.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        bool IsKnownSaveImpact(const AZStd::string& value)
        {
            return value == "none"
                || value == "compatible"
                || value == "migration"
                || value == "destructive"
                || value == "unknown";
        }

        bool HasLoadedPack(
            const AZStd::vector<PackManifest>& packs,
            const AZStd::string& packId)
        {
            for (const PackManifest& pack : packs)
            {
                if (pack.m_packId == packId)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    AZStd::vector<BlockerRecord> FoundationValidationService::Evaluate(
        const WorkspaceModel& workspace,
        const AZStd::vector<PackManifest>& packs,
        const SourceEvidenceRegistry& sourceRegistry,
        const AZStd::vector<ImportIssue>& importIssues,
        const CatalogDatabase& catalog) const
    {
        AZStd::vector<BlockerRecord> blockers;

        if (!IsStableContractId(workspace.m_workspaceId)
            || workspace.m_rootPath.empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.root",
                "error",
                "workspace",
                "workspace:active",
                "Configure a bounded stable workspace ID and canonical existing workspace root before importing or authoring data."));
        }

        PathPolicyService pathPolicy;
        auto root = pathPolicy.ResolveWorkspaceRoot(workspace, {}, true);
        if (!root.IsSuccess())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.root-resolution",
                "error",
                "workspace",
                "workspace:active",
                AZStd::string("Workspace root resolution failed: ")
                    + root.GetError(),
                { "source_intake", "generate", "package", "deploy" }));
        }
        else
        {
            auto pathValidation = pathPolicy.ValidateWorkspacePaths(
                workspace,
                root.GetValue());
            if (!pathValidation.IsSuccess())
            {
                blockers.push_back(MakeBlocker(
                    "foundation.workspace.path-policy",
                    "error",
                    "workspace",
                    "workspace:active",
                    AZStd::string(
                        "Workspace/profile paths failed canonical directory, type, writability, separation, or storage-identity validation: ")
                        + pathValidation.GetError(),
                    { "source_intake", "generate", "package", "deploy", "runtime_handoff" }));
            }
        }

        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile || !activeProfile->IsConfigured())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.game-profile",
                "error",
                "game-profile",
                "game-profile:active",
                "Configure a stable active profile with exact version tokens, Mono/IL2CPP target, Unity context, managed assemblies, and target-appropriate loader paths before accepting evidence.",
                { "source_intake", "catalog_promotion", "runtime_handoff" }));
        }
        else if (activeProfile->m_diagnosticsPath.empty()
            || activeProfile->m_extractedDataPath.empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.research-paths",
                "warning",
                "game-profile",
                activeProfile->m_profileId,
                "Configure dedicated contained writable diagnostic and extracted-data directories before source intake.",
                { "source_intake" }));
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
                const AZStd::string subject = pack.m_packId.empty()
                    ? AZStd::string("pack:unnamed")
                    : pack.m_packId;
                if (!pack.HasStableIdentity())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.identity." + subject,
                        "error",
                        "pack-manifest",
                        subject,
                        "Pack identity must be bounded and namespaced, owner identity must be safe, and version must satisfy strict SemVer including prerelease/build identifier rules."));
                }
                if (pack.m_targetGameVersion.empty()
                    || pack.m_targetBranch.empty())
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
                    const bool versionCompatible =
                        pack.m_targetGameVersion == activeProfile->m_gameVersion
                        || Contains(
                            pack.m_compatibleGameVersions,
                            activeProfile->m_gameVersion);
                    if (!versionCompatible
                        || pack.m_targetBranch != activeProfile->m_branch)
                    {
                        blockers.push_back(MakeBlocker(
                            "foundation.pack.profile-mismatch." + subject,
                            "error",
                            "compatibility",
                            subject,
                            "The active exact game profile is outside the pack's declared game-version or branch compatibility.",
                            { "build", "package", "deploy", "runtime_handoff" }));
                    }
                }
                if (pack.m_requiredCoreVersion.empty()
                    || pack.m_requiredAdapterVersion.empty())
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
                            "The same pack is declared as both a dependency and an incompatibility: "
                                + dependency));
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
                            "The same mod is declared as both required and incompatible: "
                                + requiredMod));
                    }
                }
                if (pack.m_runtimeActionsEnabled)
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.runtime-disabled." + subject,
                        "error",
                        "runtime-boundary",
                        subject,
                        "Runtime actions are not available in the editor-owned research foundation.",
                        { "all_runtime_actions" }));
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
                if (pack.m_buildConfiguration.empty()
                    || pack.m_releaseChannel.empty())
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.release-config." + subject,
                        "warning",
                        "release",
                        subject,
                        "Declare a build configuration and release channel before packaging."));
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
        for (const SourceRecord& source : sourceRegistry.GetSources())
        {
            if (!IsUsableImportStatus(source.m_importStatus)
                || !IsSha256Fingerprint(source.m_fingerprint)
                || !IsStrictUtcTimestamp(source.m_capturedAt)
                || !IsStrictUtcTimestamp(source.m_importedAt))
            {
                blockers.push_back(MakeBlocker(
                    "foundation.source.provenance." + source.m_sourceId,
                    "error",
                    "source-intake",
                    source.m_sourceId,
                    "The source lacks usable import status, exact SHA-256 fingerprint, or real UTC capture/import provenance.",
                    { "evidence_review", "catalog_promotion" }));
            }
            if (activeProfile
                && (source.m_profileId != activeProfile->m_profileId
                    || source.m_gameVersion != activeProfile->m_gameVersion
                    || source.m_branch != activeProfile->m_branch
                    || source.m_runtimeTarget != activeProfile->m_runtimeTarget))
            {
                blockers.push_back(MakeBlocker(
                    "foundation.source.profile-mismatch." + source.m_sourceId,
                    "error",
                    "source-intake",
                    source.m_sourceId,
                    "The source is bound to a different FoA profile, build, branch, or runtime target than the active workspace profile.",
                    { "evidence_review", "catalog_promotion" }));
            }
        }
        for (const EvidenceRecord& evidence : sourceRegistry.GetEvidence())
        {
            const SourceRecord* source =
                sourceRegistry.FindSource(evidence.m_sourceId);
            if (!source
                || !IsUsableImportStatus(source->m_importStatus)
                || evidence.m_sourceFingerprint != source->m_fingerprint
                || evidence.m_subjectRef.empty()
                || evidence.m_claim.empty()
                || evidence.m_evidenceKind.empty()
                || evidence.m_confidence.empty()
                || evidence.m_locator.empty()
                || evidence.m_recordPath.empty()
                || !IsStrictUtcTimestamp(evidence.m_extractedAt))
            {
                blockers.push_back(MakeBlocker(
                    "foundation.evidence.provenance." + evidence.m_evidenceId,
                    "error",
                    "evidence-registry",
                    evidence.m_subjectRef.empty()
                        ? AZStd::string("evidence:unknown")
                        : evidence.m_subjectRef,
                    "Evidence lacks exact usable-source binding and complete claim/kind/confidence/locator/record-path/UTC provenance.",
                    { "catalog_promotion", "validation", "permission" }));
            }
        }
        if (!sourceRegistry.GetSources().empty()
            && sourceRegistry.GetEvidence().empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.evidence.empty",
                "warning",
                "evidence-registry",
                "evidence-set:workspace",
                "Registered sources have not yet been decomposed into evidence records."));
        }

        for (const ImportIssue& issue : importIssues)
        {
            if (issue.m_severity != "error"
                && issue.m_severity != "warning")
            {
                continue;
            }
            blockers.push_back(MakeBlocker(
                "foundation.import." + issue.m_issueId,
                issue.m_severity,
                "source-intake",
                issue.m_locator.empty()
                    ? AZStd::string("source:unknown")
                    : issue.m_locator,
                issue.m_code + ": " + issue.m_message,
                issue.m_severity == "error"
                    ? AZStd::vector<AZStd::string>{
                        "evidence_review", "catalog_promotion" }
                    : AZStd::vector<AZStd::string>{}));
        }

        if (catalog.GetRecords().empty())
        {
            blockers.push_back(MakeBlocker(
                "foundation.catalog.empty",
                "warning",
                "catalog",
                "catalog:workspace",
                "The canonical catalog contains no reviewed game-knowledge records."));
        }
        else if (activeProfile)
        {
            AZStd::string integrityError;
            if (!catalog.ValidateIntegrity(
                    workspace,
                    *activeProfile,
                    sourceRegistry,
                    &integrityError))
            {
                blockers.push_back(MakeBlocker(
                    "foundation.catalog.integrity",
                    "error",
                    "catalog",
                    "catalog:workspace",
                    "The complete catalog graph/history failed active-workspace, exact-subject evidence, chronology, current-permission, or economy-association validation: "
                        + integrityError,
                    { "catalog_use", "validation", "permission", "runtime_handoff" }));
            }
        }

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            const AZStd::string subject = record.m_subjectRef.empty()
                ? record.m_recordId
                : record.m_subjectRef;
            if (record.IsSynthetic()
                && !HasLoadedPack(packs, record.m_ownerPackId))
            {
                blockers.push_back(MakeBlocker(
                    "catalog.owner-missing." + record.m_recordId,
                    "error",
                    "catalog-identity",
                    subject,
                    "The synthetic record's owning pack is not loaded in the active workspace."));
            }
            if (record.m_validationState.empty()
                || record.m_validationState == "unvalidated")
            {
                blockers.push_back(MakeBlocker(
                    "catalog.unvalidated." + record.m_recordId,
                    "warning",
                    "catalog-validation",
                    subject,
                    "The record is reviewed but has not passed a usage-specific validation.",
                    { "validated_runtime_use" }));
            }
            for (const AZStd::string& conflictRef : record.m_conflictRefs)
            {
                blockers.push_back(MakeBlocker(
                    "catalog.conflict." + record.m_recordId + "." + conflictRef,
                    "error",
                    "catalog-conflict",
                    subject,
                    "Catalog conflict remains unresolved: " + conflictRef,
                    record.m_allowedUsages));
            }
            for (const AZStd::string& missingRef : record.m_missingRefs)
            {
                blockers.push_back(MakeBlocker(
                    "catalog.missing-ref." + record.m_recordId + "." + missingRef,
                    "error",
                    record.m_domain,
                    subject,
                    "Required reference is unresolved: " + missingRef,
                    record.m_allowedUsages));
            }
        }
        for (const CatalogRelationship& relationship : catalog.GetRelationships())
        {
            if (relationship.m_validationState.empty()
                || relationship.m_validationState == "unvalidated")
            {
                blockers.push_back(MakeBlocker(
                    "catalog.relationship-unvalidated."
                        + relationship.m_relationshipId,
                    "warning",
                    "catalog-relationship",
                    "relationship:" + relationship.m_relationshipId,
                    "The exact association has not passed validation."));
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
