/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    bool FoundationService::CreateExtensionClient(
        const AZStd::string& extensionId,
        ExtensionAPI::Client& client,
        AZStd::string* error)
    {
        const auto declarations = m_extensionApi.GetRegisteredExtensions();
        const auto found = AZStd::find_if(
            declarations.begin(), declarations.end(),
            [&extensionId](const ExtensionAPI::ExtensionDeclaration& declaration)
            {
                return declaration.m_extensionId == extensionId;
            });
        if (found == declarations.end())
        {
            if (error)
            {
                *error = "Extension client requires one previously registered extension identity.";
            }
            client = {};
            return false;
        }
        client = ExtensionAPI::Client(m_extensionApi, extensionId);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    AZStd::vector<ExtensionAPI::ExtensionDeclaration>
    FoundationService::GetRegisteredExtensions() const
    {
        return m_extensionApi.GetRegisteredExtensions();
    }
} // namespace TaintedGrailModdingSDK
