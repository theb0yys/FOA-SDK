/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "TerrainAuthoringContracts.h"

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
        }

        void Activate() override
        {
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
