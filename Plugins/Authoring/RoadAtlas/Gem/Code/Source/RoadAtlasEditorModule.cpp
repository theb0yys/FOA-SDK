/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "RoadAtlasEditorWidget.h"

#include <ExtensionRequestBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Module/Module.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/qnamespace.h>

namespace RoadAtlasAuthoring
{
    namespace
    {
        constexpr const char* ExtensionId = "extension.avalon-core-road-atlas";
        constexpr const char* PaneName = "Tainted Grail Road Atlas Editor";
    }

    class RoadAtlasEditorSystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            RoadAtlasEditorSystemComponent,
            "{3A13EBA8-419B-47A0-86E6-78C29F99AAB2}");

        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<RoadAtlasEditorSystemComponent, AZ::Component>()
                    ->Version(1);
            }
        }

        static void GetRequiredServices(
            AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC_CE("TaintedGrailModdingSDKService"));
        }

        void Activate() override
        {
            TaintedGrailModdingSDK::ExtensionAPI::ExtensionDeclaration declaration;
            declaration.m_extensionId = ExtensionId;
            declaration.m_displayName = "Road Atlas Authoring";
            declaration.m_version = "0.8.4";
            declaration.m_supportedGameVersions = { "1.23.401" };
            declaration.m_supportedBranches = { "il2cpp", "mono" };
            declaration.m_capabilities = {
                TaintedGrailModdingSDK::ExtensionAPI::Capability::ReadActiveProfile,
                TaintedGrailModdingSDK::ExtensionAPI::Capability::QueryCatalog,
                TaintedGrailModdingSDK::ExtensionAPI::Capability::SubmitCandidateEvidence,
            };
            AZStd::string error;
            TaintedGrailModdingSDK::ExtensionRequestBus::BroadcastResult(
                m_registered,
                &TaintedGrailModdingSDK::ExtensionRequests::RegisterExtension,
                declaration,
                &error);
            if (m_registered)
            {
                AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
            }
            else
            {
                AZ_Error(
                    "RoadAtlasAuthoring",
                    false,
                    "Road Atlas extension registration failed: %s",
                    error.c_str());
            }
        }

        void Deactivate() override
        {
            if (m_viewRegistered)
            {
                AzToolsFramework::UnregisterViewPane(PaneName);
                m_viewRegistered = false;
            }
            AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
            if (m_registered)
            {
                bool removed = false;
                AZStd::string error;
                TaintedGrailModdingSDK::ExtensionRequestBus::BroadcastResult(
                    removed,
                    &TaintedGrailModdingSDK::ExtensionRequests::UnregisterExtension,
                    AZStd::string(ExtensionId),
                    &error);
                AZ_Warning(
                    "RoadAtlasAuthoring",
                    removed,
                    "Road Atlas extension unregister failed: %s",
                    error.c_str());
                m_registered = false;
            }
        }

    private:
        void NotifyRegisterViews() override
        {
            if (m_viewRegistered || !m_registered)
            {
                return;
            }
            AzToolsFramework::ViewPaneOptions options;
            options.paneRect = QRect(240, 180, 1180, 900);
            options.preferedDockingArea = Qt::RightDockWidgetArea;
            options.isDeletable = true;
            options.isPreview = true;
            options.saveKeyName = QStringLiteral("RoadAtlasAuthoring.Editor");
            AzToolsFramework::RegisterViewPane<RoadAtlasEditorWidget>(
                PaneName,
                "Tainted Grail SDK",
                options);
            m_viewRegistered = true;
        }

        bool m_registered = false;
        bool m_viewRegistered = false;
    };

    class RoadAtlasEditorModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            RoadAtlasEditorModule,
            "{6E30C3CD-F5CA-4234-A39C-F3F55F41E95C}",
            AZ::Module);
        AZ_CLASS_ALLOCATOR(RoadAtlasEditorModule, AZ::SystemAllocator);

        RoadAtlasEditorModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                { RoadAtlasEditorSystemComponent::CreateDescriptor() });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<RoadAtlasEditorSystemComponent>() };
        }
    };
} // namespace RoadAtlasAuthoring

AZ_DECLARE_MODULE_CLASS(
    Gem_RoadAtlasAuthoring,
    RoadAtlasAuthoring::RoadAtlasEditorModule)
