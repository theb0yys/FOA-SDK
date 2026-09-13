/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "SourceSceneRenderComponent.h"
#include "SourceShaderRenderComponent.h"
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>
namespace TaintedGrailModdingSDK
{
    void SourceSceneRenderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc=azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<SourceSceneRenderComponent,EditorComponentBase>()->Version(1)->Field("Binding",&SourceSceneRenderComponent::m_binding);
            if (auto* ec=sc->GetEditContext())
            {
                ec->Class<SourceSceneRenderComponent>("Source scene rendering","Draws an explicit source shader binding using the source placement component.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData,"")
                    ->Attribute(AZ::Edit::Attributes::Category,"FOA")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu,AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default,&SourceSceneRenderComponent::m_binding,"Source draw","Private source drawing binding.")
                    ->Attribute(AZ::Edit::Attributes::Visibility,AZ::Edit::PropertyVisibility::Hide);
            }
        }
        if (auto* bc=azrtti_cast<AZ::BehaviorContext*>(context))
        {
            bc->EBus<SourceSceneRenderBus>("SourceSceneRenderBus")
                ->Attribute(AZ::Script::Attributes::Scope,AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Module,"foa")
                ->Event("BindDraw",&SourceSceneRenderRequests::BindDraw)
                ->Event("GetBinding",&SourceSceneRenderRequests::GetBinding)
                ->Event("GetStatus",&SourceSceneRenderRequests::GetStatus);
        }
    }
    void SourceSceneRenderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("SourceSceneRenderService")); }
    void SourceSceneRenderComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& s) { s.push_back(AZ_CRC_CE("SourceSceneRenderService")); }
    void SourceSceneRenderComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& s)
    { s.push_back(AZ_CRC_CE("TransformService")); s.push_back(AZ_CRC_CE("SourceScenePlacementService")); }
    void SourceSceneRenderComponent::Activate()
    {
        EditorComponentBase::Activate();
        m_registration="UNBOUND";
        if (!m_binding.empty())
        {
            m_registration="FAILED: native rendering service is unavailable";
            SourceShaderRenderBus::BroadcastResult(m_registration,&SourceShaderRenderRequests::AddEntityDraw,GetEntityId(),m_binding);
        }
        SourceSceneRenderBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler::BusConnect(GetEntityId());
        SyncVisibility();
    }
    void SourceSceneRenderComponent::Deactivate()
    {
        AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect(); SourceSceneRenderBus::Handler::BusDisconnect();
        SourceShaderRenderBus::Broadcast(&SourceShaderRenderRequests::RemoveEntityDraw,GetEntityId());
        EditorComponentBase::Deactivate();
    }
    bool SourceSceneRenderComponent::BindDraw(const AZStd::string& binding)
    {
        if (!m_binding.empty()) { const auto status=GetStatus(); return m_binding==binding && (status=="READY" || status=="LOADING"); }
        AZStd::string result="FAILED: native rendering service is unavailable";
        SourceShaderRenderBus::BroadcastResult(result,&SourceShaderRenderRequests::AddEntityDraw,GetEntityId(),binding);
        if (result!="LOADING") { return false; }
        m_binding=binding; m_registration=result; SyncVisibility(); SetDirty(); return true;
    }
    AZStd::string SourceSceneRenderComponent::GetBinding() const { return m_binding; }
    AZStd::string SourceSceneRenderComponent::GetStatus() const
    {
        if (m_binding.empty()) { return "UNBOUND"; }
        AZStd::string result="ABSENT";
        SourceShaderRenderBus::BroadcastResult(result,&SourceShaderRenderRequests::GetEntityDrawStatus,GetEntityId());
        if (result!="ABSENT") { return result; }
        return m_registration=="LOADING" ? "FAILED: source draw registration was lost" : m_registration;
    }
    void SourceSceneRenderComponent::SyncVisibility()
    {
        bool visible=false;
        AzToolsFramework::EditorEntityInfoRequestBus::EventResult(visible,GetEntityId(),&AzToolsFramework::EditorEntityInfoRequests::IsVisible);
        OnEntityVisibilityChanged(visible);
    }
    void SourceSceneRenderComponent::OnTransformChanged(const AZ::Transform&, const AZ::Transform&)
    { SourceShaderRenderBus::Broadcast(&SourceShaderRenderRequests::InvalidateEntityPlacement,GetEntityId()); }
    void SourceSceneRenderComponent::OnEntityVisibilityChanged(bool visible)
    { SourceShaderRenderBus::Broadcast(&SourceShaderRenderRequests::SetEntityVisibility,GetEntityId(),visible); }
}
