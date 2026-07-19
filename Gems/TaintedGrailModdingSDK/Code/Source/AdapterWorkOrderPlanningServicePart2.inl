/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

        bool EvidenceSetIsValid(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& expectedSubjectRef,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            return EvidenceSetIsValidForSubjects(
                evidenceIds,
                { expectedSubjectRef },
                profile,
                sourceRegistry);
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

        bool RelationshipMatchesCapability(
            const CatalogRelationship& relationship,
            AdapterCapability capability)
        {
            if (!IsCurrentValidated(relationship))
            {
                return false;
            }
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

        AZStd::vector<const CatalogRelationship*> FindRelationshipsForCapability(
            const CatalogRecord& record,
            AdapterCapability capability,
            const CatalogDatabase& catalog)
        {
            AZStd::vector<const CatalogRelationship*> relationships;
            if (capability != AdapterCapability::VendorMutation
                && capability != AdapterCapability::LootMutation
                && capability != AdapterCapability::RewardMutation)
            {
                return relationships;
            }
            for (const CatalogRelationship& relationship : catalog.GetRelationships())
            {
                if (relationship.m_fromRecordId == record.m_recordId
                    && RelationshipMatchesCapability(relationship, capability))
                {
                    relationships.push_back(&relationship);
                }
            }
            AZStd::sort(
                relationships.begin(),
                relationships.end(),
                [](const CatalogRelationship* left, const CatalogRelationship* right)
                {
                    return left->m_relationshipId < right->m_relationshipId;
                });
            return relationships;
        }

        bool IsEligibleSubject(
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
                    && !FindRelationshipsForCapability(record, capability, catalog).empty();
            case AdapterCapability::RewardMutation:
                return (record.m_recordKind == "item" || record.m_recordKind == "recipe")
                    && !FindRelationshipsForCapability(record, capability, catalog).empty();
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
            AZStd::vector<AZStd::string>& validationEvidenceIds)
        {
            const CatalogValidationEvent* validation = catalog.FindValidationById(validationId);
            if (!validation
                || validation->GetSubjectKind() != "record"
                || validation->GetSubjectId() != record.m_recordId
                || validation->m_state != "validated"
                || validation->m_profileId != profile.m_profileId
                || validation->m_gameVersion != profile.m_gameVersion
                || validation->m_branch != profile.m_branch
                || !IsStrictUtcTimestamp(validation->m_checkedAt)
                || !EvidenceSetIsValid(
                    validation->m_evidenceIds,
                    record.m_subjectRef,
                    profile,
                    sourceRegistry))
            {
                return false;
            }
            AddAllUnique(validationEvidenceIds, validation->m_evidenceIds);
            return true;
        }

        bool CollectRelationshipValidationProof(
            const CatalogRelationship& relationship,
            const CatalogRecord&,
            const CatalogRecord&,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::vector<AZStd::string>& evidenceIds,
            AZStd::vector<AZStd::string>& validationIds)
        {
            const AZStd::vector<AZStd::string> allowedSubjectRefs = {
                "relationship:" + relationship.m_relationshipId,
            };
            if (!EvidenceSetIsValidForSubjects(
                    relationship.m_evidenceIds,
                    allowedSubjectRefs,
                    profile,
                    sourceRegistry))
            {
                return false;
            }
            AddAllUnique(evidenceIds, relationship.m_evidenceIds);

            bool validProofFound = false;
            for (const CatalogValidationEvent& validation : catalog.GetValidationHistory())
            {
                if (validation.GetSubjectKind() != "relationship"
                    || validation.GetSubjectId() != relationship.m_relationshipId
                    || validation.m_state != "validated"
                    || validation.m_profileId != profile.m_profileId
                    || validation.m_gameVersion != profile.m_gameVersion
                    || validation.m_branch != profile.m_branch
                    || !IsStrictUtcTimestamp(validation.m_checkedAt))
                {
                    continue;
                }
