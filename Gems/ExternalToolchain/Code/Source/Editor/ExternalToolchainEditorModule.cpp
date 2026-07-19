/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainEditorSystemComponent.h"

#include <ExternalToolchain/ExternalToolchainTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace ExternalToolchain
{
    class ExternalToolchainEditorModule final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            ExternalToolchainEditorModule,
            ExternalToolchainEditorModuleTypeId,
            AZ::Module);
        AZ_CLASS_ALLOCATOR(ExternalToolchainEditorModule, AZ::SystemAllocator);

        ExternalToolchainEditorModule()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    ExternalToolchainEditorSystemComponent::CreateDescriptor(),
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<ExternalToolchainEditorSystemComponent>(),
            };
        }
    };
} // namespace ExternalToolchain

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(
    AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor),
    ExternalToolchain::ExternalToolchainEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(
    Gem_ExternalToolchain_Editor,
    ExternalToolchain::ExternalToolchainEditorModule)
#endif
