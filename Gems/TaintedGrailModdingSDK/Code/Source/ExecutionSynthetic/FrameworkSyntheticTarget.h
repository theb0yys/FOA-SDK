/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "ExecutionFramework/FrameworkExecutionTypes.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    // Host-private, fixed synthetic deployment provider. Never accepts an installation path.
    class FrameworkSyntheticTarget
    {
    public:
        static constexpr const char* Capability = "capability.synthetic-spine.v1";
        static constexpr const char* Baseline = "FOA synthetic baseline v1\n";
        static constexpr const char* Payload = "FOA-SYNTHETIC-V1\nVALUE=42\n";
        static constexpr const char* Canary = "Untouched synthetic canary v1\n";
        static AZStd::string Profile();
        static Result Create(const AZStd::string& privateRoot, const AZStd::string& owner, std::shared_ptr<FrameworkSyntheticTarget>&);
        static Result Reopen(const AZStd::string& privateRoot, const AZStd::string& owner, std::shared_ptr<FrameworkSyntheticTarget>&);
        ~FrameworkSyntheticTarget();
        AZStd::string Root() const;
        AZStd::string Inventory() const;
        CE::ArtifactReferenceV1 Backup() const;
        bool Accepts(const CE::CapabilityPhasePlanV1&) const;
        Result Begin(const CE::CapabilityExecutionPlanV1&);
        Result Apply(const CE::CapabilityExecutionPlanV1&, const AZStd::string& artifactPath, AZStd::vector<CE::TargetObservationV1>&);
        Result Rollback(const CE::CapabilityExecutionPlanV1&, CE::RollbackReceiptV1&);
        bool Pending() const;
        Result Check(bool deployed) const;

    private:
        FrameworkSyntheticTarget();
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
