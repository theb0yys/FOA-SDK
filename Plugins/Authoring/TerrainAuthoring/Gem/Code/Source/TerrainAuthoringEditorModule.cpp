/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainAuthoringContracts.h"
#include "TerrainImportWidget.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <QRect>
#include <QDir>
#include <ExternalToolchain/ExternalToolchainBus.h>

#include <ExtensionRequestBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Module/Module.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>

namespace TerrainAuthoring
{
    class TerrainAuthoringShellComponent final
        : public AZ::Component
        , private AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        AZ_COMPONENT(
            TerrainAuthoringShellComponent,
            "{7F20FB2D-2D5A-47E4-99A1-E8B20215BD6A}");

        static void Reflect(AZ::ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<TerrainAuthoringShellComponent, AZ::Component>()
                    ->Version(1);
            }
        }

        static void GetRequiredServices(
            AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC_CE("TaintedGrailModdingSDKService"));
            required.push_back(AZ_CRC_CE("ExternalToolchainService"));
        }

        void Activate() override
        {
            using namespace ExternalToolchain;
            ExternalToolProviderDescriptor provider;
            provider.m_providerId = "foa.terrain-extraction";
            provider.m_displayName = "Campaign Heightmap Extraction";
            provider.m_providerVersion = "1.0.0";
            provider.m_minimumHostApiVersion = {1, 1, 0};
            provider.m_toolFamily = ToolFamily::Utility;
            provider.m_platforms = {"windows"};
            provider.m_capabilities.m_supportsBatch = true;
            provider.m_capabilities.m_supportsHeadless = true;
            provider.m_capabilities.m_producesAssetSources = true;
            const auto runtime = QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath("FOA-SDK/Tools/Heightmap/Scripts/python.exe").toUtf8();
            provider.m_configuration = {
                {"executable-path", "Terrain Python runtime", ConfigurationValueKind::Path, AZStd::string(runtime.constData()), true, false},
                {"tool-version", "UnityPy version", ConfigurationValueKind::SemanticVersion, "1.24.2", true, false}};
            ExternalToolCommandDescriptor command;
            command.m_commandId = "export-campaign";
            command.m_displayName = "Export campaign ground heightmap";
            command.m_mode = CommandMode::Batch;
            command.m_inputKinds = {"foa.campaign-heightmap-export.request"};
            command.m_outputKinds = {"foa.campaign-heightmap-export.result"};
            command.m_supportsCancellation = true;
            provider.m_commands.push_back(command);
            ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = "terrain-python";
            probe.m_pathConfigurationKey = "executable-path";
            probe.m_versionConfigurationKey = "tool-version";
            probe.m_minimumSupportedVersion = "1.24.2";
            probe.m_maximumSupportedVersion = "1.24.2";
            probe.m_platforms = {"windows"};
            provider.m_discoveryProbes.push_back(probe);
            ProviderOperationResult registration;
            ExternalToolchainRequestBus::BroadcastResult(registration, &ExternalToolchainRequests::RegisterProvider, provider);
            AZ_Warning("TerrainAuthoring", registration.m_success, "Campaign provider registration: %s", registration.m_message.c_str());
            AZStd::string error;
            if (!ValidateShellContract(&error))
            {
                AZ_Error(
                    "TerrainAuthoring",
                    false,
                    "Terrain Authoring shell contract validation failed: %s",
                    error.c_str());
                return;
            }

            TaintedGrailModdingSDK::ExtensionRequestBus::BroadcastResult(
                m_registered,
                &TaintedGrailModdingSDK::ExtensionRequests::RegisterExtension,
                BuildExtensionDeclaration(),
                &error);
            if (m_registered)
            {
                AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
            }
            if (!m_registered)
            {
                AZ_Error(
                    "TerrainAuthoring",
                    false,
                    "Terrain Authoring extension registration failed: %s",
                    error.c_str());
            }
        }

        void Deactivate() override
        {
            if (m_viewRegistered) { AzToolsFramework::UnregisterViewPane("Heightmap Importer"); m_viewRegistered = false; }
            AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
            if (!m_registered)
            {
                return;
            }

            bool removed = false;
            AZStd::string error;
            TaintedGrailModdingSDK::ExtensionRequestBus::BroadcastResult(
                removed,
                &TaintedGrailModdingSDK::ExtensionRequests::UnregisterExtension,
                AZStd::string(TerrainAuthoringExtensionId),
                &error);
            AZ_Warning(
                "TerrainAuthoring",
                removed,
                "Terrain Authoring extension unregister failed: %s",
                error.c_str());
            m_registered = false;
        }

    private:
        void NotifyRegisterViews() override
        {
            if (!m_registered || m_viewRegistered) { return; }
            AzToolsFramework::ViewPaneOptions options;
            options.paneRect = QRect(100, 100, 440, 680);
            options.preferedDockingArea = Qt::LeftDockWidgetArea;
            options.isDeletable = true;
            options.isPreview = true;
            options.showInMenu = false;
            options.saveKeyName = QStringLiteral("TerrainAuthoring.HeightmapImporter");
            AzToolsFramework::RegisterViewPane<TerrainImportWidget>("Heightmap Importer", "Tainted Grail SDK", options);
            m_viewRegistered = true;
        }
        bool m_viewRegistered = false;
        bool m_registered = false;
    };

    class TerrainAuthoringModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            TerrainAuthoringModule,
            "{166901E5-FA0A-4B6B-A841-8662DC6249FB}",
            AZ::Module);
        AZ_CLASS_ALLOCATOR(TerrainAuthoringModule, AZ::SystemAllocator);

        TerrainAuthoringModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                { TerrainAuthoringShellComponent::CreateDescriptor() });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return { azrtti_typeid<TerrainAuthoringShellComponent>() };
        }
    };
} // namespace TerrainAuthoring

AZ_DECLARE_MODULE_CLASS(
    Gem_TerrainAuthoring,
    TerrainAuthoring::TerrainAuthoringModule)
