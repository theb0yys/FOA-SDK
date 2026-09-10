/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "QuestDefinitionContract.h"
#include <AzCore/RTTI/RTTI.h>
namespace AZ { class ReflectContext; }
namespace TaintedGrailModdingSDK
{
    struct QuestLabel
    {
        AZ_TYPE_INFO(QuestLabel, "{A5FB47A2-C21C-4577-9CCC-BC91B7F6DB52}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_id, m_text;
    };
    struct QuestStateKey
    {
        AZ_TYPE_INFO(QuestStateKey, "{EC64C121-07B5-4773-92FC-5014C26C4E2A}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_keyId, m_type, m_defaultValue, m_description;
    };
    struct QuestSubjectBinding
    {
        AZ_TYPE_INFO(QuestSubjectBinding, "{3F25882A-A89E-4632-A3C0-D5E69B5A1602}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_subjectId, m_recordId;
    };
    struct QuestAuthoringProfile
    {
        AZ_TYPE_INFO(QuestAuthoringProfile, "{A43B80A2-E644-43B8-A9B6-D97C0392D0BE}");
        static void Reflect(AZ::ReflectContext*);
        AZStd::string m_recordId, m_definitionJson;
        AZStd::vector<QuestLabel> m_labels;
        AZStd::vector<QuestStateKey> m_stateKeys;
        AZStd::vector<QuestSubjectBinding> m_bindings;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct QuestAuthoringDraft
    {
        QuestDefinitionV1 m_definition;
        AZStd::vector<QuestLabel> m_labels;
        AZStd::vector<QuestStateKey> m_stateKeys;
        AZStd::vector<QuestSubjectBinding> m_bindings;
    };
    struct QuestInspection
    {
        AZStd::vector<AZStd::string> m_errors, m_warnings;
        bool IsValid() const { return m_errors.empty(); }
    };
}
