/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "../CapabilityExecutionValidation.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace CE = CapabilityExecution;
    // Strict readers of M1 canonical bytes. Decoding establishes consistency, never admission.
    bool Decode(const AZStd::string& json, CE::CapabilityDescriptorV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityProviderBindingV1& output);
    bool Decode(const AZStd::string& json, CE::ArtifactReferenceV1& output);
    bool Decode(const AZStd::string& json, CE::ExpectedArtifactV1& output);
    bool Decode(const AZStd::string& json, CE::ArtifactRecordV1& output);
    bool Decode(const AZStd::string& json, CE::OptionV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionRequestV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilitySupportDecisionV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityQualificationDecisionV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityEnvironmentDecisionV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityPolicyDecisionV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityAuthorizationReceiptV1& output);
    bool Decode(const AZStd::string& json, CE::TargetMutationClaimV1& output);
    bool Decode(const AZStd::string& json, CE::RollbackStepV1& output);
    bool Decode(const AZStd::string& json, CE::RollbackPlanV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityPhasePlanV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionPlanV1& output);
    bool Decode(const AZStd::string& json, CE::FailureRecordV1& output);
    bool Decode(const AZStd::string& json, CE::DiagnosticReferenceV1& output);
    bool Decode(const AZStd::string& json, CE::TargetObservationV1& output);
    bool Decode(const AZStd::string& json, CE::PhaseExtensionReferenceV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityPhaseReceiptV1& output);
    bool Decode(const AZStd::string& json, CE::RollbackStepReceiptV1& output);
    bool Decode(const AZStd::string& json, CE::RollbackReceiptV1& output);
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionReceiptV1& output);
    template<class T>
    bool Seal(T& value)
    {
        auto encoded = CE::Canonicalize(value);
        if (!encoded.IsSuccess())
        {
            return false;
        }
        value.m_fingerprint = encoded.GetValue().m_fingerprint;
        return CE::Validate(value).IsSuccess();
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
