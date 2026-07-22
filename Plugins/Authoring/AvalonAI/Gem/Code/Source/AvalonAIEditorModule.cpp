/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AvalonAIEditorWidget.h"

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

namespace AvalonAIAuthoring
{
    namespace
    {
        constexpr const char* ExtensionId = "extension.avalon-ai-authoring";
        constexpr const char* PaneName = "Tainted Grail Avalon AI Editor";
    }

    class AvalonAIEditorSystemComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            AvalonAIEditorSystemComponent,
            "{1B15B1AE-36C9-4459-9BB2-0CEEF0D8B3F4}");

        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<AvalonAIEditorSystemComponent, AZ::Component>()
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
            declaration.m_displayName = "Avalon AI Authoring";
            declaration.m_version = "0.8.0";
            declaration.m_supportedGameVersions = { "1.23.401" };
            declaration.m_supportedBranches = { "il2cpp", "mono" };
            declaration.m_capabilities = {
                TaintedGrailModdingSDK::ExtensionAPI::Capability::ReadActiveProfile,
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
                    "AvalonAIAuthoring",
                    false,
                    "Avalon AI extension registration failed: %s",
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
                    "AvalonAIAuthoring",
                    removed,
                    "Avalon AI extension unregister failed: %s",
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
            options.paneRect = QRect(260, 200, 1180, 900);
            options.preferedDockingArea = Qt::RightDockWidgetArea;
            options.isDeletable = true;
            options.isPreview = true;
            options.saveKeyName = QStringLiteral("AvalonAIAuthoring.Editor");
            AzToolsFramework::RegisterViewPane<AvalonAIEditorWidget>(
                PaneName,
                "Tainted Grail SDK",
                options);
            m_viewRegistered = true;
        }

        bool m_registered = false;
        bool m_viewRegistered = false;
    };

    class AvalonAIEditorModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            AvalonAIEditorModule,
            "{73EA3659-1CCF-4936-BE3F-3EBE2FCD5927}",
            AZ::Module);
        AZ_CLASS_ALLOCATOR(AvalonAIEditorModule, AZ::SystemAllocator);

        AvalonAIEditorModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                { AvalonAIEditorSystemComponent::CreateDescriptor() });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<AvalonAIEditorSystemComponent>() };
        }
    };
} // namespace AvalonAIAuthoring

AZ_DECLARE_MODULE_CLASS(
    Gem_AvalonAIAuthoring,
    AvalonAIAuthoring::AvalonAIEditorModule)
