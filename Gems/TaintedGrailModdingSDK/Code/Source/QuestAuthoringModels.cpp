/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "QuestAuthoringModels.h"
#include <AzCore/Serialization/SerializeContext.h>
namespace TaintedGrailModdingSDK
{
    void QuestLabel::Reflect(AZ::ReflectContext* c)
    { if (auto* s = azrtti_cast<AZ::SerializeContext*>(c)) { s->Class<QuestLabel>()->Version(1)->Field("Id", &QuestLabel::m_id)->Field("Text", &QuestLabel::m_text); } }
    void QuestStateKey::Reflect(AZ::ReflectContext* c)
    { if (auto* s = azrtti_cast<AZ::SerializeContext*>(c)) { s->Class<QuestStateKey>()->Version(1)->Field("KeyId", &QuestStateKey::m_keyId)->Field("Type", &QuestStateKey::m_type)->Field("DefaultValue", &QuestStateKey::m_defaultValue)->Field("Description", &QuestStateKey::m_description); } }
    void QuestSubjectBinding::Reflect(AZ::ReflectContext* c)
    { if (auto* s = azrtti_cast<AZ::SerializeContext*>(c)) { s->Class<QuestSubjectBinding>()->Version(1)->Field("SubjectId", &QuestSubjectBinding::m_subjectId)->Field("RecordId", &QuestSubjectBinding::m_recordId); } }
    void QuestAuthoringProfile::Reflect(AZ::ReflectContext* c)
    {
        QuestLabel::Reflect(c); QuestStateKey::Reflect(c); QuestSubjectBinding::Reflect(c);
        if (auto* s = azrtti_cast<AZ::SerializeContext*>(c)) { s->Class<QuestAuthoringProfile>()->Version(1)
            ->Field("RecordId", &QuestAuthoringProfile::m_recordId)->Field("DefinitionJson", &QuestAuthoringProfile::m_definitionJson)
            ->Field("Labels", &QuestAuthoringProfile::m_labels)->Field("StateKeys", &QuestAuthoringProfile::m_stateKeys)
            ->Field("Bindings", &QuestAuthoringProfile::m_bindings)->Field("EvidenceIds", &QuestAuthoringProfile::m_evidenceIds); }
    }
}
