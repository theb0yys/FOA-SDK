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
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace ExternalToolchain
{
    namespace
    {
        constexpr const char* DiagnosticsViewPaneName = "External Toolchain";
    }

    void ExternalToolchainEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<ExternalToolchainEditorSystemComponent, AZ::Component>()
                ->Version(1);
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
        m_registry.Clear();
    }

    ProviderOperationResult ExternalToolchainEditorSystemComponent::RegisterProvider(
        const ExternalToolProviderDescriptor& descriptor)
    {
        ProviderOperationResult result = m_registry.RegisterProvider(descriptor);
        if (result.m_success)
        {
            ExternalToolchainNotificationBus::Broadcast(
                &ExternalToolchainNotifications::OnExternalToolProviderRegistered,
                descriptor);
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

    void ExternalToolchainEditorSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(160, 160, 1040, 720);
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
        const ProviderOperationResult result = FinalizeProviderRegistration();
        AZ_Error(
            "ExternalToolchain",
            result.m_success,
            "Failed to finalize external tool provider registration: %s",
            result.m_message.c_str());
    }
} // namespace ExternalToolchain
