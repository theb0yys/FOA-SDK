/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ { class ReflectContext; }
namespace TaintedGrailModdingSDK
{
    struct EncounterEntry
    {
        AZ_TYPE_INFO(EncounterEntry, "{1824B7A2-97FA-4C34-82D8-D7E061A5F304}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_entryId;
        AZStd::string m_targetRecordId;
        AZ::u32 m_minimumCount = 1;
        AZ::u32 m_maximumCount = 1;
    };

    struct EncounterDefinition
    {
        AZ_TYPE_INFO(EncounterDefinition, "{BD8F3B2C-75A6-4F02-8C6A-E924074D7AF3}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_recordId;
        AZStd::string m_placementRecordId;
        AZStd::string m_placementSubjectRef;
        AZStd::string m_activationMode = "manual";
        AZStd::vector<AZStd::string> m_conditions;
        AZ::u32 m_maximumActiveInstances = 1;
        AZ::u32 m_populationLimit = 1000;
        bool m_uniqueEncounter = false;
        AZStd::string m_cleanupNotes;
        AZStd::string m_rollbackNotes;
        AZStd::vector<EncounterEntry> m_entries;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
}
