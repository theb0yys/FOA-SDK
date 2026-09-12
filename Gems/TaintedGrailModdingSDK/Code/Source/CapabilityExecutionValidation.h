/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "CapabilityExecutionCanonical.h"

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    using ContractValidation = AZ::Outcome<void, AZStd::string>;
    //! Contract-valid only. These functions neither evaluate policy nor execute any operation.
    ContractValidation Validate(const CapabilityDescriptorV1& value);
    ContractValidation Validate(const CapabilityProviderBindingV1& value);
    ContractValidation Validate(const ArtifactReferenceV1& value);
    ContractValidation Validate(const ExpectedArtifactV1& value);
    ContractValidation Validate(const ArtifactRecordV1& value);
    ContractValidation Validate(const OptionV1& value);
    ContractValidation Validate(const CapabilityExecutionRequestV1& value);
    ContractValidation Validate(const CapabilitySupportDecisionV1& value);
    ContractValidation Validate(const CapabilityQualificationDecisionV1& value);
    ContractValidation Validate(const CapabilityEnvironmentDecisionV1& value);
    ContractValidation Validate(const CapabilityPolicyDecisionV1& value);
    ContractValidation Validate(const CapabilityAuthorizationReceiptV1& value);
    ContractValidation Validate(const TargetMutationClaimV1& value);
    ContractValidation Validate(const RollbackStepV1& value);
    ContractValidation Validate(const RollbackPlanV1& value);
    ContractValidation Validate(const CapabilityPhasePlanV1& value);
    ContractValidation Validate(const CapabilityExecutionPlanV1& value);
    ContractValidation Validate(const FailureRecordV1& value);
    ContractValidation Validate(const DiagnosticReferenceV1& value);
    ContractValidation Validate(const TargetObservationV1& value);
    ContractValidation Validate(const PhaseExtensionReferenceV1& value);
    ContractValidation Validate(const CapabilityPhaseReceiptV1& value);
    ContractValidation Validate(const RollbackStepReceiptV1& value);
    ContractValidation Validate(const RollbackReceiptV1& value);
    ContractValidation Validate(const CapabilityExecutionReceiptV1& value);

    ContractValidation Validate(const CapabilityProviderBindingV1&, const CapabilityDescriptorV1&);
    ContractValidation Validate(const CapabilityExecutionRequestV1&, const CapabilityDescriptorV1&);
    ContractValidation Validate(const CapabilityPhasePlanV1&, const CapabilityProviderBindingV1&);
    ContractValidation Validate(const CapabilityExecutionPlanV1&, const CapabilityDescriptorV1&,
        const CapabilityExecutionRequestV1&, const AZStd::vector<CapabilityProviderBindingV1>&);
    ContractValidation Validate(const CapabilityPhaseReceiptV1&, const CapabilityExecutionPlanV1&,
        const CapabilityPhasePlanV1&, const AZStd::optional<PhaseExtensionReferenceV1>& expectedExtension = {});
    ContractValidation Validate(const RollbackReceiptV1&, const CapabilityExecutionPlanV1&, const CapabilityPhasePlanV1&);
    ContractValidation Validate(const CapabilityExecutionReceiptV1&, const CapabilityExecutionPlanV1&,
        const CapabilityAuthorizationReceiptV1&, const AZStd::vector<PhaseExtensionReferenceV1>& expectedExtensions = {});
}
