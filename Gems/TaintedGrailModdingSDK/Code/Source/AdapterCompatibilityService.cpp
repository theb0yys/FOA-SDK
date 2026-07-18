/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterCompatibilityService.h"

#include <AzCore/std/algorithm.h>
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
            return AZStd::find(values.begin(), values.end(), value) != values.end();
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
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        const AdapterCapabilityDeclaration* FindCapabilityDeclaration(
            const AdapterDeclaration& declaration,
            AdapterCapability capability)
        {
            for (const AdapterCapabilityDeclaration& candidate : declaration.m_capabilities)
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
            if (evidenceIds.empty())
            {
                return false;
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                if (!evidence || evidence->m_subjectRef != expectedSubjectRef)
                {
                    return false;
                }
                const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source
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

        bool HasRelationshipForCapability(
            const CatalogRecord& record,
            AdapterCapability capability,
            const CatalogDatabase& catalog)
        {
            if (capability != AdapterCapability::VendorMutation
                && capability != AdapterCapability::LootMutation
                && capability != AdapterCapability::RewardMutation)
            {
                return true;
            }

            for (const CatalogRelationship& relationship
                : catalog.FindRelationshipsForRecord(record.m_recordId))
            {
                if (relationship.m_fromRecordId != record.m_recordId)
                {
                    continue;
                }
                if (!IsCurrentValidated(relationship))
                {
                    continue;
                }
                if (capability == AdapterCapability::VendorMutation
                    && relationship.m_relationshipKind == "sold_by")
                {
                    return true;
                }
                if (capability == AdapterCapability::LootMutation
                    && (relationship.m_relationshipKind == "dropped_by"
                        || relationship.m_relationshipKind == "found_at"))
                {
                    return true;
                }
                if (capability == AdapterCapability::RewardMutation
                    && (relationship.m_relationshipKind == "rewarded_by"
                        || relationship.m_relationshipKind == "granted_by"))
                {
                    return true;
                }
            }
            return false;
        }

        bool IsEligiblePermissionSubject(
            const CatalogRecord& record,
            AdapterCapability capability,
            const CatalogDatabase& catalog)
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
                return record.m_recordKind == "recipe" && !record.IsSynthetic();
            case AdapterCapability::RecipeAppend:
                return record.m_recordKind == "recipe";
            case AdapterCapability::CustomItemRegistration:
                return record.m_recordKind == "item" && record.IsSynthetic();
            case AdapterCapability::CustomRecipeRegistration:
                return record.m_recordKind == "recipe" && record.IsSynthetic();
            case AdapterCapability::VendorMutation:
            case AdapterCapability::LootMutation:
                return record.m_recordKind == "item"
                    && HasRelationshipForCapability(record, capability, catalog);
            case AdapterCapability::RewardMutation:
                return (record.m_recordKind == "item" || record.m_recordKind == "recipe")
                    && HasRelationshipForCapability(record, capability, catalog);
            case AdapterCapability::Persistence:
            case AdapterCapability::Cleanup:
            case AdapterCapability::Rollback:
                return false;
            }
            return false;
        }

        bool ValidationProofIsValid(
            const AZStd::string& validationId,
            const CatalogRecord& record,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::vector<AZStd::string>& evidenceIds)
        {
            const CatalogValidationEvent* validation = catalog.FindValidationById(validationId);
            if (!validation
                || validation->GetSubjectKind() != "record"
                || validation->GetSubjectId() != record.m_recordId
                || validation->m_state != "validated"
                || validation->m_profileId != profile.m_profileId
                || validation->m_gameVersion != profile.m_gameVersion
                || validation->m_branch != profile.m_branch
                || !EvidenceSetIsValid(
                    validation->m_evidenceIds,
                    record.m_subjectRef,
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
            for (const CatalogGovernanceEvent& event : catalog.GetGovernanceHistory())
            {
                if (event.m_subjectKind != "record"
                    || event.m_subjectId != record.m_recordId
                    || event.m_axis != "permission"
                    || event.m_usage != usage
                    || event.m_newValue != "allow"
                    || !EvidenceSetIsValid(
                        event.m_evidenceIds,
                        record.m_subjectRef,
                        profile,
                        sourceRegistry)
                    || event.m_validationIds.empty())
                {
                    continue;
                }

                bool allValidationProofValid = true;
                AZStd::vector<AZStd::string> validationEvidence;
                for (const AZStd::string& validationId : event.m_validationIds)
                {
                    if (!ValidationProofIsValid(
                        validationId,
                        record,
                        profile,
                        sourceRegistry,
                        catalog,
                        validationEvidence))
                    {
                        allValidationProofValid = false;
                        break;
                    }
                }
                if (allValidationProofValid)
                {
                    AddAllUnique(evidenceIds, event.m_evidenceIds);
                    AddAllUnique(evidenceIds, validationEvidence);
                    AddAllUnique(validationIds, event.m_validationIds);
                    return true;
                }
            }
            return false;
        }

        bool HasOpenBlocker(
            const CatalogRecord& record,
            const AZStd::string& usage,
            const AZStd::vector<BlockerRecord>& blockers,
            AZStd::vector<AZStd::string>& reasons)
        {
            bool blocked = false;
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status != "open" || blocker.m_subjectRef != record.m_subjectRef)
                {
                    continue;
                }
                if (!blocker.m_affectedUsages.empty()
                    && !Contains(blocker.m_affectedUsages, usage))
                {
                    continue;
                }
                blocked = true;
                AddUnique(reasons, blocker.m_reason);
            }
            return blocked;
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
            bool openBlockerFound = false;
            for (const CatalogRecord& record : catalog.GetRecords())
            {
                if (record.m_ownerPackId != pack.m_packId
                    || !IsEligiblePermissionSubject(record, capability, catalog))
                {
                    continue;
                }

                eligibleSubjectFound = true;
                AddUnique(result.m_subjectIds, record.m_recordId);
                if (!Contains(record.m_allowedUsages, usage))
                {
                    continue;
                }
                allowedSubjectFound = true;
                if (!IsCurrentValidated(record))
                {
                    continue;
                }

                AZStd::vector<AZStd::string> proofEvidenceIds;
                AZStd::vector<AZStd::string> validationIds;
                if (PermissionProofIsValid(
                    record,
                    usage,
                    profile,
                    sourceRegistry,
                    catalog,
                    proofEvidenceIds,
                    validationIds))
                {
                    if (HasOpenBlocker(record, usage, blockers, result.m_reasons))
                    {
                        openBlockerFound = true;
                        continue;
                    }
                    result.m_readiness = PermissionReadiness::Ready;
                    AddAllUnique(result.m_evidenceIds, proofEvidenceIds);
                    AddAllUnique(result.m_validationIds, validationIds);
                }
            }

            if (result.m_readiness == PermissionReadiness::Ready)
            {
                SortUnique(result.m_subjectIds);
                SortUnique(result.m_evidenceIds);
                SortUnique(result.m_validationIds);
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
                if (openBlockerFound)
                {
                    result.m_reasons.push_back(
                        "An open blocker prevents this permitted capability from being treated as ready: "
                        + usage);
                }
                else
                {
                    result.m_reasons.push_back(
                        "The required allowed usage exists, but no valid same-subject permission and "
                        "validation proof chain is available: " + usage);
                }
            }
            SortUnique(result.m_subjectIds);
            return result;
        }

        bool AdapterProofIsValid(
            const AdapterDeclaration& declaration,
            const AdapterCapabilityDeclaration& capability,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            const AZStd::string identitySubject = "adapter:" + declaration.m_adapterId;
            const AZStd::string capabilitySubject = identitySubject
                + ":capability:"
                + ToString(capability.m_capability);
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
                || !SupportsRuntimeTarget(declaration, profile->m_runtimeTarget))
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
                return row;
            }
            if (permission.m_readiness == PermissionReadiness::ProofMissing)
            {
                row.m_status = "proof_missing";
                return row;
            }
            if (!AdapterProofIsValid(
                    declaration,
                    *capabilityDeclaration,
                    *profile,
                    sourceRegistry))
            {
                row.m_status = "proof_missing";
                row.m_reasons.push_back(
                    "The adapter identity or capability declaration lacks exact profile-bound evidence.");
                return row;
            }

            row.m_status = "supported";
            row.m_reasons.push_back(
                "The adapter declaration, semantic version, runtime target, permission, and proof "
                "gates are satisfied.");
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
