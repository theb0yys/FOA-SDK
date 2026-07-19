/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseArtifactPaneSystemComponent.h"

#include "AdapterReleaseArtifactContracts.h"
#include "AdapterReleaseArtifactWidget.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ReleaseArtifactViewPaneName =
            "Tainted Grail Release Artifact Provenance and Signing Intent";
    }

    void AdapterReleaseArtifactPaneSystemComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<AdapterReleaseArtifactPaneSystemComponent, AZ::Component>()
                ->Version(1);
        }
    }

    void AdapterReleaseArtifactPaneSystemComponent::Activate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void AdapterReleaseArtifactPaneSystemComponent::Deactivate()
    {
        if (m_viewRegistered)
        {
            AzToolsFramework::UnregisterViewPane(ReleaseArtifactViewPaneName);
            m_viewRegistered = false;
        }
        AdapterReleaseArtifactRegistry::Get().Clear();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
    }

    void AdapterReleaseArtifactPaneSystemComponent::NotifyRegisterViews()
    {
        if (m_viewRegistered)
        {
            return;
        }

        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(480, 480, 1560, 1140);
        options.preferedDockingArea = Qt::BottomDockWidgetArea;
        options.isDeletable = true;
        options.isPreview = true;
        options.saveKeyName =
            QStringLiteral("TaintedGrailModdingSDK.ReleaseArtifactProvenance");
        AzToolsFramework::RegisterViewPane<AdapterReleaseArtifactWidget>(
            ReleaseArtifactViewPaneName,
            "Tainted Grail SDK",
            options);
        m_viewRegistered = true;
    }
} // namespace TaintedGrailModdingSDK
