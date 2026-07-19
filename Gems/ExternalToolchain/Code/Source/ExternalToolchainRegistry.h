/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ExternalToolchain/ExternalToolchainTypes.h>

namespace ExternalToolchain
{
    class ExternalToolchainRegistry
    {
    public:
        ProviderOperationResult RegisterProvider(
            const ExternalToolProviderDescriptor& descriptor);
        ProviderOperationResult FinalizeRegistration();
        void Clear();

        bool IsRegistrationFinalized() const;
        const AZStd::vector<ExternalToolProviderDescriptor>& GetProviders() const;
        const ExternalToolProviderDescriptor* FindProvider(
            const AZStd::string& providerId) const;

    private:
        static ProviderOperationResult ValidateDescriptor(
            const ExternalToolProviderDescriptor& descriptor);

        AZStd::vector<ExternalToolProviderDescriptor> m_providers;
        bool m_registrationFinalized = false;
    };
} // namespace ExternalToolchain
