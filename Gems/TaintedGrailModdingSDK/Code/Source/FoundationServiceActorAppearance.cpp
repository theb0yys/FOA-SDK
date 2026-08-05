/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    bool FoundationService::BindActorAppearancePreview(
        const ActorAppearanceBindingRequest& request,
        AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            if (error)
            {
                *error = "Configure an exact active FoA game profile before binding actor appearance previews.";
            }
            return false;
        }

        auto candidateResult = ActorAppearanceBindingService::BuildCandidate(
            request,
            *profile,
            m_sourceRegistry,
            m_catalog);
        if (!candidateResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(candidateResult.GetError());
            }
            return false;
        }

        ActorAppearanceBindingResult candidate = candidateResult.TakeValue();
        if (!PersistCatalogCandidate(candidate.m_catalog, error))
        {
            return false;
        }

        m_catalog = AZStd::move(candidate.m_catalog);
        RefreshSnapshot();
        return true;
    }
} // namespace TaintedGrailModdingSDK
