/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseAssemblyPaneSystemComponent.h"

#include "AdapterReleaseAssemblyResultContracts.h"
#include "AdapterReleaseAssemblyResultWidget.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ReleaseAssemblyViewPaneName =
            "Tainted Grail Release Assembly and Checksum Results";
    }

    void AdapterReleaseAssemblyPaneSystemComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<AdapterReleaseAssemblyPaneSystemComponent, AZ::Component>()
                ->Version(1);
        }
    }

    void AdapterReleaseAssemblyPaneSystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void AdapterReleaseAssemblyPaneSystemComponent::Deactivate()
    {
        if (m_viewRegistered)
        {
            AzToolsFramework::UnregisterViewPane(ReleaseAssemblyViewPaneName);
            m_viewRegistered = false;
        }
        AdapterReleaseAssemblyResultRegistry::Get().Clear();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void AdapterReleaseAssemblyPaneSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(500, 500, 1640, 1160);
        options.preferedDockingArea = Qt::BottomDockWidgetArea;
        options.isDeletable = true;
        options.isPreview = true;
        options.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.ReleaseAssemblyResults");
        AzToolsFramework::RegisterViewPane<AdapterReleaseAssemblyResultWidget>(
            ReleaseAssemblyViewPaneName,
            "Tainted Grail SDK",
            options);
        m_viewRegistered = true;
    }
} // namespace TaintedGrailModdingSDK
