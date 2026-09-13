// Copyright (c) Contributors to the Open 3D Engine Project.
// For complete copyright and license terms please see the LICENSE at the root of this distribution.
// SPDX-License-Identifier: Apache-2.0 OR MIT
#pragma once
#include <SceneAPI/SceneCore/Components/GenerationComponent.h>
#include <SceneAPI/SceneCore/Events/GenerateEventContext.h>

namespace TaintedGrailModdingSDK
{
    // Only explicit _foamesh.glb source projections use this preservation pass.
    class SourceMeshPreservationComponent final : public AZ::SceneAPI::SceneCore::GenerationComponent
    {
    public:
        AZ_COMPONENT(SourceMeshPreservationComponent, "{3576B668-08D8-4B01-A9DF-2544AA385CFA}", AZ::SceneAPI::SceneCore::GenerationComponent);
        SourceMeshPreservationComponent();
        static void Reflect(AZ::ReflectContext* context);
    private:
        AZ::SceneAPI::Events::ProcessingResult Preserve(AZ::SceneAPI::Events::PostGenerateEventContext& context);
    };
}
