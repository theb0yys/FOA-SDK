/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/Component/Component.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AzCore/std/parallel/atomic.h>

namespace TaintedGrailModdingSDK
{
    //! Preserves explicitly bound source shader programs in native Atom assets.
    //! Material values, pass selection and runtime state belong to the source renderer.
    class SourceShaderBuilderComponent final
        : public AZ::Component
        , private AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_COMPONENT(SourceShaderBuilderComponent, "{ADAE1B0C-058A-477D-B4E5-AE52E49DA396}");
        static void Reflect(AZ::ReflectContext* context);
        void Activate() override;
        void Deactivate() override;
    private:
        void ShutDown() override;
        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest&, AssetBuilderSDK::CreateJobsResponse&);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest&, AssetBuilderSDK::ProcessJobResponse&);
        AZStd::atomic_bool m_stopping{false};
    };
}
