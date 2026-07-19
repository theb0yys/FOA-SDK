/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterBuildManifestService.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    enum class AdapterBuildManifestReviewDecision : AZ::u8
    {
        Unknown,
        Accepted,
        Rejected,
    };

    enum class AdapterPackageAssemblyPreviewStatus : AZ::u8
    {
        Ready,
        ManifestNotReady,
        ManifestUnreviewed,
        InventoryBindingMismatch,
        InventoryUntrusted,
        OutputMissing,
        FingerprintMissing,
        PathInvalid,
        Collision,
        RedistributionBlocked,
    };

    AZStd::string ToString(AdapterBuildManifestReviewDecision decision);
    AZStd::string ToString(AdapterPackageAssemblyPreviewStatus status);
    bool TryParseAdapterBuildManifestReviewDecision(
        const AZStd::string& value,
        AdapterBuildManifestReviewDecision& decision);
    bool TryParseAdapterPackageAssemblyPreviewStatus(
        const AZStd::string& value,
        AdapterPackageAssemblyPreviewStatus& status);

    struct AdapterBuildManifestReview
    {
        AZStd::string m_reviewId;
        AZStd::string m_manifestId;
        AZStd::string m_manifestFingerprint;
        AdapterBuildManifestReviewDecision m_decision =
            AdapterBuildManifestReviewDecision::Unknown;
        AZStd::string m_reviewer;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_notes;
    };

    struct AdapterStagingInventoryEntry
    {
        AZStd::string m_entryId;
        AZStd::string m_stagingPath;
        AZStd::string m_packagePath;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZStd::string m_outputFingerprint;
        AZ::u64 m_byteSize = 0;
        bool m_projectOwned = false;
        bool m_redistributable = false;
        bool m_includeInPackage = true;
    };

    struct AdapterStagingInventory
    {
        AZ::u32 m_formatVersion = 1;
        AZStd::string m_inventoryId;
        AZStd::string m_manifestId;
        AZStd::string m_manifestFingerprint;
        AZStd::string m_packId;
        AZStd::string m_packageRoot;
        AZStd::vector<AdapterStagingInventoryEntry> m_entries;
    };

    struct AdapterPackageLayoutEntry
    {
        AZStd::string m_inventoryEntryId;
        AZStd::string m_stagingPath;
        AZStd::string m_packagePath;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZStd::string m_outputDigest;
        AZ::u64 m_byteSize = 0;
        bool m_projectOwned = false;
        bool m_redistributable = false;
    };

    struct AdapterPackageAssemblyOmission
    {
        AZStd::string m_expectedPath;
        AZStd::string m_role;
        AZStd::string m_reason;
    };

    struct AdapterPackageAssemblyCollision
    {
        AZStd::string m_packagePath;
        AZStd::vector<AZStd::string> m_inventoryEntryIds;
    };

    struct AdapterPackageAssemblyBlocker
    {
        AZStd::string m_code;
        AZStd::string m_packagePath;
        AZStd::string m_reason;
    };

    struct AdapterPackageAssemblyPreviewRequest
    {
        AdapterBuildManifest m_manifest;
        AdapterBuildManifestReview m_review;
        AdapterStagingInventory m_inventory;
    };

    struct AdapterPackageAssemblyPreview
    {
        AZ::u32 m_formatVersion = 1;
        AZStd::string m_previewId;
        AZStd::string m_manifestId;
        AZStd::string m_manifestFingerprint;
        AZStd::string m_packId;
        AZStd::string m_packVersion;
        AZStd::string m_adapterId;
        AZStd::string m_adapterVersion;
        AZStd::string m_planId;
        AZStd::string m_inventoryId;
        AZStd::string m_packageRoot;
        AdapterPackageAssemblyPreviewStatus m_status =
            AdapterPackageAssemblyPreviewStatus::ManifestNotReady;
        AZStd::vector<AdapterPackageLayoutEntry> m_layout;
        AZStd::vector<AdapterPackageAssemblyOmission> m_omissions;
        AZStd::vector<AdapterPackageAssemblyCollision> m_collisions;
        AZStd::vector<AdapterPackageAssemblyBlocker> m_blockers;
        AZStd::string m_canonicalJson;
        bool m_assemblyAllowed = false;
        bool m_archiveAllowed = false;
        bool m_deploymentAllowed = false;
    };

    class AdapterPackageAssemblyPreviewRegistry
    {
    public:
        static AdapterPackageAssemblyPreviewRegistry& Get();

        bool RegisterRequest(
            const AdapterPackageAssemblyPreviewRequest& request,
            AZStd::string* error = nullptr);
        void Clear();
        const AZStd::vector<AdapterPackageAssemblyPreviewRequest>& GetRequests() const;

    private:
        AZStd::vector<AdapterPackageAssemblyPreviewRequest> m_requests;
    };

    class AdapterPackageAssemblyPreviewService
    {
    public:
        AdapterPackageAssemblyPreview BuildPreview(
            const AdapterPackageAssemblyPreviewRequest& request) const;
        AZStd::string SerializeCanonicalPreview(
            const AdapterPackageAssemblyPreview& preview) const;
    };
} // namespace TaintedGrailModdingSDK
