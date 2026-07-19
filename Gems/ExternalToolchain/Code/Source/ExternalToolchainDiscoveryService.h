/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "ExternalToolchainConfigurationService.h"

namespace ExternalToolchain
{
    struct ExternalToolPathObservation
    {
        bool m_exists = false;
        bool m_isDirectory = false;
        AZStd::string m_message;
    };

    class ExternalToolPathProbe
    {
    public:
        virtual ~ExternalToolPathProbe() = default;

        virtual ExternalToolPathObservation Inspect(
            const AZStd::string& path) const = 0;
    };

    class SystemFileExternalToolPathProbe final
        : public ExternalToolPathProbe
    {
    public:
        ExternalToolPathObservation Inspect(
            const AZStd::string& path) const override;
    };

    class ExternalToolchainDiscoveryService
    {
    public:
        ExternalToolchainDiscoveryService(
            const ExternalToolPathProbe& pathProbe,
            const ExternalToolchainSettingsSource& settingsSource);

        ProviderOperationResult Refresh(
            const AZStd::vector<ExternalToolProviderDescriptor>& providers,
            const ExternalToolchainConfigurationService& configurationService,
            const AZStd::string& platformId);

        const AZStd::vector<ExternalToolDiscoveryResult>& GetResults() const;
        const ExternalToolDiscoveryResult* FindResult(
            const AZStd::string& providerId) const;
        void Clear();

    private:
        ExternalToolDiscoveryResult DiscoverProvider(
            const ExternalToolProviderDescriptor& provider,
            const ExternalToolchainConfigurationService& configurationService,
            const AZStd::string& platformId,
            AZ::u64 maximumProbes,
            AZ::u64 providerBudgetMilliseconds) const;

        const ExternalToolPathProbe& m_pathProbe;
        const ExternalToolchainSettingsSource& m_settingsSource;
        AZStd::vector<ExternalToolDiscoveryResult> m_results;
    };

    AZStd::string GetCurrentHostPlatformId();
} // namespace ExternalToolchain
