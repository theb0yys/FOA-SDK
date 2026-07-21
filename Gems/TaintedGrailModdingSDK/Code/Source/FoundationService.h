/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "CatalogGovernanceBlockerService.h"
#include "CatalogGovernanceService.h"
#include "CatalogPersistenceService.h"
#include "CatalogPromotionService.h"
#include "CatalogTransactionService.h"
#include "EconomyBlockerService.h"
#include "ExtensionAPI.h"
#include "FoundationPersistenceBoundary.h"
#include "FoundationValidationService.h"
#include "FoundationWorkspaceLoadService.h"
#include "PopulationAuthoringService.h"
#include "SourceEvidencePersistenceService.h"
#include "SourceEvidenceRegistry.h"
#include "SourceImportService.h"

namespace TaintedGrailModdingSDK
{
    class FoundationService
    {
    public:
        static FoundationService& Get();
        explicit FoundationService(FoundationWorkspaceLoadDependencies workspaceLoadDependencies);

        void Initialize();
        void Shutdown();
        bool IsInitialized() const;

        void SetWorkspace(const WorkspaceModel& workspace);
        bool SaveWorkspace(const AZStd::string& filePath, AZStd::string* error = nullptr);
        bool SaveWorkspace(AZStd::string* error = nullptr);
        bool LoadWorkspace(const AZStd::string& filePath, AZStd::string* error = nullptr);

        bool UpsertPack(const PackManifest& pack, AZStd::string* error = nullptr);
        bool SetActivePack(const PackManifest& pack, AZStd::string* error = nullptr);
        bool SaveActivePack(const AZStd::string& filePath, AZStd::string* error = nullptr);
        bool SaveActivePack(AZStd::string* error = nullptr);
        bool LoadPack(const AZStd::string& filePath, AZStd::string* error = nullptr);
        void ClearActivePack();

        bool ImportSource(
            const SourceImportRequest& request,
            SourceEvidenceDocumentPaths* savedPaths = nullptr,
            AZStd::string* error = nullptr);
        bool ReloadSourceEvidence(AZStd::string* error = nullptr);
        AZStd::vector<SourceImporterContract> GetSourceImporterContracts() const;

        bool PromoteEvidenceToCatalog(
            const CatalogPromotionRequest& request,
            AZStd::string* error = nullptr);
        bool ApplyCatalogGovernanceDecision(
            const CatalogGovernanceRequest& request,
            AZStd::string* error = nullptr);
        bool ApplyCatalogValidationDecision(
            const CatalogValidationRequest& request,
            AZStd::string* error = nullptr);
        bool UpsertCatalogRelationship(
            const CatalogRelationship& relationship,
            AZStd::string* error = nullptr);
        bool AddCatalogValidationEvent(
            const CatalogValidationEvent& validation,
            AZStd::string* error = nullptr);
        bool UpsertEconomyItemProfile(
            const EconomyItemProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertEconomyRecipeProfile(
            const EconomyRecipeProfile& profile,
            AZStd::string* error = nullptr);
        bool UpsertEconomyRecipeIngredient(
            const EconomyRecipeIngredient& ingredient,
            AZStd::string* error = nullptr);
        bool UpsertEconomyRecipeOutput(
            const EconomyRecipeOutput& output,
            AZStd::string* error = nullptr);
        bool UpsertPopulationActorProfile(
            const PopulationActorProfile& actor,
            AZStd::string* error = nullptr);
        bool UpsertPopulationTroopDefinition(
            const PopulationTroopDefinition& definition,
            AZStd::string* error = nullptr);
        bool UpsertPopulationTroopProfile(
            const PopulationTroopProfile& troop,
            AZStd::string* error = nullptr);
        bool UpsertPopulationTroopMember(
            const PopulationTroopMember& member,
            AZStd::string* error = nullptr);
        bool SaveCatalog(AZStd::string* error = nullptr);
        bool ReloadCatalog(AZStd::string* error = nullptr);

        bool RegisterSource(const SourceRecord& source, AZStd::string* error = nullptr);
        bool RegisterEvidence(const EvidenceRecord& evidence, AZStd::string* error = nullptr);

        const WorkspaceModel& GetWorkspace() const;
        const AZStd::string& GetWorkspaceFilePath() const;
        const AZStd::string& GetWorkspaceRootPath() const;
        const AZStd::vector<PackManifest>& GetPacks() const;
        const PackManifest* GetActivePack() const;
        const AZStd::string& GetActivePackFilePath() const;
        const SourceEvidenceRegistry& GetSourceRegistry() const;
        const AZStd::vector<ImportIssue>& GetImportIssues() const;
        const CatalogDatabase& GetCatalog() const;
        const AZStd::string& GetCatalogFilePath() const;
        const FoundationSnapshot& GetSnapshot() const;
        ExtensionAPI::Service& GetExtensionAPI();
        const ExtensionAPI::Service& GetExtensionAPI() const;

        void RefreshSnapshot();

    private:
        FoundationService();

        void ClearWorkspaceScopedState(bool clearWorkspaceLocation);
        bool UpsertCatalogRecord(const CatalogRecord& record, AZStd::string* error = nullptr);
        bool PersistCatalogCandidate(const CatalogDatabase& candidate, AZStd::string* error);
        PackManifest* FindPackById(const AZStd::string& packId);
        const PackManifest* FindPackById(const AZStd::string& packId) const;
        static ImportIssue MakeRegistryIssue(
            AZStd::string severity,
            AZStd::string code,
            AZStd::string message,
            AZStd::string locator);

        WorkspaceModel m_workspace;
        AZStd::string m_workspaceFilePath;
        AZStd::string m_workspaceRootPath;
        AZStd::vector<PackManifest> m_packs;
        AZStd::string m_activePackId;
        AZStd::string m_activePackFilePath;
        SourceEvidenceRegistry m_sourceRegistry;
        AZStd::vector<ImportIssue> m_importIssues;
        CatalogDatabase m_catalog;
        ExtensionAPI::Service m_extensionApi;
        AZStd::string m_catalogFilePath;
        FoundationValidationService m_validationService;
        CatalogGovernanceBlockerService m_governanceBlockerService;
        EconomyBlockerService m_economyBlockerService;
        PathPolicyService m_pathPolicy;
        FoundationWorkspacePersistenceBoundary m_workspacePersistence;
        FoundationPackPersistenceBoundary m_packPersistence;
        SourceImportService m_sourceImportService;
        SourceEvidencePersistenceService m_sourceEvidencePersistence;
        CatalogPersistenceService m_catalogPersistence;
        CatalogTransactionService m_catalogTransaction;
        CatalogPromotionService m_catalogPromotion;
        CatalogGovernanceService m_catalogGovernance;
        PopulationAuthoringService m_populationAuthoring;
        FoundationWorkspaceLoadService m_workspaceLoadService;
        FoundationSnapshot m_snapshot;
        bool m_initialized = false;
    };
} // namespace TaintedGrailModdingSDK
