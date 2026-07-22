/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "CanonicalInterchangeTypes.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::Interchange
{
    // Pinned AzCore supplies equality but not relational operators for vector.
    // Validation needs an explicit lexicographic order for the final related-ID
    // tie-breaker; keep that compatibility rule local to the interchange domain.
    inline bool operator<(
        const AZStd::vector<AZStd::string>& left,
        const AZStd::vector<AZStd::string>& right)
    {
        const size_t sharedSize = left.size() < right.size() ? left.size() : right.size();
        for (size_t index = 0; index < sharedSize; ++index)
        {
            if (left[index] != right[index])
            {
                return left[index] < right[index];
            }
        }
        return left.size() < right.size();
    }

    // Validation compares referenced provenance records by canonical identity.
    // Mutable evidence bodies remain outside this Core value contract.
    inline bool operator==(const ProvenanceRecordV1& left, const ProvenanceRecordV1& right)
    {
        return left.m_provenanceId == right.m_provenanceId;
    }

    namespace IssueCodeV1
    {
        inline constexpr AZStd::string_view UnsupportedVersion = "interchange.schema.unsupported-version";
        inline constexpr AZStd::string_view UnsupportedProfile = "interchange.schema.unsupported-profile";
        inline constexpr AZStd::string_view UnknownField = "interchange.schema.unknown-field";
        inline constexpr AZStd::string_view ForbiddenField = "interchange.schema.forbidden-field";
        inline constexpr AZStd::string_view BomForbidden = "interchange.canonical.bom-forbidden";
        inline constexpr AZStd::string_view DuplicateKey = "interchange.canonical.duplicate-key";
        inline constexpr AZStd::string_view InvalidJson = "interchange.canonical.invalid-json";
        inline constexpr AZStd::string_view InvalidString = "interchange.canonical.invalid-string";
        inline constexpr AZStd::string_view SizeExceeded = "interchange.canonical.size-exceeded";
        inline constexpr AZStd::string_view DepthExceeded = "interchange.canonical.depth-exceeded";
        inline constexpr AZStd::string_view NonFiniteNumber = "interchange.canonical.non-finite-number";
        inline constexpr AZStd::string_view NegativeZero = "interchange.canonical.negative-zero";
        inline constexpr AZStd::string_view OrderInvalid = "interchange.canonical.order-invalid";
        inline constexpr AZStd::string_view IdentityInvalid = "interchange.identity.invalid";
        inline constexpr AZStd::string_view IdentityDuplicate = "interchange.identity.duplicate";
        inline constexpr AZStd::string_view RevisionMismatch = "interchange.fingerprint.revision-mismatch";
        inline constexpr AZStd::string_view PathAbsolute = "interchange.path.absolute";
        inline constexpr AZStd::string_view PathTraversal = "interchange.path.traversal";
        inline constexpr AZStd::string_view PathCaseCollision = "interchange.path.case-collision";
        inline constexpr AZStd::string_view PathWindowsAlias = "interchange.path.windows-alias";
        inline constexpr AZStd::string_view ReferenceMissing = "interchange.reference.missing";
        inline constexpr AZStd::string_view DependencyMissing = "interchange.dependency.missing";
        inline constexpr AZStd::string_view DependencyCycle = "interchange.dependency.cycle";
        inline constexpr AZStd::string_view PayloadLimitExceeded = "interchange.payload.limit-exceeded";
        inline constexpr AZStd::string_view MappingInvalid = "interchange.identity.mapping-invalid";
        inline constexpr AZStd::string_view BindingInvalid = "interchange.binding.invalid";
        inline constexpr AZStd::string_view BindingUnresolvedRequired = "interchange.binding.unresolved-required";
        inline constexpr AZStd::string_view BindingConflict = "interchange.binding.conflict";
        inline constexpr AZStd::string_view BindingSupersessionInvalid = "interchange.binding.supersession-invalid";
        inline constexpr AZStd::string_view TransformSequenceInvalid = "interchange.transform.sequence-invalid";
        inline constexpr AZStd::string_view TransformMatrixInvalid = "interchange.transform.matrix-invalid";
        inline constexpr AZStd::string_view TransformParameterInvalid = "interchange.transform.parameter-invalid";
        inline constexpr AZStd::string_view TransformToleranceInvalid = "interchange.transform.tolerance-invalid";
        inline constexpr AZStd::string_view LossIncomplete = "interchange.loss.incomplete";
        inline constexpr AZStd::string_view LossBlocker = "interchange.loss.blocker";
        inline constexpr AZStd::string_view ProvenanceIncomplete = "interchange.provenance.incomplete";
        inline constexpr AZStd::string_view LicensingInvalid = "interchange.licensing.invalid";
        inline constexpr AZStd::string_view LicensingNoAssertionBlocked = "interchange.licensing.noassertion-blocked";
        inline constexpr AZStd::string_view ContentProhibited = "interchange.content.prohibited";
        inline constexpr AZStd::string_view LockIncomplete = "interchange.lock.incomplete";
        inline constexpr AZStd::string_view ExtensionUnapproved = "interchange.extension.unapproved";
        inline constexpr AZStd::string_view ExtensionDigestMismatch = "interchange.extension.digest-mismatch";
        inline constexpr AZStd::string_view PackageDeclarationMismatch = "interchange.fingerprint.package-declaration-mismatch";
        inline constexpr AZStd::string_view MigrationSourceInvalid = "interchange.migration.source-invalid";
        inline constexpr AZStd::string_view MigrationUnavailable = "interchange.migration.unavailable";
        inline constexpr AZStd::string_view MigrationSemanticDrift = "interchange.migration.semantic-drift";
    }

    struct CanonicalInterchangeIssueV1
    {
        IssueSeverityV1 m_severity = IssueSeverityV1::Error;
        AZStd::string m_code;
        AZStd::string m_subjectId;
        AZStd::string m_propertyPath;
        AZStd::vector<AZStd::string> m_relatedIds;

        bool operator==(const CanonicalInterchangeIssueV1&) const = default;
    };

    struct CanonicalInterchangeValidationResultV1
    {
        AZStd::vector<CanonicalInterchangeIssueV1> m_issues;

        bool IsValid() const;
        bool IsBlocked() const;
    };

    CanonicalInterchangeValidationResultV1 ValidateCanonicalManifestV1(
        const CanonicalInterchangeManifestV1& manifest);

    void SortCanonicalInterchangeIssuesV1(
        CanonicalInterchangeValidationResultV1& result);
} // namespace TaintedGrailModdingSDK::Interchange
