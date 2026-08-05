/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorAppearancePreviewService.h"

#include "CatalogDatabase.h"
#include "EconomyModels.h"
#include "FoundationModels.h"
#include "PopulationModels.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        const AZStd::vector<AZStd::pair<AZStd::string, AZStd::string>>& SlotKinds()
        {
            static const AZStd::vector<AZStd::pair<AZStd::string, AZStd::string>> values = {
                { "equips_head", "head" },
                { "equips_torso", "torso" },
                { "equips_hands", "hands" },
                { "equips_legs", "legs" },
                { "equips_feet", "feet" },
                { "equips_main_hand", "main_hand" },
                { "equips_off_hand", "off_hand" },
                { "equips_two_hand", "two_hand" },
                { "equips_back", "back" },
                { "equips_accessory", "accessory" },
                { "equips_other", "other" },
            };
            return values;
        }

        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool IsRelationshipCurrent(const CatalogRelationship& relationship)
        {
            return relationship.m_supersededByRelationshipId.empty()
                && relationship.m_stalenessState != "stale"
                && relationship.m_validationState != "failed"
                && relationship.m_validationState != "blocked"
                && !relationship.IsBlocked();
        }

        void AddBlocker(AZStd::vector<AZStd::string>& blockers, AZStd::string blocker)
        {
            if (!Contains(blockers, blocker))
            {
                blockers.push_back(AZStd::move(blocker));
            }
        }
    } // namespace

    bool ActorAppearancePreviewService::IsEquipmentRelationshipKind(
        const AZStd::string& relationshipKind)
    {
        return !SlotForRelationshipKind(relationshipKind).empty();
    }

    AZStd::string ActorAppearancePreviewService::SlotForRelationshipKind(
        const AZStd::string& relationshipKind)
    {
        for (const auto& value : SlotKinds())
        {
            if (value.first == relationshipKind)
            {
                return value.second;
            }
        }
        return {};
    }

    const AZStd::vector<AZStd::string>& ActorAppearancePreviewService::OrderedSlots()
    {
        static const AZStd::vector<AZStd::string> slots = []
        {
            AZStd::vector<AZStd::string> result;
            result.reserve(SlotKinds().size());
            for (const auto& value : SlotKinds())
            {
                result.push_back(value.second);
            }
            return result;
        }();
        return slots;
    }

    ActorAppearancePreviewView ActorAppearancePreviewService::BuildView(
        const CatalogDatabase& catalog,
        const AZStd::string& actorRecordId)
    {
        ActorAppearancePreviewView view;
        view.m_actorRecordId = actorRecordId;

        const CatalogRecord* actorRecord = catalog.FindByRecordId(actorRecordId);
        if (!actorRecord || actorRecord->m_domain != "population"
            || actorRecord->m_recordKind != "actor")
        {
            AddBlocker(view.m_blockers, "Selected record is not a canonical population actor.");
            return view;
        }
        view.m_actorSubjectRef = actorRecord->m_subjectRef;

        const PopulationActorProfile* actorProfile =
            catalog.FindPopulationActorProfile(actorRecordId);
        if (!actorProfile)
        {
            AddBlocker(view.m_blockers, "The actor has no typed PopulationActorProfile.");
            return view;
        }
        view.m_portraitAssetRef = actorProfile->m_portraitAssetRef;
        view.m_modelAssetRef = actorProfile->m_modelAssetRef;
        if (view.m_portraitAssetRef.empty())
        {
            AddBlocker(view.m_blockers, "The actor portrait reference is unbound.");
        }
        if (view.m_modelAssetRef.empty())
        {
            AddBlocker(view.m_blockers, "The actor model reference is unbound.");
        }

        AZStd::unordered_map<AZStd::string, size_t> currentSlotCounts;
        bool hasMainHand = false;
        bool hasOffHand = false;
        bool hasTwoHand = false;

        for (const CatalogRelationship& relationship : catalog.GetRelationships())
        {
            ++view.m_relationshipsExamined;
            if (relationship.m_fromRecordId != actorRecordId
                || !IsEquipmentRelationshipKind(relationship.m_relationshipKind))
            {
                continue;
            }

            ActorEquipmentPreviewEntry entry;
            entry.m_slot = SlotForRelationshipKind(relationship.m_relationshipKind);
            entry.m_relationshipId = relationship.m_relationshipId;
            entry.m_itemRecordId = relationship.m_toRecordId;
            entry.m_itemSubjectRef = relationship.m_targetSubjectRef;
            entry.m_evidenceIds = relationship.m_evidenceIds;
            entry.m_current = IsRelationshipCurrent(relationship);

            const CatalogRecord* itemRecord = relationship.m_toRecordId.empty()
                ? nullptr
                : catalog.FindByRecordId(relationship.m_toRecordId);
            if (!itemRecord)
            {
                AddBlocker(entry.m_blockers, "Equipment target does not resolve to a canonical item.");
            }
            else if (itemRecord->m_domain != "economy" || itemRecord->m_recordKind != "item")
            {
                AddBlocker(entry.m_blockers, "Equipment target is not a canonical economy item.");
            }
            else
            {
                if (!relationship.m_targetSubjectRef.empty()
                    && relationship.m_targetSubjectRef != itemRecord->m_subjectRef)
                {
                    AddBlocker(entry.m_blockers, "Equipment target record and exact subject reference disagree.");
                }
                const EconomyItemProfile* itemProfile = catalog.FindEconomyItem(itemRecord->m_recordId);
                if (!itemProfile)
                {
                    AddBlocker(entry.m_blockers, "Equipment item has no typed EconomyItemProfile.");
                }
                else
                {
                    entry.m_itemAssetRef = itemProfile->m_assetRef;
                    if (entry.m_itemAssetRef.empty())
                    {
                        AddBlocker(entry.m_blockers, "Equipment item asset reference is unbound.");
                    }
                }
            }
            if (entry.m_evidenceIds.empty())
            {
                AddBlocker(entry.m_blockers, "Equipment relationship has no evidence IDs.");
            }
            if (!entry.m_current)
            {
                AddBlocker(entry.m_blockers, "Equipment relationship is stale, superseded, failed, or blocked.");
            }

            if (entry.m_current)
            {
                const size_t count = ++currentSlotCounts[entry.m_slot];
                if (count > 1)
                {
                    AddBlocker(entry.m_blockers, "Multiple current relationships occupy the same equipment slot.");
                }
                hasMainHand = hasMainHand || entry.m_slot == "main_hand";
                hasOffHand = hasOffHand || entry.m_slot == "off_hand";
                hasTwoHand = hasTwoHand || entry.m_slot == "two_hand";
            }
            view.m_equipment.push_back(AZStd::move(entry));
        }

        if (hasTwoHand && (hasMainHand || hasOffHand))
        {
            AddBlocker(view.m_blockers, "The two-hand slot conflicts with a current main-hand or off-hand relationship.");
        }
        for (const auto& slotCount : currentSlotCounts)
        {
            if (slotCount.second > 1)
            {
                AddBlocker(view.m_blockers, "At least one equipment slot has multiple current relationships.");
                break;
            }
        }

        AZStd::sort(
            view.m_equipment.begin(),
            view.m_equipment.end(),
            [](const ActorEquipmentPreviewEntry& left, const ActorEquipmentPreviewEntry& right)
            {
                const auto& slots = OrderedSlots();
                const auto leftPosition = AZStd::find(slots.begin(), slots.end(), left.m_slot);
                const auto rightPosition = AZStd::find(slots.begin(), slots.end(), right.m_slot);
                if (leftPosition != rightPosition)
                {
                    return leftPosition < rightPosition;
                }
                return left.m_relationshipId < right.m_relationshipId;
            });

        bool entryBlocked = false;
        for (const ActorEquipmentPreviewEntry& entry : view.m_equipment)
        {
            entryBlocked = entryBlocked || !entry.m_blockers.empty();
        }
        if (!view.m_blockers.empty() || entryBlocked)
        {
            view.m_state = ActorAppearancePreviewState::Blocked;
        }
        else if (view.m_equipment.empty())
        {
            view.m_state = ActorAppearancePreviewState::Partial;
        }
        else
        {
            // The first Stage 8 cohort never claims reconstructed actor composition.
            view.m_state = ActorAppearancePreviewState::Partial;
        }
        return view;
    }
} // namespace TaintedGrailModdingSDK
