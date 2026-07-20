/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterContractRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/string/regex.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsIdentifierCharacter(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'A' && value <= 'Z')
                || (value >= 'a' && value <= 'z')
                || value == '-';
        }

        bool IsValidIdentifierList(
            const AZStd::string& value,
            bool enforceNumericLeadingZero)
        {
            if (value.empty())
            {
                return false;
            }

            size_t start = 0;
            while (start < value.size())
            {
                const size_t end = value.find('.', start);
                const size_t length = (end == AZStd::string::npos)
                    ? value.size() - start
                    : end - start;
                if (length == 0)
                {
                    return false;
                }

                bool numeric = true;
                for (size_t index = start; index < start + length; ++index)
                {
                    if (!IsIdentifierCharacter(value[index]))
                    {
                        return false;
                    }
                    numeric = numeric && value[index] >= '0' && value[index] <= '9';
                }
                if (enforceNumericLeadingZero && numeric && length > 1 && value[start] == '0')
                {
                    return false;
                }

                if (end == AZStd::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

        bool ParseNumber(const AZStd::string& value, AZ::u64& result)
        {
            if (value.empty() || (value.size() > 1 && value[0] == '0'))
            {
                return false;
            }

            result = 0;
            for (char character : value)
            {
                if (character < '0' || character > '9')
                {
                    return false;
                }
                const AZ::u64 digit = static_cast<AZ::u64>(character - '0');
                if (result > (AZStd::numeric_limits<AZ::u64>::max() - digit) / 10)
                {
                    return false;
                }
                result = result * 10 + digit;
            }
            return true;
        }

        AZStd::vector<AZStd::string> SplitIdentifiers(const AZStd::string& value)
        {
            AZStd::vector<AZStd::string> result;
            size_t start = 0;
            while (start < value.size())
            {
                const size_t end = value.find('.', start);
                result.push_back(value.substr(
                    start,
                    end == AZStd::string::npos ? AZStd::string::npos : end - start));
                if (end == AZStd::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return result;
        }

        bool IsNumericIdentifier(const AZStd::string& value)
        {
            if (value.empty())
            {
                return false;
            }
            for (char character : value)
            {
                if (character < '0' || character > '9')
                {
                    return false;
                }
            }
            return true;
        }

        int ComparePrereleaseIdentifier(
            const AZStd::string& left,
            const AZStd::string& right)
        {
            const bool leftNumeric = IsNumericIdentifier(left);
            const bool rightNumeric = IsNumericIdentifier(right);
            if (leftNumeric && rightNumeric)
            {
                if (left.size() != right.size())
                {
                    return left.size() < right.size() ? -1 : 1;
                }
            }
            else if (leftNumeric != rightNumeric)
            {
                return leftNumeric ? -1 : 1;
            }

            if (left == right)
            {
                return 0;
            }
            return left < right ? -1 : 1;
        }

        bool ContainsCapability(
            const AZStd::vector<AdapterCapabilityDeclaration>& declarations,
            AdapterCapability capability)
        {
            for (const AdapterCapabilityDeclaration& declaration : declarations)
            {
                if (declaration.m_capability == capability)
                {
                    return true;
                }
            }
            return false;
        }

        bool ContainsRuntimeTarget(
            const AZStd::vector<AZStd::string>& runtimeTargets,
            const AZStd::string& runtimeTarget)
        {
            return AZStd::find(runtimeTargets.begin(), runtimeTargets.end(), runtimeTarget)
                != runtimeTargets.end();
        }
    } // namespace

    AZStd::string ToString(AdapterCapability capability)
    {
        switch (capability)
        {
        case AdapterCapability::ItemGrant:
            return "item_grant";
        case AdapterCapability::RecipeLearn:
            return "recipe_learn";
        case AdapterCapability::RecipeAppend:
            return "recipe_append";
        case AdapterCapability::CustomItemRegistration:
            return "custom_item_registration";
        case AdapterCapability::CustomRecipeRegistration:
            return "custom_recipe_registration";
        case AdapterCapability::VendorMutation:
            return "vendor_mutation";
        case AdapterCapability::LootMutation:
            return "loot_mutation";
        case AdapterCapability::RewardMutation:
            return "reward_mutation";
        case AdapterCapability::Persistence:
            return "persistence";
        case AdapterCapability::Cleanup:
            return "cleanup";
        case AdapterCapability::Rollback:
            return "rollback";
        }
        return "unknown";
    }

    bool TryParseAdapterCapability(
        const AZStd::string& value,
        AdapterCapability& capability)
    {
        static const AdapterCapability capabilities[] = {
            AdapterCapability::ItemGrant,
            AdapterCapability::RecipeLearn,
            AdapterCapability::RecipeAppend,
            AdapterCapability::CustomItemRegistration,
            AdapterCapability::CustomRecipeRegistration,
            AdapterCapability::VendorMutation,
            AdapterCapability::LootMutation,
            AdapterCapability::RewardMutation,
            AdapterCapability::Persistence,
            AdapterCapability::Cleanup,
            AdapterCapability::Rollback,
        };
        for (AdapterCapability candidate : capabilities)
        {
            if (ToString(candidate) == value)
            {
                capability = candidate;
                return true;
            }
        }
        return false;
    }

    bool TryParseAdapterSemanticVersion(
        const AZStd::string& value,
        AdapterSemanticVersion& version)
    {
        if (value.empty())
        {
            return false;
        }

        const size_t buildPosition = value.find('+');
        const AZStd::string withoutBuild = value.substr(0, buildPosition);
        if (buildPosition != AZStd::string::npos)
        {
            const AZStd::string build = value.substr(buildPosition + 1);
            if (!IsValidIdentifierList(build, false))
            {
                return false;
            }
        }

        const size_t prereleasePosition = withoutBuild.find('-');
        const AZStd::string core = withoutBuild.substr(0, prereleasePosition);
        const AZStd::string prerelease = prereleasePosition == AZStd::string::npos
            ? AZStd::string{}
            : withoutBuild.substr(prereleasePosition + 1);
        if (prereleasePosition != AZStd::string::npos
            && !IsValidIdentifierList(prerelease, true))
        {
            return false;
        }

        const size_t firstDot = core.find('.');
        const size_t secondDot = firstDot == AZStd::string::npos
            ? AZStd::string::npos
            : core.find('.', firstDot + 1);
        if (firstDot == AZStd::string::npos
            || secondDot == AZStd::string::npos
            || core.find('.', secondDot + 1) != AZStd::string::npos)
        {
            return false;
        }

        AdapterSemanticVersion parsed;
        if (!ParseNumber(core.substr(0, firstDot), parsed.m_major)
            || !ParseNumber(
                core.substr(firstDot + 1, secondDot - firstDot - 1),
                parsed.m_minor)
            || !ParseNumber(core.substr(secondDot + 1), parsed.m_patch))
        {
            return false;
        }
        if (!prerelease.empty())
        {
            parsed.m_prerelease = SplitIdentifiers(prerelease);
        }
        version = AZStd::move(parsed);
        return true;
    }

    int CompareAdapterSemanticVersions(
        const AdapterSemanticVersion& left,
        const AdapterSemanticVersion& right)
    {
        if (left.m_major != right.m_major)
        {
            return left.m_major < right.m_major ? -1 : 1;
        }
        if (left.m_minor != right.m_minor)
        {
            return left.m_minor < right.m_minor ? -1 : 1;
        }
        if (left.m_patch != right.m_patch)
        {
            return left.m_patch < right.m_patch ? -1 : 1;
        }
        if (left.m_prerelease.empty() != right.m_prerelease.empty())
        {
            return left.m_prerelease.empty() ? 1 : -1;
        }

        const size_t count = AZStd::min(
            left.m_prerelease.size(),
            right.m_prerelease.size());
        for (size_t index = 0; index < count; ++index)
        {
            const int result = ComparePrereleaseIdentifier(
                left.m_prerelease[index],
                right.m_prerelease[index]);
            if (result != 0)
            {
                return result;
            }
        }
        if (left.m_prerelease.size() == right.m_prerelease.size())
        {
            return 0;
        }
        return left.m_prerelease.size() < right.m_prerelease.size() ? -1 : 1;
    }

    bool IsAdapterVersionCompatible(
        const AZStd::string& requiredVersion,
        const AZStd::string& declaredVersion)
    {
        AdapterSemanticVersion required;
        AdapterSemanticVersion declared;
        if (!TryParseAdapterSemanticVersion(requiredVersion, required)
            || !TryParseAdapterSemanticVersion(declaredVersion, declared))
        {
            return false;
        }
        if (required.m_major != declared.m_major)
        {
            return false;
        }
        if (required.m_major == 0 && required.m_minor != declared.m_minor)
        {
            return false;
        }
        return CompareAdapterSemanticVersions(declared, required) >= 0;
    }

    AdapterContractRegistry& AdapterContractRegistry::Get()
    {
        static AdapterContractRegistry registry;
        return registry;
    }

    bool AdapterContractRegistry::RegisterDeclaration(
        const AdapterDeclaration& declaration,
        AZStd::string* error)
    {
        static const AZStd::regex adapterIdPattern(
            "^[a-z0-9][a-z0-9._-]*\\.[a-z0-9][a-z0-9._-]*$");

        AdapterSemanticVersion parsedVersion;
        if (!AZStd::regex_match(declaration.m_adapterId, adapterIdPattern)
            || declaration.m_displayName.empty()
            || !TryParseAdapterSemanticVersion(declaration.m_version, parsedVersion))
        {
            if (error)
            {
                *error = "Adapter declarations require a lowercase namespaced ID, display name, and semantic version.";
            }
            return false;
        }
        if (FindByAdapterId(declaration.m_adapterId))
        {
            if (error)
            {
                *error = "Adapter declaration ID already exists.";
            }
            return false;
        }
        if (declaration.m_runtimeTargets.empty())
        {
            if (error)
            {
                *error = "Adapter declarations require at least one runtime target.";
            }
            return false;
        }
        AZStd::vector<AZStd::string> runtimeTargets;
        for (const AZStd::string& runtimeTarget : declaration.m_runtimeTargets)
        {
            if ((runtimeTarget != "Mono" && runtimeTarget != "IL2CPP")
                || ContainsRuntimeTarget(runtimeTargets, runtimeTarget))
            {
                if (error)
                {
                    *error = "Adapter runtime targets must be unique and exactly Mono or IL2CPP.";
                }
                return false;
            }
            runtimeTargets.push_back(runtimeTarget);
        }
        if (declaration.m_capabilities.empty())
        {
            if (error)
            {
                *error = "Adapter declarations require at least one explicit capability.";
            }
            return false;
        }
        AZStd::vector<AdapterCapabilityDeclaration> capabilities;
        for (const AdapterCapabilityDeclaration& capability : declaration.m_capabilities)
        {
            if (ContainsCapability(capabilities, capability.m_capability))
            {
                if (error)
                {
                    *error = "Adapter capabilities must be unique within a declaration.";
                }
                return false;
            }
            capabilities.push_back(capability);
        }

        m_declarations.push_back(declaration);
        AZStd::sort(
            m_declarations.begin(),
            m_declarations.end(),
            [](const AdapterDeclaration& left, const AdapterDeclaration& right)
            {
                return left.m_adapterId < right.m_adapterId;
            });
        return true;
    }

    void AdapterContractRegistry::Clear()
    {
        m_declarations.clear();
    }

    const AdapterDeclaration* AdapterContractRegistry::FindByAdapterId(
        const AZStd::string& adapterId) const
    {
        for (const AdapterDeclaration& declaration : m_declarations)
        {
            if (declaration.m_adapterId == adapterId)
            {
                return &declaration;
            }
        }
        return nullptr;
    }

    const AZStd::vector<AdapterDeclaration>& AdapterContractRegistry::GetDeclarations() const
    {
        return m_declarations;
    }
} // namespace TaintedGrailModdingSDK
