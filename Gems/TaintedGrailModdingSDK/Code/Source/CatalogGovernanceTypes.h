/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class CatalogSubjectKind
    {
        Record,
        Relationship,
    };

    enum class GovernanceAxis
    {
        Maturity,
        Confidence,
        OperationalRisk,
        Staleness,
        Permission,
        Supersession,
    };

    enum class ResearchStage
    {
        Unset,
        Unknown,
        S0,
        S1,
        S2,
        S3,
        S4,
        S5,
        S6,
        S7,
        S8,
        Reviewed,
        Reconciled,
        Validated,
        AuthoringReady,
        RuntimeApproved,
    };

    enum class ConfidenceLevel
    {
        Unset,
        Unknown,
        Unrated,
        Hypothesis,
        Inferred,
        Documented,
        RuntimeObserved,
        Validated,
    };

    enum class OperationalRisk
    {
        Unset,
        Unknown,
        Low,
        Medium,
        High,
        Critical,
    };

    enum class ValidationState
    {
        Unset,
        Unvalidated,
        Pending,
        Validated,
        Failed,
        Stale,
        Blocked,
    };

    enum class StalenessState
    {
        Unset,
        Unknown,
        Current,
        PotentiallyStale,
        Stale,
    };

    enum class PermissionDecision
    {
        Allow,
        Forbid,
        Clear,
    };

    struct GovernedSubjectState
    {
        CatalogSubjectKind m_subjectKind = CatalogSubjectKind::Record;
        AZStd::string m_subjectId;
        ResearchStage m_researchStage = ResearchStage::Unset;
        ConfidenceLevel m_confidence = ConfidenceLevel::Unset;
        OperationalRisk m_operationalRisk = OperationalRisk::Unset;
        ValidationState m_validationState = ValidationState::Unset;
        StalenessState m_stalenessState = StalenessState::Unset;
        AZStd::vector<AZStd::string> m_allowedUsages;
        AZStd::vector<AZStd::string> m_forbiddenUsages;
        AZStd::vector<AZStd::string> m_missingRefs;
        AZStd::vector<AZStd::string> m_conflictRefs;
        AZStd::string m_supersededById;
        AZStd::string m_updatedAt;
    };

    AZ::Outcome<CatalogSubjectKind, AZStd::string> ParseCatalogSubjectKind(const AZStd::string& value);
    AZ::Outcome<GovernanceAxis, AZStd::string> ParseGovernanceAxis(const AZStd::string& value);
    AZ::Outcome<ResearchStage, AZStd::string> ParseResearchStage(const AZStd::string& value);
    AZ::Outcome<ConfidenceLevel, AZStd::string> ParseConfidenceLevel(const AZStd::string& value);
    AZ::Outcome<OperationalRisk, AZStd::string> ParseOperationalRisk(const AZStd::string& value);
    AZ::Outcome<ValidationState, AZStd::string> ParseValidationState(const AZStd::string& value);
    AZ::Outcome<StalenessState, AZStd::string> ParseStalenessState(const AZStd::string& value);
    AZ::Outcome<PermissionDecision, AZStd::string> ParsePermissionDecision(const AZStd::string& value);

    AZStd::string ToString(CatalogSubjectKind value);
    AZStd::string ToString(GovernanceAxis value);
    AZStd::string ToString(ResearchStage value);
    AZStd::string ToString(ConfidenceLevel value);
    AZStd::string ToString(OperationalRisk value);
    AZStd::string ToString(ValidationState value);
    AZStd::string ToString(StalenessState value);
    AZStd::string ToString(PermissionDecision value);
} // namespace TaintedGrailModdingSDK
