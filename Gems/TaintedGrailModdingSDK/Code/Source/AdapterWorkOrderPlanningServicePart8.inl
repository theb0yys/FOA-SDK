/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

                    if (capability == AdapterCapability::VendorMutation
                        || capability == AdapterCapability::LootMutation
                        || capability == AdapterCapability::RewardMutation)
                    {
                        if (readySubject.m_relationships.empty())
                        {
                            planningErrors.push_back(
                                readySubject.m_record->m_recordId
                                + ": no exact current relationship remained for capability "
                                + ToString(capability));
                            continue;
                        }
                        for (const CatalogRelationship* relationship : readySubject.m_relationships)
                        {
                            const AZStd::string usage = PermissionUsage(capability);
                            if (HasOpenRelationshipBlocker(
                                    *relationship,
                                    usage,
                                    blockers))
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": an open error blocker applies to the exact relationship usage.");
                                continue;
                            }
                            if (relationship->m_toRecordId.empty())
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": canonical plans require a resolved target record ID.");
                                continue;
                            }
                            const CatalogRecord* targetRecord = catalog.FindByRecordId(
                                relationship->m_toRecordId);
                            if (!targetRecord)
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": the resolved target record does not exist.");
                                continue;
                            }
                            if (!IsCurrentValidated(*targetRecord))
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": the resolved target record is not current and validated.");
                                continue;
                            }
                            AZStd::string targetPayloadError;
                            if (!RecordPayloadIsComplete(
                                    *targetRecord,
                                    capability,
                                    *profile,
                                    sourceRegistry,
                                    catalog,
                                    targetPayloadError))
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": target input evidence is invalid: "
                                    + targetPayloadError);
                                continue;
                            }
                            if (HasOpenBlocker(
                                    *targetRecord,
                                    usage,
                                    blockers))
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": an open error blocker applies to the resolved target record usage.");
                                continue;
                            }
                            AZStd::vector<AZStd::string> relationshipValidationEvidenceIds;
                            AZStd::vector<AZStd::string> relationshipValidationIds;
                            if (!CollectRelationshipValidationProof(
                                    *relationship,
                                    *readySubject.m_record,
                                    *targetRecord,
                                    *profile,
                                    sourceRegistry,
                                    catalog,
                                    relationshipValidationEvidenceIds,
                                    relationshipValidationIds))
                            {
                                planningErrors.push_back(
                                    relationship->m_relationshipId
                                    + ": no exact relationship-bound validation proof is available.");
                                continue;
                            }
                            plan.m_steps.push_back(BuildRelationshipStep(
                                plan,
                                capability,
                                readySubject,
                                *relationship,
                                *targetRecord,
                                relationshipValidationEvidenceIds,
                                relationshipValidationIds,
                                *matrixRow,
                                catalog));
                        }
                    }
                    else
                    {
                        plan.m_steps.push_back(BuildRecordStep(
                            plan,
                            capability,
                            readySubject,
                            *matrixRow,
                            catalog));
                    }
                }
            }

            if (!planningErrors.empty() || plan.m_steps.empty())
            {
                AdapterWorkOrderRefusal refusal = BuildCompatibilityRefusal(group, profile);
                refusal.m_failedCapabilities.push_back("planning_payload");
                refusal.m_compatibilityStatuses.push_back("planning_payload:refused");
                AddAllUnique(refusal.m_reasons, planningErrors);
                if (plan.m_steps.empty())
                {
                    refusal.m_reasons.push_back(
                        "A canonical plan cannot be published without at least one deterministic step.");
                }
                SortUnique(refusal.m_failedCapabilities);
                SortUnique(refusal.m_compatibilityStatuses);
                SortUnique(refusal.m_reasons);
                result.m_refusals.push_back(AZStd::move(refusal));
                continue;
            }

            SortSteps(plan.m_steps);
            plan.m_canonicalJson = SerializeCanonicalPlan(plan);
            result.m_stepCount += static_cast<AZ::u64>(plan.m_steps.size());
            result.m_plans.push_back(AZStd::move(plan));
        }

        AZStd::sort(
            result.m_plans.begin(),
            result.m_plans.end(),
            [](const AdapterWorkOrderPlan& left, const AdapterWorkOrderPlan& right)
            {
                return left.m_planId < right.m_planId;
            });
        AZStd::sort(
            result.m_refusals.begin(),
            result.m_refusals.end(),
            [](const AdapterWorkOrderRefusal& left, const AdapterWorkOrderRefusal& right)
            {
                return left.m_planId < right.m_planId;
            });
        result.m_generatedPlanCount = static_cast<AZ::u64>(result.m_plans.size());
        result.m_refusedPlanCount = static_cast<AZ::u64>(result.m_refusals.size());
        return result;
    }

    AZStd::string AdapterWorkOrderPlanningService::SerializeCanonicalPlan(
        const AdapterWorkOrderPlan& plan) const
    {
        AdapterWorkOrderPlan canonicalPlan = plan;
        SortSteps(canonicalPlan.m_steps);
        AZStd::string output;
        output += "{\"FormatVersion\":";
        AppendUnsigned(output, canonicalPlan.m_formatVersion);
        output += ",\"PlanId\":";
        AppendJsonString(output, canonicalPlan.m_planId);
        output += ",\"PackId\":";
        AppendJsonString(output, canonicalPlan.m_packId);
        output += ",\"PackVersion\":";
        AppendJsonString(output, canonicalPlan.m_packVersion);
        output += ",\"AdapterId\":";
        AppendJsonString(output, canonicalPlan.m_adapterId);
        output += ",\"AdapterVersion\":";
        AppendJsonString(output, canonicalPlan.m_adapterVersion);
        output += ",\"RequiredAdapterVersion\":";
        AppendJsonString(output, canonicalPlan.m_requiredAdapterVersion);
        output += ",\"ProfileId\":";
        AppendJsonString(output, canonicalPlan.m_profileId);
        output += ",\"GameVersion\":";
        AppendJsonString(output, canonicalPlan.m_gameVersion);
        output += ",\"Branch\":";
        AppendJsonString(output, canonicalPlan.m_branch);
        output += ",\"RuntimeTarget\":";
        AppendJsonString(output, canonicalPlan.m_runtimeTarget);
        output += ",\"ExecutionAllowed\":";
        output += canonicalPlan.m_executionAllowed ? "true" : "false";
        output += ",\"Steps\":[";
