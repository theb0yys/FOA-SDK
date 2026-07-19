namespace TaintedGrailModdingSDK
{
    AdapterPackageAssemblyPreviewRegistry& AdapterPackageAssemblyPreviewRegistry::Get()
    {
        static AdapterPackageAssemblyPreviewRegistry registry;
        return registry;
    }

    bool AdapterPackageAssemblyPreviewRegistry::RegisterRequest(
        const AdapterPackageAssemblyPreviewRequest& request,
        AZStd::string* error)
    {
        if (!IsStableNamespacedId(request.m_review.m_reviewId)
            || !IsStableNamespacedId(request.m_inventory.m_inventoryId))
        {
            if (error)
            {
                *error = "ReviewId and InventoryId must be lowercase namespaced stable IDs.";
            }
            return false;
        }
        if (!IsSha256Fingerprint(request.m_review.m_manifestFingerprint)
            || !IsSha256Fingerprint(request.m_inventory.m_manifestFingerprint))
        {
            if (error)
            {
                *error = "Review and inventory manifest fingerprints must use lowercase SHA-256.";
            }
            return false;
        }
        const AZStd::string derivedFingerprint =
            ComputeCanonicalFingerprint(request.m_manifest.m_canonicalJson);
        if (request.m_manifest.m_canonicalJson.empty()
            || request.m_review.m_manifestFingerprint != derivedFingerprint
            || request.m_inventory.m_manifestFingerprint != derivedFingerprint)
        {
            if (error)
            {
                *error = "Review and staging inventory must bind to SHA-256 of the exact canonical build manifest.";
            }
            return false;
        }
        for (const AdapterPackageAssemblyPreviewRequest& existing : m_requests)
        {
            if (existing.m_inventory.m_inventoryId == request.m_inventory.m_inventoryId)
            {
                if (error)
                {
                    *error = "Staging inventory ID already exists in the transient preview registry.";
                }
                return false;
            }
        }
        m_requests.push_back(request);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterPackageAssemblyPreviewRegistry::Clear()
    {
        m_requests.clear();
    }

    const AZStd::vector<AdapterPackageAssemblyPreviewRequest>&
        AdapterPackageAssemblyPreviewRegistry::GetRequests() const
    {
        return m_requests;
    }

    AdapterPackageAssemblyPreview AdapterPackageAssemblyPreviewService::BuildPreview(
        const AdapterPackageAssemblyPreviewRequest& request) const
    {
        AdapterPackageAssemblyPreview preview;
        const AdapterBuildManifest& manifest = request.m_manifest;
        const AdapterBuildManifestReview& review = request.m_review;
        const AdapterStagingInventory& inventory = request.m_inventory;
        const AZStd::string derivedManifestFingerprint =
            ComputeCanonicalFingerprint(manifest.m_canonicalJson);

        preview.m_manifestId = manifest.m_manifestId;
        preview.m_manifestFingerprint = derivedManifestFingerprint;
        preview.m_packId = manifest.m_packId;
        preview.m_packVersion = manifest.m_packVersion;
        preview.m_adapterId = manifest.m_adapterId;
        preview.m_adapterVersion = manifest.m_adapterVersion;
        preview.m_planId = manifest.m_planId;
        preview.m_inventoryId = inventory.m_inventoryId;
        PackagePathIdentity packageRootIdentity;
        preview.m_packageRoot = TryBuildPackagePathIdentity(
                manifest.m_packageRoot,
                packageRootIdentity)
            ? packageRootIdentity.m_normalizedPath
            : manifest.m_packageRoot;
        preview.m_previewId = AZStd::string("packagepreview:")
            + manifest.m_manifestId + ":" + inventory.m_inventoryId;
        preview.m_assemblyAllowed = false;
        preview.m_archiveAllowed = false;
        preview.m_deploymentAllowed = false;

        bool manifestInvalid = false;
        if (manifest.m_status != AdapterBuildManifestStatus::Ready
            || manifest.m_buildAllowed
            || manifest.m_manifestId.empty()
            || manifest.m_canonicalJson.empty()
            || manifest.m_expectedOutputs.empty()
            || !IsSafePackageRelativePath(manifest.m_packageRoot))
        {
            manifestInvalid = true;
            AddBlocker(
                preview.m_blockers,
                "package_preview.manifest_not_ready",
                {},
                "A reviewed ready build manifest with BuildAllowed=false, canonical JSON, outputs, and an unambiguous package root is required.");
        }

        bool reviewInvalid = false;
        if (review.m_decision != AdapterBuildManifestReviewDecision::Accepted
            || !IsStableNamespacedId(review.m_reviewId)
            || review.m_reviewer.empty()
            || review.m_evidenceIds.empty()
            || review.m_manifestId != manifest.m_manifestId
            || review.m_manifestFingerprint != derivedManifestFingerprint)
        {
            reviewInvalid = true;
            AddBlocker(
                preview.m_blockers,
                "package_preview.manifest_unreviewed",
                {},
                "An accepted evidence-backed review bound to SHA-256 of the exact canonical manifest is required.");
        }

        bool bindingInvalid = false;
        if (inventory.m_formatVersion != 1
            || !IsStableNamespacedId(inventory.m_inventoryId)
            || inventory.m_manifestId != manifest.m_manifestId
            || inventory.m_manifestFingerprint != derivedManifestFingerprint
            || inventory.m_packId != manifest.m_packId
            || !PackagePathsEqual(inventory.m_packageRoot, manifest.m_packageRoot))
        {
            bindingInvalid = true;
            AddBlocker(
                preview.m_blockers,
                "package_preview.inventory_binding_mismatch",
                {},
                "The staging inventory must bind to the exact canonical manifest hash, pack, and Windows-stable package root identity.");
        }

        bool inventoryUntrusted = false;
        bool outputMissing = false;
        bool fingerprintMissing = false;
        bool pathInvalid = false;
        bool collision = false;
        bool redistributionBlocked = false;
        AZStd::vector<AZStd::string> entryIds;
        AZStd::vector<AZStd::string> packagePathIdentities;
        for (const AdapterStagingInventoryEntry& entry : inventory.m_entries)
        {
            PackagePathIdentity stagingIdentity;
            PackagePathIdentity packageIdentity;
            const bool stagingPathValid = TryBuildPackagePathIdentity(
                entry.m_stagingPath,
                stagingIdentity);
            const bool packagePathValid = TryBuildPackagePathIdentity(
                entry.m_packagePath,
                packageIdentity);
            if (entry.m_entryId.empty()
                || entry.m_role.empty()
                || entry.m_mediaType.empty())
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.inventory_entry_incomplete",
                    entry.m_packagePath,
                    "Every staging entry requires an ID, role, and media type.");
            }
            if (ContainsValue(entryIds, entry.m_entryId))
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.inventory_id_duplicate",
                    entry.m_packagePath,
                    "Staging inventory entry IDs must be unique.");
            }
            entryIds.push_back(entry.m_entryId);

            if (entry.m_includeInPackage && packagePathValid)
            {
                if (ContainsValue(
                        packagePathIdentities,
                        packageIdentity.m_windowsIdentity))
                {
                    collision = true;
                    AddBlocker(
                        preview.m_blockers,
                        "package_preview.windows_path_collision",
                        packageIdentity.m_normalizedPath,
                        "Multiple staging entries resolve to the same Windows package-file identity.");
                }
                packagePathIdentities.push_back(packageIdentity.m_windowsIdentity);
            }
            if (entry.m_includeInPackage && !entry.m_projectOwned)
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.project_ownership_required",
                    entry.m_packagePath,
                    "Package candidates must come from project-owned staging entries.");
            }
            if (entry.m_includeInPackage
                && !FindExpectedOutput(manifest, entry.m_packagePath))
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_undeclared",
                    entry.m_packagePath,
                    "Included staging output is not declared by the reviewed build manifest under the same Windows path identity.");
            }
            if (entry.m_includeInPackage
                && !IsSha256Fingerprint(entry.m_outputFingerprint))
            {
                fingerprintMissing = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_digest_missing",
                    entry.m_packagePath,
                    "Every staged output requires a lowercase SHA-256 digest.");
            }
            if (entry.m_includeInPackage
                && (!stagingPathValid
                    || !packagePathValid
                    || !IsPackagePathInsideRoot(
                        manifest.m_packageRoot,
                        entry.m_packagePath)))
            {
                pathInvalid = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.path_invalid",
                    entry.m_packagePath,
                    "Staging paths and package outputs must have unambiguous Windows-stable identities and remain below PackageRoot.");
            }
            if (entry.m_includeInPackage && !entry.m_redistributable)
            {
                redistributionBlocked = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.redistribution_blocked",
                    entry.m_packagePath,
                    "Non-redistributable outputs cannot enter the package layout.");
            }
        }

        for (const AdapterBuildExpectedOutput& output : manifest.m_expectedOutputs)
        {
            AZStd::vector<const AdapterStagingInventoryEntry*> matches =
                FindIncludedEntries(inventory, output.m_relativePath);
            if (matches.empty())
            {
                outputMissing = true;
                AddOmission(
                    preview.m_omissions,
                    output,
                    "Expected manifest output is absent or excluded from the staging inventory.");
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_missing",
                    output.m_relativePath,
                    "Expected package output is missing from staging inventory.");
                continue;
            }
            if (matches.size() > 1)
            {
                collision = true;
                AdapterPackageAssemblyCollision conflict;
                PackagePathIdentity outputIdentity;
                conflict.m_packagePath = TryBuildPackagePathIdentity(
                        output.m_relativePath,
                        outputIdentity)
                    ? outputIdentity.m_normalizedPath
                    : output.m_relativePath;
                for (const AdapterStagingInventoryEntry* entry : matches)
                {
                    conflict.m_inventoryEntryIds.push_back(entry->m_entryId);
                }
                preview.m_collisions.push_back(AZStd::move(conflict));
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_collision",
                    output.m_relativePath,
                    "Multiple staging entries target the same Windows package output identity.");
                continue;
            }

            const AdapterStagingInventoryEntry& entry = *matches.front();
            PackagePathIdentity stagingIdentity;
            PackagePathIdentity packageIdentity;
            const bool stagingPathValid = TryBuildPackagePathIdentity(
                entry.m_stagingPath,
                stagingIdentity);
            const bool packagePathValid = TryBuildPackagePathIdentity(
                entry.m_packagePath,
                packageIdentity);
            AdapterPackageLayoutEntry layout;
            layout.m_inventoryEntryId = entry.m_entryId;
            layout.m_stagingPath = stagingPathValid
                ? stagingIdentity.m_normalizedPath
                : entry.m_stagingPath;
            layout.m_packagePath = packagePathValid
                ? packageIdentity.m_normalizedPath
                : entry.m_packagePath;
            layout.m_role = entry.m_role;
            layout.m_mediaType = entry.m_mediaType;
            layout.m_outputDigest = entry.m_outputFingerprint;
            layout.m_byteSize = entry.m_byteSize;
            layout.m_projectOwned = entry.m_projectOwned;
            layout.m_redistributable = entry.m_redistributable;
            preview.m_layout.push_back(AZStd::move(layout));

            if (entry.m_role != output.m_role
                || entry.m_mediaType != output.m_mediaType)
            {
                inventoryUntrusted = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_contract_mismatch",
                    output.m_relativePath,
                    "Staged output role and media type must match the reviewed manifest.");
            }
            if (!IsSha256Fingerprint(entry.m_outputFingerprint))
            {
                fingerprintMissing = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.output_digest_missing",
                    output.m_relativePath,
                    "Every staged output requires a lowercase SHA-256 digest.");
            }
            if (!stagingPathValid
                || !packagePathValid
                || !IsPackagePathInsideRoot(
                    manifest.m_packageRoot,
                    entry.m_packagePath))
            {
                pathInvalid = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.path_invalid",
                    output.m_relativePath,
                    "Staging paths and package outputs must have unambiguous Windows-stable identities below PackageRoot.");
            }
            if (!entry.m_redistributable || !output.m_redistributable)
            {
                redistributionBlocked = true;
                AddBlocker(
                    preview.m_blockers,
                    "package_preview.redistribution_blocked",
                    output.m_relativePath,
                    "Non-redistributable outputs cannot enter the package layout.");
            }
        }

        if (manifestInvalid)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::ManifestNotReady;
        }
        else if (reviewInvalid)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::ManifestUnreviewed;
        }
        else if (bindingInvalid)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::InventoryBindingMismatch;
        }
        else if (inventoryUntrusted)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::InventoryUntrusted;
        }
        else if (outputMissing)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::OutputMissing;
        }
        else if (fingerprintMissing)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::FingerprintMissing;
        }
        else if (pathInvalid)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::PathInvalid;
        }
        else if (collision)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::Collision;
        }
        else if (redistributionBlocked)
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::RedistributionBlocked;
        }
        else
        {
            preview.m_status = AdapterPackageAssemblyPreviewStatus::Ready;
        }

        SortPreviewCollections(preview);
        preview.m_canonicalJson = SerializeCanonicalPreview(preview);
        return preview;
    }
} // namespace TaintedGrailModdingSDK
