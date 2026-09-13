/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
namespace TaintedGrailModdingSDK
{
    // Private native drawing boundary. It does not select game passes or invent resource values.
    class SourceShaderRenderRequests : public AZ::EBusTraits
    {
    public:
        static constexpr auto HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr auto AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual AZStd::string SetDraw(const AZStd::string& descriptor) = 0;
        virtual AZStd::string GetStatus() const = 0;
        virtual void ClearDraw() = 0;
        virtual AZStd::string GetStatistics() const = 0;
        virtual AZ::Matrix4x4 GetViewportWorldToClip() const = 0;
        virtual AZ::Vector3 GetViewportCameraPosition() const = 0;
        // Entity operations are native-only; automation uses the persistent entity component.
        virtual AZStd::string AddEntityDraw(AZ::EntityId entity, const AZStd::string& binding) = 0;
        virtual AZStd::string GetEntityDrawStatus(AZ::EntityId entity) const = 0;
        virtual void RemoveEntityDraw(AZ::EntityId entity) = 0;
        virtual void InvalidateEntityPlacement(AZ::EntityId entity) = 0;
        virtual void SetEntityVisibility(AZ::EntityId entity, bool visible) = 0;
    };
    using SourceShaderRenderBus = AZ::EBus<SourceShaderRenderRequests>;
    class SourceShaderRenderComponent final
        : public AZ::Component
        , private SourceShaderRenderBus::Handler
        , private AZ::TickBus::Handler
        , private AZ::RPI::ViewportContextNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(SourceShaderRenderComponent, "{70B93727-3183-4C1F-9483-E7DE61C9D10B}");
        SourceShaderRenderComponent();
        ~SourceShaderRenderComponent() override;
        static void Reflect(AZ::ReflectContext* context);
        void Activate() override;
        void Deactivate() override;
    private:
        struct Draw;
        struct SharedGeometryBuffer;
        struct SharedStage;
        AZStd::unique_ptr<Draw> m_draw;
        AZStd::vector<AZStd::unique_ptr<Draw>> m_retired;
        void CollectRetired();
        AZStd::unordered_map<AZ::EntityId, AZStd::vector<AZStd::unique_ptr<Draw>>> m_entities;
        AZStd::unordered_map<AZStd::string, AZStd::shared_ptr<SharedGeometryBuffer>> m_geometryBuffers;
        AZStd::unordered_map<AZStd::string, AZStd::shared_ptr<SharedStage>> m_sharedStages;
        size_t m_stageReuses = 0, m_samplerReservations = 0;
        AZStd::vector<Draw*> m_entityOrder;
        size_t m_submittedEntities = 0, m_submittedEntityDraws = 0;
        size_t m_residentBytes = 0, m_dataBufferBytes = 0, m_geometryBytes = 0, m_geometryBuilds = 0, m_geometryReuses = 0;
        size_t m_lastTickVisits = 0, m_peakTickVisits = 0;
        bool Admit(Draw& draw);
        void Release(Draw& draw);
        size_t m_nextEntity = 0, m_lastTickWork = 0, m_peakTickWork = 0;
        AZ::RPI::ViewPtr m_cameraView;
        AZ::Matrix4x4 m_worldToClip = AZ::Matrix4x4::CreateIdentity();
        AZ::Vector3 m_cameraPosition = AZ::Vector3::CreateZero();
        bool m_cameraValid = false;
        void RefreshCamera();
        void Retire(AZStd::unique_ptr<Draw> draw);
        bool Queue(Draw& draw);
        void Advance(Draw& draw, size_t& work);
        bool UpdateMatrices(Draw& draw, bool upload);
        AZStd::string AddEntityDraw(AZ::EntityId entity, const AZStd::string& binding) override;
        AZStd::string GetEntityDrawStatus(AZ::EntityId entity) const override;
        void RemoveEntityDraw(AZ::EntityId entity) override;
        void InvalidateEntityPlacement(AZ::EntityId entity) override;
        void SetEntityVisibility(AZ::EntityId entity, bool visible) override;
        AZStd::string SetDraw(const AZStd::string& descriptor) override;
        AZStd::string GetStatus() const override;
        void ClearDraw() override;
        AZStd::string GetStatistics() const override;
        AZ::Matrix4x4 GetViewportWorldToClip() const override;
        AZ::Vector3 GetViewportCameraPosition() const override;
        void OnTick(float, AZ::ScriptTimePoint) override;
        void OnRenderTick() override;
        bool Build(Draw& draw);
    };
}
