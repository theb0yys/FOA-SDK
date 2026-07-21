/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    ExtensionAPI::Service& FoundationService::GetExtensionAPI()
    {
        return m_extensionApi;
    }

    const ExtensionAPI::Service& FoundationService::GetExtensionAPI() const
    {
        return m_extensionApi;
    }
} // namespace TaintedGrailModdingSDK
