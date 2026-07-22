/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "CanonicalInterchangeTypes.h"

#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::Interchange
{
    // Gate 5 canonical functions are pure and operate only on caller-supplied
    // values. They perform no filesystem, clock, environment, provider, host,
    // runtime, deployment, save, evidence-promotion, signing, or publication
    // operation.
    bool IsValidUtf8V1(AZStd::string_view value);
    bool AppendCanonicalPresentationStringV1(
        AZStd::string& output,
        AZStd::string_view value);
    bool FormatCanonicalFiniteNumberV1(
        double value,
        AZStd::string& output);
    Sha256DigestV1 CalculateCanonicalBytesDigestV1(
        AZStd::string_view bytes);
    bool CanonicalBytesDigestMatchesV1(
        AZStd::string_view bytes,
        const Sha256DigestV1& digest);

    // Returns an empty string when a supplied value cannot be represented by
    // the closed Schema-1 canonical profile. Intrinsic issue reporting belongs
    // to CanonicalInterchangeValidation; this layer never repairs input.
    AZStd::string SerializeCanonicalManifestV1(
        const CanonicalInterchangeManifestV1& manifest);
    AZStd::string SerializeDeclaredPackageFingerprintInputV1(
        const CanonicalInterchangeManifestV1& manifest);

    Sha256DigestV1 CalculateCanonicalManifestDigestV1(
        const CanonicalInterchangeManifestV1& manifest);
    Sha256DigestV1 CalculateDeclaredPackageFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest);
    RevisionFingerprintV1 CalculateDocumentRevisionFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest,
        const DomainDocumentRecordV1& document);
    RevisionFingerprintV1 CalculateAssetRevisionFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest,
        const AssetRecordV1& asset);

    bool CanonicalManifestBytesMatchV1(
        AZStd::string_view bytes,
        const CanonicalInterchangeManifestV1& manifest);
} // namespace TaintedGrailModdingSDK::Interchange
