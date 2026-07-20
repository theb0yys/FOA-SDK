/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterCompatibilityService.h"

#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        enum class PermissionReadiness
        {
            NotRequired,
            Missing,
            ProofMissing,
            Ready,
        };

        struct PermissionEvaluation
        {
            PermissionReadiness m_readiness = PermissionReadiness::Missing;
            AZStd::vector<AZStd::string> m_subjectIds;
            AZStd::vector<AZStd::string> m_evidenceIds;
            AZStd::vector<AZStd::string> m_validationIds;
            AZStd::vector<AZStd::string> m_reasons;
        };

        const AdapterCapability AllCapabilities[] = {
            AdapterCapability::ItemGrant,
            AdapterCapability::RecipeLearn,
            AdapterCapability::RecipeAppend,
            AdapterCapability::CustomItemRegistration,
            AdapterCapability::CustomRecipeRegistration,
            AdapterCapability::VendorMutation,
            AdapterCapability::LootMutation,
            AdapterCapability::RewardMutation,
            AdapterCapability::Persistence,
            AdapterCapability::Cleanup,
            AdapterCapability::Rollback,
        };

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        void AddAllUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::vector<AZStd::string>& additions)
        {
            for (const AZStd::string& value : additions)
            {
                AddUnique(values, value);
            }
        }

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(
                AZStd::unique(values.begin(), values.end()),
                values.end());
        }

        const AdapterCapabilityDeclaration* FindCapabilityDeclaration(
            const AdapterDeclaration& declaration,
            AdapterCapability capability)
        {
            for (const AdapterCapabilityDeclaration& candidate :
                 declaration.m_capabilities)
            {
                if (candidate.m_capability == capability)
                {
                    return &candidate;
                }
            }
            return nullptr;
        }

        bool SupportsRuntimeTarget(
            const AdapterDeclaration& declaration,
            const AZStd::string& runtimeTarget)
        {
            return Contains(declaration.m_runtimeTargets, runtimeTarget);
        }

        bool IsCurrentValidated(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
        }

        bool IsCurrentValidated(const CatalogRelationship& relationship)
        {
            return relationship.m_validationState == "validated"
                && relationship.m_stalenessState == "current"
                && relationship.m_missingRefs.empty()
                && relationship.m_conflictRefs.empty()
                && relationship.m_supersededByRelationshipId.empty();
        }

        bool EvidenceSetIsValid(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& expectedSubjectRef,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            if (evidenceIds.empty() || expectedSubjectRef.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> uniqueIds = evidenceIds;
            SortUnique(uniqueIds);
            if (uniqueIds.size() != evidenceIds.size())
            {
                return false;
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence
                    || evidence->m_subjectRef != expectedSubjectRef
                    || evidence->m_claim.empty()
                    || evidence->m_evidenceKind.empty()
                    || evidence->m_locator.empty()
                    || evidence->m_recordPath.empty()
                    || !IsStrictUtcTimestamp(evidence->m_extractedAt)
                    || !IsSha256Fingerprint(evidence->m_sourceFingerprint))
                {
                    return false;
                }
                const SourceRecord* source =
                    sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source
                    || !IsUsableImportStatus(source->m_importStatus)
                    || evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != source->m_profileId
                    || evidence->m_gameVersion != source->m_gameVersion
                    || evidence->m_branch != source->m_branch
                    || source->m_profileId != profile.m_profileId
                    || source->m_gameVersion != profile.m_gameVersion
                    || source->m_branch != profile.m_branch
                    || source->m_runtimeTarget != profile.m_runtimeTarget)
                {
                    return false;
                }
            }
            return true;
        }

        AZStd::string PermissionUsage(AdapterCapability capability)
        {
            switch (capability)
            {
            case AdapterCapability::ItemGrant:
                return "existing_item_grant";
            case AdapterCapability::RecipeLearn:
                return "existing_recipe_learn";
            case AdapterCapability::RecipeAppend:
                return "runtime_recipe_append";
            case AdapterCapability::CustomItemRegistration:
                return "custom_item_registration";
            case AdapterCapability::CustomRecipeRegistration:
                return "custom_recipe_registration";
            case AdapterCapability::VendorMutation:
            case AdapterCapability::LootMutation:
                return "vendor_or_loot_injection";
            case AdapterCapability::RewardMutation:
                return "quest_or_contract_reward_injection";
            case AdapterCapability::Persistence:
            case AdapterCapability::Cleanup:
            case AdapterCapability::Rollback:
                return {};
            }
            return {};
        }

        bool PermissionIsRequired(AdapterCapability capability)
        {
            return capability != AdapterCapability::Persistence
                && capability != AdapterCapability::Cleanup
                && capability != AdapterCapability::Rollback;
        }

        bool RelationshipKindMatchesCapability(
            const CatalogRelationship& relationship,
            AdapterCapability capability)
        {
            if (capability == AdapterCapability::VendorMutation)
            {
                return relationship.m_relationshipKind == "sold_by";
            }
            if (capability == AdapterCapability::LootMutation)
            {
                return relationship.m_relationshipKind == "dropped_by"
                    || relationship.m_relationshipKind == "found_at";
            }
            if (capability == AdapterCapability::RewardMutation)
            {
                return relationship.m_relationshipKind == "rewarded_by"
                    || relationship.m_relationshipKind == "granted_by";
            }
            return false;
        }

        bool CapabilityUsesRelationships(AdapterCapability capability)
        {
            return capability == AdapterCapability::VendorMutation
                || capability == AdapterCapability::LootMutation
                || capability == AdapterCapability::RewardMutation;
        }

        bool IsEligiblePermissionSubject(
            const CatalogRecord& record,
            AdapterCapability capability)
        {
            if (record.m_domain != "economy")
            {
                return false;
            }
            switch (capability)
            {
            case AdapterCapability::ItemGrant:
                return record.m_recordKind == "item" && !record.IsSynthetic();
            case AdapterCapability::RecipeLearn:
                return record.m_recordKind == "recipe"
                    && !record.IsSynthetic();
            case AdapterCapability::RecipeAppend:
                return record.m_recordKind == "recipe";
            case AdapterCapability::CustomItemRegistration:
                return record.m_recordKind == "item" && record.IsSynthetic();
            case AdapterCapability::CustomRecipeRegistration:
                return record.m_recordKind == "recipe" && record.IsSynthetic();
            case AdapterCapability::VendorMutation:
            case AdapterCapability::LootMutation:
                return record.m_recordKind == "item";
            case AdapterCapability::RewardMutation:
                return record.m_recordKind == "item"
                    || record.m_recordKind == "recipe";
            case AdapterCapability::Persistence:
            case AdapterCapability::Cleanup:
            case AdapterCapability::Rollback:
                return false;
            }
            return false;
        }

        bool HasOpenErrorBlockerForSubject(
            const AZStd::string& subjectRef,
            const AZStd::string& usage,
            const AZStd::vector<BlockerRecord>& blockers,
            AZStd::vector<AZStd::string>& reasons)
        {
            if (subjectRef.empty() || usage.empty())
            {
                return false;
            }
            bool blocked = false;
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status != "open"
                    || blocker.m_severity != "error"
                    || blocker.m_subjectRef != subjectRef
                    || blocker.m_affectedUsages.empty()
                    || !Contains(blocker.m_affectedUsages, usage))
                {
                    continue;
                }
                blocked = true;
                AddUnique(reasons, blocker.m_reason);
            }
            return blocked;
        }

        bool ValidationProofIsValid(
            const AZStd::string& validationId,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& expectedSubjectRef,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::vector<AZStd::string>& evidenceIds)
        {
            const CatalogValidationEvent* validation =
                catalog.FindValidationById(validationId);
            if (!validation
                || validation->GetSubjectKind() != subjectKind
                || validation->GetSubjectId() != subjectId
                || validation->m_state != "validated"
                || validation->m_profileId != profile.m_profileId
                || validation->m_gameVersion != profile.m_gameVersion
                || validation->m_branch != profile.m_branch
                || !IsStrictUtcTimestamp(validation->m_checkedAt)
                || !EvidenceSetIsValid(
                    validation->m_evidenceIds,
                    expectedSubjectRef,
                    profile,
                    sourceRegistry))
            {
                return false;
            }
            AddAllUnique(evidenceIds, validation->m_evidenceIds);
            return true;
        }

        bool PermissionProofIsValid(
            const CatalogRecord& record,
            const AZStd::string& usage,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::vector<AZStd::string>& evidenceIds,
            AZStd::vector<AZStd::string>& validationIds)
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
                || event->m_validationIds.empty()
                || !EvidenceSetIsValid(
                    event->m_evidenceIds,
                    record.m_subjectRef,
                    profile,
                    sourceRegistry))
            {
                return false;
            }

            AZStd::vector<AZStd::string> validationEvidence;
            for (const AZStd::string& validationId : event->m_validationIds)
            {
                if (!ValidationProofIsValid(
                        validationId,
                        "record",
                        record.m_recordId,
                        record.m_subjectRef,
                        profile,
                        sourceRegistry,
                        catalog,
                        validationEvidence))
                {
                    return false;
                }
            }
            const CatalogValidationEvent* latest =
                catalog.FindLatestValidationForSubject(
                    "record",
                    record.m_recordId);
            if (!latest
                || latest->m_state != "validated"
                || !Contains(event->m_validationIds, latest->m_validationId))
            {
                return false;
            }

            AddAllUnique(evidenceIds, event->m_evidenceIds);
            AddAllUnique(evidenceIds, validationEvidence);
            AddAllUnique(validationIds, event->m_validationIds);
            SortUnique(evidenceIds);
            SortUnique(validationIds);
            return true;
        }

        bool RelationshipProofIsValid(
            const CatalogRelationship& relationship,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::vector<AZStd::string>& evidenceIds,
            AZStd::vector<AZStd::string>& validationIds)
        {
            const AZStd::string subjectRef =
                "relationship:" + relationship.m_relationshipId;
            if (!IsCurrentValidated(relationship)
                || relationship.m_toRecordId.empty()
                || !relationship.m_targetSubjectRef.empty()
                || !EvidenceSetIsValid(
                    relationship.m_evidenceIds,
                    subjectRef,
                    profile,
                    sourceRegistry))
            {
                return false;
            }
            const CatalogValidationEvent* latest =
                catalog.FindLatestValidationForSubject(
                    "relationship",
                    relationship.m_relationshipId);
            if (!latest
                || !ValidationProofIsValid(
                    latest->m_validationId,
                    "relationship",
                    relationship.m_relationshipId,
                    subjectRef,
                    profile,
                    sourceRegistry,
                    catalog,
                    evidenceIds))
            {
                return false;
            }
            AddAllUnique(evidenceIds, relationship.m_evidenceIds);
            AddUnique(validationIds, latest->m_validationId);
            return true;
        }

        bool HasSafeRelationshipForCapability(
            const CatalogRecord& sourceRecord,
            AdapterCapability capability,
            const AZStd::string& usage,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers,
            AZStd::vector<AZStd::string>& evidenceIds,
            AZStd::vector<AZStd::string>& validationIds,
            AZStd::vector<AZStd::string>& reasons)
        {
            bool found = false;
            for (const CatalogRelationship& relationship :
                 catalog.FindRelationshipsForRecord(sourceRecord.m_recordId))
            {
                if (relationship.m_fromRecordId != sourceRecord.m_recordId
                    || !RelationshipKindMatchesCapability(
                        relationship,
                        capability))
                {
                    continue;
                }

                AZStd::vector<AZStd::string> relationEvidence;
                AZStd::vector<AZStd::string> relationValidation;
                if (!RelationshipProofIsValid(
                        relationship,
                        profile,
                        sourceRegistry,
                        catalog,
                        relationEvidence,
                        relationValidation))
                {
                    continue;
                }
                AZStd::vector<AZStd::string> relationshipReasons;
                if (HasOpenErrorBlockerForSubject(
                        relationship.m_relationshipId,
                        usage,
                        blockers,
                        relationshipReasons)
                    || HasOpenErrorBlockerForSubject(
                        "relationship:" + relationship.m_relationshipId,
                        usage,
                        blockers,
                        relationshipReasons))
                {
                    AddAllUnique(reasons, relationshipReasons);
                    continue;
                }

                const CatalogRecord* target =
                    catalog.FindByRecordId(relationship.m_toRecordId);
                if (!target
                    || !IsCurrentValidated(*target)
                    || !EvidenceSetIsValid(
                        target->m_evidenceIds,
                        target->m_subjectRef,
                        profile,
                        sourceRegistry))
                {
                    continue;
                }
                AZStd::vector<AZStd::string> targetReasons;
                if (HasOpenErrorBlockerForSubject(
                        target->m_subjectRef,
                        usage,
                        blockers,
                        targetReasons))
                {
                    AddAllUnique(reasons, targetReasons);
                    continue;
                }

                found = true;
                AddAllUnique(evidenceIds, relationEvidence);
                AddAllUnique(evidenceIds, target->m_evidenceIds);
                AddAllUnique(validationIds, relationValidation);
            }
            SortUnique(evidenceIds);
            SortUnique(validationIds);
            SortUnique(reasons);
            return found;
        }

        PermissionEvaluation EvaluatePermission(
            const PackManifest& pack,
            AdapterCapability capability,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            PermissionEvaluation result;
            if (!PermissionIsRequired(capability))
            {
                result.m_readiness = PermissionReadiness::NotRequired;
                return result;
            }

            const AZStd::string usage = PermissionUsage(capability);
            bool eligibleSubjectFound = false;
            bool allowedSubjectFound = false;
            bool currentSubjectFound = false;
            bool proofFound = false;
            bool blockerFound = false;
            for (const CatalogRecord& record : catalog.GetRecords())
            {
                if (record.m_ownerPackId != pack.m_packId
                    || !IsEligiblePermissionSubject(record, capability))
                {
                    continue;
                }
                eligibleSubjectFound = true;
                if (!Contains(record.m_allowedUsages, usage))
                {
                    continue;
                }
                allowedSubjectFound = true;
                if (!IsCurrentValidated(record)
                    || !EvidenceSetIsValid(
                        record.m_evidenceIds,
                        record.m_subjectRef,
                        profile,
                        sourceRegistry))
                {
                    continue;
                }
                currentSubjectFound = true;

                AZStd::vector<AZStd::string> proofEvidenceIds;
                AZStd::vector<AZStd::string> proofValidationIds;
                if (!PermissionProofIsValid(
                        record,
                        usage,
                        profile,
                        sourceRegistry,
                        catalog,
                        proofEvidenceIds,
                        proofValidationIds))
                {
                    continue;
                }
                proofFound = true;

                AZStd::vector<AZStd::string> subjectReasons;
                if (HasOpenErrorBlockerForSubject(
                        record.m_subjectRef,
                        usage,
                        blockers,
                        subjectReasons))
                {
                    blockerFound = true;
                    AddAllUnique(result.m_reasons, subjectReasons);
                    continue;
                }

                if (CapabilityUsesRelationships(capability))
                {
                    AZStd::vector<AZStd::string> relationshipEvidence;
                    AZStd::vector<AZStd::string> relationshipValidation;
                    AZStd::vector<AZStd::string> relationshipReasons;
                    if (!HasSafeRelationshipForCapability(
                            record,
                            capability,
                            usage,
                            profile,
                            sourceRegistry,
                            catalog,
                            blockers,
                            relationshipEvidence,
                            relationshipValidation,
                            relationshipReasons))
                    {
                        AddAllUnique(result.m_reasons, relationshipReasons);
                        continue;
                    }
                    AddAllUnique(proofEvidenceIds, relationshipEvidence);
                    AddAllUnique(proofValidationIds, relationshipValidation);
                }

                result.m_readiness = PermissionReadiness::Ready;
                AddUnique(result.m_subjectIds, record.m_recordId);
                AddAllUnique(result.m_evidenceIds, proofEvidenceIds);
                AddAllUnique(result.m_validationIds, proofValidationIds);
            }

            if (result.m_readiness == PermissionReadiness::Ready)
            {
                SortUnique(result.m_subjectIds);
                SortUnique(result.m_evidenceIds);
                SortUnique(result.m_validationIds);
                SortUnique(result.m_reasons);
                return result;
            }
            if (!eligibleSubjectFound || !allowedSubjectFound)
            {
                result.m_readiness = PermissionReadiness::Missing;
                result.m_reasons.push_back(
                    "No eligible current pack-owned catalog subject has the required allowed usage: "
                    + usage);
            }
            else
            {
                result.m_readiness = PermissionReadiness::ProofMissing;
                if (!currentSubjectFound)
                {
                    result.m_reasons.push_back(
                        "Allowed subjects exist, but none has current exact-subject evidence and validation state: "
                        + usage);
                }
                else if (!proofFound)
                {
                    result.m_reasons.push_back(
                        "Allowed subjects exist, but no current effective permission and validation proof chain is available: "
                        + usage);
                }
                else if (blockerFound)
                {
                    result.m_reasons.push_back(
                        "An open error blocker applies to every otherwise proven subject for usage: "
                        + usage);
                }
                else
                {
                    result.m_reasons.push_back(
                        "No exact relationship and target proof chain remains ready for usage: "
                        + usage);
                }
            }
            result.m_subjectIds.clear();
            SortUnique(result.m_reasons);
            return result;
        }

        bool AdapterProofIsValid(
            const AdapterDeclaration& declaration,
            const AdapterCapabilityDeclaration& capability,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            const AZStd::string identitySubject =
                "adapter:" + declaration.m_adapterId;
            const AZStd::string capabilitySubject = identitySubject
                + ":capability:" + ToString(capability.m_capability);
            return EvidenceSetIsValid(
                    declaration.m_identityEvidenceIds,
                    identitySubject,
                    profile,
                    sourceRegistry)
                && EvidenceSetIsValid(
                    capability.m_evidenceIds,
                    capabilitySubject,
                    profile,
                    sourceRegistry);
        }

        void CountRow(
            AdapterCapabilityMatrix& matrix,
            const AdapterCapabilityMatrixRow& row)
        {
            ++matrix.m_rowCount;
            if (row.m_status == "supported")
            {
                ++matrix.m_supportedCount;
            }
            else if (row.m_status == "unsupported")
            {
                ++matrix.m_unsupportedCount;
            }
            else if (row.m_status == "version_mismatch")
            {
                ++matrix.m_versionMismatchCount;
            }
            else if (row.m_status == "permission_missing")
            {
                ++matrix.m_permissionMissingCount;
            }
            else if (row.m_status == "proof_missing")
            {
                ++matrix.m_proofMissingCount;
            }
        }

        AdapterCapabilityMatrixRow BuildUnsupportedRow(
            const PackManifest& pack,
            AdapterCapability capability,
            const GameProfile* profile)
        {
            AdapterCapabilityMatrixRow row;
            row.m_packId = pack.m_packId;
            row.m_requiredAdapterVersion = pack.m_requiredAdapterVersion;
            row.m_adapterId = "none";
            row.m_runtimeTarget = profile ? profile->m_runtimeTarget : "unknown";
            row.m_capability = ToString(capability);
            row.m_status = "unsupported";
            row.m_reasons.push_back(
                "No typed adapter declaration is registered for compatibility evaluation.");
            return row;
        }

        AdapterCapabilityMatrixRow BuildRow(
            const PackManifest& pack,
            const AdapterDeclaration& declaration,
            AdapterCapability capability,
            const GameProfile* profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            AdapterCapabilityMatrixRow row;
            row.m_packId = pack.m_packId;
            row.m_requiredAdapterVersion = pack.m_requiredAdapterVersion;
            row.m_adapterId = declaration.m_adapterId;
            row.m_adapterVersion = declaration.m_version;
            row.m_runtimeTarget = profile ? profile->m_runtimeTarget : "unknown";
            row.m_capability = ToString(capability);

            const AdapterCapabilityDeclaration* capabilityDeclaration =
                FindCapabilityDeclaration(declaration, capability);
            if (!profile
                || !capabilityDeclaration
                || !SupportsRuntimeTarget(
                    declaration,
                    profile->m_runtimeTarget))
            {
                row.m_status = "unsupported";
                row.m_reasons.push_back(
                    "The adapter declaration does not support this capability for the active runtime target.");
                return row;
            }

            row.m_declarationEvidenceIds = declaration.m_identityEvidenceIds;
            AddAllUnique(
                row.m_declarationEvidenceIds,
                capabilityDeclaration->m_evidenceIds);
            SortUnique(row.m_declarationEvidenceIds);

            if (!IsAdapterVersionCompatible(
                    pack.m_requiredAdapterVersion,
                    declaration.m_version))
            {
                row.m_status = "version_mismatch";
                row.m_reasons.push_back(
                    "The declared adapter semantic version is outside the pack requirement compatibility policy.");
                return row;
            }

            const PermissionEvaluation permission = EvaluatePermission(
                pack,
                capability,
                *profile,
                sourceRegistry,
                catalog,
                blockers);
            row.m_subjectIds = permission.m_subjectIds;
            row.m_permissionEvidenceIds = permission.m_evidenceIds;
            row.m_validationProofIds = permission.m_validationIds;
            AddAllUnique(row.m_reasons, permission.m_reasons);

            if (permission.m_readiness == PermissionReadiness::Missing)
            {
                row.m_status = "permission_missing";
                row.m_subjectIds.clear();
                return row;
            }
            if (permission.m_readiness == PermissionReadiness::ProofMissing)
            {
                row.m_status = "proof_missing";
                row.m_subjectIds.clear();
                return row;
            }
            if (!AdapterProofIsValid(
                    declaration,
                    *capabilityDeclaration,
                    *profile,
                    sourceRegistry))
            {
                row.m_status = "proof_missing";
                row.m_subjectIds.clear();
                row.m_reasons.push_back(
                    "The adapter identity or capability declaration lacks exact profile-bound usable-source evidence.");
                return row;
            }

            row.m_status = "supported";
            row.m_reasons.push_back(
                "Only the listed exact subjects passed adapter declaration, semantic version, runtime target, effective permission, exact evidence, relationship, validation, and blocker gates.");
            SortUnique(row.m_subjectIds);
            SortUnique(row.m_declarationEvidenceIds);
            SortUnique(row.m_permissionEvidenceIds);
            SortUnique(row.m_validationProofIds);
            SortUnique(row.m_reasons);
            return row;
        }
    } // namespace

    AdapterCapabilityMatrix AdapterCompatibilityService::BuildCapabilityMatrix(
        const WorkspaceModel& workspace,
        const AZStd::vector<PackManifest>& packs,
        const AdapterContractRegistry& adapterRegistry,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        AdapterCapabilityMatrix matrix;
        const GameProfile* profile = workspace.FindActiveGameProfile();
        const AZStd::vector<AdapterDeclaration>& declarations =
            adapterRegistry.GetDeclarations();

        for (const PackManifest& pack : packs)
        {
            if (declarations.empty())
            {
                for (AdapterCapability capability : AllCapabilities)
                {
                    AdapterCapabilityMatrixRow row =
                        BuildUnsupportedRow(pack, capability, profile);
                    CountRow(matrix, row);
                    matrix.m_rows.push_back(AZStd::move(row));
                }
                continue;
            }

            for (const AdapterDeclaration& declaration : declarations)
            {
                for (AdapterCapability capability : AllCapabilities)
                {
                    AdapterCapabilityMatrixRow row = BuildRow(
                        pack,
                        declaration,
                        capability,
                        profile,
                        sourceRegistry,
                        catalog,
                        blockers);
                    CountRow(matrix, row);
                    matrix.m_rows.push_back(AZStd::move(row));
                }
            }
        }

        AZStd::sort(
            matrix.m_rows.begin(),
            matrix.m_rows.end(),
            [](const AdapterCapabilityMatrixRow& left,
               const AdapterCapabilityMatrixRow& right)
            {
                if (left.m_packId != right.m_packId)
                {
                    return left.m_packId < right.m_packId;
                }
                if (left.m_adapterId != right.m_adapterId)
                {
                    return left.m_adapterId < right.m_adapterId;
                }
                return left.m_capability < right.m_capability;
            });
        return matrix;
    }
} // namespace TaintedGrailModdingSDK
