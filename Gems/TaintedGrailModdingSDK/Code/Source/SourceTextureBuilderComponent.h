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
    //! Builds source-oriented GPU images; source shaders and sampler binding are separate.
    class SourceTextureBuilderComponent final
        : public AZ::Component
        , private AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_COMPONENT(SourceTextureBuilderComponent, "{31836E31-096E-42E0-A560-FC5F8B9D7274}");
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
