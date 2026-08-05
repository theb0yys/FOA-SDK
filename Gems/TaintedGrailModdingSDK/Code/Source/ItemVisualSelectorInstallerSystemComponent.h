/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/Component.h>

class QObject;

namespace TaintedGrailModdingSDK
{
    //! Installs the Alpha visual selector as a tab in the existing Item/Recipe pane.
    class ItemVisualSelectorInstallerSystemComponent final
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(
            ItemVisualSelectorInstallerSystemComponent,
            "{AFA9DB8F-CC2A-4FEA-AD37-E2E2A4AF751D}");

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;

    private:
        QObject* m_eventFilter = nullptr;
    };
} // namespace TaintedGrailModdingSDK
