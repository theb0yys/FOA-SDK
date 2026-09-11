/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "AssetLocalisationModels.h"
#include <AzCore/Outcome/Outcome.h>
namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;
    class AssetLocalisationService final
    {
    public:
        static constexpr AZ::u64 MaximumImageBytes = 8 * 1024 * 1024;
        static constexpr AZ::u64 MaximumImagePixels = 4 * 1024 * 1024;
        static bool IsText(const AZStd::string& text, size_t maximum, bool multiline = false);
        static bool IsLanguage(const AZStd::string& language);
        static AZStd::string ImagePath(const AZStd::string& owner, const AZStd::string& fingerprint, const AZStd::string& mediaType);
        static AZStd::string BindingId(const AZStd::string& owner, const AZStd::string& target, const AZStd::string& slot);
        static AZStd::string Revision(const ProjectAssetProfile& asset, const AZStd::string& name);
        static AZStd::string Revision(const LocalisationEntry& entry);
        static AZStd::string Revision(const PresentationBinding& binding);
        static AZ::Outcome<void, AZStd::string> ValidateAsset(const ProjectAssetProfile&, const AZStd::string& owner);
        static AZ::Outcome<void, AZStd::string> ValidateEntry(const LocalisationEntry&);
        static AZ::Outcome<void, AZStd::string> ValidateBinding(const PresentationBinding&, const CatalogDatabase&);
        static LocalisedText Resolve(const LocalisationEntry&, const AZStd::string& language);
        static LocalisationEntry CanonicalEntry(LocalisationEntry entry);
    };
}
