/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceTypes.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class Enum>
        AZ::Outcome<Enum, AZStd::string> UnknownValue(const char* category, const AZStd::string& value)
        {
            AZStd::string message = "Unsupported ";
            message += category;
            message += " value: ";
            message += value;
            return AZ::Failure(AZStd::move(message));
        }
    } // namespace

    AZ::Outcome<CatalogSubjectKind, AZStd::string> ParseCatalogSubjectKind(const AZStd::string& value)
    {
        if (value == "record")
        {
            return AZ::Success(CatalogSubjectKind::Record);
        }
        if (value == "relationship")
        {
            return AZ::Success(CatalogSubjectKind::Relationship);
        }
        return UnknownValue<CatalogSubjectKind>("catalog subject kind", value);
    }

    AZ::Outcome<GovernanceAxis, AZStd::string> ParseGovernanceAxis(const AZStd::string& value)
    {
        if (value == "maturity")
        {
            return AZ::Success(GovernanceAxis::Maturity);
        }
        if (value == "confidence")
        {
            return AZ::Success(GovernanceAxis::Confidence);
        }
        if (value == "operational_risk")
        {
            return AZ::Success(GovernanceAxis::OperationalRisk);
        }
        if (value == "staleness")
        {
            return AZ::Success(GovernanceAxis::Staleness);
        }
        if (value == "permission")
        {
            return AZ::Success(GovernanceAxis::Permission);
        }
        if (value == "supersession")
        {
            return AZ::Success(GovernanceAxis::Supersession);
        }
        return UnknownValue<GovernanceAxis>("governance axis", value);
    }

    AZ::Outcome<ResearchStage, AZStd::string> ParseResearchStage(const AZStd::string& value)
    {
        if (value == "S0") return AZ::Success(ResearchStage::S0);
        if (value == "S1") return AZ::Success(ResearchStage::S1);
        if (value == "S2") return AZ::Success(ResearchStage::S2);
        if (value == "S3") return AZ::Success(ResearchStage::S3);
        if (value == "S4") return AZ::Success(ResearchStage::S4);
        if (value == "S5") return AZ::Success(ResearchStage::S5);
        if (value == "S6") return AZ::Success(ResearchStage::S6);
        if (value == "S7") return AZ::Success(ResearchStage::S7);
        if (value == "S8") return AZ::Success(ResearchStage::S8);
        if (value == "reviewed") return AZ::Success(ResearchStage::Reviewed);
        if (value == "reconciled") return AZ::Success(ResearchStage::Reconciled);
        if (value == "validated") return AZ::Success(ResearchStage::Validated);
        if (value == "authoring_ready") return AZ::Success(ResearchStage::AuthoringReady);
        if (value == "runtime_approved") return AZ::Success(ResearchStage::RuntimeApproved);
        return UnknownValue<ResearchStage>("research stage", value);
    }

    AZ::Outcome<ConfidenceLevel, AZStd::string> ParseConfidenceLevel(const AZStd::string& value)
    {
        if (value == "unknown") return AZ::Success(ConfidenceLevel::Unknown);
        if (value == "hypothesis") return AZ::Success(ConfidenceLevel::Hypothesis);
        if (value == "inferred") return AZ::Success(ConfidenceLevel::Inferred);
        if (value == "documented") return AZ::Success(ConfidenceLevel::Documented);
        if (value == "runtime_observed") return AZ::Success(ConfidenceLevel::RuntimeObserved);
        if (value == "validated") return AZ::Success(ConfidenceLevel::Validated);
        return UnknownValue<ConfidenceLevel>("confidence", value);
    }

    AZ::Outcome<OperationalRisk, AZStd::string> ParseOperationalRisk(const AZStd::string& value)
    {
        if (value == "unknown") return AZ::Success(OperationalRisk::Unknown);
        if (value == "low") return AZ::Success(OperationalRisk::Low);
        if (value == "medium") return AZ::Success(OperationalRisk::Medium);
        if (value == "high") return AZ::Success(OperationalRisk::High);
        if (value == "critical") return AZ::Success(OperationalRisk::Critical);
        return UnknownValue<OperationalRisk>("operational risk", value);
    }

    AZ::Outcome<ValidationState, AZStd::string> ParseValidationState(const AZStd::string& value)
    {
        if (value == "unvalidated") return AZ::Success(ValidationState::Unvalidated);
        if (value == "pending") return AZ::Success(ValidationState::Pending);
        if (value == "validated") return AZ::Success(ValidationState::Validated);
        if (value == "failed") return AZ::Success(ValidationState::Failed);
        if (value == "stale") return AZ::Success(ValidationState::Stale);
        if (value == "blocked") return AZ::Success(ValidationState::Blocked);
        return UnknownValue<ValidationState>("validation state", value);
    }

    AZ::Outcome<StalenessState, AZStd::string> ParseStalenessState(const AZStd::string& value)
    {
        if (value == "unknown") return AZ::Success(StalenessState::Unknown);
        if (value == "current") return AZ::Success(StalenessState::Current);
        if (value == "potentially_stale") return AZ::Success(StalenessState::PotentiallyStale);
        if (value == "stale") return AZ::Success(StalenessState::Stale);
        return UnknownValue<StalenessState>("staleness state", value);
    }

    AZ::Outcome<PermissionDecision, AZStd::string> ParsePermissionDecision(const AZStd::string& value)
    {
        if (value == "allow") return AZ::Success(PermissionDecision::Allow);
        if (value == "forbid") return AZ::Success(PermissionDecision::Forbid);
        if (value == "clear") return AZ::Success(PermissionDecision::Clear);
        return UnknownValue<PermissionDecision>("permission decision", value);
    }

    AZStd::string ToString(CatalogSubjectKind value)
    {
        return value == CatalogSubjectKind::Record ? "record" : "relationship";
    }

    AZStd::string ToString(GovernanceAxis value)
    {
        switch (value)
        {
        case GovernanceAxis::Maturity: return "maturity";
        case GovernanceAxis::Confidence: return "confidence";
        case GovernanceAxis::OperationalRisk: return "operational_risk";
        case GovernanceAxis::Staleness: return "staleness";
        case GovernanceAxis::Permission: return "permission";
        case GovernanceAxis::Supersession: return "supersession";
        }
        return {};
    }

    AZStd::string ToString(ResearchStage value)
    {
        switch (value)
        {
        case ResearchStage::S0: return "S0";
        case ResearchStage::S1: return "S1";
        case ResearchStage::S2: return "S2";
        case ResearchStage::S3: return "S3";
        case ResearchStage::S4: return "S4";
        case ResearchStage::S5: return "S5";
        case ResearchStage::S6: return "S6";
        case ResearchStage::S7: return "S7";
        case ResearchStage::S8: return "S8";
        case ResearchStage::Reviewed: return "reviewed";
        case ResearchStage::Reconciled: return "reconciled";
        case ResearchStage::Validated: return "validated";
        case ResearchStage::AuthoringReady: return "authoring_ready";
        case ResearchStage::RuntimeApproved: return "runtime_approved";
        }
        return {};
    }

    AZStd::string ToString(ConfidenceLevel value)
    {
        switch (value)
        {
        case ConfidenceLevel::Unknown: return "unknown";
        case ConfidenceLevel::Hypothesis: return "hypothesis";
        case ConfidenceLevel::Inferred: return "inferred";
        case ConfidenceLevel::Documented: return "documented";
        case ConfidenceLevel::RuntimeObserved: return "runtime_observed";
        case ConfidenceLevel::Validated: return "validated";
        }
        return {};
    }

    AZStd::string ToString(OperationalRisk value)
    {
        switch (value)
        {
        case OperationalRisk::Unknown: return "unknown";
        case OperationalRisk::Low: return "low";
        case OperationalRisk::Medium: return "medium";
        case OperationalRisk::High: return "high";
        case OperationalRisk::Critical: return "critical";
        }
        return {};
    }

    AZStd::string ToString(ValidationState value)
    {
        switch (value)
        {
        case ValidationState::Unvalidated: return "unvalidated";
        case ValidationState::Pending: return "pending";
        case ValidationState::Validated: return "validated";
        case ValidationState::Failed: return "failed";
        case ValidationState::Stale: return "stale";
        case ValidationState::Blocked: return "blocked";
        }
        return {};
    }

    AZStd::string ToString(StalenessState value)
    {
        switch (value)
        {
        case StalenessState::Unknown: return "unknown";
        case StalenessState::Current: return "current";
        case StalenessState::PotentiallyStale: return "potentially_stale";
        case StalenessState::Stale: return "stale";
        }
        return {};
    }

    AZStd::string ToString(PermissionDecision value)
    {
        switch (value)
        {
        case PermissionDecision::Allow: return "allow";
        case PermissionDecision::Forbid: return "forbid";
        case PermissionDecision::Clear: return "clear";
        }
        return {};
    }
} // namespace TaintedGrailModdingSDK
