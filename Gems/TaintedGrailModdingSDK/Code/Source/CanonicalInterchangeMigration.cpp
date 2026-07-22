/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeMigration.h"

#include "CanonicalInterchangeCanonical.h"

#include <AzCore/JSON/document.h>

namespace TaintedGrailModdingSDK::Interchange
{
    namespace
    {
        void AddBlocker(
            CanonicalInterchangeValidationResultV1& validation,
            AZStd::string_view code,
            AZStd::string_view propertyPath)
        {
            validation.m_issues.push_back({
                IssueSeverityV1::Blocker,
                AZStd::string(code),
                {},
                AZStd::string(propertyPath),
                {} });
        }

        void MergeValidation(
            CanonicalInterchangeValidationResultV1& destination,
            const CanonicalInterchangeValidationResultV1& source)
        {
            destination.m_issues.insert(
                destination.m_issues.end(),
                source.m_issues.begin(),
                source.m_issues.end());
        }

        bool HasUtf8Bom(AZStd::string_view bytes)
        {
            return bytes.size() >= 3 &&
                static_cast<unsigned char>(bytes[0]) == 0xef &&
                static_cast<unsigned char>(bytes[1]) == 0xbb &&
                static_cast<unsigned char>(bytes[2]) == 0xbf;
        }

        bool InspectSourceVersion(
            AZStd::string_view bytes,
            AZ::u32& sourceVersion)
        {
            if (bytes.empty() || bytes.size() > MaxCanonicalManifestBytesV1 ||
                HasUtf8Bom(bytes) || !IsValidUtf8V1(bytes))
            {
                return false;
            }

            rapidjson::Document document;
            document.Parse<
                rapidjson::kParseValidateEncodingFlag |
                rapidjson::kParseFullPrecisionFlag>(bytes.data(), bytes.size());
            if (document.HasParseError() || !document.IsObject())
            {
                return false;
            }

            size_t versionCount = 0;
            for (auto member = document.MemberBegin();
                 member != document.MemberEnd(); ++member)
            {
                const AZStd::string_view name(
                    member->name.GetString(),
                    member->name.GetStringLength());
                if (name == "schema_version")
                {
                    ++versionCount;
                    if (!member->value.IsUint())
                    {
                        return false;
                    }
                    sourceVersion = member->value.GetUint();
                }
            }
            return versionCount == 1;
        }

        CanonicalInterchangeValidationResultV1 ParseSourceForMigration(
            AZStd::string_view sourceCanonicalJson,
            CanonicalInterchangeManifestV1& sourceManifest)
        {
            auto validation =
                ParseCanonicalManifestV1(sourceCanonicalJson, sourceManifest);
            if (!validation.IsValid())
            {
                AddBlocker(
                    validation,
                    IssueCodeV1::MigrationSourceInvalid,
                    "source");
                SortCanonicalInterchangeIssuesV1(validation);
            }
            return validation;
        }

        MigrationResultV1 MakeBaseResult(
            AZStd::string_view sourceCanonicalJson,
            AZ::u32 targetSchemaVersion)
        {
            MigrationResultV1 result;
            result.m_targetVersion = targetSchemaVersion;
            result.m_sourceDigest =
                CalculateCanonicalBytesDigestV1(sourceCanonicalJson);
            return result;
        }
    } // namespace

    bool MigrationResultV1::HasCandidate() const
    {
        return !m_candidateCanonicalJson.empty() &&
            IsSha256HexV1(m_candidateDigest.m_hex) &&
            CanonicalBytesDigestMatchesV1(
                m_candidateCanonicalJson,
                m_candidateDigest);
    }

    CanonicalInterchangeValidationResultV1 ValidateMigrationCandidateV1(
        AZStd::string_view sourceCanonicalJson,
        AZStd::string_view candidateCanonicalJson,
        const MigrationStepV1& step)
    {
        CanonicalInterchangeValidationResultV1 result;
        CanonicalInterchangeManifestV1 sourceManifest;
        CanonicalInterchangeManifestV1 candidateManifest;

        const auto sourceValidation =
            ParseCanonicalManifestV1(sourceCanonicalJson, sourceManifest);
        const auto candidateValidation =
            ParseCanonicalManifestV1(candidateCanonicalJson, candidateManifest);
        MergeValidation(result, sourceValidation);
        MergeValidation(result, candidateValidation);

        if (!sourceValidation.IsValid())
        {
            AddBlocker(
                result,
                IssueCodeV1::MigrationSourceInvalid,
                "source");
        }

        if (step.m_sourceVersion != SchemaVersionV1 ||
            step.m_targetVersion != SchemaVersionV1)
        {
            AddBlocker(
                result,
                IssueCodeV1::MigrationUnavailable,
                "step");
        }

        const Sha256DigestV1 expectedSourceDigest =
            CalculateCanonicalBytesDigestV1(sourceCanonicalJson);
        const Sha256DigestV1 expectedCandidateDigest =
            CalculateCanonicalBytesDigestV1(candidateCanonicalJson);

        const bool digestBindingValid =
            step.m_sourceDigest == expectedSourceDigest &&
            step.m_candidateDigest == expectedCandidateDigest;
        const bool exactIdentityValid =
            sourceCanonicalJson == candidateCanonicalJson;
        const bool evidenceIsEmpty =
            step.m_mappingIds.empty() &&
            step.m_transformationIds.empty() &&
            step.m_lossIds.empty();

        if (!candidateValidation.IsValid() ||
            !digestBindingValid ||
            !exactIdentityValid ||
            !evidenceIsEmpty)
        {
            AddBlocker(
                result,
                IssueCodeV1::MigrationSemanticDrift,
                "candidate");
        }

        SortCanonicalInterchangeIssuesV1(result);
        return result;
    }

    MigrationResultV1 MigrateCanonicalInterchange(
        AZStd::string_view sourceCanonicalJson,
        AZ::u32 targetSchemaVersion)
    {
        MigrationResultV1 result =
            MakeBaseResult(sourceCanonicalJson, targetSchemaVersion);

        AZ::u32 sourceVersion = 0;
        if (!InspectSourceVersion(sourceCanonicalJson, sourceVersion))
        {
            CanonicalInterchangeManifestV1 ignored;
            result.m_validation =
                ParseSourceForMigration(sourceCanonicalJson, ignored);
            if (result.m_validation.IsValid())
            {
                AddBlocker(
                    result.m_validation,
                    IssueCodeV1::MigrationSourceInvalid,
                    "source.schema_version");
                SortCanonicalInterchangeIssuesV1(result.m_validation);
            }
            result.m_status = MigrationStatusV1::SourceInvalid;
            return result;
        }

        result.m_sourceVersion = sourceVersion;
        if (sourceVersion != SchemaVersionV1)
        {
            result.m_status =
                MigrationStatusV1::UnsupportedSourceVersion;
            AddBlocker(
                result.m_validation,
                IssueCodeV1::UnsupportedVersion,
                "source.schema_version");
            SortCanonicalInterchangeIssuesV1(result.m_validation);
            return result;
        }

        if (targetSchemaVersion < sourceVersion)
        {
            result.m_status = MigrationStatusV1::DowngradeForbidden;
            return result;
        }

        if (targetSchemaVersion == sourceVersion)
        {
            CanonicalInterchangeManifestV1 sourceManifest;
            result.m_validation =
                ParseSourceForMigration(
                    sourceCanonicalJson,
                    sourceManifest);
            if (!result.m_validation.IsValid())
            {
                result.m_status = MigrationStatusV1::SourceInvalid;
                return result;
            }

            result.m_candidateCanonicalJson =
                AZStd::string(sourceCanonicalJson);
            result.m_candidateDigest = result.m_sourceDigest;

            MigrationStepV1 identityStep;
            identityStep.m_sourceVersion = sourceVersion;
            identityStep.m_targetVersion = targetSchemaVersion;
            identityStep.m_sourceDigest = result.m_sourceDigest;
            identityStep.m_candidateDigest = result.m_candidateDigest;
            result.m_validation = ValidateMigrationCandidateV1(
                sourceCanonicalJson,
                result.m_candidateCanonicalJson,
                identityStep);
            if (!result.m_validation.IsValid())
            {
                result.m_status = MigrationStatusV1::SemanticDrift;
                result.m_candidateCanonicalJson.clear();
                result.m_candidateDigest = {};
                return result;
            }

            result.m_status = MigrationStatusV1::AlreadyCurrent;
            result.m_migrationPerformed = false;
            return result;
        }

        if (targetSchemaVersion == SchemaVersionV1 + 1)
        {
            result.m_status = MigrationStatusV1::MigratorUnavailable;
            AddBlocker(
                result.m_validation,
                IssueCodeV1::MigrationUnavailable,
                "target_schema_version");
            SortCanonicalInterchangeIssuesV1(result.m_validation);
            return result;
        }

        result.m_status =
            MigrationStatusV1::UnsupportedTargetVersion;
        return result;
    }
} // namespace TaintedGrailModdingSDK::Interchange
