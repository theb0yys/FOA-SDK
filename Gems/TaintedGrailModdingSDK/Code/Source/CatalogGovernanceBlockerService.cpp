/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceBlockerService.h"

#include "CatalogGovernanceTypes.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
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

        AZStd::string ExactSubjectRef(
            const CatalogDatabase& catalog,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId)
        {
            if (subjectKind == "record")
            {
                const CatalogRecord* record = catalog.FindByRecordId(subjectId);
                return record ? record->m_subjectRef : AZStd::string{};
            }
            if (subjectKind == "relationship"
                && catalog.FindRelationshipById(subjectId))
            {
                return "relationship:" + subjectId;
            }
            return {};
        }

        bool EvidenceProvesSubject(
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& evidenceId)
        {
            const GameProfile* profile = workspace.FindActiveGameProfile();
            const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
            if (!profile || !evidence)
            {
                return false;
            }
            const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
            return source
                && IsUsableImportStatus(source->m_importStatus)
                && evidence->m_sourceFingerprint == source->m_fingerprint
                && IsSha256Fingerprint(evidence->m_sourceFingerprint)
                && evidence->m_profileId == profile->m_profileId
                && evidence->m_gameVersion == profile->m_gameVersion
                && evidence->m_branch == profile->m_branch
                && source->m_runtimeTarget == profile->m_runtimeTarget
                && evidence->m_subjectRef
                    == ExactSubjectRef(catalog, subjectKind, subjectId)
                && !evidence->m_claim.empty()
                && !evidence->m_evidenceKind.empty();
        }

        bool HasValidatedBasis(
            const WorkspaceModel& workspace,
            const CatalogDatabase& catalog,
            const CatalogGovernanceEvent& event)
        {
            const GameProfile* profile = workspace.FindActiveGameProfile();
            if (!profile || event.m_validationIds.empty())
            {
                return false;
            }
            for (const AZStd::string& validationId : event.m_validationIds)
            {
                const CatalogValidationEvent* validation =
                    catalog.FindValidationById(validationId);
                if (!validation
                    || validation->m_state != "validated"
                    || validation->GetSubjectKind() != event.m_subjectKind
                    || validation->GetSubjectId() != event.m_subjectId
                    || validation->m_profileId != profile->m_profileId
                    || validation->m_gameVersion != profile->m_gameVersion
                    || validation->m_branch != profile->m_branch
                    || !IsStrictUtcTimestamp(validation->m_checkedAt))
                {
                    return false;
                }
            }
            const CatalogValidationEvent* latest =
                catalog.FindLatestValidationForSubject(
                    event.m_subjectKind,
                    event.m_subjectId);
            return latest
                && latest->m_state == "validated"
                && Contains(event.m_validationIds, latest->m_validationId);
        }

        bool EventEvidenceProvesSubject(
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const CatalogGovernanceEvent& event)
        {
            const bool optional = event.m_axis == "permission"
                && event.m_newValue == "clear";
            if (event.m_evidenceIds.empty())
            {
                return optional;
            }
            for (const AZStd::string& evidenceId : event.m_evidenceIds)
            {
                if (!EvidenceProvesSubject(
                        workspace,
                        sourceRegistry,
                        catalog,
                        event.m_subjectKind,
                        event.m_subjectId,
                        evidenceId))
                {
                    return false;
                }
            }
            return true;
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
            const AZStd::string subject = record.m_subjectRef.empty()
                ? record.m_recordId
                : record.m_subjectRef;
            if (record.m_researchStage.empty()
                || record.m_researchStage == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.maturity." + record.m_recordId,
                    "warning",
                    subject,
                    "The record has no reviewed maturity stage."));
            }
            if (record.m_confidence.empty()
                || record.m_confidence == "unknown"
                || record.m_confidence == "unrated")
            {
                blockers.push_back(MakeBlocker(
                    "governance.confidence." + record.m_recordId,
                    "warning",
                    subject,
                    "The record confidence remains unknown or unrated."));
            }
            if (record.m_operationalRisk.empty()
                || record.m_operationalRisk == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.risk." + record.m_recordId,
                    "warning",
                    subject,
                    "The record operational risk has not been assessed."));
            }
            if (record.m_stalenessState.empty()
                || record.m_stalenessState == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.staleness-unknown." + record.m_recordId,
                    "warning",
                    subject,
                    "The record has not been assessed against the active game build for staleness."));
            }
            else if (record.m_stalenessState == "potentially_stale"
                || record.m_stalenessState == "stale")
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
                const CatalogGovernanceEvent* event =
                    catalog.FindEffectiveGovernanceEvent(
                        "record",
                        record.m_recordId,
                        "permission",
                        usage);
                if (!event
                    || event->m_newValue != "allow"
                    || !IsStrictUtcTimestamp(event->m_decidedAt)
                    || !EventEvidenceProvesSubject(
                        workspace,
                        sourceRegistry,
                        catalog,
                        *event)
                    || !HasValidatedBasis(workspace, catalog, *event))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.permission-proof." + record.m_recordId
                            + "." + usage,
                        "error",
                        subject,
                        "Allowed usage has no current chronological permission event backed by exact-subject evidence and the effective validated proof: "
                            + usage,
                        { usage }));
                }
            }
            if (!record.m_supersededByRecordId.empty()
                && !record.m_allowedUsages.empty())
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
            if (relationship.m_researchStage.empty()
                || relationship.m_researchStage == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-maturity." + subject,
                    "warning",
                    subject,
                    "The relationship has no reviewed maturity stage."));
            }
            if (relationship.m_confidence.empty()
                || relationship.m_confidence == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-confidence." + subject,
                    "warning",
                    subject,
                    "The relationship confidence remains unknown."));
            }
            if (relationship.m_operationalRisk.empty()
                || relationship.m_operationalRisk == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-risk." + subject,
                    "warning",
                    subject,
                    "The relationship operational risk has not been assessed."));
            }
            if (relationship.m_stalenessState.empty()
                || relationship.m_stalenessState == "unknown")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-staleness-unknown." + subject,
                    "warning",
                    subject,
                    "The relationship has not been assessed for staleness."));
            }
            else if (relationship.m_stalenessState == "potentially_stale"
                || relationship.m_stalenessState == "stale")
            {
                blockers.push_back(MakeBlocker(
                    "governance.relationship-staleness." + subject,
                    relationship.m_stalenessState == "stale"
                        ? "error"
                        : "warning",
                    subject,
                    "The relationship is not current for unrestricted use.",
                    relationship.m_allowedUsages));
            }
            for (const AZStd::string& usage : relationship.m_allowedUsages)
            {
                const CatalogGovernanceEvent* event =
                    catalog.FindEffectiveGovernanceEvent(
                        "relationship",
                        subject,
                        "permission",
                        usage);
                if (!event
                    || event->m_newValue != "allow"
                    || !IsStrictUtcTimestamp(event->m_decidedAt)
                    || !EventEvidenceProvesSubject(
                        workspace,
                        sourceRegistry,
                        catalog,
                        *event)
                    || !HasValidatedBasis(workspace, catalog, *event))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.relationship-permission-proof." + subject
                            + "." + usage,
                        "error",
                        subject,
                        "Allowed relationship usage has no current exact-association evidence and effective validated proof: "
                            + usage,
                        { usage }));
                }
            }
        }

        for (const CatalogValidationEvent& validation :
             catalog.GetValidationHistory())
        {
            const AZStd::string subjectId = validation.GetSubjectId();
            if (validation.m_validator.empty()
                || !IsStrictUtcTimestamp(validation.m_checkedAt))
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-shape." + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation history requires a named validator and a real UTC date/time."));
            }
            if (validation.m_evidenceIds.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-evidence-empty."
                        + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation history has no evidence IDs."));
            }
            for (const AZStd::string& evidenceId : validation.m_evidenceIds)
            {
                if (!EvidenceProvesSubject(
                        workspace,
                        sourceRegistry,
                        catalog,
                        validation.GetSubjectKind(),
                        subjectId,
                        evidenceId))
                {
                    blockers.push_back(MakeBlocker(
                        "governance.validation-evidence."
                            + validation.m_validationId + "." + evidenceId,
                        "error",
                        subjectId,
                        "Validation history evidence does not prove the exact active-profile subject: "
                            + evidenceId));
                }
            }
            if (profile
                && (validation.m_profileId != profile->m_profileId
                    || validation.m_gameVersion != profile->m_gameVersion
                    || validation.m_branch != profile->m_branch))
            {
                blockers.push_back(MakeBlocker(
                    "governance.validation-profile."
                        + validation.m_validationId,
                    "error",
                    subjectId,
                    "Validation proof belongs to a different active profile, game version, or branch."));
            }
        }

        for (const CatalogGovernanceEvent& event :
             catalog.GetGovernanceHistory())
        {
            if (!ParseGovernanceAxis(event.m_axis).IsSuccess()
                || !IsStrictUtcTimestamp(event.m_decidedAt))
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-shape." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history contains an unsupported axis or impossible decision time."));
            }
            if (event.m_reviewer.empty())
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-reviewer." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history has no named reviewer."));
            }
            if (!EventEvidenceProvesSubject(
                    workspace,
                    sourceRegistry,
                    catalog,
                    event))
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-evidence." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history is not backed by exact-subject active-profile evidence."));
            }
            if (!event.m_validationIds.empty()
                && !HasValidatedBasis(workspace, catalog, event))
            {
                blockers.push_back(MakeBlocker(
                    "governance.event-validation." + event.m_eventId,
                    "error",
                    event.m_subjectId,
                    "Governance history validation references must all be exact-subject validated events and include the current effective validation."));
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
