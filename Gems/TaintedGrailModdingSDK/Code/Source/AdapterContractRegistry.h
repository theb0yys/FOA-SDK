/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterCapability : AZ::u8
    {
        ItemGrant,
        RecipeLearn,
        RecipeAppend,
        CustomItemRegistration,
        CustomRecipeRegistration,
        VendorMutation,
        LootMutation,
        RewardMutation,
        Persistence,
        Cleanup,
        Rollback,
    };

    AZStd::string ToString(AdapterCapability capability);
    bool TryParseAdapterCapability(const AZStd::string& value, AdapterCapability& capability);

    struct AdapterSemanticVersion
    {
        AZ::u64 m_major = 0;
        AZ::u64 m_minor = 0;
        AZ::u64 m_patch = 0;
        AZStd::vector<AZStd::string> m_prerelease;
    };

    bool TryParseAdapterSemanticVersion(
        const AZStd::string& value,
        AdapterSemanticVersion& version);
    int CompareAdapterSemanticVersions(
        const AdapterSemanticVersion& left,
        const AdapterSemanticVersion& right);
    bool IsAdapterVersionCompatible(
        const AZStd::string& requiredVersion,
        const AZStd::string& declaredVersion);

    struct AdapterCapabilityDeclaration
    {
        AdapterCapability m_capability = AdapterCapability::ItemGrant;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };

    struct AdapterDeclaration
    {
        AZStd::string m_adapterId;
        AZStd::string m_displayName;
        AZStd::string m_version;
        AZStd::vector<AZStd::string> m_runtimeTargets;
        AZStd::vector<AZStd::string> m_identityEvidenceIds;
        AZStd::vector<AdapterCapabilityDeclaration> m_capabilities;
    };

    class AdapterContractRegistry
    {
    public:
        static AdapterContractRegistry& Get();

        bool RegisterDeclaration(
            const AdapterDeclaration& declaration,
            AZStd::string* error = nullptr);
        void Clear();

        const AdapterDeclaration* FindByAdapterId(
            const AZStd::string& adapterId) const;
        const AZStd::vector<AdapterDeclaration>& GetDeclarations() const;

    private:
        AZStd::vector<AdapterDeclaration> m_declarations;
    };
} // namespace TaintedGrailModdingSDK
