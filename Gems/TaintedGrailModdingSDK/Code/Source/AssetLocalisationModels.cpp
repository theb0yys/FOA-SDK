/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetLocalisationModels.h"
#include <AzCore/Serialization/SerializeContext.h>
namespace TaintedGrailModdingSDK
{
    void ProjectAssetProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        {
            s->Class<ProjectAssetProfile>()->Version(1)
                ->Field("RecordId", &ProjectAssetProfile::m_recordId)
                ->Field("SourcePath", &ProjectAssetProfile::m_sourcePath)
                ->Field("Fingerprint", &ProjectAssetProfile::m_fingerprint)
                ->Field("MediaType", &ProjectAssetProfile::m_mediaType)
                ->Field("Provenance", &ProjectAssetProfile::m_provenance)
                ->Field("SourceRights", &ProjectAssetProfile::m_sourceRights)
                ->Field("Licence", &ProjectAssetProfile::m_licence)
                ->Field("Redistribution", &ProjectAssetProfile::m_redistribution)
                ->Field("ByteSize", &ProjectAssetProfile::m_byteSize)
                ->Field("Width", &ProjectAssetProfile::m_width)->Field("Height", &ProjectAssetProfile::m_height)
                ->Field("EvidenceIds", &ProjectAssetProfile::m_evidenceIds);
        }
    }
    void LocalisationVariant::Reflect(AZ::ReflectContext* context)
    {
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        { s->Class<LocalisationVariant>()->Version(1)->Field("Language", &LocalisationVariant::m_language)->Field("Text", &LocalisationVariant::m_text); }
    }
    void LocalisationEntry::Reflect(AZ::ReflectContext* context)
    {
        LocalisationVariant::Reflect(context);
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        {
            s->Class<LocalisationEntry>()->Version(1)->Field("RecordId", &LocalisationEntry::m_recordId)
                ->Field("Key", &LocalisationEntry::m_key)->Field("DefaultLanguage", &LocalisationEntry::m_defaultLanguage)
                ->Field("Variants", &LocalisationEntry::m_variants)->Field("EvidenceIds", &LocalisationEntry::m_evidenceIds);
        }
    }
    void PresentationBinding::Reflect(AZ::ReflectContext* context)
    {
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(context))
        {
            s->Class<PresentationBinding>()->Version(1)->Field("BindingId", &PresentationBinding::m_bindingId)
                ->Field("OwnerPackId", &PresentationBinding::m_ownerPackId)->Field("TargetRecordId", &PresentationBinding::m_targetRecordId)
                ->Field("Slot", &PresentationBinding::m_slot)->Field("ValueRecordId", &PresentationBinding::m_valueRecordId)
                ->Field("EvidenceIds", &PresentationBinding::m_evidenceIds);
        }
    }
}
