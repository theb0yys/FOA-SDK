/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPersistenceService.h"

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QDir>
#include <QFileInfo>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void AddUnique(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        void RemoveValue(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            values.erase(AZStd::remove(values.begin(), values.end(), value), values.end());
        }

        const CatalogRecord* FindRecord(const CatalogDocument& document, const AZStd::string& recordId)
        {
            for (const CatalogRecord& record : document.m_records)
            {
                if (record.m_recordId == recordId)
                {
                    return &record;
                }
            }
            return nullptr;
        }

        const CatalogRelationship* FindRelationship(
            const CatalogDocument& document,
            const AZStd::string& relationshipId)
        {
            for (const CatalogRelationship& relationship : document.m_relationships)
            {
                if (relationship.m_relationshipId == relationshipId)
                {
                    return &relationship;
                }
            }
            return nullptr;
        }

        const CatalogValidationEvent* FindValidation(
            const CatalogDocument& document,
            const AZStd::string& validationId)
        {
            for (const CatalogValidationEvent& validation : document.m_validationHistory)
            {
                if (validation.m_validationId == validationId)
                {
                    return &validation;
                }
            }
            return nullptr;
        }

        const CatalogGovernanceEvent* FindLatestPermissionEvent(
            const CatalogDocument& document,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& usage)
        {
            const CatalogGovernanceEvent* latest = nullptr;
            for (const CatalogGovernanceEvent& event : document.m_governanceHistory)
            {
                if (event.m_subjectKind == subjectKind
                    && event.m_subjectId == subjectId
                    && event.m_axis == "permission"
                    && event.m_usage == usage)
                {
                    latest = &event;
                }
            }
            return latest;
        }

        bool HasValidatedPermissionBasis(
            const CatalogDocument& document,
            const CatalogGovernanceEvent& event)
        {
            if (event.m_reviewer.empty() || event.m_evidenceIds.empty() || event.m_validationIds.empty())
            {
                return false;
            }

            bool hasValidatedBasis = false;
            for (const AZStd::string& validationId : event.m_validationIds)
            {
                const CatalogValidationEvent* validation = FindValidation(document, validationId);
                if (!validation
                    || validation->GetSubjectKind() != event.m_subjectKind
                    || validation->GetSubjectId() != event.m_subjectId)
                {
                    return false;
                }
                if (validation->m_state == "validated")
                {
                    hasValidatedBasis = true;
                }
            }
            return hasValidatedBasis;
        }

        bool IsReadyForPermission(
            const AZStd::string& validationState,
            const AZStd::string& stalenessState,
            const AZStd::vector<AZStd::string>& missingRefs,
            const AZStd::vector<AZStd::string>& conflictRefs,
            const AZStd::string& supersededById)
        {
            return validationState == "validated"
                && stalenessState == "current"
                && missingRefs.empty()
                && conflictRefs.empty()
                && supersededById.empty();
        }

        bool HasProofBackedAllowance(
            const CatalogDocument& document,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& usage,
            const AZStd::string& validationState,
            const AZStd::string& stalenessState,
            const AZStd::vector<AZStd::string>& missingRefs,
            const AZStd::vector<AZStd::string>& conflictRefs,
            const AZStd::string& supersededById)
        {
            if (!IsReadyForPermission(
                    validationState,
                    stalenessState,
                    missingRefs,
                    conflictRefs,
                    supersededById))
            {
                return false;
            }

            const CatalogGovernanceEvent* event = FindLatestPermissionEvent(
                document,
                subjectKind,
                subjectId,
                usage);
            return event
                && event->m_newValue == "allow"
                && HasValidatedPermissionBasis(document, *event);
        }

        AZStd::string CompatibilityTime(
            const AZStd::string& updatedAt,
            const AZStd::string& createdAt)
        {
            if (!updatedAt.empty())
            {
                return updatedAt;
            }
            if (!createdAt.empty())
            {
                return createdAt;
            }
            return "legacy-schema-1";
        }

        void AddLegacyPermissionClearEvent(
            CatalogDocument& document,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& usage,
            const AZStd::string& decidedAt,
            size_t sequence)
        {
            CatalogGovernanceEvent event;
            event.m_eventId = "governance.compatibility.";
            event.m_eventId += subjectKind;
            event.m_eventId += ".";
            event.m_eventId += subjectId;
            event.m_eventId += ".";
            event.m_eventId += AZStd::to_string(sequence);
            event.m_subjectKind = subjectKind;
            event.m_subjectId = subjectId;
            event.m_axis = "permission";
            event.m_previousValue = "allow";
            event.m_newValue = "clear";
            event.m_usage = usage;
            event.m_reviewer = "compatibility-normalizer";
            event.m_decidedAt = decidedAt;
            event.m_notes =
                "Legacy schema-1 allowance was cleared because it had no current, validated, reviewed proof history.";
            document.m_governanceHistory.push_back(AZStd::move(event));
        }

        void NormalizeAllowedUsages(
            CatalogDocument& document,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            AZStd::vector<AZStd::string>& allowedUsages,
            AZStd::vector<AZStd::string>& forbiddenUsages,
            const AZStd::string& validationState,
            const AZStd::string& stalenessState,
            const AZStd::vector<AZStd::string>& missingRefs,
            const AZStd::vector<AZStd::string>& conflictRefs,
            const AZStd::string& supersededById,
            const AZStd::string& decidedAt,
            size_t& sequence)
        {
            const AZStd::vector<AZStd::string> persistedAllowed = allowedUsages;
            bool clearedAllowance = false;
            for (const AZStd::string& usage : persistedAllowed)
            {
                if (HasProofBackedAllowance(
                        document,
                        subjectKind,
                        subjectId,
                        usage,
                        validationState,
                        stalenessState,
                        missingRefs,
                        conflictRefs,
                        supersededById))
                {
                    continue;
                }

                RemoveValue(allowedUsages, usage);
                AddLegacyPermissionClearEvent(
                    document,
                    subjectKind,
                    subjectId,
                    usage,
                    decidedAt,
                    sequence++);
                clearedAllowance = true;
            }
            if (clearedAllowance)
            {
                AddUnique(forbiddenUsages, "legacy_permission_review_required");
            }
        }

        void NormalizeLegacyGovernanceState(CatalogDocument& document)
        {
            size_t sequence = document.m_governanceHistory.size() + 1;
            for (CatalogRecord& record : document.m_records)
            {
                if (record.m_stalenessState.empty())
                {
                    record.m_stalenessState = "unknown";
                }
                NormalizeAllowedUsages(
                    document,
                    "record",
                    record.m_recordId,
                    record.m_allowedUsages,
                    record.m_forbiddenUsages,
                    record.m_validationState,
                    record.m_stalenessState,
                    record.m_missingRefs,
                    record.m_conflictRefs,
                    record.m_supersededByRecordId,
                    CompatibilityTime(record.m_updatedAt, record.m_createdAt),
                    sequence);
            }

            for (CatalogRelationship& relationship : document.m_relationships)
            {
                if (relationship.m_stalenessState.empty())
                {
                    relationship.m_stalenessState = "unknown";
                }
                NormalizeAllowedUsages(
                    document,
                    "relationship",
                    relationship.m_relationshipId,
                    relationship.m_allowedUsages,
                    relationship.m_forbiddenUsages,
                    relationship.m_validationState,
                    relationship.m_stalenessState,
                    relationship.m_missingRefs,
                    relationship.m_conflictRefs,
                    relationship.m_supersededByRelationshipId,
                    CompatibilityTime(relationship.m_updatedAt, relationship.m_createdAt),
                    sequence);
            }
        }

        void NormalizeLegacyValidationHistory(CatalogDocument& document)
        {
            for (CatalogValidationEvent& validation : document.m_validationHistory)
            {
                const bool missingValidator = validation.m_validator.empty();
                const bool missingEvidence = validation.m_evidenceIds.empty();

                if (validation.m_subjectKind.empty())
                {
                    validation.m_subjectKind = "record";
                }
                if (validation.m_subjectId.empty() && !validation.m_recordId.empty())
                {
                    validation.m_subjectId = validation.m_recordId;
                }
                if (validation.m_subjectKind == "record" && validation.m_recordId.empty())
                {
                    validation.m_recordId = validation.m_subjectId;
                }
                if (missingValidator)
                {
                    validation.m_validator = "legacy-unattributed";
                }
                if (missingEvidence)
                {
                    if (validation.m_subjectKind == "record")
                    {
                        if (const CatalogRecord* record = FindRecord(document, validation.m_subjectId))
                        {
                            validation.m_evidenceIds = record->m_evidenceIds;
                        }
                    }
                    else if (validation.m_subjectKind == "relationship")
                    {
                        if (const CatalogRelationship* relationship = FindRelationship(document, validation.m_subjectId))
                        {
                            validation.m_evidenceIds = relationship->m_evidenceIds;
                        }
                    }
                }
                if (missingValidator || missingEvidence)
                {
                    validation.m_state = "blocked";
                    if (!validation.m_notes.empty())
                    {
                        validation.m_notes += " ";
                    }
                    validation.m_notes +=
                        "Compatibility normalization: legacy validation proof was incomplete; fresh validation is required.";
                }
            }
        }

        AZ::Outcome<void, AZStd::string> ValidatePersistedIdentity(const CatalogDocument& document)
        {
            for (const CatalogRecord& record : document.m_records)
            {
                const bool knownKind = record.m_identityKind == "native"
                    || record.m_identityKind == "synthetic"
                    || record.m_identityKind == "composite"
                    || record.m_identityKind == "source_scoped";
                if (!knownKind)
                {
                    return AZ::Failure(AZStd::string(
                        "Canonical catalog contains an unsupported identity kind."));
                }
                if (record.m_identityKind == "native" && !record.m_ownerPackId.empty())
                {
                    return AZ::Failure(AZStd::string(
                        "Native catalog records must not claim custom pack ownership."));
                }
                if (record.m_identityKind == "synthetic" && !record.m_nativeRefExact.empty())
                {
                    return AZ::Failure(AZStd::string(
                        "Synthetic catalog records must not borrow an exact native reference."));
                }
            }
            return AZ::Success();
        }
    } // namespace

    AZStd::string CatalogPersistenceService::GetCatalogPath(const AZStd::string& workspaceRoot) const
    {
        if (workspaceRoot.empty())
        {
            return {};
        }
        return ToAzString(QDir(ToQString(workspaceRoot)).filePath(QStringLiteral("Catalog/catalog.tgcatalog.json")));
    }

    bool CatalogPersistenceService::Exists(const AZStd::string& workspaceRoot) const
    {
        const AZStd::string path = GetCatalogPath(workspaceRoot);
        return !path.empty() && QFileInfo::exists(ToQString(path));
    }

    AZ::Outcome<AZStd::string, AZStd::string> CatalogPersistenceService::Save(
        const CatalogDocument& document,
        const AZStd::string& workspaceRoot) const
    {
        if (workspaceRoot.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before the canonical catalog can be saved."));
        }
        if (!document.UsesSupportedSchema())
        {
            return AZ::Failure(AZStd::string("Unsupported canonical catalog schema version."));
        }
        if (document.m_workspaceId.empty() || document.m_profileId.empty()
            || document.m_gameVersion.empty() || document.m_branch.empty())
        {
            return AZ::Failure(AZStd::string(
                "Canonical catalog documents require workspace, profile, game version, and branch binding."));
        }
        const AZ::Outcome<void, AZStd::string> identityResult = ValidatePersistedIdentity(document);
        if (!identityResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(identityResult.GetError()));
        }

        const AZStd::string path = GetCatalogPath(workspaceRoot);
        const QString directory = QFileInfo(ToQString(path)).absolutePath();
        if (!QDir().mkpath(directory))
        {
            return AZ::Failure(AZStd::string("Unable to create the catalog directory inside the workspace."));
        }

        const AZ::Outcome<void, AZStd::string> saveResult = AZ::JsonSerializationUtils::SaveObjectToFile(&document, path);
        if (!saveResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(saveResult.GetError()));
        }
        return AZ::Success(path);
    }

    AZ::Outcome<CatalogDocument, AZStd::string> CatalogPersistenceService::Load(
        const AZStd::string& workspaceRoot) const
    {
        const AZStd::string path = GetCatalogPath(workspaceRoot);
        if (path.empty())
        {
            return AZ::Failure(AZStd::string("A workspace root is required before the canonical catalog can be loaded."));
        }
        if (!QFileInfo::exists(ToQString(path)))
        {
            return AZ::Failure(AZStd::string("The workspace does not contain a canonical catalog document."));
        }

        CatalogDocument document;
        const AZ::Outcome<void, AZStd::string> loadResult = AZ::JsonSerializationUtils::LoadObjectFromFile(document, path);
        if (!loadResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(loadResult.GetError()));
        }
        if (!document.UsesSupportedSchema())
        {
            return AZ::Failure(AZStd::string("Unsupported canonical catalog schema version."));
        }
        const AZ::Outcome<void, AZStd::string> identityResult = ValidatePersistedIdentity(document);
        if (!identityResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(identityResult.GetError()));
        }

        NormalizeLegacyValidationHistory(document);
        NormalizeLegacyGovernanceState(document);
        return AZ::Success(AZStd::move(document));
    }
} // namespace TaintedGrailModdingSDK
