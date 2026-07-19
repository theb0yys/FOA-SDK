/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

        for (size_t index = 0; index < canonicalPlan.m_steps.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const AdapterWorkOrderStep& step = canonicalPlan.m_steps[index];
            output += "{\"StepId\":";
            AppendJsonString(output, step.m_stepId);
            output += ",\"Sequence\":";
            AppendUnsigned(output, step.m_sequence);
            output += ",\"Capability\":";
            AppendJsonString(output, step.m_capability);
            output += ",\"SubjectKind\":";
            AppendJsonString(output, step.m_subjectKind);
            output += ",\"SubjectId\":";
            AppendJsonString(output, step.m_subjectId);
            output += ",\"SubjectRef\":";
            AppendJsonString(output, step.m_subjectRef);
            output += ",\"SourceRecordId\":";
            AppendJsonString(output, step.m_sourceRecordId);
            output += ",\"RelationshipId\":";
            AppendJsonString(output, step.m_relationshipId);
            output += ",\"TargetRecordId\":";
            AppendJsonString(output, step.m_targetRecordId);
            output += ",\"TargetSubjectRef\":";
            AppendJsonString(output, step.m_targetSubjectRef);
            output += ",\"ExecutionAllowed\":";
            output += step.m_executionAllowed ? "true" : "false";
            output += ",\"Arguments\":[";
            for (size_t argumentIndex = 0; argumentIndex < step.m_arguments.size(); ++argumentIndex)
            {
                if (argumentIndex != 0)
                {
                    output.push_back(',');
                }
                output += "{\"Key\":";
                AppendJsonString(output, step.m_arguments[argumentIndex].m_key);
                output += ",\"Value\":";
                AppendJsonString(output, step.m_arguments[argumentIndex].m_value);
                output.push_back('}');
            }
            output += "],\"InputEvidenceIds\":";
            AppendJsonStringArray(output, step.m_inputEvidenceIds);
            output += ",\"DeclarationEvidenceIds\":";
            AppendJsonStringArray(output, step.m_declarationEvidenceIds);
            output += ",\"PermissionEventIds\":";
            AppendJsonStringArray(output, step.m_permissionEventIds);
            output += ",\"PermissionEvidenceIds\":";
            AppendJsonStringArray(output, step.m_permissionEvidenceIds);
            output += ",\"ValidationProofIds\":";
            AppendJsonStringArray(output, step.m_validationProofIds);
            output.push_back('}');
        }
        output += "]}";
        return output;
    }
} // namespace TaintedGrailModdingSDK
