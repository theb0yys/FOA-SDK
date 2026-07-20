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
    class AdapterReleaseAssemblyPaneSystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            AdapterReleaseAssemblyPaneSystemComponent,
            "{C34FD7A4-2A86-4AC8-9A3C-5EE4A7C092A4}");

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;

        bool m_viewRegistered = false;
    };
} // namespace TaintedGrailModdingSDK
