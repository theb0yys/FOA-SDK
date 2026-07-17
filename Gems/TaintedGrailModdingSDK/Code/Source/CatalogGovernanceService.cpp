/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void AddUnique(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        void RemoveValue(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            values.erase(AZStd::remove(values.begin(), values.end(), value), values.end());
        }

        bool IsKnownMaturity(const AZStd::string& value)
        {
            return value == "S0" || value == "S1" || value == "S2" || value == "S3" || value == "S4"
                || value == "S5" || value == "S6" || value == "S7" || value == "S8"
                || value == "reviewed" || value == "reconciled" || value == "validated"
                || value == "authoring_ready" || value == "runtime_approved";
        }

        bool IsKnownConfidence(const AZStd::string& value)
        {
            return value == "unknown" || value == "hypothesis" || value == "inferred"
                || value == "documented" || value == "runtime_observed" || value == "validated";
        }

        bool IsKnownRisk(const AZStd::string& value)
        {
            return value == "unknown" || value == "low" || value == "medium"
                || value == "high" || value == "critical";
        }

        bool IsKnownStaleness(const AZStd::string& value)
        {
            return value == "unknown" || value == "current" || value == "potentially_stale" || value == "stale";
        }

        bool IsKnownValidation(const AZStd::string& value)
        {
            return value == "unvalidated" || value == "pending" || value == "validated"
                || value == "failed" || value == "stale" || value == "blocked";
        }

        AZStd::string NowIso()
        {
            return ToAzString(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        }

        QString Sanitize(const AZStd::string& value)
        {
            QString output = QString::fromUtf8(value.c_str()).toLower();
            for (qsizetype index = 0; index < output.size(); ++index)
            {
                const QChar character = output.at(index);
                if (!character.isLetterOrNumber() && character != '.' && character != '-' && character != '_')
                {
                    output[index] = '-';
                }
            }
            return output;
        }

        bool IsRecordReadyForPermission(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
        }

        bool IsRelationshipReadyForPermission(const CatalogRelationship& relationship)
        {
            return relationship.m_validationState == "validated"
                && relationship.m_stalenessState == "current"
                && relationship.m_missingRefs.empty()
                && relationship.m_conflictRefs.empty()
                && relationship.m_supersededByRelationshipId.empty();
        }
    } // namespace

    AZ::Outcome<CatalogGovernanceEvent, AZStd::string> CatalogGovernanceService::ApplyDecision(
        const CatalogGovernanceRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        CatalogDatabase& catalog) const
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact active game profile is required before governance decisions."));
        }
        if ((request.m_subjectKind != "record" && request.m_subjectKind != "relationship")
            || request.m_subjectId.empty() || request.m_axis.empty() || request.m_reviewer.empty())
        {
            return AZ::Failure(AZStd::string(
                "Governance decisions require record/relationship subject, subject ID, axis, and reviewer."));
        }

        const CatalogRecord* currentRecord = request.m_subjectKind == "record"
            ? catalog.FindByRecordId(request.m_subjectId)
            : nullptr;
        const CatalogRelationship* currentRelationship = request.m_subjectKind == "relationship"
            ? catalog.FindRelationshipById(request.m_subjectId)
            : nullptr;
        if (!currentRecord && !currentRelationship)
        {
            return AZ::Failure(AZStd::string("Governance decision references an unknown catalog subject."));
        }

        const bool clearingPermission = request.m_axis == "permission" && request.m_value == "clear";
        if (!clearingPermission && request.m_evidenceIds.empty())
        {
            return AZ::Failure(AZStd::string("Governance decisions require evidence IDs."));
        }
        AZStd::string evidenceError;
        if (!request.m_evidenceIds.empty()
            && !ValidateEvidence(
                request.m_evidenceIds,
                request.m_subjectKind,
                request.m_subjectId,
                workspace,
                sourceRegistry,
                catalog,
                evidenceError))
        {
            return AZ::Failure(evidenceError);
        }

        CatalogGovernanceEvent event;
        event.m_eventId = BuildEventId(
            "governance",
            request.m_subjectKind,
            request.m_subjectId,
            catalog.GetGovernanceHistory().size() + 1);
        event.m_subjectKind = request.m_subjectKind;
        event.m_subjectId = request.m_subjectId;
        event.m_axis = request.m_axis;
        event.m_newValue = request.m_value;
        event.m_usage = request.m_usage;
        event.m_evidenceIds = request.m_evidenceIds;
        event.m_validationIds = request.m_validationIds;
        event.m_reviewer = request.m_reviewer;
        event.m_decidedAt = NowIso();
        event.m_notes = request.m_notes;

        AZStd::string catalogError;
        if (currentRecord)
        {
            CatalogRecord updated = *currentRecord;
            if (request.m_axis == "maturity")
            {
                if (!IsKnownMaturity(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported maturity value."));
                }
                event.m_previousValue = updated.m_researchStage;
                updated.m_researchStage = request.m_value;
            }
            else if (request.m_axis == "confidence")
            {
                if (!IsKnownConfidence(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported confidence value."));
                }
                event.m_previousValue = updated.m_confidence;
                updated.m_confidence = request.m_value;
            }
            else if (request.m_axis == "operational_risk")
            {
                if (!IsKnownRisk(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported operational-risk value."));
                }
                event.m_previousValue = updated.m_operationalRisk;
                updated.m_operationalRisk = request.m_value;
            }
            else if (request.m_axis == "staleness")
            {
                if (!IsKnownStaleness(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported staleness value."));
                }
                event.m_previousValue = updated.m_stalenessState;
                updated.m_stalenessState = request.m_value;
                if (request.m_value == "current")
                {
                    RemoveValue(updated.m_forbiddenUsages, "stale_or_unverified");
                }
                else
                {
                    updated.m_allowedUsages.clear();
                    AddUnique(updated.m_forbiddenUsages, "stale_or_unverified");
                }
            }
            else if (request.m_axis == "permission")
            {
                if (request.m_usage.empty()
                    || (request.m_value != "allow" && request.m_value != "forbid" && request.m_value != "clear"))
                {
                    return AZ::Failure(AZStd::string("Permission decisions require usage and allow, forbid, or clear."));
                }
                event.m_previousValue = Contains(updated.m_allowedUsages, request.m_usage)
                    ? "allow"
                    : (Contains(updated.m_forbiddenUsages, request.m_usage) ? "forbid" : "unset");
                if (request.m_value == "allow")
                {
                    if (!IsRecordReadyForPermission(updated))
                    {
                        return AZ::Failure(AZStd::string(
                            "Usage permission requires a validated, current, unresolved-free, non-superseded record."));
                    }
                    AZStd::string basisError;
                    if (!ValidatePermissionBasis(request, catalog, basisError))
                    {
                        return AZ::Failure(basisError);
                    }
                    RemoveValue(updated.m_forbiddenUsages, request.m_usage);
                    AddUnique(updated.m_allowedUsages, request.m_usage);
                }
                else if (request.m_value == "forbid")
                {
                    RemoveValue(updated.m_allowedUsages, request.m_usage);
                    AddUnique(updated.m_forbiddenUsages, request.m_usage);
                }
                else
                {
                    RemoveValue(updated.m_allowedUsages, request.m_usage);
                    RemoveValue(updated.m_forbiddenUsages, request.m_usage);
                }
            }
            else if (request.m_axis == "supersession")
            {
                if (request.m_value.empty() || request.m_value == request.m_subjectId
                    || !catalog.FindByRecordId(request.m_value))
                {
                    return AZ::Failure(AZStd::string(
                        "Record supersession requires a different existing replacement record ID."));
                }
                event.m_previousValue = updated.m_supersededByRecordId;
                updated.m_supersededByRecordId = request.m_value;
                updated.m_stalenessState = "stale";
                updated.m_allowedUsages.clear();
                AddUnique(updated.m_forbiddenUsages, "superseded");
            }
            else
            {
                return AZ::Failure(AZStd::string("Unsupported governance axis."));
            }

            updated.m_updatedAt = event.m_decidedAt;
            if (!catalog.Upsert(updated, &catalogError))
            {
                return AZ::Failure(catalogError);
            }
        }
        else
        {
            CatalogRelationship updated = *currentRelationship;
            if (request.m_axis == "maturity")
            {
                if (!IsKnownMaturity(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported maturity value."));
                }
                event.m_previousValue = updated.m_researchStage;
                updated.m_researchStage = request.m_value;
            }
            else if (request.m_axis == "confidence")
            {
                if (!IsKnownConfidence(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported confidence value."));
                }
                event.m_previousValue = updated.m_confidence;
                updated.m_confidence = request.m_value;
            }
            else if (request.m_axis == "operational_risk")
            {
                if (!IsKnownRisk(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported operational-risk value."));
                }
                event.m_previousValue = updated.m_operationalRisk;
                updated.m_operationalRisk = request.m_value;
            }
            else if (request.m_axis == "staleness")
            {
                if (!IsKnownStaleness(request.m_value))
                {
                    return AZ::Failure(AZStd::string("Unsupported staleness value."));
                }
                event.m_previousValue = updated.m_stalenessState;
                updated.m_stalenessState = request.m_value;
                if (request.m_value == "current")
                {
                    RemoveValue(updated.m_forbiddenUsages, "stale_or_unverified");
                }
                else
                {
                    updated.m_allowedUsages.clear();
                    AddUnique(updated.m_forbiddenUsages, "stale_or_unverified");
                }
            }
            else if (request.m_axis == "permission")
            {
                if (request.m_usage.empty()
                    || (request.m_value != "allow" && request.m_value != "forbid" && request.m_value != "clear"))
                {
                    return AZ::Failure(AZStd::string("Permission decisions require usage and allow, forbid, or clear."));
                }
                event.m_previousValue = Contains(updated.m_allowedUsages, request.m_usage)
                    ? "allow"
                    : (Contains(updated.m_forbiddenUsages, request.m_usage) ? "forbid" : "unset");
                if (request.m_value == "allow")
                {
                    if (!IsRelationshipReadyForPermission(updated))
                    {
                        return AZ::Failure(AZStd::string(
                            "Usage permission requires a validated, current, unresolved-free, non-superseded relationship."));
                    }
                    AZStd::string basisError;
                    if (!ValidatePermissionBasis(request, catalog, basisError))
                    {
                        return AZ::Failure(basisError);
                    }
                    RemoveValue(updated.m_forbiddenUsages, request.m_usage);
                    AddUnique(updated.m_allowedUsages, request.m_usage);
                }
                else if (request.m_value == "forbid")
                {
                    RemoveValue(updated.m_allowedUsages, request.m_usage);
                    AddUnique(updated.m_forbiddenUsages, request.m_usage);
                }
                else
                {
                    RemoveValue(updated.m_allowedUsages, request.m_usage);
                    RemoveValue(updated.m_forbiddenUsages, request.m_usage);
                }
            }
            else if (request.m_axis == "supersession")
            {
                if (request.m_value.empty() || request.m_value == request.m_subjectId
                    || !catalog.FindRelationshipById(request.m_value))
                {
                    return AZ::Failure(AZStd::string(
                        "Relationship supersession requires a different existing replacement relationship ID."));
                }
                event.m_previousValue = updated.m_supersededByRelationshipId;
                updated.m_supersededByRelationshipId = request.m_value;
                updated.m_stalenessState = "stale";
                updated.m_allowedUsages.clear();
                AddUnique(updated.m_forbiddenUsages, "superseded");
            }
            else
            {
                return AZ::Failure(AZStd::string("Unsupported governance axis."));
            }

            updated.m_updatedAt = event.m_decidedAt;
            if (!catalog.UpsertRelationship(updated, &catalogError))
            {
                return AZ::Failure(catalogError);
            }
        }

        if (!catalog.AddGovernanceEvent(event, &catalogError))
        {
            return AZ::Failure(catalogError);
        }
        return AZ::Success(AZStd::move(event));
    }

    AZ::Outcome<CatalogValidationEvent, AZStd::string> CatalogGovernanceService::ApplyValidation(
        const CatalogValidationRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        CatalogDatabase& catalog) const
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact active game profile is required before validation decisions."));
        }
        if ((request.m_subjectKind != "record" && request.m_subjectKind != "relationship")
            || request.m_subjectId.empty() || !IsKnownValidation(request.m_state)
            || request.m_method.empty() || request.m_validator.empty() || request.m_evidenceIds.empty())
        {
            return AZ::Failure(AZStd::string(
                "Validation decisions require subject, known state, method, validator, and evidence IDs."));
        }

        AZStd::string evidenceError;
        if (!ValidateEvidence(
            request.m_evidenceIds,
            request.m_subjectKind,
            request.m_subjectId,
            workspace,
            sourceRegistry,
            catalog,
            evidenceError))
        {
            return AZ::Failure(evidenceError);
        }

        CatalogValidationEvent validation;
        validation.m_validationId = BuildEventId(
            "validation",
            request.m_subjectKind,
            request.m_subjectId,
            catalog.GetValidationHistory().size() + 1);
        validation.m_subjectKind = request.m_subjectKind;
        validation.m_subjectId = request.m_subjectId;
        if (request.m_subjectKind == "record")
        {
            validation.m_recordId = request.m_subjectId;
        }
        validation.m_state = request.m_state;
        validation.m_method = request.m_method;
        validation.m_validator = request.m_validator;
        validation.m_checkedAt = NowIso();
        validation.m_profileId = profile->m_profileId;
        validation.m_gameVersion = profile->m_gameVersion;
        validation.m_branch = profile->m_branch;
        validation.m_evidenceIds = request.m_evidenceIds;
        validation.m_notes = request.m_notes;

        AZStd::string catalogError;
        if (request.m_subjectKind == "record")
        {
            const CatalogRecord* current = catalog.FindByRecordId(request.m_subjectId);
            if (!current)
            {
                return AZ::Failure(AZStd::string("Validation decision references an unknown record."));
            }
            CatalogRecord updated = *current;
            updated.m_validationState = request.m_state;
            updated.m_updatedAt = validation.m_checkedAt;
            if (request.m_state == "validated")
            {
                RemoveValue(updated.m_forbiddenUsages, "no_unvalidated_runtime_use");
                RemoveValue(updated.m_forbiddenUsages, "validation_failed");
            }
            else
            {
                updated.m_allowedUsages.clear();
                AddUnique(updated.m_forbiddenUsages, "no_unvalidated_runtime_use");
                if (request.m_state == "failed" || request.m_state == "blocked")
                {
                    AddUnique(updated.m_forbiddenUsages, "validation_failed");
                }
                if (request.m_state == "stale")
                {
                    updated.m_stalenessState = "stale";
                    AddUnique(updated.m_forbiddenUsages, "stale_or_unverified");
                }
            }
            if (!catalog.Upsert(updated, &catalogError))
            {
                return AZ::Failure(catalogError);
            }
        }
        else
        {
            const CatalogRelationship* current = catalog.FindRelationshipById(request.m_subjectId);
            if (!current)
            {
                return AZ::Failure(AZStd::string("Validation decision references an unknown relationship."));
            }
            CatalogRelationship updated = *current;
            updated.m_validationState = request.m_state;
            updated.m_updatedAt = validation.m_checkedAt;
            if (request.m_state == "validated")
            {
                RemoveValue(updated.m_forbiddenUsages, "no_unvalidated_runtime_use");
                RemoveValue(updated.m_forbiddenUsages, "validation_failed");
            }
            else
            {
                updated.m_allowedUsages.clear();
                AddUnique(updated.m_forbiddenUsages, "no_unvalidated_runtime_use");
                if (request.m_state == "failed" || request.m_state == "blocked")
                {
                    AddUnique(updated.m_forbiddenUsages, "validation_failed");
                }
                if (request.m_state == "stale")
                {
                    updated.m_stalenessState = "stale";
                    AddUnique(updated.m_forbiddenUsages, "stale_or_unverified");
                }
            }
            if (!catalog.UpsertRelationship(updated, &catalogError))
            {
                return AZ::Failure(catalogError);
            }
        }

        if (!catalog.AddValidationEvent(validation, &catalogError))
        {
            return AZ::Failure(catalogError);
        }
        return AZ::Success(AZStd::move(validation));
    }

    bool CatalogGovernanceService::ValidateEvidence(
        const AZStd::vector<AZStd::string>& evidenceIds,
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        AZStd::string& error)
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile)
        {
            error = "No active game profile is available for evidence validation.";
            return false;
        }

        const CatalogRecord* record = subjectKind == "record" ? catalog.FindByRecordId(subjectId) : nullptr;
        const CatalogRelationship* relationship = subjectKind == "relationship"
            ? catalog.FindRelationshipById(subjectId)
            : nullptr;
        if (!record && !relationship)
        {
            error = "Evidence validation references an unknown catalog subject.";
            return false;
        }

        for (const AZStd::string& evidenceId : evidenceIds)
        {
            const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
            if (!evidence)
            {
                error = "Governance decision references an unknown evidence ID: ";
                error += evidenceId;
                return false;
            }
            if (evidence->m_profileId != profile->m_profileId
                || evidence->m_gameVersion != profile->m_gameVersion
                || evidence->m_branch != profile->m_branch)
            {
                error = "Governance evidence is outside the active game-profile scope.";
                return false;
            }
            if (record && evidence->m_subjectRef != record->m_subjectRef)
            {
                error = "Governance evidence subject does not match the catalog record subject.";
                return false;
            }
            if (relationship && !Contains(relationship->m_evidenceIds, evidenceId))
            {
                error = "Relationship governance must use evidence already linked to that relationship.";
                return false;
            }
        }
        return true;
    }

    bool CatalogGovernanceService::ValidatePermissionBasis(
        const CatalogGovernanceRequest& request,
        const CatalogDatabase& catalog,
        AZStd::string& error)
    {
        if (request.m_validationIds.empty())
        {
            error = "Allowed usage requires at least one validated proof event.";
            return false;
        }

        bool hasValidatedBasis = false;
        for (const AZStd::string& validationId : request.m_validationIds)
        {
            const CatalogValidationEvent* validation = catalog.FindValidationById(validationId);
            if (!validation)
            {
                error = "Permission decision references an unknown validation ID: ";
                error += validationId;
                return false;
            }
            if (validation->GetSubjectKind() != request.m_subjectKind
                || validation->GetSubjectId() != request.m_subjectId)
            {
                error = "Permission validation proof belongs to a different catalog subject.";
                return false;
            }
            if (validation->m_state == "validated")
            {
                hasValidatedBasis = true;
            }
        }
        if (!hasValidatedBasis)
        {
            error = "Allowed usage requires a validation event whose state is validated.";
            return false;
        }
        return true;
    }

    AZStd::string CatalogGovernanceService::BuildEventId(
        const char* prefix,
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId,
        size_t sequence)
    {
        return ToAzString(QStringLiteral("%1.%2.%3.%4.%5")
            .arg(QString::fromUtf8(prefix))
            .arg(QString::fromUtf8(subjectKind.c_str()))
            .arg(Sanitize(subjectId))
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(static_cast<qulonglong>(sequence)));
    }
} // namespace TaintedGrailModdingSDK
