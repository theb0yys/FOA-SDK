/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

            if (record.m_recordKind == "item")
            {
                if (const EconomyItemProfile* itemProfile =
                        catalog.FindEconomyItem(record.m_recordId))
                {
                    if (!EvidenceSetIsValid(
                            itemProfile->m_evidenceIds,
                            record.m_subjectRef,
                            profile,
                            sourceRegistry))
                    {
                        reason =
                            "The typed item profile lacks exact profile-bound input evidence.";
                        return false;
                    }
                }
            }
            else if (record.m_recordKind == "recipe")
            {
                if (const EconomyRecipeProfile* recipeProfile =
                        catalog.FindEconomyRecipe(record.m_recordId))
                {
                    if (!EvidenceSetIsValid(
                            recipeProfile->m_evidenceIds,
                            record.m_subjectRef,
                            profile,
                            sourceRegistry))
                    {
                        reason =
                            "The typed recipe profile lacks exact profile-bound input evidence.";
                        return false;
                    }
                }

                for (const EconomyRecipeIngredient& ingredient :
                     catalog.FindIngredientsForRecipe(record.m_recordId))
                {
                    if (!EvidenceSetIsValid(
                            ingredient.m_evidenceIds,
                            "economy-recipe-ingredient:" + ingredient.m_linkId,
                            profile,
                            sourceRegistry))
                    {
                        reason =
                            "A typed recipe ingredient lacks evidence for the exact ingredient association, quantity, and direction: "
                            + ingredient.m_linkId;
                        return false;
                    }
                }

                for (const EconomyRecipeOutput& output :
                     catalog.FindOutputsForRecipe(record.m_recordId))
                {
                    if (!EvidenceSetIsValid(
                            output.m_evidenceIds,
                            "economy-recipe-output:" + output.m_linkId,
                            profile,
                            sourceRegistry))
                    {
                        reason =
                            "A typed recipe output lacks evidence for the exact output association, quantity, and probability: "
                            + output.m_linkId;
                        return false;
                    }
                }
            }
            return true;
        }

        bool RequiresTypedItemProfile(AdapterCapability capability)
        {
            return capability == AdapterCapability::CustomItemRegistration;
        }

        bool RequiresTypedRecipePayload(AdapterCapability capability)
        {
            return capability == AdapterCapability::RecipeAppend
                || capability == AdapterCapability::CustomRecipeRegistration;
        }

        bool RecordPayloadIsComplete(
            const CatalogRecord& record,
            AdapterCapability capability,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::string& reason)
        {
            if (!RecordInputEvidenceIsValid(
                    record,
                    profile,
                    sourceRegistry,
                    catalog,
                    reason))
            {
                return false;
            }
            if (RequiresTypedItemProfile(capability)
                && !catalog.FindEconomyItem(record.m_recordId))
            {
                reason =
                    "Custom item registration requires a typed item profile.";
                return false;
            }
            if (RequiresTypedRecipePayload(capability))
            {
                const EconomyRecipeProfile* recipeProfile =
                    catalog.FindEconomyRecipe(record.m_recordId);
                if (!recipeProfile)
                {
                    reason =
                        "Recipe append and custom registration require a typed recipe profile.";
                    return false;
                }
                if (catalog.FindOutputsForRecipe(record.m_recordId).empty())
                {
                    reason =
                        "Recipe append and custom registration require at least one typed output join.";
                    return false;
                }
            }
            return true;
        }

        AdapterWorkOrderStep BuildRecordStep(
            const AdapterWorkOrderPlan& plan,
            AdapterCapability capability,
            const ReadySubject& readySubject,
            const AdapterCapabilityMatrixRow& matrixRow,
            const CatalogDatabase& catalog)
        {
            const CatalogRecord& record = *readySubject.m_record;
            AdapterWorkOrderStep step;
            step.m_capability = ToString(capability);
            step.m_subjectKind = "record";
            step.m_subjectId = record.m_recordId;
            step.m_subjectRef = record.m_subjectRef;
            step.m_sourceRecordId = record.m_recordId;
            step.m_stepId = BuildStepId(
                plan.m_planId,
                step.m_capability,
                step.m_subjectKind,
                step.m_subjectId);
            step.m_inputEvidenceIds = CollectRecordInputEvidence(record, catalog);
            AddAllUnique(
                step.m_inputEvidenceIds,
                readySubject.m_validationEvidenceIds);
            step.m_declarationEvidenceIds = matrixRow.m_declarationEvidenceIds;
            step.m_permissionEventIds = readySubject.m_permissionEventIds;
            step.m_permissionEvidenceIds = readySubject.m_permissionEvidenceIds;
            step.m_validationProofIds = readySubject.m_validationIds;
            AddRecordArguments(step, record, catalog);
            SortUnique(step.m_inputEvidenceIds);
            SortUnique(step.m_declarationEvidenceIds);
            SortUnique(step.m_permissionEventIds);
            SortUnique(step.m_permissionEvidenceIds);
            SortUnique(step.m_validationProofIds);
            return step;
        }

        AdapterWorkOrderStep BuildRelationshipStep(
            const AdapterWorkOrderPlan& plan,
            AdapterCapability capability,
            const ReadySubject& readySubject,
            const CatalogRelationship& relationship,
            const CatalogRecord& targetRecord,
            const AZStd::vector<AZStd::string>&
                relationshipValidationEvidenceIds,
            const AZStd::vector<AZStd::string>& relationshipValidationIds,
            const AdapterCapabilityMatrixRow& matrixRow,
            const CatalogDatabase& catalog)
        {
            const CatalogRecord& record = *readySubject.m_record;
            AdapterWorkOrderStep step;
            step.m_capability = ToString(capability);
            step.m_subjectKind = "relationship";
            step.m_subjectId = relationship.m_relationshipId;
            step.m_subjectRef = "relationship:" + relationship.m_relationshipId;
            step.m_sourceRecordId = record.m_recordId;
            step.m_relationshipId = relationship.m_relationshipId;
            step.m_targetRecordId = targetRecord.m_recordId;
            step.m_targetSubjectRef = targetRecord.m_subjectRef;
            step.m_stepId = BuildStepId(
                plan.m_planId,
                step.m_capability,
                step.m_subjectKind,
                step.m_subjectId);
            step.m_inputEvidenceIds = CollectRecordInputEvidence(record, catalog);
            AddAllUnique(step.m_inputEvidenceIds, relationship.m_evidenceIds);
            AddAllUnique(
                step.m_inputEvidenceIds,
                CollectRecordInputEvidence(targetRecord, catalog));
            AddAllUnique(
                step.m_inputEvidenceIds,
                readySubject.m_validationEvidenceIds);
            AddAllUnique(
                step.m_inputEvidenceIds,
                relationshipValidationEvidenceIds);
            step.m_declarationEvidenceIds = matrixRow.m_declarationEvidenceIds;
            step.m_permissionEventIds = readySubject.m_permissionEventIds;
            step.m_permissionEvidenceIds = readySubject.m_permissionEvidenceIds;
            step.m_validationProofIds = readySubject.m_validationIds;
            AddAllUnique(step.m_validationProofIds, relationshipValidationIds);
            AddRecordArguments(step, record, catalog);
            AddArgument(
                step.m_arguments,
                "relationship_id",
                relationship.m_relationshipId);
            AddArgument(
                step.m_arguments,
                "relationship_kind",
                relationship.m_relationshipKind);
            AddArgument(
                step.m_arguments,
                "target_record_id",
                targetRecord.m_recordId);
            AddArgument(
                step.m_arguments,
                "target_subject_ref",
                targetRecord.m_subjectRef);
            AddArguments(
                step.m_arguments,
                "relationship_attribute",
                relationship.m_attributes);
            SortArguments(step.m_arguments);
            SortUnique(step.m_inputEvidenceIds);
            SortUnique(step.m_declarationEvidenceIds);
            SortUnique(step.m_permissionEventIds);
            SortUnique(step.m_permissionEvidenceIds);
            SortUnique(step.m_validationProofIds);
            return step;
        }
