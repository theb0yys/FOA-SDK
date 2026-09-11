/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "CapabilityExecutionContracts.h"

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    struct CanonicalValueV1
    {
        AZStd::string m_json;
        AZStd::string m_fingerprint;
    };
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityDescriptorV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityProviderBindingV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ArtifactReferenceV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ExpectedArtifactV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ArtifactRecordV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const OptionV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionRequestV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilitySupportDecisionV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityQualificationDecisionV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityEnvironmentDecisionV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPolicyDecisionV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityAuthorizationReceiptV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const TargetMutationClaimV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackStepV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackPlanV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPhasePlanV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionPlanV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const FailureRecordV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const DiagnosticReferenceV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const TargetObservationV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const PhaseExtensionReferenceV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPhaseReceiptV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackStepReceiptV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackReceiptV1& value);
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionReceiptV1& value);

    //! Creates a reference only to an already fingerprinted supplied value. No lookup occurs.
    template<class T>
    AZ::Outcome<UpstreamReferenceV1, AZStd::string> Reference(const T& value)
    {
        auto canonical = Canonicalize(value);
        if (!canonical.IsSuccess()) { return AZ::Failure(canonical.GetError()); }
        if (value.m_fingerprint != canonical.GetValue().m_fingerprint)
        { return AZ::Failure(AZStd::string("Upstream value fingerprint does not match its canonical projection.")); }
        UpstreamReferenceV1 reference;
        reference.m_id = value.m_id; reference.m_kind = Kind(value);
        reference.m_canonicalJson = canonical.GetValue().m_json;
        reference.m_fingerprint = canonical.GetValue().m_fingerprint;
        return AZ::Success(AZStd::move(reference));
    }
}
