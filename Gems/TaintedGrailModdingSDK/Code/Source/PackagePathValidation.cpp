/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackagePathValidation.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void SetError(AZStd::string* error, const char* message)
        {
            if (error)
            {
                *error = message;
            }
        }

        char LowerAscii(char value)
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        }

        bool IsReservedWindowsBaseName(const AZStd::string& component)
        {
            AZStd::string base = component;
            const size_t dot = base.find('.');
            if (dot != AZStd::string::npos)
            {
                base.resize(dot);
            }
            AZStd::transform(
                base.begin(),
                base.end(),
                base.begin(),
                LowerAscii);
            if (base == "con"
                || base == "prn"
                || base == "aux"
                || base == "nul"
                || base == "clock$")
            {
                return true;
            }
            if (base.size() == 4
                && base[3] >= '1'
                && base[3] <= '9')
            {
                const AZStd::string prefix = base.substr(0, 3);
                return prefix == "com" || prefix == "lpt";
            }
            return false;
        }

        bool ValidateComponent(
            const AZStd::string& component,
            AZStd::string* error)
        {
            if (component.empty()
                || component == "."
                || component == ".."
                || component.size() > 255)
            {
                SetError(error, "Package path components must be non-empty, bounded, and may not be dot segments.");
                return false;
            }
            if (component.back() == '.' || component.back() == ' ')
            {
                SetError(error, "Package path components may not end in a Windows-ignored dot or space.");
                return false;
            }
            if (component.find('~') != AZStd::string::npos)
            {
                SetError(error, "Package paths may not use DOS short-name alias syntax.");
                return false;
            }
            if (IsReservedWindowsBaseName(component))
            {
                SetError(error, "Package paths may not use a reserved Windows device name.");
                return false;
            }
            for (char character : component)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte < 0x21 || byte > 0x7e
                    || character == '\\'
                    || character == ':'
                    || character == '"'
                    || character == '<'
                    || character == '>'
                    || character == '|'
                    || character == '?'
                    || character == '*')
                {
                    SetError(
                        error,
                        "Package paths are restricted to unambiguous printable ASCII without Windows-reserved characters.");
                    return false;
                }
            }
            return true;
        }

        bool StartsWith(const AZStd::string& value, const char* prefix)
        {
            const AZStd::string prefixValue(prefix);
            return value.size() >= prefixValue.size()
                && value.substr(0, prefixValue.size()) == prefixValue;
        }

        bool EndsWith(const AZStd::string& value, const char* suffix)
        {
            const AZStd::string suffixValue(suffix);
            return value.size() >= suffixValue.size()
                && value.substr(value.size() - suffixValue.size()) == suffixValue;
        }

        bool IsLocalTerrainPackageIdentity(const AZStd::string& windowsIdentity)
        {
            return StartsWith(windowsIdentity, "derived/terrain/")
                || StartsWith(windowsIdentity, "staging/terrain/")
                || StartsWith(windowsIdentity, "sourceobservations/terrain/")
                || EndsWith(windowsIdentity, ".tgheightmap.json")
                || EndsWith(windowsIdentity, ".terrain.u16le")
                || windowsIdentity.find("_gsi") != AZStd::string::npos;
        }
    } // namespace

    bool TryBuildPackagePathIdentity(
        const AZStd::string& value,
        PackagePathIdentity& identity,
        AZStd::string* error)
    {
        identity = {};
        if (value.empty()
            || value.size() > 2048
            || value.front() == '/'
            || value.front() == '\\'
            || (value.size() > 1 && value[1] == ':'))
        {
            SetError(error, "Package paths must be bounded relative paths.");
            return false;
        }

        size_t start = 0;
        while (start <= value.size())
        {
            const size_t end = value.find('/', start);
            const size_t length = end == AZStd::string::npos
                ? value.size() - start
                : end - start;
            const AZStd::string component = value.substr(start, length);
            if (!ValidateComponent(component, error))
            {
                return false;
            }
            if (!identity.m_normalizedPath.empty())
            {
                identity.m_normalizedPath.push_back('/');
                identity.m_windowsIdentity.push_back('/');
            }
            identity.m_normalizedPath += component;
            for (char character : component)
            {
                identity.m_windowsIdentity.push_back(LowerAscii(character));
            }
            if (end == AZStd::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool IsSafePackageRelativePath(const AZStd::string& value)
    {
        PackagePathIdentity identity;
        return TryBuildPackagePathIdentity(value, identity);
    }

    bool IsLocalTerrainPackageExcludedPath(const AZStd::string& value)
    {
        PackagePathIdentity identity;
        return TryBuildPackagePathIdentity(value, identity)
            && IsLocalTerrainPackageIdentity(identity.m_windowsIdentity);
    }

    bool TryValidatePackManifestPackagePath(
        const AZStd::string& value,
        PackagePathIdentity& identity,
        AZStd::string* error)
    {
        if (!TryBuildPackagePathIdentity(value, identity, error))
        {
            return false;
        }
        if (IsLocalTerrainPackageIdentity(identity.m_windowsIdentity))
        {
            SetError(
                error,
                "Pack manifests cannot declare local terrain heightmap manifests, tile payloads, source observations, staging paths, or generated terrain preview projections.");
            return false;
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool IsPackagePathInsideRoot(
        const AZStd::string& root,
        const AZStd::string& path)
    {
        PackagePathIdentity rootIdentity;
        PackagePathIdentity pathIdentity;
        if (!TryBuildPackagePathIdentity(root, rootIdentity)
            || !TryBuildPackagePathIdentity(path, pathIdentity))
        {
            return false;
        }
        AZStd::string prefix = rootIdentity.m_windowsIdentity;
        prefix.push_back('/');
        return pathIdentity.m_windowsIdentity.size() > prefix.size()
            && pathIdentity.m_windowsIdentity.substr(0, prefix.size()) == prefix;
    }
} // namespace TaintedGrailModdingSDK
