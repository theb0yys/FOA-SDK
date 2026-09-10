/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace TaintedGrailModdingSDK
{
    inline constexpr AZ::u32 LegacyCatalogSchemaVersion = 1;
    inline constexpr AZ::u32 PopulationCatalogSchemaVersion = 2;

    // Schemas 1, 2, 3 and 4 remain load-only migration inputs. New documents use schema 5;
    // a schema-1 load retains its version until bound validation succeeds and
    // CatalogDatabase rebuilds it under the population-capable current schema.
    inline constexpr AZ::u32 EncounterCatalogSchemaVersion = 3;
    inline constexpr AZ::u32 SocietyCatalogSchemaVersion = 4;
    inline constexpr AZ::u32 WorldCatalogSchemaVersion = 5;
    inline constexpr AZ::u32 CurrentCatalogSchemaVersion =
        WorldCatalogSchemaVersion;

    enum class PopulationActorKind : AZ::u8
    {
        Npc,
        Creature,
        Animal,
        Construct,
        Other,
    };

    enum class PopulationTroopKind : AZ::u8
    {
        Party,
        Patrol,
        EncounterGroup,
        Reinforcement,
        Other,
    };

    enum class PopulationTroopMemberRole : AZ::u8
    {
        Leader,
        Melee,
        Ranged,
        Support,
        Specialist,
        Other,
    };

    AZ::Outcome<PopulationActorKind, AZStd::string> ParsePopulationActorKind(
        const AZStd::string& value);
    AZ::Outcome<PopulationTroopKind, AZStd::string> ParsePopulationTroopKind(
        const AZStd::string& value);
    AZ::Outcome<PopulationTroopMemberRole, AZStd::string>
    ParsePopulationTroopMemberRole(const AZStd::string& value);

    AZStd::string ToString(PopulationActorKind value);
    AZStd::string ToString(PopulationTroopKind value);
    AZStd::string ToString(PopulationTroopMemberRole value);

    struct PopulationActorProfile
    {
        AZ_TYPE_INFO(
            PopulationActorProfile,
            "{7EEAB43A-9C51-4080-AC7C-3E8B803BCBAD}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_recordId;
        AZStd::string m_actorKind;
        AZStd::string m_archetype;
        AZStd::string m_templateRecordId;
        AZStd::string m_templateSubjectRef;
        AZ::u32 m_minimumLevel = 0;
        AZ::u32 m_maximumLevel = 0;
        bool m_uniqueActor = false;
        bool m_essentialActor = false;
        bool m_persistentActor = false;
        AZStd::string m_localisationNameRef;
        AZStd::string m_localisationDescriptionRef;
        AZStd::string m_portraitAssetRef;
        AZStd::string m_modelAssetRef;
        AZStd::vector<AZStd::string> m_tags;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };

    struct PopulationTroopProfile
    {
        AZ_TYPE_INFO(
            PopulationTroopProfile,
            "{B4138679-5C64-4A8A-B4C3-B751473036AD}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_recordId;
        AZStd::string m_troopKind;
        AZStd::string m_leaderActorRecordId;
        AZStd::string m_leaderActorSubjectRef;
        AZ::u32 m_minimumSize = 1;
        AZ::u32 m_maximumSize = 1;
        AZStd::string m_formation;
        AZStd::vector<AZStd::string> m_tags;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };

    struct PopulationTroopMember
    {
        AZ_TYPE_INFO(
            PopulationTroopMember,
            "{29720556-EDA0-434E-B629-58114C3D99B7}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_linkId;
        AZStd::string m_troopRecordId;
        AZStd::string m_actorRecordId;
        AZStd::string m_actorSubjectRef;
        AZStd::string m_role;
        AZ::u32 m_minimumCount = 1;
        AZ::u32 m_maximumCount = 1;
        double m_weight = 1.0;
        bool m_required = false;
        AZStd::vector<AZStd::string> m_conditions;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
} // namespace TaintedGrailModdingSDK
