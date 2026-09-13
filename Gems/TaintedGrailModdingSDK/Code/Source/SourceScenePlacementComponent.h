/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
namespace TaintedGrailModdingSDK
{
    // Source matrices stay immutable. Native Transform supplies a separate placement edit.
    class SourceScenePlacementRequests : public AZ::ComponentBus
    {
    public:
        static constexpr auto HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        virtual bool BindSource(const AZStd::string& descriptor) = 0;
        virtual AZStd::string GetSource() const = 0;
        virtual bool CommitAssemblyState() = 0;
        virtual AZStd::string GetStatus() const = 0;
        virtual AZ::Matrix4x4 GetWorldMatrix() const = 0;
        virtual AZStd::vector<AZ::u32> GetWorldMatrixBits() const = 0;
        virtual AZ::Aabb GetWorldBounds() const = 0;
    };
    using SourceScenePlacementBus = AZ::EBus<SourceScenePlacementRequests>;
    class SourceScenePlacementComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private SourceScenePlacementBus::Handler
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(SourceScenePlacementComponent, "{4D4466F2-3EF9-4C3D-A341-CEBCFA9B7BCA}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        void Activate() override;
        void Deactivate() override;
    private:
        AZStd::string m_source;
        AZ::Matrix4x4 m_originalWorld = AZ::Matrix4x4::CreateIdentity();
        AZ::Vector3 m_originalAnchor = AZ::Vector3::CreateZero();
        AZ::Aabb m_localBounds = AZ::Aabb::CreateNull();
        bool m_valid = false;
        bool BindSource(const AZStd::string& descriptor) override;
        AZStd::string GetSource() const override;
        bool CommitAssemblyState() override;
        AZStd::string GetStatus() const override;
        AZ::Matrix4x4 GetWorldMatrix() const override;
        AZStd::vector<AZ::u32> GetWorldMatrixBits() const override;
        AZ::Aabb GetWorldBounds() const override;
        AZ::Aabb GetEditorSelectionBoundsViewport(const AzFramework::ViewportInfo&) override;
    };
}
