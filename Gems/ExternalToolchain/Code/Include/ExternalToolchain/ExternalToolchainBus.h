/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ExternalToolchain/ExternalToolchainTypeIds.h>
#include <ExternalToolchain/ExternalToolchainTypes.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>

namespace ExternalToolchain
{
    class ExternalToolchainRequests
        : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(ExternalToolchainRequests, ExternalToolchainRequestsTypeId);

        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~ExternalToolchainRequests() = default;

        virtual ProviderOperationResult RegisterProvider(
            const ExternalToolProviderDescriptor& descriptor) = 0;
        virtual ProviderOperationResult FinalizeProviderRegistration() = 0;
        virtual bool IsProviderRegistrationFinalized() const = 0;
        virtual AZStd::vector<ExternalToolProviderDescriptor> EnumerateProviders() const = 0;
        virtual bool GetProvider(
            const AZStd::string& providerId,
            ExternalToolProviderDescriptor& descriptor) const = 0;
    };

    using ExternalToolchainRequestBus = AZ::EBus<ExternalToolchainRequests>;
    using ExternalToolchainInterface = AZ::Interface<ExternalToolchainRequests>;

    class ExternalToolchainNotifications
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~ExternalToolchainNotifications() = default;

        virtual void OnExternalToolProviderRegistered(
            const ExternalToolProviderDescriptor&) {}
        virtual void OnExternalToolProviderRegistrationFinalized(AZ::u64) {}
    };

    using ExternalToolchainNotificationBus = AZ::EBus<ExternalToolchainNotifications>;
} // namespace ExternalToolchain
