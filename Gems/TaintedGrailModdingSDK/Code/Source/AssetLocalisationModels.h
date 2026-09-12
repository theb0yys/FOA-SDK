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
    struct ProjectAssetProfile
    {
        AZ_TYPE_INFO(ProjectAssetProfile, "{2511D22B-1DE7-4D31-851F-6044EAB05F49}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_recordId, m_sourcePath, m_fingerprint, m_mediaType;
        AZStd::string m_provenance, m_sourceRights, m_licence, m_redistribution = "not_reviewed";
        AZ::u64 m_byteSize = 0;
        AZ::u32 m_width = 0, m_height = 0;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct LocalisationVariant
    {
        AZ_TYPE_INFO(LocalisationVariant, "{542A406C-DC72-40D8-8247-F1967AD12554}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_language, m_text;
    };
    struct LocalisationEntry
    {
        AZ_TYPE_INFO(LocalisationEntry, "{A8E86E87-73BF-4F5D-907A-A76F8BD97A03}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_recordId, m_key, m_defaultLanguage = "en";
        AZStd::vector<LocalisationVariant> m_variants;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct PresentationBinding
    {
        AZ_TYPE_INFO(PresentationBinding, "{D745ACCE-89E5-4083-BD3B-1310791EF176}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_bindingId, m_ownerPackId, m_targetRecordId, m_slot, m_valueRecordId;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct LocalisedText
    {
        AZStd::string m_text, m_language, m_error;
        bool m_usedFallback = false;
        bool IsSuccess() const { return m_error.empty(); }
    };
}
