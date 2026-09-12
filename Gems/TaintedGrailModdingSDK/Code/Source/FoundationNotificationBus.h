/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/EBus/EBus.h>

namespace TaintedGrailModdingSDK
{
    class FoundationService;

    class FoundationNotifications
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~FoundationNotifications() = default;
        virtual void OnFoundationChanged() {}

        //! Synchronous, trusted-host admission on the Editor thread. Any false vetoes replacement.
        //! The old workspace is still current, so handlers may save drafts here. No replacement
        //! notification follows a veto. Ignore service instances the handler does not own.
        virtual bool CanChangeWorkspace(const FoundationService&) { return true; }
        //! Published only after a successful replacement, including same-workspace reloads.
        virtual void OnWorkspaceChanged(const FoundationService&) {}
    };

    using FoundationNotificationBus = AZ::EBus<FoundationNotifications>;
} // namespace TaintedGrailModdingSDK
