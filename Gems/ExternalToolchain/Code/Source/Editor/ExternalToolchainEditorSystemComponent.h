/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

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

    private:
        void NotifyRegisterViews() override;
        void OnPostActionManagerRegistrationHook() override;

        ExternalToolchainRegistry m_registry;
        bool m_viewRegistered = false;
    };
} // namespace ExternalToolchain
