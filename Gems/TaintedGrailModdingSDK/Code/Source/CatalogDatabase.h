/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    class CatalogGovernanceService;

    class CatalogDatabase
    {
    public:
        bool InsertNew(const CatalogRecord& record, AZStd::string* error = nullptr);
        bool Upsert(const CatalogRecord& record, AZStd::string* error = nullptr);
        bool UpsertRelationship(
            const CatalogRelationship& relationship,
            AZStd::string* error = nullptr);
        bool AddValidationEventBound(
            const CatalogValidationEvent& validation,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error = nullptr);
        bool AddGovernanceEventBound(
            const CatalogGovernanceEvent& event,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error = nullptr);
        bool UpsertEconomyItem(
            const EconomyItemProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertEconomyRecipe(
            const EconomyRecipeProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertRecipeIngredient(
            const EconomyRecipeIngredient& ingredient,
            AZStd::string* error = nullptr);
        bool UpsertRecipeOutput(
            const EconomyRecipeOutput& output,
            AZStd::string* error = nullptr);
        bool UpsertPopulationActorProfile(
            const PopulationActorProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertPopulationTroopProfile(
            const PopulationTroopProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertPopulationTroopMember(
            const PopulationTroopMember& member,
            AZStd::string* error = nullptr);
        bool ReplaceFromBoundDocument(
            const CatalogDocument& document,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error = nullptr);
        bool ValidateIntegrity(
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error = nullptr) const;
        void Clear();

        const CatalogRecord* FindByRecordId(const AZStd::string& recordId) const;
        const CatalogRecord* FindByExactNativeRef(
            const AZStd::string& nativeRefExact) const;
        const CatalogRelationship* FindRelationshipById(
            const AZStd::string& relationshipId) const;
        const CatalogValidationEvent* FindValidationById(
            const AZStd::string& validationId) const;
        const CatalogValidationEvent* FindLatestValidationForSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId) const;
        const CatalogGovernanceEvent* FindEffectiveGovernanceEvent(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& axis,
            const AZStd::string& usage = {}) const;
        const EconomyItemProfile* FindEconomyItem(
            const AZStd::string& recordId) const;
        const EconomyRecipeProfile* FindEconomyRecipe(
            const AZStd::string& recordId) const;
        const PopulationActorProfile* FindPopulationActorProfile(
            const AZStd::string& recordId) const;
        const PopulationTroopProfile* FindPopulationTroopProfile(
            const AZStd::string& recordId) const;

        AZStd::vector<CatalogRecord> Query(const CatalogQuery& query) const;
        AZStd::vector<CatalogRelationship> FindRelationshipsForRecord(
            const AZStd::string& recordId) const;
        AZStd::vector<CatalogValidationEvent> FindValidationForRecord(
            const AZStd::string& recordId) const;
        AZStd::vector<CatalogValidationEvent> FindValidationForSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId) const;
        AZStd::vector<CatalogGovernanceEvent> FindGovernanceForSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId) const;
        AZStd::vector<EconomyRecipeIngredient> FindIngredientsForRecipe(
            const AZStd::string& recipeRecordId) const;
        AZStd::vector<EconomyRecipeOutput> FindOutputsForRecipe(
            const AZStd::string& recipeRecordId) const;
        AZStd::vector<PopulationTroopMember> FindPopulationMembersForTroop(
            const AZStd::string& troopRecordId) const;
        AZStd::vector<DomainCoverage> BuildCoverage() const;

        CatalogDocument BuildDocument(
            const WorkspaceModel& workspace,
            const GameProfile& profile) const;

        const AZStd::vector<CatalogRecord>& GetRecords() const;
        const AZStd::vector<CatalogRelationship>& GetRelationships() const;
        const AZStd::vector<CatalogValidationEvent>& GetValidationHistory() const;
        const AZStd::vector<CatalogGovernanceEvent>& GetGovernanceHistory() const;
        const AZStd::vector<EconomyItemProfile>& GetEconomyItems() const;
        const AZStd::vector<EconomyRecipeProfile>& GetEconomyRecipes() const;
        const AZStd::vector<EconomyRecipeIngredient>& GetRecipeIngredients() const;
        const AZStd::vector<EconomyRecipeOutput>& GetRecipeOutputs() const;
        const AZStd::vector<PopulationActorProfile>& GetPopulationActorProfiles() const;
        const AZStd::vector<PopulationTroopProfile>& GetPopulationTroopProfiles() const;
        const AZStd::vector<PopulationTroopMember>& GetPopulationTroopMembers() const;

    private:
        friend class CatalogGovernanceService;

        CatalogRecord* FindMutableRecordById(const AZStd::string& recordId);
        CatalogRelationship* FindMutableRelationshipById(
            const AZStd::string& relationshipId);
        bool AddValidationEvent(
            const CatalogValidationEvent& validation,
            AZStd::string* error);
        bool AddGovernanceEvent(
            const CatalogGovernanceEvent& event,
            AZStd::string* error);
        bool ReplaceFromDocument(
            const CatalogDocument& document,
            AZStd::string* error);
        bool ValidateRecord(
            const CatalogRecord& record,
            const CatalogRecord* replacing,
            AZStd::string* error) const;
        bool ValidateRelationship(
            const CatalogRelationship& relationship,
            AZStd::string* error) const;
        bool ValidateGovernanceEvent(
            const CatalogGovernanceEvent& event,
            AZStd::string* error) const;
        bool ValidateEconomyItem(
            const EconomyItemProfile& profile,
            AZStd::string* error) const;
        bool ValidateEconomyRecipe(
            const EconomyRecipeProfile& profile,
            AZStd::string* error) const;
        bool ValidateRecipeIngredient(
            const EconomyRecipeIngredient& ingredient,
            AZStd::string* error) const;
        bool ValidateRecipeOutput(
            const EconomyRecipeOutput& output,
            AZStd::string* error) const;
        bool ValidatePopulationActorProfile(
            const PopulationActorProfile& profile,
            AZStd::string* error) const;
        bool ValidatePopulationTroopProfile(
            const PopulationTroopProfile& profile,
            AZStd::string* error) const;
        bool ValidatePopulationTroopMember(
            const PopulationTroopMember& member,
            AZStd::string* error) const;

        bool ReplaceFromDocumentWithoutPopulation(
            const CatalogDocument& document,
            AZStd::string* error);
        void ClearWithoutPopulation();
        CatalogDocument BuildDocumentWithoutPopulation(
            const WorkspaceModel& workspace,
            const GameProfile& profile) const;
        bool ReplaceFromBoundDocumentWithoutPopulation(
            const CatalogDocument& document,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error);
        bool ValidateIntegrityWithoutPopulation(
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string* error) const;

        AZStd::vector<CatalogRecord> m_records;
        AZStd::vector<CatalogRelationship> m_relationships;
        AZStd::vector<CatalogValidationEvent> m_validationHistory;
        AZStd::vector<CatalogGovernanceEvent> m_governanceHistory;
        AZStd::vector<EconomyItemProfile> m_economyItems;
        AZStd::vector<EconomyRecipeProfile> m_economyRecipes;
        AZStd::vector<EconomyRecipeIngredient> m_recipeIngredients;
        AZStd::vector<EconomyRecipeOutput> m_recipeOutputs;
        AZStd::vector<PopulationActorProfile> m_populationActorProfiles;
        AZStd::vector<PopulationTroopProfile> m_populationTroopProfiles;
        AZStd::vector<PopulationTroopMember> m_populationTroopMembers;
    };
} // namespace TaintedGrailModdingSDK
