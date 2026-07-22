/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeCanonical.h"

namespace TaintedGrailModdingSDK::Interchange
{
    // CanonicalInterchangeCanonical.cpp is compiled under these explicit legacy
    // names by the per-source definitions in CMakeLists.txt. The wrappers below
    // correct the accepted Schema-1 omit-when-empty display_name contract while
    // preserving the reviewed field writer and every other projection.
    AZStd::string SerializeCanonicalManifestIncludingEmptyDisplayNameV1(
        const CanonicalInterchangeManifestV1& manifest);
    AZStd::string SerializeDeclaredPackageFingerprintInputIncludingEmptyDisplayNameV1(
        const CanonicalInterchangeManifestV1& manifest);

    namespace
    {
        constexpr AZStd::string_view EmptyDisplayNameMember =
            ",\"display_name\":\"\"";
        constexpr AZStd::string_view ManifestDigestPrefix =
            "{\"manifest_digest\":\"";
    }

    AZStd::string SerializeCanonicalManifestV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        AZStd::string output =
            SerializeCanonicalManifestIncludingEmptyDisplayNameV1(manifest);
        if (output.empty() || !manifest.m_displayName.empty())
        {
            return output;
        }

        const size_t position = output.find(EmptyDisplayNameMember);
        if (position == AZStd::string::npos)
        {
            return {};
        }
        output.erase(position, EmptyDisplayNameMember.size());
        return output;
    }

    AZStd::string SerializeDeclaredPackageFingerprintInputV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        AZStd::string input =
            SerializeDeclaredPackageFingerprintInputIncludingEmptyDisplayNameV1(manifest);
        if (input.empty() || !manifest.m_displayName.empty())
        {
            return input;
        }

        const AZStd::string legacyManifest =
            SerializeCanonicalManifestIncludingEmptyDisplayNameV1(manifest);
        const AZStd::string canonicalManifest = SerializeCanonicalManifestV1(manifest);
        if (legacyManifest.empty() || canonicalManifest.empty() ||
            !input.starts_with(ManifestDigestPrefix))
        {
            return {};
        }

        const Sha256DigestV1 legacyDigest =
            CalculateCanonicalBytesDigestV1(legacyManifest);
        const Sha256DigestV1 canonicalDigest =
            CalculateCanonicalBytesDigestV1(canonicalManifest);
        const size_t digestOffset = ManifestDigestPrefix.size();
        if (!IsSha256HexV1(legacyDigest.m_hex) ||
            !IsSha256HexV1(canonicalDigest.m_hex) ||
            input.size() < digestOffset + legacyDigest.m_hex.size() ||
            input.compare(digestOffset, legacyDigest.m_hex.size(), legacyDigest.m_hex) != 0)
        {
            return {};
        }

        input.replace(
            digestOffset,
            legacyDigest.m_hex.size(),
            canonicalDigest.m_hex);
        return input;
    }

    Sha256DigestV1 CalculateCanonicalManifestDigestV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string bytes = SerializeCanonicalManifestV1(manifest);
        return bytes.empty()
            ? Sha256DigestV1{}
            : CalculateCanonicalBytesDigestV1(bytes);
    }

    Sha256DigestV1 CalculateDeclaredPackageFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string input =
            SerializeDeclaredPackageFingerprintInputV1(manifest);
        return input.empty()
            ? Sha256DigestV1{}
            : CalculateCanonicalBytesDigestV1(input);
    }

    bool CanonicalManifestBytesMatchV1(
        AZStd::string_view bytes,
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string expected = SerializeCanonicalManifestV1(manifest);
        return !expected.empty() && bytes == expected;
    }
} // namespace TaintedGrailModdingSDK::Interchange
