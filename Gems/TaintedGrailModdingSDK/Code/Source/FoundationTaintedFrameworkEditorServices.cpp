/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    TaintedFrameworkEditorServices::Service&
    FoundationService::GetTaintedFrameworkEditorServices()
    {
        return m_taintedFrameworkEditorServices;
    }

    const TaintedFrameworkEditorServices::Service&
    FoundationService::GetTaintedFrameworkEditorServices() const
    {
        return m_taintedFrameworkEditorServices;
    }
} // namespace TaintedGrailModdingSDK
