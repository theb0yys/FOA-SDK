/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterContractRegistry.h"
#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterCapabilityMatrixRow
    {
        AZStd::string m_packId;
        AZStd::string m_requiredAdapterVersion;
        AZStd::string m_adapterId;
        AZStd::string m_adapterVersion;
        AZStd::string m_runtimeTarget;
        AZStd::string m_capability;
        AZStd::string m_status;
        AZStd::vector<AZStd::string> m_subjectIds;
        AZStd::vector<AZStd::string> m_declarationEvidenceIds;
        AZStd::vector<AZStd::string> m_permissionEvidenceIds;
        AZStd::vector<AZStd::string> m_validationProofIds;
        AZStd::vector<AZStd::string> m_reasons;
    };

    struct AdapterCapabilityMatrix
    {
        AZ::u64 m_rowCount = 0;
        AZ::u64 m_supportedCount = 0;
        AZ::u64 m_unsupportedCount = 0;
        AZ::u64 m_versionMismatchCount = 0;
        AZ::u64 m_permissionMissingCount = 0;
        AZ::u64 m_proofMissingCount = 0;
        AZStd::vector<AdapterCapabilityMatrixRow> m_rows;
    };

    class AdapterCompatibilityService
    {
    public:
        AdapterCapabilityMatrix BuildCapabilityMatrix(
            const WorkspaceModel& workspace,
            const AZStd::vector<PackManifest>& packs,
            const AdapterContractRegistry& adapterRegistry,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers) const;
    };
} // namespace TaintedGrailModdingSDK
