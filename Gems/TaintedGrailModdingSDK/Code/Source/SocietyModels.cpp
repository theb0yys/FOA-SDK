/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SocietyModels.h"
#include <AzCore/Serialization/SerializeContext.h>

namespace TaintedGrailModdingSDK
{
    void CultureProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<CultureProfile>()->Version(1)
                ->Field("RecordId", &CultureProfile::m_recordId)
                ->Field("Description", &CultureProfile::m_description)
                ->Field("Language", &CultureProfile::m_language)
                ->Field("EvidenceIds", &CultureProfile::m_evidenceIds);
        }
    }
    void FactionProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<FactionProfile>()->Version(1)
                ->Field("RecordId", &FactionProfile::m_recordId)
                ->Field("CultureRecordId", &FactionProfile::m_cultureRecordId)
                ->Field("Description", &FactionProfile::m_description)
                ->Field("AuthorityNotes", &FactionProfile::m_authorityNotes)
                ->Field("EvidenceIds", &FactionProfile::m_evidenceIds);
        }
    }
    void FactionLink::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<FactionLink>()->Version(1)
                ->Field("LinkId", &FactionLink::m_linkId)
                ->Field("FactionRecordId", &FactionLink::m_factionRecordId)
                ->Field("Kind", &FactionLink::m_kind)
                ->Field("TargetRecordId", &FactionLink::m_targetRecordId)
                ->Field("TargetSubjectRef", &FactionLink::m_targetSubjectRef)
                ->Field("Value", &FactionLink::m_value)
                ->Field("Notes", &FactionLink::m_notes)
                ->Field("EvidenceIds", &FactionLink::m_evidenceIds);
        }
    }
}
