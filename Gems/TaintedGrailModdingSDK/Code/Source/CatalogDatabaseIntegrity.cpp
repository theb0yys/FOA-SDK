/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

#include "CatalogGovernanceTypes.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
            }
        }

        bool SameProfile(
            const GameProfile& left,
            const GameProfile& right)
        {
            return left.m_profileId == right.m_profileId
                && left.m_gameVersion == right.m_gameVersion
                && left.m_branch == right.m_branch
                && left.m_runtimeTarget == right.m_runtimeTarget;
        }

        bool HasUniqueStableIds(
            const AZStd::vector<AZStd::string>& values,
            bool allowEmpty,
            AZStd::string& error)
        {
            if (!allowEmpty && values.empty())
            {
                error = "At least one stable evidence identity is required.";
                return false;
            }
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsStableContractId(value))
                {
                    error = "Every evidence and validation reference must be a bounded stable identity.";
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            if (AZStd::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
            {
                error = "Evidence and validation reference identities must be unique.";
                return false;
            }
            return true;
        }

        AZStd::string ExpectedSubjectRef(
            const CatalogDatabase& catalog,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId)
        {
            if (subjectKind == "record")
            {
                const CatalogRecord* record = catalog.FindByRecordId(subjectId);
                return record ? record->m_subjectRef : AZStd::string{};
            }
            if (subjectKind == "relationship")
            {
                return catalog.FindRelationshipById(subjectId)
                    ? "relationship:" + subjectId
                    : AZStd::string{};
            }
            return {};
        }

        bool ValidateEvidenceForSubject(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& expectedSubjectRef,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            bool allowEmpty,
            AZStd::string& error)
        {
            if (expectedSubjectRef.empty())
            {
                error = "Evidence validation requires one exact known subject.";
                return false;
            }
            if (!HasUniqueStableIds(evidenceIds, allowEmpty, error))
            {
                return false;
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    error = "Evidence reference is not registered: " + evidenceId;
                    return false;
                }
                const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source || !IsUsableImportStatus(source->m_importStatus))
                {
                    error = "Evidence is not backed by one successfully imported source: "
                        + evidenceId;
                    return false;
                }
                if (evidence->m_sourceFingerprint != source->m_fingerprint
                    || !IsSha256Fingerprint(evidence->m_sourceFingerprint)
                    || evidence->m_profileId != profile.m_profileId
                    || evidence->m_gameVersion != profile.m_gameVersion
                    || evidence->m_branch != profile.m_branch
                    || source->m_runtimeTarget != profile.m_runtimeTarget)
                {
                    error = "Evidence is outside the exact active profile and source fingerprint: "
                        + evidenceId;
                    return false;
                }
                if (evidence->m_subjectRef != expectedSubjectRef)
                {
                    error = "Evidence does not prove the exact catalog subject "
                        + expectedSubjectRef + ": " + evidenceId;
                    return false;
                }
                if (evidence->m_claim.empty()
                    || evidence->m_evidenceKind.empty()
                    || evidence->m_locator.empty()
                    || evidence->m_recordPath.empty()
                    || !IsStrictUtcTimestamp(evidence->m_extractedAt))
                {
                    error = "Evidence lacks complete claim, kind, locator, record path, or UTC extraction provenance: "
                        + evidenceId;
                    return false;
                }
            }
            return true;
        }

        bool ValidateEventProfile(
            const AZStd::string& profileId,
            const AZStd::string& gameVersion,
            const AZStd::string& branch,
            const GameProfile& profile,
            AZStd::string& error)
        {
            if (profileId != profile.m_profileId
                || gameVersion != profile.m_gameVersion
                || branch != profile.m_branch)
            {
                error = "Catalog history is bound to a different active profile, game version, or branch.";
                return false;
            }
            return true;
        }

        bool ValidateGovernanceValue(
            const CatalogGovernanceEvent& event,
            const CatalogDatabase& catalog,
            AZStd::string& error)
        {
            const AZ::Outcome<GovernanceAxis, AZStd::string> axisResult =
                ParseGovernanceAxis(event.m_axis);
            if (!axisResult.IsSuccess())
            {
                error = AZStd::string(axisResult.GetError());
                return false;
            }
            switch (axisResult.GetValue())
            {
            case GovernanceAxis::Maturity:
            {
                const auto result = ParseResearchStage(event.m_newValue);
                if (!result.IsSuccess() || result.GetValue() == ResearchStage::Unset)
                {
                    error = "Maturity governance history contains an invalid decision value.";
                    return false;
                }
                break;
            }
            case GovernanceAxis::Confidence:
            {
                const auto result = ParseConfidenceLevel(event.m_newValue);
                if (!result.IsSuccess() || result.GetValue() == ConfidenceLevel::Unset)
                {
                    error = "Confidence governance history contains an invalid decision value.";
                    return false;
                }
                break;
            }
            case GovernanceAxis::OperationalRisk:
            {
                const auto result = ParseOperationalRisk(event.m_newValue);
                if (!result.IsSuccess() || result.GetValue() == OperationalRisk::Unset)
                {
                    error = "Operational-risk governance history contains an invalid decision value.";
                    return false;
                }
                break;
            }
            case GovernanceAxis::Staleness:
            {
                const auto result = ParseStalenessState(event.m_newValue);
                if (!result.IsSuccess() || result.GetValue() == StalenessState::Unset)
                {
                    error = "Staleness governance history contains an invalid decision value.";
                    return false;
                }
                break;
            }
            case GovernanceAxis::Permission:
            {
                const auto result = ParsePermissionDecision(event.m_newValue);
                if (!result.IsSuccess() || event.m_usage.empty())
                {
                    error = "Permission governance history requires one supported decision and exact usage.";
                    return false;
                }
                break;
            }
            case GovernanceAxis::Supersession:
                if (event.m_newValue.empty()
                    || event.m_newValue == event.m_subjectId
                    || (event.m_subjectKind == "record"
                        && !catalog.FindByRecordId(event.m_newValue))
                    || (event.m_subjectKind == "relationship"
                        && !catalog.FindRelationshipById(event.m_newValue)))
                {
                    error = "Supersession governance history requires a different existing replacement subject.";
                    return false;
                }
                break;
            }
            return true;
        }

        bool IsNewer(
            const AZStd::string& candidateTime,
            const AZStd::string& candidateId,
            const AZStd::string& currentTime,
            const AZStd::string& currentId)
        {
            return candidateTime > currentTime
                || (candidateTime == currentTime && candidateId > currentId);
        }

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }
    } // namespace

    const CatalogValidationEvent* CatalogDatabase::FindLatestValidationForSubject(
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId) const
    {
        const CatalogValidationEvent* latest = nullptr;
        for (const CatalogValidationEvent& validation : m_validationHistory)
        {
            if (validation.GetSubjectKind() != subjectKind
                || validation.GetSubjectId() != subjectId)
            {
                continue;
            }
            if (!latest
                || IsNewer(
                    validation.m_checkedAt,
                    validation.m_validationId,
                    latest->m_checkedAt,
                    latest->m_validationId))
            {
                latest = &validation;
            }
        }
        return latest;
    }

    const CatalogGovernanceEvent* CatalogDatabase::FindEffectiveGovernanceEvent(
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId,
        const AZStd::string& axis,
        const AZStd::string& usage) const
    {
        const CatalogGovernanceEvent* latest = nullptr;
        for (const CatalogGovernanceEvent& event : m_governanceHistory)
        {
            if (event.m_subjectKind != subjectKind
                || event.m_subjectId != subjectId
                || event.m_axis != axis
                || (axis == "permission" && event.m_usage != usage))
            {
                continue;
            }
            if (!latest
                || IsNewer(
                    event.m_decidedAt,
                    event.m_eventId,
                    latest->m_decidedAt,
                    latest->m_eventId))
            {
                latest = &event;
            }
        }
        return latest;
    }

    bool CatalogDatabase::AddValidationEventBound(
        const CatalogValidationEvent& validation,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error)
    {
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        AZStd::string validationError;
        if (!activeProfile || !SameProfile(*activeProfile, profile))
        {
            SetError(error, "Validation history requires the exact active workspace profile.");
            return false;
        }
        const AZStd::string subjectKind = validation.GetSubjectKind();
        const AZStd::string subjectId = validation.GetSubjectId();
        if (!IsStableContractId(validation.m_validationId)
            || !IsStableContractId(subjectId)
            || (subjectKind != "record" && subjectKind != "relationship")
            || !IsStrictUtcTimestamp(validation.m_checkedAt)
            || validation.m_method.empty()
            || validation.m_validator.empty())
        {
            SetError(error, "Validation history requires stable identities, supported subject kind, method, validator, and a real UTC date/time.");
            return false;
        }
        const auto stateResult = ParseValidationState(validation.m_state);
        if (!stateResult.IsSuccess()
            || stateResult.GetValue() == ValidationState::Unset)
        {
            SetError(error, "Validation history contains an unsupported validation state.");
            return false;
        }
        if (!ValidateEventProfile(
                validation.m_profileId,
                validation.m_gameVersion,
                validation.m_branch,
                profile,
                validationError))
        {
            SetError(error, AZStd::move(validationError));
            return false;
        }
        const AZStd::string subjectRef =
            ExpectedSubjectRef(*this, subjectKind, subjectId);
        if (!ValidateEvidenceForSubject(
                validation.m_evidenceIds,
                subjectRef,
                profile,
                sourceRegistry,
                false,
                validationError))
        {
            SetError(error, AZStd::move(validationError));
            return false;
        }
        if (const CatalogValidationEvent* latest =
                FindLatestValidationForSubject(subjectKind, subjectId))
        {
            if (validation.m_checkedAt < latest->m_checkedAt)
            {
                SetError(error, "Backdated validation history cannot supersede a newer validation decision.");
                return false;
            }
        }
        return AddValidationEvent(validation, error);
    }

    bool CatalogDatabase::AddGovernanceEventBound(
        const CatalogGovernanceEvent& event,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error)
    {
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        AZStd::string validationError;
        if (!activeProfile || !SameProfile(*activeProfile, profile))
        {
            SetError(error, "Governance history requires the exact active workspace profile.");
            return false;
        }
        if (!IsStableContractId(event.m_eventId)
            || !IsStableContractId(event.m_subjectId)
            || (event.m_subjectKind != "record"
                && event.m_subjectKind != "relationship")
            || event.m_reviewer.empty()
            || !IsStrictUtcTimestamp(event.m_decidedAt)
            || !ValidateGovernanceValue(event, *this, validationError))
        {
            SetError(
                error,
                validationError.empty()
                    ? "Governance history requires stable identity, exact subject, supported decision, reviewer, and a real UTC date/time."
                    : AZStd::move(validationError));
            return false;
        }
        const bool evidenceMayBeEmpty =
            event.m_axis == "permission" && event.m_newValue == "clear";
        const AZStd::string subjectRef =
            ExpectedSubjectRef(*this, event.m_subjectKind, event.m_subjectId);
        if (!ValidateEvidenceForSubject(
                event.m_evidenceIds,
                subjectRef,
                profile,
                sourceRegistry,
                evidenceMayBeEmpty,
                validationError))
        {
            SetError(error, AZStd::move(validationError));
            return false;
        }
        if (!HasUniqueStableIds(
                event.m_validationIds,
                true,
                validationError))
        {
            SetError(error, AZStd::move(validationError));
            return false;
        }
        for (const AZStd::string& validationId : event.m_validationIds)
        {
            const CatalogValidationEvent* validation = FindValidationById(validationId);
            if (!validation
                || validation->GetSubjectKind() != event.m_subjectKind
                || validation->GetSubjectId() != event.m_subjectId
                || validation->m_state != "validated"
                || validation->m_profileId != profile.m_profileId
                || validation->m_gameVersion != profile.m_gameVersion
                || validation->m_branch != profile.m_branch)
            {
                SetError(error, "Every governance validation reference must be a validated exact-subject active-profile event.");
                return false;
            }
        }
        if (event.m_axis == "permission" && event.m_newValue == "allow")
        {
            const CatalogValidationEvent* latest =
                FindLatestValidationForSubject(event.m_subjectKind, event.m_subjectId);
            if (!latest
                || latest->m_state != "validated"
                || !Contains(event.m_validationIds, latest->m_validationId))
            {
                SetError(error, "Permission allow requires the current effective validated proof event, not stale historical validation.");
                return false;
            }
        }
        if (const CatalogGovernanceEvent* current = FindEffectiveGovernanceEvent(
                event.m_subjectKind,
                event.m_subjectId,
                event.m_axis,
                event.m_usage))
        {
            if (event.m_decidedAt < current->m_decidedAt)
            {
                SetError(error, "Backdated governance history cannot become the effective decision.");
                return false;
            }
            if (!event.m_previousValue.empty()
                && event.m_previousValue != current->m_newValue)
            {
                SetError(error, "Governance previous value does not match the current effective decision.");
                return false;
            }
        }
        return AddGovernanceEvent(event, error);
    }

    bool CatalogDatabase::ReplaceFromBoundDocument(
        const CatalogDocument& document,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error)
    {
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile
            || !SameProfile(*activeProfile, profile)
            || document.m_workspaceId != workspace.m_workspaceId
            || document.m_profileId != profile.m_profileId
            || document.m_gameVersion != profile.m_gameVersion
            || document.m_branch != profile.m_branch)
        {
            SetError(error, "Catalog document binding does not match the exact active workspace and game profile.");
            return false;
        }

        CatalogDatabase candidate;
        if (!candidate.ReplaceFromDocument(document, error)
            || !candidate.ValidateIntegrity(
                workspace,
                profile,
                sourceRegistry,
                error))
        {
            return false;
        }
        *this = AZStd::move(candidate);
        return true;
    }

    bool CatalogDatabase::ValidateIntegrity(
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        AZStd::string* error) const
    {
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (!activeProfile
            || !SameProfile(*activeProfile, profile)
            || workspace.m_workspaceId.empty())
        {
            SetError(error, "Catalog integrity requires one exact active workspace profile.");
            return false;
        }

        AZStd::string validationError;
        for (const CatalogRecord& record : m_records)
        {
            if (!ValidateRecord(record, &record, &validationError)
                || !ValidateEvidenceForSubject(
                    record.m_evidenceIds,
                    record.m_subjectRef,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Catalog record integrity failed for "
                    + record.m_recordId + ": " + validationError);
                return false;
            }
            for (const AZStd::string& usage : record.m_allowedUsages)
            {
                const CatalogGovernanceEvent* effective =
                    FindEffectiveGovernanceEvent(
                        "record",
                        record.m_recordId,
                        "permission",
                        usage);
                if (!effective || effective->m_newValue != "allow")
                {
                    SetError(error, "Allowed record usage has no current effective allow decision: "
                        + record.m_recordId + "/" + usage);
                    return false;
                }
            }
        }

        for (const CatalogRelationship& relationship : m_relationships)
        {
            if (!ValidateRelationship(relationship, &validationError))
            {
                SetError(error, "Catalog relationship integrity failed for "
                    + relationship.m_relationshipId + ": " + validationError);
                return false;
            }
            const bool hasRecordTarget = !relationship.m_toRecordId.empty();
            const bool hasSubjectTarget = !relationship.m_targetSubjectRef.empty();
            if (hasRecordTarget == hasSubjectTarget)
            {
                SetError(error, "Catalog relationships require exactly one target record ID or unresolved subject reference: "
                    + relationship.m_relationshipId);
                return false;
            }
            if (!ValidateEvidenceForSubject(
                    relationship.m_evidenceIds,
                    "relationship:" + relationship.m_relationshipId,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Relationship evidence does not prove the exact association "
                    + relationship.m_relationshipId + ": " + validationError);
                return false;
            }
            for (const AZStd::string& usage : relationship.m_allowedUsages)
            {
                const CatalogGovernanceEvent* effective =
                    FindEffectiveGovernanceEvent(
                        "relationship",
                        relationship.m_relationshipId,
                        "permission",
                        usage);
                if (!effective || effective->m_newValue != "allow")
                {
                    SetError(error, "Allowed relationship usage has no current effective allow decision: "
                        + relationship.m_relationshipId + "/" + usage);
                    return false;
                }
            }
        }

        CatalogDatabase replay;
        for (const CatalogRecord& record : m_records)
        {
            if (!replay.InsertNew(record, &validationError))
            {
                SetError(error, "Catalog record replay failed: " + validationError);
                return false;
            }
        }
        for (const CatalogRelationship& relationship : m_relationships)
        {
            if (!replay.UpsertRelationship(relationship, &validationError))
            {
                SetError(error, "Catalog relationship replay failed: " + validationError);
                return false;
            }
        }
        for (const CatalogValidationEvent& validation : m_validationHistory)
        {
            if (!replay.AddValidationEventBound(
                    validation,
                    workspace,
                    profile,
                    sourceRegistry,
                    &validationError))
            {
                SetError(error, "Catalog validation-history integrity failed for "
                    + validation.m_validationId + ": " + validationError);
                return false;
            }
        }
        for (const CatalogGovernanceEvent& event : m_governanceHistory)
        {
            if (!replay.AddGovernanceEventBound(
                    event,
                    workspace,
                    profile,
                    sourceRegistry,
                    &validationError))
            {
                SetError(error, "Catalog governance-history integrity failed for "
                    + event.m_eventId + ": " + validationError);
                return false;
            }
        }

        for (const EconomyItemProfile& item : m_economyItems)
        {
            if (!ValidateEconomyItem(item, &validationError)
                || !IsFiniteNonNegative(item.m_weight)
                || !IsFiniteNonNegative(item.m_baseValue)
                || !IsFiniteNonNegative(item.m_durability))
            {
                SetError(error, "Economy item contains non-finite or invalid numeric state: "
                    + item.m_recordId);
                return false;
            }
            const CatalogRecord* record = FindByRecordId(item.m_recordId);
            if (!record
                || !ValidateEvidenceForSubject(
                    item.m_evidenceIds,
                    record->m_subjectRef,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Economy item evidence integrity failed: "
                    + item.m_recordId + ": " + validationError);
                return false;
            }
        }
        for (const EconomyRecipeProfile& recipe : m_economyRecipes)
        {
            if (!ValidateEconomyRecipe(recipe, &validationError))
            {
                SetError(error, "Economy recipe integrity failed: "
                    + recipe.m_recordId + ": " + validationError);
                return false;
            }
            const CatalogRecord* record = FindByRecordId(recipe.m_recordId);
            if (!record
                || !ValidateEvidenceForSubject(
                    recipe.m_evidenceIds,
                    record->m_subjectRef,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Economy recipe evidence integrity failed: "
                    + recipe.m_recordId + ": " + validationError);
                return false;
            }
        }
        for (const EconomyRecipeIngredient& ingredient : m_recipeIngredients)
        {
            if (!ValidateRecipeIngredient(ingredient, &validationError)
                || !ValidateEvidenceForSubject(
                    ingredient.m_evidenceIds,
                    "economy-recipe-ingredient:" + ingredient.m_linkId,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Recipe ingredient relationship evidence integrity failed: "
                    + ingredient.m_linkId + ": " + validationError);
                return false;
            }
        }
        for (const EconomyRecipeOutput& output : m_recipeOutputs)
        {
            if (!ValidateRecipeOutput(output, &validationError)
                || !IsFiniteProbability(output.m_chance)
                || !ValidateEvidenceForSubject(
                    output.m_evidenceIds,
                    "economy-recipe-output:" + output.m_linkId,
                    profile,
                    sourceRegistry,
                    false,
                    validationError))
            {
                SetError(error, "Recipe output relationship evidence integrity failed: "
                    + output.m_linkId + ": " + validationError);
                return false;
            }
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
