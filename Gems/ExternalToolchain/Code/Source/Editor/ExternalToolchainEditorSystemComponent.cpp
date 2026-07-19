/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainEditorSystemComponent.h"

#include "ExternalToolchainDiagnosticsWidget.h"

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/utility/move.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace ExternalToolchain
{
    namespace
    {
        constexpr const char* DiagnosticsViewPaneName = "External Toolchain";

        ProviderOperationResult MissingProvider()
        {
            return ProviderOperationResult{
                false,
                "Provider ID is not registered." };
        }
    } // namespace

    ExternalToolchainEditorSystemComponent::ExternalToolchainEditorSystemComponent()
        : m_configurationService(m_settingsSource)
        , m_discoveryService(m_pathProbe, m_settingsSource)
    {
    }

    void ExternalToolchainEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ExternalToolchainEditorSystemComponent, AZ::Component>()
                ->Version(2);
        }
    }

    void ExternalToolchainEditorSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("ExternalToolchainService"));
    }

    void ExternalToolchainEditorSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("ExternalToolchainService"));
    }

    void ExternalToolchainEditorSystemComponent::Activate()
    {
        ExternalToolchainRequestBus::Handler::BusConnect();
        ExternalToolchainInterface::Register(this);
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();

        AZ_Printf(
            "ExternalToolchain",
            "External tool provider registration opened for host API %s.\n",
            ToString(HostApiVersion).c_str());
    }

    void ExternalToolchainEditorSystemComponent::Deactivate()
    {
        if (m_viewRegistered)
        {
            AzToolsFramework::UnregisterViewPane(DiagnosticsViewPaneName);
            m_viewRegistered = false;
        }

        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        ExternalToolchainInterface::Unregister(this);
        ExternalToolchainRequestBus::Handler::BusDisconnect();
        m_discoveryService.Clear();
        m_configurationService.Clear();
        m_registry.Clear();
    }

    ProviderOperationResult ExternalToolchainEditorSystemComponent::RegisterProvider(
        const ExternalToolProviderDescriptor& descriptor)
    {
        ProviderOperationResult result = m_registry.RegisterProvider(descriptor);
        if (result.m_success)
        {
            const ExternalToolProviderDescriptor* registered =
                m_registry.FindProvider(descriptor.m_providerId);
            ExternalToolchainNotificationBus::Broadcast(
                &ExternalToolchainNotifications::OnExternalToolProviderRegistered,
                registered ? *registered : descriptor);
        }
        return result;
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::FinalizeProviderRegistration()
    {
        const bool wasFinalized = m_registry.IsRegistrationFinalized();
        ProviderOperationResult result = m_registry.FinalizeRegistration();
        if (result.m_success && !wasFinalized)
        {
            ExternalToolchainNotificationBus::Broadcast(
                &ExternalToolchainNotifications::OnExternalToolProviderRegistrationFinalized,
                static_cast<AZ::u64>(m_registry.GetProviders().size()));
        }
        return result;
    }

    bool ExternalToolchainEditorSystemComponent::IsProviderRegistrationFinalized() const
    {
        return m_registry.IsRegistrationFinalized();
    }

    AZStd::vector<ExternalToolProviderDescriptor>
    ExternalToolchainEditorSystemComponent::EnumerateProviders() const
    {
        return m_registry.GetProviders();
    }

    bool ExternalToolchainEditorSystemComponent::GetProvider(
        const AZStd::string& providerId,
        ExternalToolProviderDescriptor& descriptor) const
    {
        if (const ExternalToolProviderDescriptor* provider =
                m_registry.FindProvider(providerId))
        {
            descriptor = *provider;
            return true;
        }
        return false;
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::SetSessionConfigurationOverride(
        const AZStd::string& providerId,
        const AZStd::string& key,
        const AZStd::string& value)
    {
        const ExternalToolProviderDescriptor* provider =
            m_registry.FindProvider(providerId);
        if (!provider)
        {
            return MissingProvider();
        }

        ProviderOperationResult result =
            m_configurationService.SetSessionOverride(*provider, key, value);
        if (result.m_success)
        {
            NotifyConfigurationChanged(providerId);
        }
        return result;
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::ClearSessionConfigurationOverride(
        const AZStd::string& providerId,
        const AZStd::string& key)
    {
        const ExternalToolProviderDescriptor* provider =
            m_registry.FindProvider(providerId);
        if (!provider)
        {
            return MissingProvider();
        }

        ProviderOperationResult result =
            m_configurationService.ClearSessionOverride(*provider, key);
        if (result.m_success)
        {
            NotifyConfigurationChanged(providerId);
        }
        return result;
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::SetSessionProviderEnabled(
        const AZStd::string& providerId,
        bool enabled)
    {
        const ExternalToolProviderDescriptor* provider =
            m_registry.FindProvider(providerId);
        if (!provider)
        {
            return MissingProvider();
        }

        ProviderOperationResult result =
            m_configurationService.SetSessionProviderEnabled(*provider, enabled);
        if (result.m_success)
        {
            NotifyConfigurationChanged(providerId);
        }
        return result;
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::ClearSessionProviderEnabled(
        const AZStd::string& providerId)
    {
        const ExternalToolProviderDescriptor* provider =
            m_registry.FindProvider(providerId);
        if (!provider)
        {
            return MissingProvider();
        }

        ProviderOperationResult result =
            m_configurationService.ClearSessionProviderEnabled(*provider);
        if (result.m_success)
        {
            NotifyConfigurationChanged(providerId);
        }
        return result;
    }

    AZStd::vector<ExternalToolResolvedConfigurationValue>
    ExternalToolchainEditorSystemComponent::EnumerateResolvedConfiguration(
        const AZStd::string& providerId) const
    {
        if (const ExternalToolProviderDescriptor* provider =
                m_registry.FindProvider(providerId))
        {
            return m_configurationService.ResolveAll(*provider);
        }
        return {};
    }

    ProviderOperationResult
    ExternalToolchainEditorSystemComponent::RefreshProviderDiscovery()
    {
        if (!m_registry.IsRegistrationFinalized())
        {
            return ProviderOperationResult{
                false,
                "Provider discovery requires finalized registration." };
        }

        ProviderOperationResult result = m_discoveryService.Refresh(
            m_registry.GetProviders(),
            m_configurationService,
            GetCurrentHostPlatformId());
        if (result.m_success)
        {
            ExternalToolchainNotificationBus::Broadcast(
                &ExternalToolchainNotifications::OnExternalToolDiscoveryRefreshed,
                static_cast<AZ::u64>(m_discoveryService.GetResults().size()));
        }
        return result;
    }

    AZStd::vector<ExternalToolDiscoveryResult>
    ExternalToolchainEditorSystemComponent::EnumerateDiscoveryResults() const
    {
        return m_discoveryService.GetResults();
    }

    bool ExternalToolchainEditorSystemComponent::GetDiscoveryResult(
        const AZStd::string& providerId,
        ExternalToolDiscoveryResult& result) const
    {
        if (const ExternalToolDiscoveryResult* found =
                m_discoveryService.FindResult(providerId))
        {
            result = *found;
            return true;
        }
        return false;
    }

    void ExternalToolchainEditorSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(160, 160, 1180, 760);
        options.preferedDockingArea = Qt::BottomDockWidgetArea;
        options.isDeletable = true;
        options.isPreview = true;
        options.saveKeyName = QStringLiteral("ExternalToolchain.Diagnostics");

        AzToolsFramework::RegisterViewPane<ExternalToolchainDiagnosticsWidget>(
            DiagnosticsViewPaneName,
            "External Tools",
            options);
        m_viewRegistered = true;
    }

    void ExternalToolchainEditorSystemComponent::OnPostActionManagerRegistrationHook()
    {
        const ProviderOperationResult finalization =
            FinalizeProviderRegistration();
        AZ_Error(
            "ExternalToolchain",
            finalization.m_success,
            "Failed to finalize external tool provider registration: %s",
            finalization.m_message.c_str());
        if (!finalization.m_success)
        {
            return;
        }

        const ProviderOperationResult discovery = RefreshProviderDiscovery();
        AZ_Error(
            "ExternalToolchain",
            discovery.m_success,
            "Failed to refresh external tool discovery: %s",
            discovery.m_message.c_str());
    }

    void ExternalToolchainEditorSystemComponent::NotifyConfigurationChanged(
        const AZStd::string& providerId)
    {
        ExternalToolchainNotificationBus::Broadcast(
            &ExternalToolchainNotifications::OnExternalToolConfigurationChanged,
            providerId);

        if (m_registry.IsRegistrationFinalized())
        {
            const ProviderOperationResult discovery = RefreshProviderDiscovery();
            AZ_Warning(
                "ExternalToolchain",
                discovery.m_success,
                "Configuration changed but discovery refresh failed: %s",
                discovery.m_message.c_str());
        }
    }
} // namespace ExternalToolchain
