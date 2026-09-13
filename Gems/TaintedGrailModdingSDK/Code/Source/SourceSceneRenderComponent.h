/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/Component/TransformBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
namespace TaintedGrailModdingSDK
{
    class SourceSceneRenderRequests : public AZ::ComponentBus
    {
    public:
        static constexpr auto HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        virtual bool BindDraw(const AZStd::string& binding) = 0;
        virtual AZStd::string GetBinding() const = 0;
        virtual AZStd::string GetStatus() const = 0;
    };
    using SourceSceneRenderBus = AZ::EBus<SourceSceneRenderRequests>;
    class SourceSceneRenderComponent final
        : public AzToolsFramework::Components::EditorComponentBase
        , private SourceSceneRenderBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(SourceSceneRenderComponent, "{873203CF-475F-44CC-B940-D6BEA70656AC}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        void Activate() override;
        void Deactivate() override;
    private:
        AZStd::string m_binding;
        AZStd::string m_registration = "UNBOUND";
        bool BindDraw(const AZStd::string& binding) override;
        AZStd::string GetBinding() const override;
        AZStd::string GetStatus() const override;
        void SyncVisibility();
        void OnTransformChanged(const AZ::Transform&, const AZ::Transform&) override;
        void OnEntityVisibilityChanged(bool visible) override;
    };
}
