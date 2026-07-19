/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "ExternalToolchainConfigurationService.h"
#include "ExternalToolchainDiscoveryService.h"
#include "ExternalToolchainRegistry.h"

#include <ExternalToolchain/ExternalToolchainBus.h>
#include <ExternalToolchain/ExternalToolchainTypeIds.h>

#include <AzCore/Component/Component.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>

namespace ExternalToolchain
{
    class ExternalToolchainEditorSystemComponent final
        : public AZ::Component
        , public ExternalToolchainRequestBus::Handler
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , protected AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(
            ExternalToolchainEditorSystemComponent,
            ExternalToolchainEditorSystemComponentTypeId);

        ExternalToolchainEditorSystemComponent();

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(
            AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(
            AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

        ProviderOperationResult RegisterProvider(
            const ExternalToolProviderDescriptor& descriptor) override;
        ProviderOperationResult FinalizeProviderRegistration() override;
        bool IsProviderRegistrationFinalized() const override;
        AZStd::vector<ExternalToolProviderDescriptor> EnumerateProviders() const override;
        bool GetProvider(
            const AZStd::string& providerId,
            ExternalToolProviderDescriptor& descriptor) const override;

        ProviderOperationResult SetSessionConfigurationOverride(
            const AZStd::string& providerId,
            const AZStd::string& key,
            const AZStd::string& value) override;
        ProviderOperationResult ClearSessionConfigurationOverride(
            const AZStd::string& providerId,
            const AZStd::string& key) override;
        ProviderOperationResult SetSessionProviderEnabled(
            const AZStd::string& providerId,
            bool enabled) override;
        ProviderOperationResult ClearSessionProviderEnabled(
            const AZStd::string& providerId) override;
        AZStd::vector<ExternalToolResolvedConfigurationValue>
            EnumerateResolvedConfiguration(
                const AZStd::string& providerId) const override;

        ProviderOperationResult RefreshProviderDiscovery() override;
        AZStd::vector<ExternalToolDiscoveryResult>
            EnumerateDiscoveryResults() const override;
        bool GetDiscoveryResult(
            const AZStd::string& providerId,
            ExternalToolDiscoveryResult& result) const override;

    private:
        void NotifyRegisterViews() override;
        void OnPostActionManagerRegistrationHook() override;
        void NotifyConfigurationChanged(const AZStd::string& providerId);

        ExternalToolchainRegistry m_registry;
        SettingsRegistryExternalToolchainSettingsSource m_settingsSource;
        ExternalToolchainConfigurationService m_configurationService;
        SystemFileExternalToolPathProbe m_pathProbe;
        ExternalToolchainDiscoveryService m_discoveryService;
        bool m_viewRegistered = false;
    };
} // namespace ExternalToolchain
