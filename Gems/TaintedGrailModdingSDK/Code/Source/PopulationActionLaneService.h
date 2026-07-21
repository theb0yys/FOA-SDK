/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    //! Closed population usage lanes presented by the authoring editor.
    enum class PopulationActionLane : AZ::u8
    {
        Display,
        AuthorProfile,
        ComposeTroop,
        Planning,
        SpawnCandidate,
        RuntimeSpawn,
        SaveMutation,
    };

    //! Read-only decision states. None of these states grants runtime authority.
    enum class PopulationActionLaneState : AZ::u8
    {
        Allowed,
        Unset,
        Forbidden,
        Blocked,
        Unavailable,
        NotApplicable,
    };

    AZStd::string ToString(PopulationActionLane lane);
    AZStd::string ToString(PopulationActionLaneState state);

    struct PopulationActionLaneDecision
    {
        PopulationActionLane m_lane = PopulationActionLane::Display;
        PopulationActionLaneState m_state = PopulationActionLaneState::Unset;
        AZStd::vector<AZStd::string> m_blockerIds;
        AZStd::vector<AZStd::string> m_reasons;
    };

    //! Derives deterministic editor-only lane decisions from published state.
    //!
    //! The service is deliberately read-only. It does not update governance,
    //! suppress blockers, publish catalog candidates, or invoke a runtime.
    class PopulationActionLaneService final
    {
    public:
        AZ::Outcome<AZStd::vector<PopulationActionLaneDecision>, AZStd::string>
        BuildActionLaneMatrix(
            const AZStd::string& recordId,
            const WorkspaceModel& workspace,
            const AZStd::string& resolvedWorkspaceRoot,
            const PackManifest* activePack,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers) const;
    };
} // namespace TaintedGrailModdingSDK
