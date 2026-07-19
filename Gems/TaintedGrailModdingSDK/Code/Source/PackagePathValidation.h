/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct PackagePathIdentity
    {
        AZStd::string m_normalizedPath;
        AZStd::string m_windowsIdentity;
    };

    bool TryBuildPackagePathIdentity(
        const AZStd::string& value,
        PackagePathIdentity& identity,
        AZStd::string* error = nullptr);
    bool IsSafePackageRelativePath(const AZStd::string& value);
    bool IsPackagePathInsideRoot(
        const AZStd::string& root,
        const AZStd::string& path);
} // namespace TaintedGrailModdingSDK
