/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "EncounterModels.h"
#include <AzCore/Serialization/SerializeContext.h>

namespace TaintedGrailModdingSDK
{
    void EncounterEntry::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<EncounterEntry>()->Version(1)
                ->Field("EntryId", &EncounterEntry::m_entryId)
                ->Field("TargetRecordId", &EncounterEntry::m_targetRecordId)
                ->Field("MinimumCount", &EncounterEntry::m_minimumCount)
                ->Field("MaximumCount", &EncounterEntry::m_maximumCount);
        }
    }
    void EncounterDefinition::Reflect(AZ::ReflectContext* context)
    {
        EncounterEntry::Reflect(context);
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<EncounterDefinition>()->Version(1)
                ->Field("RecordId", &EncounterDefinition::m_recordId)
                ->Field("PlacementRecordId", &EncounterDefinition::m_placementRecordId)
                ->Field("PlacementSubjectRef", &EncounterDefinition::m_placementSubjectRef)
                ->Field("ActivationMode", &EncounterDefinition::m_activationMode)
                ->Field("Conditions", &EncounterDefinition::m_conditions)
                ->Field("MaximumActiveInstances", &EncounterDefinition::m_maximumActiveInstances)
                ->Field("PopulationLimit", &EncounterDefinition::m_populationLimit)
                ->Field("UniqueEncounter", &EncounterDefinition::m_uniqueEncounter)
                ->Field("CleanupNotes", &EncounterDefinition::m_cleanupNotes)
                ->Field("RollbackNotes", &EncounterDefinition::m_rollbackNotes)
                ->Field("Entries", &EncounterDefinition::m_entries)
                ->Field("EvidenceIds", &EncounterDefinition::m_evidenceIds);
        }
    }
}
