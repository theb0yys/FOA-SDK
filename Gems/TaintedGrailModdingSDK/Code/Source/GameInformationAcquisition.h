/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::GameInformationAcquisition
{
    enum class ProviderKind
    {
        LocalInstall,
        PinnedGitHub,
        MerlinWorkshop,
    };

    enum class QualificationState
    {
        ContractOnly,
        SourcePinned,
        ExactInstallBound,
    };

    struct ProviderDescriptor
    {
        AZStd::string m_providerId;
        AZStd::string m_displayName;
        AZStd::string m_contractVersion;
        ProviderKind m_kind = ProviderKind::LocalInstall;
        QualificationState m_qualification = QualificationState::ContractOnly;
        AZStd::string m_sourceRepository;
        AZStd::string m_sourceRevision;
        AZStd::string m_licenseExpression;
        int m_precedence = 0;
        bool m_optional = false;
        bool m_requiresLocalFileRead = false;
        bool m_requiresNetwork = false;
        bool m_autoPromotionAllowed = false;
    };

    struct CandidateObservation
    {
        AZStd::string m_observationId;
        AZStd::string m_providerId;
        AZStd::string m_providerVersion;
        AZStd::string m_sourceId;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_sourceRevision;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_subjectRef;
        AZStd::string m_claim;
        AZStd::string m_valueFingerprint;
        AZStd::string m_evidenceKind;
        AZStd::string m_confidence;
        AZStd::string m_locator;
        AZStd::string m_recordPath;
        AZStd::string m_capturedAtUtc;
        bool m_promoteAutomatically = false;
        bool m_grantsRuntimePermission = false;
    };

    struct Conflict
    {
        AZStd::string m_subjectRef;
        AZStd::vector<AZStd::string> m_observationIds;
        AZStd::vector<AZStd::string> m_valueFingerprints;
        AZStd::string m_reason;
    };

    struct ReconciliationResult
    {
        AZStd::vector<CandidateObservation> m_candidates;
        AZStd::vector<Conflict> m_conflicts;
        AZStd::vector<AZStd::string> m_rejectedObservationIds;
        bool m_canPromoteAutomatically = false;
        bool m_grantsRuntimePermission = false;
    };

    const AZStd::vector<ProviderDescriptor>& GetCanonicalProviders();
    const ProviderDescriptor* FindProvider(const AZStd::string& providerId);

    bool ValidateProvider(
        const ProviderDescriptor& provider,
        AZStd::string* error = nullptr);

    bool ValidateObservation(
        const CandidateObservation& observation,
        const GameProfile& profile,
        AZStd::string* error = nullptr);

    ReconciliationResult Reconcile(
        const AZStd::vector<CandidateObservation>& observations,
        const GameProfile& profile);

    bool BuildEvidenceCandidate(
        const CandidateObservation& observation,
        const GameProfile& profile,
        EvidenceRecord& evidence,
        AZStd::string* error = nullptr);
} // namespace TaintedGrailModdingSDK::GameInformationAcquisition
