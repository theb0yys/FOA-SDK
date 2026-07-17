/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceBlockerService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        const CatalogGovernanceEvent* FindLatestPermissionEvent(
            const CatalogDatabase& catalog,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& usage)
        {
            const CatalogGovernanceEvent* latest = nullptr;
            for (const CatalogGovernanceEvent& event : catalog.GetGovernanceHistory())
            {
                if (event.m_subjectKind == subjectKind && event.m_subjectId == subjectId
                    && event.m_axis == "permission" && event.m_usage == usage)
                {
                    latest = &event;
                }
            }
            return latest;
        }

        bool HasValidatedBasis(const CatalogDatabase& catalog, const CatalogGovernanceEvent& event)
        {
            for (const AZStd::string& validationId : event.m_validationIds)
            {
                const CatalogValidationEvent* validation = catalog.FindValidationById(validationId);
                if (validation && validation->m_state == "validated"
                    && validation->GetSubjectKind() == event.m_subjectKind
                    && validation->GetSubjectId() == event.m_subjectId)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsKnownAxis(const AZStd::string& axis)
        {
            return axis == "maturity" || axis == "confidence" || axis == "operational_risk"
                || axis == "staleness" || axis == "permission" || axis == "supersession";
        }
    } // namespace

    AZStd::vector<BlockerRecord> CatalogGovernanceBlockerService::Evaluate(
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        AZStd::vector<BlockerRecord> blockers;
        const GameProfile* profile = workspace.FindActiveGameProfile();

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            const AZStd::string subject = record.m_subjectRef.empty() ? record.m_recordId : record.m_subjectRef;
            if (record.m_researchStage.empty() || record.m_researchStage == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.maturity." + record.m_recordId,
                    "warning",
                    subject,
                    "The record has no reviewed maturity stage."));
            }
            if (record.m_confidence.empty() || record.m_confidence == "unknown" || record.m_confidence == "unrated")
            {
                blockers.push_back(MakeBlocker(
                    "governance.confidence." + record.m_recordId,
                    "warning",
                    subject,
                    "The record confidence remains unknown or unrated."));
            }
            if (record.m_operationalRisk.empty() || record.m_operationalRisk == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.risk." + record.m_recordId,
                    "warning",
                    subject,
                    "The record operational risk has not been assessed."));
            }
            if (record.m_stalenessState.empty() || record.m_stalenessState == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.staleness-unknown." + record.m_recordId,
                    "warning",
                    subject,
                    "The record has not been assessed against the active game build for staleness."));
            }
            else if (record.m_stalenessState == "potentially_stale" || record.m_stalenessState == "stale")
            {
                blockers.push_back(MakeBlocker(
                    "governance.staleness." + record.m_recordId,
                    record.m_stalenessState == "stale" ? "error" : "warning",
                    subject,
                    "The record is not current for unrestricted use.",
                    record.m_allowedUsages));
            }

            for (const AZStd::string& usage : record.m_allowedUsages)
            {
                const CatalogGovernanceEvent* event = FindLatestPermissionEvent(catalog, "record", record.m_recordId, usage);
                if (!event || event->m_newValue != "allow" || event->m_evidenceIds.empty()
                    || !HasValidatedBasis(catalog, *event))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.permission-proof." + record.m_recordId + "." + usage,
                        "error",
                        subject,
                        "Allowed usage has no current reviewed permission event backed by evidence and validated proof: " + usage,
                        { usage }));
                }
            }
            if (!record.m_supersededByRecordId.empty() && !record.m_allowedUsages.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.superseded-permission." + record.m_recordId,
                    "error",
                    subject,
                    "A superseded record must not retain allowed usages.",
                    record.m_allowedUsages));
            }
        }

        for (const CatalogRelationship& relationship : catalog.GetRelationships())
        {
            const AZStd::string subject = relationship.m_relationshipId;
            if (relationship.m_researchStage.empty() || relationship.m_researchStage == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-maturity." + subject,
                    "warning",
                    subject,
                    "The relationship has no reviewed maturity stage."));
            }
            if (relationship.m_confidence.empty() || relationship.m_confidence == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-confidence." + subject,
                    "warning",
                    subject,
                    "The relationship confidence remains unknown."));
            }
            if (relationship.m_operationalRisk.empty() || relationship.m_operationalRisk == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-risk." + subject,
                    "warning",
                    subject,
                    "The relationship operational risk has not been assessed."));
            }
            if (relationship.m_stalenessState.empty() || relationship.m_stalenessState == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-staleness-unknown." + subject,
                    "warning",
                    subject,
                    "The relationship has not been assessed for staleness."));
            }
            else if (relationship.m_stalenessState == "potentially_stale" || relationship.m_stalenessState == "stale")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-staleness." + subject,
                    relationship.m_stalenessState == "stale" ? "error" : "warning",
                    subject,
                    "The relationship is not current for unrestricted use.",
                    relationship.m_allowedUsages));
            }
            for (const AZStd::string& usage : relationship.m_allowedUsages)
            {
                const CatalogGovernanceEvent* event = FindLatestPermissionEvent(catalog, "relationship", subject, usage);
                if (!event || event->m_newValue != "allow" || event->m_evidenceIds.empty()
                    || !HasValidatedBasis(catalog, *event))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.relationship-permission-proof." + subject + "." + usage,
                        "error",
                        subject,
                        "Allowed relationship usage has no reviewed evidence-backed validated proof: " + usage,
                        { usage }));
                }
            }
        }

        for (const CatalogValidationEvent& validation : catalog.GetValidationHistory())
        {
            const AZStd::string subjectId = validation.GetSubjectId();
            if (validation.m_validator.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-reviewer." + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation history has no named validator."));
            }
            if (validation.m_evidenceIds.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-evidence-empty." + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation history has no evidence IDs."));
            }
            for (const AZStd::string& evidenceId : validation.m_evidenceIds)
            {
                if (!sourceRegistry.FindEvidence(evidenceId))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.validation-evidence." + validation.m_validationId + "." + evidenceId,
                        "error",
                        subjectId,
                        "Validation history references missing evidence: " + evidenceId));
                }
            }
            if (profile && (validation.m_profileId != profile->m_profileId
                || validation.m_gameVersion != profile->m_gameVersion
                || (!validation.m_branch.empty() && validation.m_branch != profile->m_branch)))
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-profile." + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation proof belongs to a different active profile, game version, or branch."));
            }
        }

        for (const CatalogGovernanceEvent& event : catalog.GetGovernanceHistory())
        {
            if (!IsKnownAxis(event.m_axis))
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-axis." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history contains an unsupported axis: " + event.m_axis));
            }
            if (event.m_reviewer.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-reviewer." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history has no named reviewer."));
            }
            const bool evidenceOptional = event.m_axis == "permission" && event.m_newValue == "clear";
            if (event.m_evidenceIds.empty() && !evidenceOptional)
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-evidence-empty." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history has no evidence IDs."));
            }
            for (const AZStd::string& evidenceId : event.m_evidenceIds)
            {
                if (!sourceRegistry.FindEvidence(evidenceId))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.event-evidence." + event.m_eventId + "." + evidenceId,
                        "error",
                        event.m_subjectId,
                        "Governance history references missing evidence: " + evidenceId));
                }
            }
            for (const AZStd::string& validationId : event.m_validationIds)
            {
                if (!catalog.FindValidationById(validationId))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.event-validation." + event.m_eventId + "." + validationId,
                        "error",
                        event.m_subjectId,
                        "Governance history references missing validation proof: " + validationId));
                }
            }
        }

        return blockers;
    }

    BlockerRecord CatalogGovernanceBlockerService::MakeBlocker(
        AZStd::string blockerId,
        AZStd::string severity,
        AZStd::string subjectRef,
        AZStd::string reason,
        AZStd::vector<AZStd::string> affectedUsages)
    {
        BlockerRecord blocker;
        blocker.m_blockerId = AZStd::move(blockerId);
        blocker.m_severity = AZStd::move(severity);
        blocker.m_area = "catalog-governance";
        blocker.m_subjectRef = AZStd::move(subjectRef);
        blocker.m_reason = AZStd::move(reason);
        blocker.m_status = "open";
        blocker.m_affectedUsages = AZStd::move(affectedUsages);
        return blocker;
    }
} // namespace TaintedGrailModdingSDK
