/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterReleaseArtifactPaneSystemComponent.h"
#include "TaintedGrailModdingSDKSystemComponent.h"

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace TaintedGrailModdingSDK
{
    class TaintedGrailModdingSDKEditorModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            TaintedGrailModdingSDKEditorModule,
            "{EEB18F9A-E681-4287-AA41-ECEC87D4ED21}",
            AZ::Module);
        AZ_CLASS_ALLOCATOR(TaintedGrailModdingSDKEditorModule, AZ::SystemAllocator);

        TaintedGrailModdingSDKEditorModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    TaintedGrailModdingSDKSystemComponent::CreateDescriptor(),
                    AdapterReleaseArtifactPaneSystemComponent::CreateDescriptor(),
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<TaintedGrailModdingSDKSystemComponent>(),
                azrtti_typeid<AdapterReleaseArtifactPaneSystemComponent>(),
            };
        }
    };
} // namespace TaintedGrailModdingSDK

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(
    AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor),
    TaintedGrailModdingSDK::TaintedGrailModdingSDKEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(
    Gem_TaintedGrailModdingSDK_Editor,
    TaintedGrailModdingSDK::TaintedGrailModdingSDKEditorModule)
#endif
