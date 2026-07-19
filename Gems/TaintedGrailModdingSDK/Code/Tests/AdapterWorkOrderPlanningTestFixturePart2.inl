/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace
{
        CatalogRecord MakeRecord(
            AZStd::string recordId,
            AZStd::string subjectRef,
            AZStd::string recordKind,
            AZStd::string identityKind,
            AZStd::vector<AZStd::string> allowedUsages)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_ownerPackId = "owner.work-order-pack";
            record.m_domain = "economy";
            record.m_recordKind = AZStd::move(recordKind);
            record.m_subjectRef = AZStd::move(subjectRef);
            record.m_identityKind = AZStd::move(identityKind);
            record.m_nativeRefExact = record.IsSynthetic()
                ? AZStd::string{}
                : "native:" + record.m_recordId;
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "validated";
            record.m_stalenessState = "current";
            record.m_allowedUsages = AZStd::move(allowedUsages);
            record.m_evidenceIds = { "evidence.record." + record.m_recordId };
            return record;
        }

        CatalogValidationEvent MakeValidation(
            AZStd::string validationId,
            AZStd::string subjectKind,
            AZStd::string subjectId,
            AZStd::string evidenceId)
        {
            CatalogValidationEvent validation;
            validation.m_validationId = AZStd::move(validationId);
            validation.m_subjectKind = AZStd::move(subjectKind);
            validation.m_subjectId = AZStd::move(subjectId);
            validation.m_state = "validated";
            validation.m_method = "deterministic-work-order-test";
            validation.m_validator = "test.validator";
            validation.m_checkedAt = "2026-01-01T00:00:00Z";
            validation.m_profileId = "profile.work-order";
            validation.m_gameVersion = "game.work-order";
            validation.m_branch = "branch.work-order";
            validation.m_evidenceIds = { AZStd::move(evidenceId) };
            return validation;
        }

        CatalogGovernanceEvent MakePermission(
            AZStd::string eventId,
            const CatalogRecord& record,
            AZStd::string usage)
        {
            CatalogGovernanceEvent event;
            event.m_eventId = AZStd::move(eventId);
            event.m_subjectKind = "record";
            event.m_subjectId = record.m_recordId;
            event.m_axis = "permission";
            event.m_newValue = "allow";
            event.m_usage = AZStd::move(usage);
            event.m_evidenceIds = record.m_evidenceIds;
            event.m_validationIds = { "validation.record." + record.m_recordId };
            event.m_reviewer = "test.reviewer";
            event.m_decidedAt = "2026-01-01T00:00:00Z";
            return event;
        }

        CatalogRelationship MakeRelationship(
            AZStd::string relationshipId,
            const CatalogRecord& source,
            const CatalogRecord& target,
            AZStd::string relationshipKind)
        {
            CatalogRelationship relationship;
            relationship.m_relationshipId = AZStd::move(relationshipId);
            relationship.m_fromRecordId = source.m_recordId;
            relationship.m_toRecordId = target.m_recordId;
            relationship.m_relationshipKind = AZStd::move(relationshipKind);
            relationship.m_evidenceIds = {
                "evidence.relationship." + relationship.m_relationshipId,
            };
            relationship.m_researchStage = "S5";
            relationship.m_confidence = "documented";
            relationship.m_operationalRisk = "low";
            relationship.m_validationState = "validated";
            relationship.m_stalenessState = "current";
            return relationship;
        }

        void RegisterEvidence(
            SourceEvidenceRegistry& registry,
            const EvidenceRecord& evidence)
        {
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterEvidence(evidence, &error)) << error.c_str();
        }

} // namespace
