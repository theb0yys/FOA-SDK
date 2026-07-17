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

        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile || !activeProfile->IsConfigured())
        {
            blockers.push_back(MakeBlocker(
                "foundation.workspace.game-profile",
                "error",
                "game-profile",
                "game-profile:active",
                "Configure an active FoA installation, exact game version, and branch before accepting evidence.",
                { "source_intake", "catalog_promotion", "runtime_handoff" }));
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
                if (!pack.HasStableIdentity())
                {
                    AZStd::string blockerId = "foundation.pack.identity.";
                    blockerId += pack.m_displayName.empty() ? "unnamed" : pack.m_displayName;
                    blockers.push_back(MakeBlocker(
                        AZStd::move(blockerId),
                        "error",
                        "pack-manifest",
                        pack.m_packId,
                        "Pack ID, owner ID, and semantic version are required before records can be promoted."));
                }
                if (pack.m_runtimeActionsEnabled)
                {
                    blockers.push_back(MakeBlocker(
                        "foundation.pack.runtime-disabled",
                        "error",
                        "runtime-boundary",
                        pack.m_packId,
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
