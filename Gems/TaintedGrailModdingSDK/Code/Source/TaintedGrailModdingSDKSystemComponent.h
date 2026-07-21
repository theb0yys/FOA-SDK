/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace TaintedGrailModdingSDK
{
    class TaintedGrailModdingSDKSystemComponent final
        : public AZ::Component
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            TaintedGrailModdingSDKSystemComponent,
            "{CCAB1D0B-9DDC-4313-906F-47B7148109AC}");

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;
        void NotifyEditorInitialized() override;

        bool m_viewRegistered = false;
    };
} // namespace TaintedGrailModdingSDK
