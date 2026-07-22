/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "CanonicalInterchangeValidation.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::Interchange
{
    struct MigrationStepV1
    {
        AZ::u32 m_sourceVersion = 0;
        AZ::u32 m_targetVersion = 0;
        Sha256DigestV1 m_sourceDigest;
        Sha256DigestV1 m_candidateDigest;
        AZStd::vector<MappingIdV1> m_mappingIds;
        AZStd::vector<TransformationIdV1> m_transformationIds;
        AZStd::vector<LossIdV1> m_lossIds;
    };

    struct MigrationResultV1
    {
        MigrationStatusV1 m_status = MigrationStatusV1::SourceInvalid;
        AZ::u32 m_sourceVersion = 0;
        AZ::u32 m_targetVersion = 0;
        Sha256DigestV1 m_sourceDigest;
        AZStd::string m_candidateCanonicalJson;
        Sha256DigestV1 m_candidateDigest;
        AZStd::vector<MigrationStepV1> m_steps;
        CanonicalInterchangeValidationResultV1 m_validation;
        bool m_migrationPerformed = false;

        bool HasCandidate() const;
    };

    // Pure candidate verification for compiled adjacent migrators. Schema 1
    // admits only exact 1 -> 1 identity: changed candidate bytes, changed
    // digests, or invented migration evidence fail with semantic drift.
    CanonicalInterchangeValidationResultV1 ValidateMigrationCandidateV1(
        AZStd::string_view sourceCanonicalJson,
        AZStd::string_view candidateCanonicalJson,
        const MigrationStepV1& step);

    // Pure byte-to-byte dispatch. The source bytes are never mutated or
    // overwritten. Candidate bytes are returned by value only; this function
    // has no filesystem, registry, publication, provider, host, runtime,
    // deployment, save, evidence-promotion, signing, or execution authority.
    MigrationResultV1 MigrateCanonicalInterchange(
        AZStd::string_view sourceCanonicalJson,
        AZ::u32 targetSchemaVersion);
} // namespace TaintedGrailModdingSDK::Interchange
