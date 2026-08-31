/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseSigningPaneSystemComponent.h"

#include "AdapterReleaseSigningResultContracts.h"
#include "AdapterReleaseSigningResultWidget.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ReleaseSigningViewPaneName =
            "Tainted Grail Release Signing Results";
    }

    void AdapterReleaseSigningPaneSystemComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<AdapterReleaseSigningPaneSystemComponent, AZ::Component>()
                ->Version(1);
        }
    }

    void AdapterReleaseSigningPaneSystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void AdapterReleaseSigningPaneSystemComponent::Deactivate()
    {
        if (m_viewRegistered)
        {
            AzToolsFramework::UnregisterViewPane(ReleaseSigningViewPaneName);
            m_viewRegistered = false;
        }
        AdapterReleaseSigningResultRegistry::Get().Clear();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void AdapterReleaseSigningPaneSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(500, 500, 1760, 1180);
        options.preferedDockingArea = Qt::BottomDockWidgetArea;
        options.isDeletable = true;
        options.isPreview = true;
        options.showInMenu = false;
        options.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.ReleaseSigningResults");
        AzToolsFramework::RegisterViewPane<AdapterReleaseSigningResultWidget>(
            ReleaseSigningViewPaneName,
            "Tainted Grail SDK",
            options);
        m_viewRegistered = true;
    }
} // namespace TaintedGrailModdingSDK