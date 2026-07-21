/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    TaintedInterfaceUi::Service& FoundationService::GetTaintedInterfaceUiUtilities()
    {
        return m_taintedInterfaceUiUtilities;
    }

    const TaintedInterfaceUi::Service& FoundationService::GetTaintedInterfaceUiUtilities() const
    {
        return m_taintedInterfaceUiUtilities;
    }
} // namespace TaintedGrailModdingSDK
