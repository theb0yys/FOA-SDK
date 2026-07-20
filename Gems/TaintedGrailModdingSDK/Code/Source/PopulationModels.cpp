/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PopulationModels.h"

#include <AzCore/Serialization/SerializeContext.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class EnumType>
        struct EnumName
        {
            EnumType m_value;
            const char* m_name;
        };

        constexpr EnumName<PopulationActorKind> ActorKinds[] = {
            { PopulationActorKind::Npc, "npc" },
            { PopulationActorKind::Creature, "creature" },
            { PopulationActorKind::Animal, "animal" },
            { PopulationActorKind::Construct, "construct" },
            { PopulationActorKind::Other, "other" },
        };

        constexpr EnumName<PopulationTroopKind> TroopKinds[] = {
            { PopulationTroopKind::Party, "party" },
            { PopulationTroopKind::Patrol, "patrol" },
            { PopulationTroopKind::EncounterGroup, "encounter_group" },
            { PopulationTroopKind::Reinforcement, "reinforcement" },
            { PopulationTroopKind::Other, "other" },
        };

        constexpr EnumName<PopulationTroopMemberRole> MemberRoles[] = {
            { PopulationTroopMemberRole::Leader, "leader" },
            { PopulationTroopMemberRole::Melee, "melee" },
            { PopulationTroopMemberRole::Ranged, "ranged" },
            { PopulationTroopMemberRole::Support, "support" },
            { PopulationTroopMemberRole::Specialist, "specialist" },
            { PopulationTroopMemberRole::Other, "other" },
        };

        template<class EnumType, size_t Count>
        AZ::Outcome<EnumType, AZStd::string> ParseClosedValue(
            const AZStd::string& value,
            const EnumName<EnumType> (&names)[Count],
            const char* label)
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_name)
                {
                    return AZ::Success(name.m_value);
                }
            }
            return AZ::Failure(
                AZStd::string("Unsupported ") + label + " value: " + value);
        }

        template<class EnumType, size_t Count>
        AZStd::string ClosedValueToString(
            EnumType value,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_value)
                {
                    return name.m_name;
                }
            }
            return "other";
        }
    } // namespace

    AZ::Outcome<PopulationActorKind, AZStd::string> ParsePopulationActorKind(
        const AZStd::string& value)
    {
        return ParseClosedValue(value, ActorKinds, "population actor kind");
    }

    AZ::Outcome<PopulationTroopKind, AZStd::string> ParsePopulationTroopKind(
        const AZStd::string& value)
    {
        return ParseClosedValue(value, TroopKinds, "population troop kind");
    }

    AZ::Outcome<PopulationTroopMemberRole, AZStd::string>
    ParsePopulationTroopMemberRole(const AZStd::string& value)
    {
        return ParseClosedValue(value, MemberRoles, "population troop member role");
    }

    AZStd::string ToString(PopulationActorKind value)
    {
        return ClosedValueToString(value, ActorKinds);
    }

    AZStd::string ToString(PopulationTroopKind value)
    {
        return ClosedValueToString(value, TroopKinds);
    }

    AZStd::string ToString(PopulationTroopMemberRole value)
    {
        return ClosedValueToString(value, MemberRoles);
    }

    void PopulationActorProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PopulationActorProfile>()
                ->Version(1)
                ->Field("RecordId", &PopulationActorProfile::m_recordId)
                ->Field("ActorKind", &PopulationActorProfile::m_actorKind)
                ->Field("Archetype", &PopulationActorProfile::m_archetype)
                ->Field("TemplateRecordId", &PopulationActorProfile::m_templateRecordId)
                ->Field("TemplateSubjectRef", &PopulationActorProfile::m_templateSubjectRef)
                ->Field("MinimumLevel", &PopulationActorProfile::m_minimumLevel)
                ->Field("MaximumLevel", &PopulationActorProfile::m_maximumLevel)
                ->Field("UniqueActor", &PopulationActorProfile::m_uniqueActor)
                ->Field("EssentialActor", &PopulationActorProfile::m_essentialActor)
                ->Field("PersistentActor", &PopulationActorProfile::m_persistentActor)
                ->Field("LocalisationNameRef", &PopulationActorProfile::m_localisationNameRef)
                ->Field(
                    "LocalisationDescriptionRef",
                    &PopulationActorProfile::m_localisationDescriptionRef)
                ->Field("PortraitAssetRef", &PopulationActorProfile::m_portraitAssetRef)
                ->Field("ModelAssetRef", &PopulationActorProfile::m_modelAssetRef)
                ->Field("Tags", &PopulationActorProfile::m_tags)
                ->Field("EvidenceIds", &PopulationActorProfile::m_evidenceIds);
        }
    }

    void PopulationTroopProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PopulationTroopProfile>()
                ->Version(1)
                ->Field("RecordId", &PopulationTroopProfile::m_recordId)
                ->Field("TroopKind", &PopulationTroopProfile::m_troopKind)
                ->Field(
                    "LeaderActorRecordId",
                    &PopulationTroopProfile::m_leaderActorRecordId)
                ->Field(
                    "LeaderActorSubjectRef",
                    &PopulationTroopProfile::m_leaderActorSubjectRef)
                ->Field("MinimumSize", &PopulationTroopProfile::m_minimumSize)
                ->Field("MaximumSize", &PopulationTroopProfile::m_maximumSize)
                ->Field("Formation", &PopulationTroopProfile::m_formation)
                ->Field("Tags", &PopulationTroopProfile::m_tags)
                ->Field("EvidenceIds", &PopulationTroopProfile::m_evidenceIds);
        }
    }

    void PopulationTroopMember::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PopulationTroopMember>()
                ->Version(1)
                ->Field("LinkId", &PopulationTroopMember::m_linkId)
                ->Field("TroopRecordId", &PopulationTroopMember::m_troopRecordId)
                ->Field("ActorRecordId", &PopulationTroopMember::m_actorRecordId)
                ->Field("ActorSubjectRef", &PopulationTroopMember::m_actorSubjectRef)
                ->Field("Role", &PopulationTroopMember::m_role)
                ->Field("MinimumCount", &PopulationTroopMember::m_minimumCount)
                ->Field("MaximumCount", &PopulationTroopMember::m_maximumCount)
                ->Field("Weight", &PopulationTroopMember::m_weight)
                ->Field("Required", &PopulationTroopMember::m_required)
                ->Field("Conditions", &PopulationTroopMember::m_conditions)
                ->Field("EvidenceIds", &PopulationTroopMember::m_evidenceIds);
        }
    }
} // namespace TaintedGrailModdingSDK
