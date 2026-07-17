/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    class CatalogDatabase
    {
    public:
        bool InsertNew(const CatalogRecord& record, AZStd::string* error = nullptr);
        bool Upsert(const CatalogRecord& record, AZStd::string* error = nullptr);
        bool UpsertRelationship(const CatalogRelationship& relationship, AZStd::string* error = nullptr);
        bool AddValidationEvent(const CatalogValidationEvent& validation, AZStd::string* error = nullptr);
        bool AddGovernanceEvent(const CatalogGovernanceEvent& event, AZStd::string* error = nullptr);
        bool ReplaceFromDocument(const CatalogDocument& document, AZStd::string* error = nullptr);
        void Clear();

        CatalogRecord* FindMutableRecordById(const AZStd::string& recordId);
        CatalogRelationship* FindMutableRelationshipById(const AZStd::string& relationshipId);
        const CatalogRecord* FindByRecordId(const AZStd::string& recordId) const;
        const CatalogRecord* FindByExactNativeRef(const AZStd::string& nativeRefExact) const;
        const CatalogRelationship* FindRelationshipById(const AZStd::string& relationshipId) const;
        const CatalogValidationEvent* FindValidationById(const AZStd::string& validationId) const;

        AZStd::vector<CatalogRecord> Query(const CatalogQuery& query) const;
        AZStd::vector<CatalogRelationship> FindRelationshipsForRecord(const AZStd::string& recordId) const;
        AZStd::vector<CatalogValidationEvent> FindValidationForRecord(const AZStd::string& recordId) const;
        AZStd::vector<CatalogValidationEvent> FindValidationForSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId) const;
        AZStd::vector<CatalogGovernanceEvent> FindGovernanceForSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId) const;
        AZStd::vector<DomainCoverage> BuildCoverage() const;

        CatalogDocument BuildDocument(
            const WorkspaceModel& workspace,
            const GameProfile& profile) const;

        const AZStd::vector<CatalogRecord>& GetRecords() const;
        const AZStd::vector<CatalogRelationship>& GetRelationships() const;
        const AZStd::vector<CatalogValidationEvent>& GetValidationHistory() const;
        const AZStd::vector<CatalogGovernanceEvent>& GetGovernanceHistory() const;

    private:
        bool ValidateRecord(const CatalogRecord& record, const CatalogRecord* replacing, AZStd::string* error) const;
        bool ValidateRelationship(const CatalogRelationship& relationship, AZStd::string* error) const;
        bool ValidateGovernanceEvent(const CatalogGovernanceEvent& event, AZStd::string* error) const;

        AZStd::vector<CatalogRecord> m_records;
        AZStd::vector<CatalogRelationship> m_relationships;
        AZStd::vector<CatalogValidationEvent> m_validationHistory;
        AZStd::vector<CatalogGovernanceEvent> m_governanceHistory;
    };
} // namespace TaintedGrailModdingSDK
