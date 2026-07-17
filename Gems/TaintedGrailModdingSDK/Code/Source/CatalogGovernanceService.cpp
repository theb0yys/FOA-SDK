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

        bool IsReadyForPermission(const GovernedSubjectState& state)
        {
            return state.m_validationState == ValidationState::Validated
                && state.m_stalenessState == StalenessState::Current
                && state.m_missingRefs.empty()
                && state.m_conflictRefs.empty()
                && state.m_supersededById.empty();
        }
    } // namespace

    AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> CatalogGovernanceService::ApplyDecision(
        const CatalogGovernanceRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::string& eventIdOverride) const
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact active game profile is required before governance decisions."));
        }
        if (request.m_subjectId.empty() || request.m_axis.empty() || request.m_reviewer.empty())
        {
            return AZ::Failure(AZStd::string(
                "Governance decisions require subject ID, axis, and reviewer."));
        }

        const AZ::Outcome<CatalogSubjectKind, AZStd::string> subjectKindResult =
            ParseCatalogSubjectKind(request.m_subjectKind);
        if (!subjectKindResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(subjectKindResult.GetError()));
        }
        const CatalogSubjectKind subjectKind = subjectKindResult.GetValue();

        const AZ::Outcome<GovernanceAxis, AZStd::string> axisResult = ParseGovernanceAxis(request.m_axis);
        if (!axisResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(axisResult.GetError()));
        }
        const GovernanceAxis axis = axisResult.GetValue();

        const bool clearingPermission = axis == GovernanceAxis::Permission && request.m_value == "clear";
        if (!clearingPermission && request.m_evidenceIds.empty())
        {
            return AZ::Failure(AZStd::string("Governance decisions require evidence IDs."));
        }

        AZStd::string evidenceError;
        if (!request.m_evidenceIds.empty()
            && !ValidateEvidence(
                request.m_evidenceIds,
                subjectKind,
                request.m_subjectId,
                workspace,
                sourceRegistry,
                catalog,
                evidenceError))
        {
            return AZ::Failure(evidenceError);
        }

        AZ::Outcome<GovernedSubjectState, AZStd::string> stateResult =
            ReadSubjectState(subjectKind, request.m_subjectId, catalog);
        if (!stateResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(stateResult.GetError()));
        }

        CatalogDatabase candidate = catalog;
        GovernedSubjectState state = stateResult.TakeValue();
        AZ::Outcome<CatalogGovernanceEvent, AZStd::string> transitionResult = ApplyTypedTransition(
            request,
            axis,
            state,
            candidate,
            NowIso());
        if (!transitionResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(transitionResult.GetError()));
        }

        CatalogGovernanceEvent event = transitionResult.TakeValue();
        event.m_eventId = eventIdOverride.empty()
            ? BuildUniqueEventId("governance", subjectKind, request.m_subjectId, candidate)
            : eventIdOverride;

        AZStd::string catalogError;
        if (!WriteSubjectState(state, candidate, catalogError))
        {
            return AZ::Failure(catalogError);
        }
        if (!candidate.AddGovernanceEvent(event, &catalogError))
        {
            return AZ::Failure(catalogError);
        }

        CatalogGovernanceApplyResult result;
        result.m_catalog = AZStd::move(candidate);
        result.m_event = AZStd::move(event);
        return AZ::Success(AZStd::move(result));
    }

    AZ::Outcome<CatalogValidationApplyResult, AZStd::string> CatalogGovernanceService::ApplyValidation(
        const CatalogValidationRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::string& validationIdOverride) const
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact active game profile is required before validation decisions."));
        }
        if (request.m_subjectId.empty() || request.m_method.empty()
            || request.m_validator.empty() || request.m_evidenceIds.empty())
        {
            return AZ::Failure(AZStd::string(
                "Validation decisions require subject, method, validator, and evidence IDs."));
        }

        const AZ::Outcome<CatalogSubjectKind, AZStd::string> subjectKindResult =
            ParseCatalogSubjectKind(request.m_subjectKind);
        if (!subjectKindResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(subjectKindResult.GetError()));
        }
        const CatalogSubjectKind subjectKind = subjectKindResult.GetValue();

        const AZ::Outcome<ValidationState, AZStd::string> validationStateResult =
            ParseValidationState(request.m_state);
        if (!validationStateResult.IsSuccess() || validationStateResult.GetValue() == ValidationState::Unset)
        {
            return AZ::Failure(validationStateResult.IsSuccess()
                ? AZStd::string("Validation decisions require a non-empty state.")
                : AZStd::string(validationStateResult.GetError()));
        }
        const ValidationState validationState = validationStateResult.GetValue();

        AZStd::string evidenceError;
        if (!ValidateEvidence(
            request.m_evidenceIds,
            subjectKind,
            request.m_subjectId,
            workspace,
            sourceRegistry,
            catalog,
            evidenceError))
        {
            return AZ::Failure(evidenceError);
        }

        AZ::Outcome<GovernedSubjectState, AZStd::string> stateResult =
            ReadSubjectState(subjectKind, request.m_subjectId, catalog);
        if (!stateResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(stateResult.GetError()));
        }

        CatalogDatabase candidate = catalog;
        GovernedSubjectState state = stateResult.TakeValue();
        ApplyValidationState(validationState, state);

        CatalogValidationEvent validation;
        validation.m_validationId = validationIdOverride.empty()
            ? BuildUniqueEventId("validation", subjectKind, request.m_subjectId, candidate)
            : validationIdOverride;
        validation.m_subjectKind = ToString(subjectKind);
        validation.m_subjectId = request.m_subjectId;
        if (subjectKind == CatalogSubjectKind::Record)
        {
            validation.m_recordId = request.m_subjectId;
        }
        validation.m_state = ToString(validationState);
        validation.m_method = request.m_method;
        validation.m_validator = request.m_validator;
        validation.m_checkedAt = NowIso();
        validation.m_profileId = profile->m_profileId;
        validation.m_gameVersion = profile->m_gameVersion;
        validation.m_branch = profile->m_branch;
        validation.m_evidenceIds = request.m_evidenceIds;
        validation.m_notes = request.m_notes;
        state.m_updatedAt = validation.m_checkedAt;

        AZStd::string catalogError;
        if (!WriteSubjectState(state, candidate, catalogError))
        {
            return AZ::Failure(catalogError);
        }
        if (!candidate.AddValidationEvent(validation, &catalogError))
        {
            return AZ::Failure(catalogError);
        }

        CatalogValidationApplyResult result;
        result.m_catalog = AZStd::move(candidate);
        result.m_event = AZStd::move(validation);
        return AZ::Success(AZStd::move(result));
    }

    AZ::Outcome<GovernedSubjectState, AZStd::string> CatalogGovernanceService::ReadSubjectState(
        CatalogSubjectKind subjectKind,
        const AZStd::string& subjectId,
        const CatalogDatabase& catalog)
    {
        GovernedSubjectState state;
        state.m_subjectKind = subjectKind;
        state.m_subjectId = subjectId;

        AZStd::string maturity;
        AZStd::string confidence;
        AZStd::string risk;
        AZStd::string validation;
        AZStd::string staleness;

        if (subjectKind == CatalogSubjectKind::Record)
        {
            const CatalogRecord* record = catalog.FindByRecordId(subjectId);
            if (!record)
            {
                return AZ::Failure(AZStd::string("Governance decision references an unknown catalog record."));
            }
            maturity = record->m_researchStage;
            confidence = record->m_confidence;
            risk = record->m_operationalRisk;
            validation = record->m_validationState;
            staleness = record->m_stalenessState;
            state.m_allowedUsages = record->m_allowedUsages;
            state.m_forbiddenUsages = record->m_forbiddenUsages;
            state.m_missingRefs = record->m_missingRefs;
            state.m_conflictRefs = record->m_conflictRefs;
            state.m_supersededById = record->m_supersededByRecordId;
            state.m_updatedAt = record->m_updatedAt;
        }
        else
        {
            const CatalogRelationship* relationship = catalog.FindRelationshipById(subjectId);
            if (!relationship)
            {
                return AZ::Failure(AZStd::string("Governance decision references an unknown catalog relationship."));
            }
            maturity = relationship->m_researchStage;
            confidence = relationship->m_confidence;
            risk = relationship->m_operationalRisk;
            validation = relationship->m_validationState;
            staleness = relationship->m_stalenessState;
            state.m_allowedUsages = relationship->m_allowedUsages;
            state.m_forbiddenUsages = relationship->m_forbiddenUsages;
            state.m_missingRefs = relationship->m_missingRefs;
            state.m_conflictRefs = relationship->m_conflictRefs;
            state.m_supersededById = relationship->m_supersededByRelationshipId;
            state.m_updatedAt = relationship->m_updatedAt;
        }

        const AZ::Outcome<ResearchStage, AZStd::string> maturityResult = ParseResearchStage(maturity);
        const AZ::Outcome<ConfidenceLevel, AZStd::string> confidenceResult = ParseConfidenceLevel(confidence);
        const AZ::Outcome<OperationalRisk, AZStd::string> riskResult = ParseOperationalRisk(risk);
        const AZ::Outcome<ValidationState, AZStd::string> validationResult = ParseValidationState(validation);
        const AZ::Outcome<StalenessState, AZStd::string> stalenessResult = ParseStalenessState(staleness);
        if (!maturityResult.IsSuccess()) return AZ::Failure(AZStd::string(maturityResult.GetError()));
        if (!confidenceResult.IsSuccess()) return AZ::Failure(AZStd::string(confidenceResult.GetError()));
        if (!riskResult.IsSuccess()) return AZ::Failure(AZStd::string(riskResult.GetError()));
        if (!validationResult.IsSuccess()) return AZ::Failure(AZStd::string(validationResult.GetError()));
        if (!stalenessResult.IsSuccess()) return AZ::Failure(AZStd::string(stalenessResult.GetError()));

        state.m_researchStage = maturityResult.GetValue();
        state.m_confidence = confidenceResult.GetValue();
        state.m_operationalRisk = riskResult.GetValue();
        state.m_validationState = validationResult.GetValue();
        state.m_stalenessState = stalenessResult.GetValue();
        return AZ::Success(AZStd::move(state));
    }

    bool CatalogGovernanceService::WriteSubjectState(
        const GovernedSubjectState& state,
        CatalogDatabase& catalog,
        AZStd::string& error)
    {
        if (state.m_subjectKind == CatalogSubjectKind::Record)
        {
            const CatalogRecord* current = catalog.FindByRecordId(state.m_subjectId);
            if (!current)
            {
                error = "Governance write references an unknown catalog record.";
                return false;
            }
            CatalogRecord updated = *current;
            updated.m_researchStage = ToString(state.m_researchStage);
            updated.m_confidence = ToString(state.m_confidence);
            updated.m_operationalRisk = ToString(state.m_operationalRisk);
            updated.m_validationState = ToString(state.m_validationState);
            updated.m_stalenessState = ToString(state.m_stalenessState);
            updated.m_allowedUsages = state.m_allowedUsages;
            updated.m_forbiddenUsages = state.m_forbiddenUsages;
            updated.m_missingRefs = state.m_missingRefs;
            updated.m_conflictRefs = state.m_conflictRefs;
            updated.m_supersededByRecordId = state.m_supersededById;
            updated.m_updatedAt = state.m_updatedAt;
            return catalog.Upsert(updated, &error);
        }

        const CatalogRelationship* current = catalog.FindRelationshipById(state.m_subjectId);
        if (!current)
        {
            error = "Governance write references an unknown catalog relationship.";
            return false;
        }
        CatalogRelationship updated = *current;
        updated.m_researchStage = ToString(state.m_researchStage);
        updated.m_confidence = ToString(state.m_confidence);
        updated.m_operationalRisk = ToString(state.m_operationalRisk);
        updated.m_validationState = ToString(state.m_validationState);
        updated.m_stalenessState = ToString(state.m_stalenessState);
        updated.m_allowedUsages = state.m_allowedUsages;
        updated.m_forbiddenUsages = state.m_forbiddenUsages;
        updated.m_missingRefs = state.m_missingRefs;
        updated.m_conflictRefs = state.m_conflictRefs;
        updated.m_supersededByRelationshipId = state.m_supersededById;
        updated.m_updatedAt = state.m_updatedAt;
        return catalog.UpsertRelationship(updated, &error);
    }

    AZ::Outcome<CatalogGovernanceEvent, AZStd::string> CatalogGovernanceService::ApplyTypedTransition(
        const CatalogGovernanceRequest& request,
        GovernanceAxis axis,
        GovernedSubjectState& state,
        const CatalogDatabase& catalog,
        const AZStd::string& decidedAt)
    {
        CatalogGovernanceEvent event;
        event.m_subjectKind = ToString(state.m_subjectKind);
        event.m_subjectId = state.m_subjectId;
        event.m_axis = ToString(axis);
        event.m_newValue = request.m_value;
        event.m_usage = request.m_usage;
        event.m_evidenceIds = request.m_evidenceIds;
        event.m_validationIds = request.m_validationIds;
        event.m_reviewer = request.m_reviewer;
        event.m_decidedAt = decidedAt;
        event.m_notes = request.m_notes;

        switch (axis)
        {
        case GovernanceAxis::Maturity:
        {
            const AZ::Outcome<ResearchStage, AZStd::string> value = ParseResearchStage(request.m_value);
            if (!value.IsSuccess() || value.GetValue() == ResearchStage::Unset)
            {
                return AZ::Failure(value.IsSuccess()
                    ? AZStd::string("Maturity decisions require a non-empty value.")
                    : AZStd::string(value.GetError()));
            }
            event.m_previousValue = ToString(state.m_researchStage);
            state.m_researchStage = value.GetValue();
            break;
        }
        case GovernanceAxis::Confidence:
        {
            const AZ::Outcome<ConfidenceLevel, AZStd::string> value = ParseConfidenceLevel(request.m_value);
            if (!value.IsSuccess() || value.GetValue() == ConfidenceLevel::Unset)
            {
                return AZ::Failure(value.IsSuccess()
                    ? AZStd::string("Confidence decisions require a non-empty value.")
                    : AZStd::string(value.GetError()));
            }
            event.m_previousValue = ToString(state.m_confidence);
            state.m_confidence = value.GetValue();
            break;
        }
        case GovernanceAxis::OperationalRisk:
        {
            const AZ::Outcome<OperationalRisk, AZStd::string> value = ParseOperationalRisk(request.m_value);
            if (!value.IsSuccess() || value.GetValue() == OperationalRisk::Unset)
            {
                return AZ::Failure(value.IsSuccess()
                    ? AZStd::string("Operational-risk decisions require a non-empty value.")
                    : AZStd::string(value.GetError()));
            }
            event.m_previousValue = ToString(state.m_operationalRisk);
            state.m_operationalRisk = value.GetValue();
            break;
        }
        case GovernanceAxis::Staleness:
        {
            const AZ::Outcome<StalenessState, AZStd::string> value = ParseStalenessState(request.m_value);
            if (!value.IsSuccess() || value.GetValue() == StalenessState::Unset)
            {
                return AZ::Failure(value.IsSuccess()
                    ? AZStd::string("Staleness decisions require a non-empty value.")
                    : AZStd::string(value.GetError()));
            }
            event.m_previousValue = ToString(state.m_stalenessState);
            state.m_stalenessState = value.GetValue();
            if (state.m_stalenessState == StalenessState::Current)
            {
                RemoveValue(state.m_forbiddenUsages, "stale_or_unverified");
            }
            else
            {
                state.m_allowedUsages.clear();
                AddUnique(state.m_forbiddenUsages, "stale_or_unverified");
            }
            break;
        }
        case GovernanceAxis::Permission:
        {
            if (request.m_usage.empty())
            {
                return AZ::Failure(AZStd::string("Permission decisions require a named usage."));
            }
            const AZ::Outcome<PermissionDecision, AZStd::string> decision = ParsePermissionDecision(request.m_value);
            if (!decision.IsSuccess()) return AZ::Failure(AZStd::string(decision.GetError()));
            event.m_previousValue = Contains(state.m_allowedUsages, request.m_usage)
                ? "allow"
                : (Contains(state.m_forbiddenUsages, request.m_usage) ? "forbid" : "unset");
            if (decision.GetValue() == PermissionDecision::Allow)
            {
                if (!IsReadyForPermission(state))
                {
                    return AZ::Failure(AZStd::string(
                        "Usage permission requires a validated, current, unresolved-free, non-superseded subject."));
                }
                AZStd::string basisError;
                if (!ValidatePermissionBasis(request, state.m_subjectKind, catalog, basisError))
                {
                    return AZ::Failure(basisError);
                }
                RemoveValue(state.m_forbiddenUsages, request.m_usage);
                AddUnique(state.m_allowedUsages, request.m_usage);
            }
            else if (decision.GetValue() == PermissionDecision::Forbid)
            {
                RemoveValue(state.m_allowedUsages, request.m_usage);
                AddUnique(state.m_forbiddenUsages, request.m_usage);
            }
            else
            {
                RemoveValue(state.m_allowedUsages, request.m_usage);
                RemoveValue(state.m_forbiddenUsages, request.m_usage);
            }
            break;
        }
        case GovernanceAxis::Supersession:
            if (request.m_value.empty() || request.m_value == state.m_subjectId)
            {
                return AZ::Failure(AZStd::string("Supersession requires a different replacement subject ID."));
            }
            if ((state.m_subjectKind == CatalogSubjectKind::Record && !catalog.FindByRecordId(request.m_value))
                || (state.m_subjectKind == CatalogSubjectKind::Relationship
                    && !catalog.FindRelationshipById(request.m_value)))
            {
                return AZ::Failure(AZStd::string("Supersession replacement does not exist in the catalog."));
            }
            event.m_previousValue = state.m_supersededById;
            state.m_supersededById = request.m_value;
            state.m_stalenessState = StalenessState::Stale;
            state.m_allowedUsages.clear();
            AddUnique(state.m_forbiddenUsages, "superseded");
            break;
        }

        state.m_updatedAt = decidedAt;
        return AZ::Success(AZStd::move(event));
    }

    void CatalogGovernanceService::ApplyValidationState(
        ValidationState validationState,
        GovernedSubjectState& state)
    {
        state.m_validationState = validationState;
        if (validationState == ValidationState::Validated)
        {
            RemoveValue(state.m_forbiddenUsages, "no_unvalidated_runtime_use");
            RemoveValue(state.m_forbiddenUsages, "validation_failed");
            return;
        }

        state.m_allowedUsages.clear();
        AddUnique(state.m_forbiddenUsages, "no_unvalidated_runtime_use");
        if (validationState == ValidationState::Failed || validationState == ValidationState::Blocked)
        {
            AddUnique(state.m_forbiddenUsages, "validation_failed");
        }
        if (validationState == ValidationState::Stale)
        {
            state.m_stalenessState = StalenessState::Stale;
            AddUnique(state.m_forbiddenUsages, "stale_or_unverified");
        }
    }

    bool CatalogGovernanceService::ValidateEvidence(
        const AZStd::vector<AZStd::string>& evidenceIds,
        CatalogSubjectKind subjectKind,
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

        const CatalogRecord* record = subjectKind == CatalogSubjectKind::Record
            ? catalog.FindByRecordId(subjectId)
            : nullptr;
        const CatalogRelationship* relationship = subjectKind == CatalogSubjectKind::Relationship
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
        CatalogSubjectKind subjectKind,
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
            const AZ::Outcome<CatalogSubjectKind, AZStd::string> validationSubjectKind =
                ParseCatalogSubjectKind(validation->GetSubjectKind());
            if (!validationSubjectKind.IsSuccess()
                || validationSubjectKind.GetValue() != subjectKind
                || validation->GetSubjectId() != request.m_subjectId)
            {
                error = "Permission validation proof belongs to a different catalog subject.";
                return false;
            }
            const AZ::Outcome<ValidationState, AZStd::string> validationState =
                ParseValidationState(validation->m_state);
            if (!validationState.IsSuccess())
            {
                error = AZStd::string(validationState.GetError());
                return false;
            }
            if (validationState.GetValue() == ValidationState::Validated)
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

    AZStd::string CatalogGovernanceService::BuildUniqueEventId(
        const char* prefix,
        CatalogSubjectKind subjectKind,
        const AZStd::string& subjectId,
        const CatalogDatabase& catalog)
    {
        size_t sequence = 1;
        while (true)
        {
            const AZStd::string candidate = BuildEventId(prefix, subjectKind, subjectId, sequence++);
            bool exists = false;
            if (AZStd::string_view(prefix) == "governance")
            {
                for (const CatalogGovernanceEvent& event : catalog.GetGovernanceHistory())
                {
                    if (event.m_eventId == candidate)
                    {
                        exists = true;
                        break;
                    }
                }
            }
            else if (catalog.FindValidationById(candidate))
            {
                exists = true;
            }
            if (!exists)
            {
                return candidate;
            }
        }
    }

    AZStd::string CatalogGovernanceService::BuildEventId(
        const char* prefix,
        CatalogSubjectKind subjectKind,
        const AZStd::string& subjectId,
        size_t sequence)
    {
        const AZStd::string subjectKindValue = ToString(subjectKind);
        return ToAzString(QStringLiteral("%1.%2.%3.%4")
            .arg(QString::fromUtf8(prefix))
            .arg(QString::fromUtf8(subjectKindValue.c_str()))
            .arg(Sanitize(subjectId))
            .arg(static_cast<qulonglong>(sequence)));
    }
} // namespace TaintedGrailModdingSDK
