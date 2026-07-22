/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "CanonicalInterchangeCanonical.h"
#include "CanonicalInterchangeMigration.h"
#include "CanonicalInterchangeValidation.h"

#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::Interchange::TestFixtures
{
    inline constexpr AZStd::string_view MinimalDocumentsJson = R"json({"schema_version":1,"schema_profile":"foa-interchange-schema-v1","canonical_profile":"foa-interchange-canonical-json-v1","package_kind":"authoring-interchange","package_id":"example.documents","pack_id":"example.documents-pack","producer":{"host_kind":"host-neutral","application_id":"foa.synthetic","application_version":"1","provider_id":"","provider_version":"","extension_id":"","extension_version":"","extension_revision":"","extension_digest":"","configuration_fingerprint":"1111111111111111111111111111111111111111111111111111111111111111","workspace_id_observation":"","profile_id_observation":""},"intended_consumers":[],"toolchain_lock":{"foa_sdk_commit":"9f8b8c6dfd842ac27b6cd68c0e55272ae753fdd3","schema_profile":"foa-interchange-schema-v1","configuration_fingerprint":"1111111111111111111111111111111111111111111111111111111111111111","fixture_revision":"schema-v1-public-example","components":[{"component_kind":"sdk","component_id":"foa.sdk","version":"1","revision":"9f8b8c6dfd842ac27b6cd68c0e55272ae753fdd3","digest":"2222222222222222222222222222222222222222222222222222222222222222"}]},"spatial_basis":{"handedness":"right-handed","right_axis":"+x","forward_axis":"+y","up_axis":"+z","linear_unit":"metre","angular_unit":"radian","vector_convention":"column-vector","multiplication_convention":"matrix-on-left","storage_order":"column-major"},"temporal_basis":{"time_unit":"second"},"documents":[{"document_id":"document.example","document_kind":"example-document","document_schema_id":"foa.example-document","document_schema_version":1,"payload_id":"payload.document","revision_fingerprint":"b82118f293b6b5c49f5834b837c18d2432f8ce7a3907d9c3dfdb3f913eec1ed6","subject_refs":["subject.example"],"asset_ids":[],"provenance_ids":[],"licensing_ids":[]}],"assets":[],"identity_mappings":[],"bindings":[],"payloads":[{"payload_id":"payload.document","relative_path":"documents/example.json","role":"canonical-document","media_type":"application/json","byte_size":2,"digest":"44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a","dependency_ids":[],"subject_ids":["subject.example"],"provenance_ids":[],"licensing_ids":[]}],"transformations":[],"losses":[],"provenance":[],"licensing":[],"evidence_refs":[],"extensions":[]})json";

    inline constexpr AZStd::string_view MinimalAssetJson = R"json({"schema_version":1,"schema_profile":"foa-interchange-schema-v1","canonical_profile":"foa-interchange-canonical-json-v1","package_kind":"authoring-interchange","package_id":"example.asset-package","pack_id":"example.asset-pack","producer":{"host_kind":"host-neutral","application_id":"foa.synthetic","application_version":"1","provider_id":"","provider_version":"","extension_id":"","extension_version":"","extension_revision":"","extension_digest":"","configuration_fingerprint":"1111111111111111111111111111111111111111111111111111111111111111","workspace_id_observation":"","profile_id_observation":""},"intended_consumers":[],"toolchain_lock":{"foa_sdk_commit":"9f8b8c6dfd842ac27b6cd68c0e55272ae753fdd3","schema_profile":"foa-interchange-schema-v1","configuration_fingerprint":"1111111111111111111111111111111111111111111111111111111111111111","fixture_revision":"schema-v1-public-example","components":[{"component_kind":"sdk","component_id":"foa.sdk","version":"1","revision":"9f8b8c6dfd842ac27b6cd68c0e55272ae753fdd3","digest":"2222222222222222222222222222222222222222222222222222222222222222"}]},"spatial_basis":{"handedness":"right-handed","right_axis":"+x","forward_axis":"+y","up_axis":"+z","linear_unit":"metre","angular_unit":"radian","vector_convention":"column-vector","multiplication_convention":"matrix-on-left","storage_order":"column-major"},"temporal_basis":{"time_unit":"second"},"documents":[{"document_id":"document.example","document_kind":"example-document","document_schema_id":"foa.example-document","document_schema_version":1,"payload_id":"payload.document","revision_fingerprint":"dfdcab795bb1f7179363ae61374f6d4959ac22c433c9a78b409653b4fe41a23a","subject_refs":["subject.example"],"asset_ids":["asset.example"],"provenance_ids":[],"licensing_ids":[]}],"assets":[{"asset_id":"asset.example","asset_kind":"example-mesh","revision_fingerprint":"20b0410ccfedd7a7dd10fb6ff5d8decbb32500a14e36e75b1729206bdcc4e718","payload_ids":["payload.asset"],"document_ids":["document.example"],"subject_refs":["subject.example"],"provenance_ids":[],"licensing_ids":[]}],"identity_mappings":[],"bindings":[],"payloads":[{"payload_id":"payload.asset","relative_path":"assets/example.mesh","role":"mesh-source","media_type":"application/octet-stream","byte_size":4,"digest":"d30ca7a7a32bf5772dc5eb2a2e7bd35737eff795ad74f2479b359716b59abdfa","dependency_ids":[],"subject_ids":["asset.example","subject.example"],"provenance_ids":[],"licensing_ids":[]},{"payload_id":"payload.document","relative_path":"documents/example.json","role":"canonical-document","media_type":"application/json","byte_size":2,"digest":"44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a","dependency_ids":[],"subject_ids":["subject.example"],"provenance_ids":[],"licensing_ids":[]}],"transformations":[],"losses":[],"provenance":[],"licensing":[],"evidence_refs":[],"extensions":[]})json";

    inline bool ContainsIssue(
        const CanonicalInterchangeValidationResultV1& result,
        AZStd::string_view code)
    {
        for (const auto& issue : result.m_issues)
        {
            if (issue.m_code == code)
            {
                return true;
            }
        }
        return false;
    }

    inline CanonicalInterchangeManifestV1 ParseValidFixture(AZStd::string_view bytes)
    {
        CanonicalInterchangeManifestV1 manifest;
        const auto validation = ParseCanonicalManifestV1(bytes, manifest);
        if (!validation.IsValid())
        {
            return {};
        }
        return manifest;
    }
} // namespace TaintedGrailModdingSDK::Interchange::TestFixtures
