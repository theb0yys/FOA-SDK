/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "../FoundationModels.h"
#include "FrameworkExecutionRepository.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    struct CandidateProjection
    {
        SourceRecord m_source;
        AZStd::vector<EvidenceRecord> m_evidence;
    };
    AZStd::string ProfileFingerprint(const GameProfile&);
    Result ProjectCandidate(const StoredAttempt&, const GameProfile&, CandidateProjection&);
} // namespace TaintedGrailModdingSDK::ExecutionFramework
