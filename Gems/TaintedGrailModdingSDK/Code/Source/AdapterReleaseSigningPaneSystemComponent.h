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
    class AdapterReleaseSigningPaneSystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            AdapterReleaseSigningPaneSystemComponent,
            "{17E13865-90DF-48FE-9D45-7CC89B3287E4}");

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;

        bool m_viewRegistered = false;
    };
} // namespace TaintedGrailModdingSDK
