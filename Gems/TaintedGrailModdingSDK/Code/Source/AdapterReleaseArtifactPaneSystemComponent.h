/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzToolsFramework/API/EditorEvents.h>

namespace TaintedGrailModdingSDK
{
    class AdapterReleaseArtifactPaneSystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            AdapterReleaseArtifactPaneSystemComponent,
            "{D45B52D1-77D4-4CDA-9A9C-6DE7F5E88A23}");

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:
        void NotifyRegisterViews() override;

        bool m_viewRegistered = false;
    };
} // namespace TaintedGrailModdingSDK
