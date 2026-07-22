/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeTypes.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>

namespace TaintedGrailModdingSDK::Interchange
{
    namespace
    {
        bool IsLowerAlpha(char value) { return value >= 'a' && value <= 'z'; }
        bool IsDigit(char value) { return value >= '0' && value <= '9'; }
        bool IsTokenByte(char value) { return IsLowerAlpha(value) || IsDigit(value) || value == '.' || value == '_' || value == '-'; }
        bool IsHexLower(char value) { return IsDigit(value) || (value >= 'a' && value <= 'f'); }

        bool IsReservedWindowsName(AZStd::string_view segment)
        {
            AZStd::string lower(segment);
            AZStd::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; });
            const size_t dot = lower.find('.');
            const AZStd::string_view stem = dot == AZStd::string::npos ? AZStd::string_view(lower) : AZStd::string_view(lower.data(), dot);
            if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") { return true; }
            if (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9') { return true; }
            return false;
        }

        template<class Enum, size_t Size>
        AZStd::string_view ToMappedToken(Enum value, const AZStd::array<AZStd::pair<Enum, AZStd::string_view>, Size>& mapping)
        {
            for (const auto& entry : mapping) { if (entry.first == value) { return entry.second; } }
            return {};
        }

        template<class Enum, size_t Size>
        bool ParseMappedToken(AZStd::string_view token, Enum& value, const AZStd::array<AZStd::pair<Enum, AZStd::string_view>, Size>& mapping)
        {
            for (const auto& entry : mapping) { if (entry.second == token) { value = entry.first; return true; } }
            return false;
        }

#define TG_ENUM_MAP(Name, ...) const auto Name = AZStd::array{__VA_ARGS__}
        TG_ENUM_MAP(PackageKindTokens, AZStd::pair{PackageKindV1::AuthoringInterchange, AZStd::string_view("authoring-interchange")});
        TG_ENUM_MAP(HostKindTokens, AZStd::pair{HostKindV1::O3de, AZStd::string_view("o3de")}, AZStd::pair{HostKindV1::Blender, AZStd::string_view("blender")}, AZStd::pair{HostKindV1::UnityEditor, AZStd::string_view("unity-editor")}, AZStd::pair{HostKindV1::HostNeutral, AZStd::string_view("host-neutral")});
        TG_ENUM_MAP(BindingStateTokens, AZStd::pair{BindingStateV1::Unresolved, AZStd::string_view("unresolved")}, AZStd::pair{BindingStateV1::Active, AZStd::string_view("active")}, AZStd::pair{BindingStateV1::Stale, AZStd::string_view("stale")}, AZStd::pair{BindingStateV1::Conflicted, AZStd::string_view("conflicted")}, AZStd::pair{BindingStateV1::Superseded, AZStd::string_view("superseded")}, AZStd::pair{BindingStateV1::Retired, AZStd::string_view("retired")});
        TG_ENUM_MAP(MappingOperationTokens, AZStd::pair{MappingOperationV1::Rename, AZStd::string_view("rename")}, AZStd::pair{MappingOperationV1::Move, AZStd::string_view("move")}, AZStd::pair{MappingOperationV1::Duplicate, AZStd::string_view("duplicate")}, AZStd::pair{MappingOperationV1::Fork, AZStd::string_view("fork")}, AZStd::pair{MappingOperationV1::Replace, AZStd::string_view("replace")}, AZStd::pair{MappingOperationV1::Merge, AZStd::string_view("merge")}, AZStd::pair{MappingOperationV1::Split, AZStd::string_view("split")}, AZStd::pair{MappingOperationV1::Aggregate, AZStd::string_view("aggregate")}, AZStd::pair{MappingOperationV1::Tombstone, AZStd::string_view("tombstone")});
        TG_ENUM_MAP(MappingCompletenessTokens, AZStd::pair{MappingCompletenessV1::Incomplete, AZStd::string_view("incomplete")}, AZStd::pair{MappingCompletenessV1::Complete, AZStd::string_view("complete")});
        TG_ENUM_MAP(PayloadRoleTokens, AZStd::pair{PayloadRoleV1::CanonicalDocument, AZStd::string_view("canonical-document")}, AZStd::pair{PayloadRoleV1::SceneSource, AZStd::string_view("scene-source")}, AZStd::pair{PayloadRoleV1::MeshSource, AZStd::string_view("mesh-source")}, AZStd::pair{PayloadRoleV1::TextureSource, AZStd::string_view("texture-source")}, AZStd::pair{PayloadRoleV1::MaterialMap, AZStd::string_view("material-map")}, AZStd::pair{PayloadRoleV1::SkeletonSource, AZStd::string_view("skeleton-source")}, AZStd::pair{PayloadRoleV1::AnimationSource, AZStd::string_view("animation-source")}, AZStd::pair{PayloadRoleV1::ColliderSource, AZStd::string_view("collider-source")}, AZStd::pair{PayloadRoleV1::Metadata, AZStd::string_view("metadata")}, AZStd::pair{PayloadRoleV1::Preview, AZStd::string_view("preview")}, AZStd::pair{PayloadRoleV1::LicenceNotice, AZStd::string_view("licence-notice")});
        TG_ENUM_MAP(TransformationPhaseTokens, AZStd::pair{TransformationPhaseV1::Authoring, AZStd::string_view("authoring")}, AZStd::pair{TransformationPhaseV1::Export, AZStd::string_view("export")}, AZStd::pair{TransformationPhaseV1::Import, AZStd::string_view("import")}, AZStd::pair{TransformationPhaseV1::Normalization, AZStd::string_view("normalization")}, AZStd::pair{TransformationPhaseV1::ReverseExport, AZStd::string_view("reverse-export")}, AZStd::pair{TransformationPhaseV1::Migration, AZStd::string_view("migration")});
        TG_ENUM_MAP(ReversibilityTokens, AZStd::pair{ReversibilityV1::Reversible, AZStd::string_view("reversible")}, AZStd::pair{ReversibilityV1::ConditionallyReversible, AZStd::string_view("conditionally-reversible")}, AZStd::pair{ReversibilityV1::Irreversible, AZStd::string_view("irreversible")}, AZStd::pair{ReversibilityV1::Unknown, AZStd::string_view("unknown")});
        TG_ENUM_MAP(LossSeverityTokens, AZStd::pair{LossSeverityV1::Information, AZStd::string_view("information")}, AZStd::pair{LossSeverityV1::Warning, AZStd::string_view("warning")}, AZStd::pair{LossSeverityV1::Error, AZStd::string_view("error")}, AZStd::pair{LossSeverityV1::Blocker, AZStd::string_view("blocker")});
        TG_ENUM_MAP(SourceKindTokens, AZStd::pair{SourceKindV1::ProjectOwned, AZStd::string_view("project-owned")}, AZStd::pair{SourceKindV1::Synthetic, AZStd::string_view("synthetic")}, AZStd::pair{SourceKindV1::UserSuppliedReviewed, AZStd::string_view("user-supplied-reviewed")}, AZStd::pair{SourceKindV1::UserSuppliedUnreviewed, AZStd::string_view("user-supplied-unreviewed")}, AZStd::pair{SourceKindV1::LocalReferenceOnly, AZStd::string_view("local-reference-only")}, AZStd::pair{SourceKindV1::UpstreamReference, AZStd::string_view("upstream-reference")}, AZStd::pair{SourceKindV1::Prohibited, AZStd::string_view("prohibited")});
        TG_ENUM_MAP(RedistributionTokens, AZStd::pair{RedistributionStateV1::Allowed, AZStd::string_view("allowed")}, AZStd::pair{RedistributionStateV1::LocalOnly, AZStd::string_view("local-only")}, AZStd::pair{RedistributionStateV1::ReviewRequired, AZStd::string_view("review-required")}, AZStd::pair{RedistributionStateV1::Blocked, AZStd::string_view("blocked")});
        TG_ENUM_MAP(IssueSeverityTokens, AZStd::pair{IssueSeverityV1::Information, AZStd::string_view("information")}, AZStd::pair{IssueSeverityV1::Warning, AZStd::string_view("warning")}, AZStd::pair{IssueSeverityV1::Error, AZStd::string_view("error")}, AZStd::pair{IssueSeverityV1::Blocker, AZStd::string_view("blocker")});
        TG_ENUM_MAP(MigrationStatusTokens, AZStd::pair{MigrationStatusV1::AlreadyCurrent, AZStd::string_view("already-current")}, AZStd::pair{MigrationStatusV1::SourceInvalid, AZStd::string_view("source-invalid")}, AZStd::pair{MigrationStatusV1::UnsupportedSourceVersion, AZStd::string_view("unsupported-source-version")}, AZStd::pair{MigrationStatusV1::UnsupportedTargetVersion, AZStd::string_view("unsupported-target-version")}, AZStd::pair{MigrationStatusV1::DowngradeForbidden, AZStd::string_view("downgrade-forbidden")}, AZStd::pair{MigrationStatusV1::MigratorUnavailable, AZStd::string_view("migrator-unavailable")}, AZStd::pair{MigrationStatusV1::SemanticDrift, AZStd::string_view("semantic-drift")}, AZStd::pair{MigrationStatusV1::Succeeded, AZStd::string_view("succeeded")});
        TG_ENUM_MAP(ParameterKindTokens, AZStd::pair{ParameterKindV1::Boolean, AZStd::string_view("boolean")}, AZStd::pair{ParameterKindV1::SignedInteger, AZStd::string_view("signed-integer")}, AZStd::pair{ParameterKindV1::UnsignedInteger, AZStd::string_view("unsigned-integer")}, AZStd::pair{ParameterKindV1::Number, AZStd::string_view("number")}, AZStd::pair{ParameterKindV1::String, AZStd::string_view("string")}, AZStd::pair{ParameterKindV1::Matrix4x4, AZStd::string_view("matrix4x4")}, AZStd::pair{ParameterKindV1::RationalRate, AZStd::string_view("rational-rate")});
#undef TG_ENUM_MAP
    }

    bool IsSemanticIdV1(AZStd::string_view value)
    {
        if (value.size() < 3 || value.size() > MaxSemanticTokenBytesV1 || !IsLowerAlpha(value.front()) || value.back() == '.' || value.back() == '_' || value.back() == '-') { return false; }
        bool hasNamespace = false;
        char previous = 0;
        for (char current : value)
        {
            if (!IsTokenByte(current)) { return false; }
            if (current == '.') { hasNamespace = true; }
            if ((current == '.' || current == '_' || current == '-') && (previous == '.' || previous == '_' || previous == '-')) { return false; }
            previous = current;
        }
        return hasNamespace;
    }

    bool IsExactVersionTokenV1(AZStd::string_view value)
    {
        if (value.empty() || value.size() > MaxSemanticTokenBytesV1) { return false; }
        for (char current : value) { if (!(IsLowerAlpha(current) || IsDigit(current) || current == '.' || current == '_' || current == '-')) { return false; } }
        return true;
    }

    bool IsSha256HexV1(AZStd::string_view value)
    {
        return value.size() == 64 && AZStd::all_of(value.begin(), value.end(), IsHexLower);
    }

    bool IsRelativePackagePathV1(AZStd::string_view value)
    {
        if (value.empty() || value.size() > MaxRelativePathBytesV1 || value.front() == '/' || value.back() == '/' || value.find('\\') != AZStd::string_view::npos || value.find(':') != AZStd::string_view::npos) { return false; }
        size_t start = 0;
        while (start < value.size())
        {
            const size_t slash = value.find('/', start);
            const size_t end = slash == AZStd::string_view::npos ? value.size() : slash;
            const AZStd::string_view segment = value.substr(start, end - start);
            if (segment.empty() || segment == "." || segment == ".." || segment.back() == '.' || segment.back() == ' ' || IsReservedWindowsName(segment)) { return false; }
            for (char current : segment) { const unsigned char byte = static_cast<unsigned char>(current); if (byte < 0x20 || byte > 0x7e) { return false; } }
            if (slash == AZStd::string_view::npos) { break; }
            start = slash + 1;
        }
        return true;
    }

#define TG_TOKEN_FUNCTIONS(EnumType, MapName) \
    AZStd::string_view ToToken(EnumType value) { return ToMappedToken(value, MapName); } \
    bool TryParseToken(AZStd::string_view token, EnumType& value) { return ParseMappedToken(token, value, MapName); }
    TG_TOKEN_FUNCTIONS(PackageKindV1, PackageKindTokens)
    TG_TOKEN_FUNCTIONS(HostKindV1, HostKindTokens)
    TG_TOKEN_FUNCTIONS(BindingStateV1, BindingStateTokens)
    TG_TOKEN_FUNCTIONS(MappingOperationV1, MappingOperationTokens)
    TG_TOKEN_FUNCTIONS(MappingCompletenessV1, MappingCompletenessTokens)
    TG_TOKEN_FUNCTIONS(PayloadRoleV1, PayloadRoleTokens)
    TG_TOKEN_FUNCTIONS(TransformationPhaseV1, TransformationPhaseTokens)
    TG_TOKEN_FUNCTIONS(ReversibilityV1, ReversibilityTokens)
    TG_TOKEN_FUNCTIONS(LossSeverityV1, LossSeverityTokens)
    TG_TOKEN_FUNCTIONS(SourceKindV1, SourceKindTokens)
    TG_TOKEN_FUNCTIONS(RedistributionStateV1, RedistributionTokens)
    TG_TOKEN_FUNCTIONS(IssueSeverityV1, IssueSeverityTokens)
    TG_TOKEN_FUNCTIONS(MigrationStatusV1, MigrationStatusTokens)
    TG_TOKEN_FUNCTIONS(ParameterKindV1, ParameterKindTokens)
#undef TG_TOKEN_FUNCTIONS
} // namespace TaintedGrailModdingSDK::Interchange
