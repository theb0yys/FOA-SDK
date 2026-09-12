/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ { class ReflectContext; }
namespace TaintedGrailModdingSDK
{
    struct CultureProfile
    {
        AZ_TYPE_INFO(CultureProfile, "{DA394C5A-A323-4B8F-BA94-580D90829629}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_recordId;
        AZStd::string m_description;
        AZStd::string m_language;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct FactionProfile
    {
        AZ_TYPE_INFO(FactionProfile, "{82630C3F-0ED7-4A15-AEC1-4F251CD7E35A}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_recordId;
        AZStd::string m_cultureRecordId;
        AZStd::string m_description;
        AZStd::string m_authorityNotes;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct FactionLink
    {
        AZ_TYPE_INFO(FactionLink, "{7440DABC-9D62-44C0-85A8-206A4B1E9E22}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_linkId;
        AZStd::string m_factionRecordId;
        AZStd::string m_kind;
        AZStd::string m_targetRecordId;
        AZStd::string m_targetSubjectRef;
        AZStd::string m_value;
        AZStd::string m_notes;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    //! Complete authoring command; canonical persistence stores profiles and links separately.
    struct FactionDefinition
    {
        FactionProfile m_profile;
        AZStd::vector<FactionLink> m_links;
    };
    struct FactionAnalysis
    {
        AZ::u32 m_members = 0;
        AZ::u32 m_dispositions = 0;
        AZ::u32 m_jurisdictions = 0;
        AZStd::string m_leaderName;
        AZStd::vector<AZStd::string> m_errors;
        AZStd::vector<AZStd::string> m_warnings;
        bool IsValid() const { return m_errors.empty(); }
    };
}
