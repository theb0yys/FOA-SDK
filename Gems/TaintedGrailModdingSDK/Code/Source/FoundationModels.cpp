/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationModels.h"

#include "ResearchContractValidation.h"

#include <AzCore/Serialization/SerializeContext.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsBoundedIdentityToken(
            const AZStd::string& value,
            size_t maximumLength = 128)
        {
            if (value.empty() || value.size() > maximumLength)
            {
                return false;
            }
            for (char character : value)
            {
                const bool allowed =
                    (character >= 'a' && character <= 'z')
                    || (character >= 'A' && character <= 'Z')
                    || (character >= '0' && character <= '9')
                    || character == '.'
                    || character == '_'
                    || character == '-';
                if (!allowed)
                {
                    return false;
                }
            }
            return value.find("..") == AZStd::string::npos
                && value.front() != '.'
                && value.back() != '.';
        }

        bool IsBoundedVersionToken(const AZStd::string& value)
        {
            if (value.empty() || value.size() > 128)
            {
                return false;
            }
            for (char character : value)
            {
                const bool allowed =
                    (character >= 'a' && character <= 'z')
                    || (character >= 'A' && character <= 'Z')
                    || (character >= '0' && character <= '9')
                    || character == '.'
                    || character == '_'
                    || character == '-'
                    || character == '+';
                if (!allowed)
                {
                    return false;
                }
            }
            return value.front() != '.'
                && value.back() != '.'
                && value.find("..") == AZStd::string::npos;
        }

        bool HasUniqueBoundedTokens(const AZStd::vector<AZStd::string>& values)
        {
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsBoundedIdentityToken(value, 256))
                {
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                == sorted.end();
        }
    } // namespace

    void GameProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GameProfile>()
                ->Version(2)
                ->Field("ProfileId", &GameProfile::m_profileId)
                ->Field("DisplayName", &GameProfile::m_displayName)
                ->Field("InstallPath", &GameProfile::m_installPath)
                ->Field("GameVersion", &GameProfile::m_gameVersion)
                ->Field("Branch", &GameProfile::m_branch)
                ->Field("RuntimeTarget", &GameProfile::m_runtimeTarget)
                ->Field("UnityVersion", &GameProfile::m_unityVersion)
                ->Field("BepInExVersion", &GameProfile::m_bepInExVersion)
                ->Field("ManagedAssembliesPath", &GameProfile::m_managedAssembliesPath)
                ->Field("PluginPath", &GameProfile::m_pluginPath)
                ->Field("DiagnosticsPath", &GameProfile::m_diagnosticsPath)
                ->Field("ExtractedDataPath", &GameProfile::m_extractedDataPath)
                ->Field("DlcScopes", &GameProfile::m_dlcScopes);
        }
    }

    bool GameProfile::IsConfigured() const
    {
        const bool monoConfigured = m_runtimeTarget != "Mono"
            || (!m_bepInExVersion.empty()
                && IsBoundedVersionToken(m_bepInExVersion)
                && !m_pluginPath.empty());
        const bool il2CppConfigured = m_runtimeTarget != "IL2CPP"
            || m_pluginPath.empty();
        return IsStableContractId(m_profileId)
            && !m_displayName.empty()
            && m_displayName.size() <= 512
            && !m_installPath.empty()
            && IsBoundedVersionToken(m_gameVersion)
            && IsBoundedIdentityToken(m_branch)
            && IsSupportedRuntimeTarget(m_runtimeTarget)
            && IsBoundedVersionToken(m_unityVersion)
            && !m_managedAssembliesPath.empty()
            && HasUniqueBoundedTokens(m_dlcScopes)
            && monoConfigured
            && il2CppConfigured;
    }

    void WorkspaceModel::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<WorkspaceModel>()
                ->Version(2)
                ->Field("WorkspaceId", &WorkspaceModel::m_workspaceId)
                ->Field("DisplayName", &WorkspaceModel::m_displayName)
                ->Field("RootPath", &WorkspaceModel::m_rootPath)
                ->Field("OutputPath", &WorkspaceModel::m_outputPath)
                ->Field("StagingPath", &WorkspaceModel::m_stagingPath)
                ->Field("DeploymentPath", &WorkspaceModel::m_deploymentPath)
                ->Field("ActiveGameProfileId", &WorkspaceModel::m_activeGameProfileId)
                ->Field("GameProfiles", &WorkspaceModel::m_gameProfiles);
        }
    }

    const GameProfile* WorkspaceModel::FindActiveGameProfile() const
    {
        for (const GameProfile& profile : m_gameProfiles)
        {
            if (profile.m_profileId == m_activeGameProfileId)
            {
                return &profile;
            }
        }
        return nullptr;
    }

    void PackManifest::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PackManifest>()
                ->Version(3)
                ->Field("SchemaVersion", &PackManifest::m_schemaVersion)
                ->Field("PackId", &PackManifest::m_packId)
                ->Field("DisplayName", &PackManifest::m_displayName)
                ->Field("OwnerId", &PackManifest::m_ownerId)
                ->Field("Version", &PackManifest::m_version)
                ->Field("TargetGameVersion", &PackManifest::m_targetGameVersion)
                ->Field("TargetBranch", &PackManifest::m_targetBranch)
                ->Field("CompatibleGameVersions", &PackManifest::m_compatibleGameVersions)
                ->Field("RequiredCoreVersion", &PackManifest::m_requiredCoreVersion)
                ->Field("RequiredAdapterVersion", &PackManifest::m_requiredAdapterVersion)
                ->Field("SaveImpact", &PackManifest::m_saveImpact)
                ->Field("DlcScopes", &PackManifest::m_dlcScopes)
                ->Field("Dependencies", &PackManifest::m_dependencies)
                ->Field("RequiredMods", &PackManifest::m_requiredMods)
                ->Field("Incompatibilities", &PackManifest::m_incompatibilities)
                ->Field("ContentDefinitionPaths", &PackManifest::m_contentDefinitionPaths)
                ->Field("AssetPaths", &PackManifest::m_assetPaths)
                ->Field("LocalisationPaths", &PackManifest::m_localisationPaths)
                ->Field("BuildConfiguration", &PackManifest::m_buildConfiguration)
                ->Field("ReleaseChannel", &PackManifest::m_releaseChannel)
                ->Field("RuntimeActionsEnabled", &PackManifest::m_runtimeActionsEnabled);
        }
    }

    bool PackManifest::HasStableIdentity() const
    {
        return m_schemaVersion == 1
            && IsStableContractId(m_packId)
            && m_packId.find(':') == AZStd::string::npos
            && IsBoundedIdentityToken(m_ownerId, 256)
            && IsStrictSemanticVersion(m_version);
    }

    bool PackManifest::UsesSupportedSchema() const
    {
        return m_schemaVersion == 1;
    }

    void SourceImporterContract::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SourceImporterContract>()
                ->Version(1)
                ->Field("ImporterId", &SourceImporterContract::m_importerId)
                ->Field("DisplayName", &SourceImporterContract::m_displayName)
                ->Field("SupportedSourceKinds", &SourceImporterContract::m_supportedSourceKinds)
                ->Field("SupportedExtensions", &SourceImporterContract::m_supportedExtensions)
                ->Field("ExtractsEvidence", &SourceImporterContract::m_extractsEvidence);
        }
    }

    void SourceRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SourceRecord>()
                ->Version(2)
                ->Field("SourceId", &SourceRecord::m_sourceId)
                ->Field("Title", &SourceRecord::m_title)
                ->Field("SourceKind", &SourceRecord::m_sourceKind)
                ->Field("Locator", &SourceRecord::m_locator)
                ->Field("Fingerprint", &SourceRecord::m_fingerprint)
                ->Field("ProfileId", &SourceRecord::m_profileId)
                ->Field("GameVersion", &SourceRecord::m_gameVersion)
                ->Field("Branch", &SourceRecord::m_branch)
                ->Field("RuntimeTarget", &SourceRecord::m_runtimeTarget)
                ->Field("ToolName", &SourceRecord::m_toolName)
                ->Field("ToolVersion", &SourceRecord::m_toolVersion)
                ->Field("ImporterId", &SourceRecord::m_importerId)
                ->Field("ImporterVersion", &SourceRecord::m_importerVersion)
                ->Field("CapturedAt", &SourceRecord::m_capturedAt)
                ->Field("ImportedAt", &SourceRecord::m_importedAt)
                ->Field("Limitations", &SourceRecord::m_limitations)
                ->Field("MediaType", &SourceRecord::m_mediaType)
                ->Field("ByteSize", &SourceRecord::m_byteSize)
                ->Field("ImportStatus", &SourceRecord::m_importStatus);
        }
    }

    void EvidenceRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EvidenceRecord>()
                ->Version(2)
                ->Field("EvidenceId", &EvidenceRecord::m_evidenceId)
                ->Field("SourceId", &EvidenceRecord::m_sourceId)
                ->Field("SourceFingerprint", &EvidenceRecord::m_sourceFingerprint)
                ->Field("ProfileId", &EvidenceRecord::m_profileId)
                ->Field("GameVersion", &EvidenceRecord::m_gameVersion)
                ->Field("Branch", &EvidenceRecord::m_branch)
                ->Field("SubjectRef", &EvidenceRecord::m_subjectRef)
                ->Field("Claim", &EvidenceRecord::m_claim)
                ->Field("EvidenceKind", &EvidenceRecord::m_evidenceKind)
                ->Field("Confidence", &EvidenceRecord::m_confidence)
                ->Field("Locator", &EvidenceRecord::m_locator)
                ->Field("RecordPath", &EvidenceRecord::m_recordPath)
                ->Field("ExtractedAt", &EvidenceRecord::m_extractedAt);
        }
    }

    void ImportIssue::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ImportIssue>()
                ->Version(1)
                ->Field("IssueId", &ImportIssue::m_issueId)
                ->Field("Severity", &ImportIssue::m_severity)
                ->Field("Code", &ImportIssue::m_code)
                ->Field("Message", &ImportIssue::m_message)
                ->Field("Locator", &ImportIssue::m_locator)
                ->Field("RecordPath", &ImportIssue::m_recordPath)
                ->Field("Line", &ImportIssue::m_line);
        }
    }

    void SourceDocument::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SourceDocument>()
                ->Version(1)
                ->Field("SchemaVersion", &SourceDocument::m_schemaVersion)
                ->Field("Source", &SourceDocument::m_source)
                ->Field("Issues", &SourceDocument::m_issues);
        }
    }

    bool SourceDocument::UsesSupportedSchema() const
    {
        return m_schemaVersion == 1;
    }

    void EvidenceDocument::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EvidenceDocument>()
                ->Version(1)
                ->Field("SchemaVersion", &EvidenceDocument::m_schemaVersion)
                ->Field("SourceId", &EvidenceDocument::m_sourceId)
                ->Field("SourceFingerprint", &EvidenceDocument::m_sourceFingerprint)
                ->Field("ProfileId", &EvidenceDocument::m_profileId)
                ->Field("GameVersion", &EvidenceDocument::m_gameVersion)
                ->Field("Branch", &EvidenceDocument::m_branch)
                ->Field("Evidence", &EvidenceDocument::m_evidence)
                ->Field("Issues", &EvidenceDocument::m_issues);
        }
    }

    bool EvidenceDocument::UsesSupportedSchema() const
    {
        return m_schemaVersion == 1;
    }

    bool SourceImportResult::HasErrors() const
    {
        for (const ImportIssue& issue : m_sourceDocument.m_issues)
        {
            if (issue.m_severity == "error")
            {
                return true;
            }
        }
        for (const ImportIssue& issue : m_evidenceDocument.m_issues)
        {
            if (issue.m_severity == "error")
            {
                return true;
            }
        }
        return false;
    }

    void CatalogRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogRecord>()
                ->Version(3)
                ->Field("RecordId", &CatalogRecord::m_recordId)
                ->Field("OwnerPackId", &CatalogRecord::m_ownerPackId)
                ->Field("Domain", &CatalogRecord::m_domain)
                ->Field("RecordKind", &CatalogRecord::m_recordKind)
                ->Field("SubjectRef", &CatalogRecord::m_subjectRef)
                ->Field("NativeRefExact", &CatalogRecord::m_nativeRefExact)
                ->Field("IdentityKind", &CatalogRecord::m_identityKind)
                ->Field("DisplayName", &CatalogRecord::m_displayName)
                ->Field("Aliases", &CatalogRecord::m_aliases)
                ->Field("SourceScopedRefs", &CatalogRecord::m_sourceScopedRefs)
                ->Field("ResearchStage", &CatalogRecord::m_researchStage)
                ->Field("Confidence", &CatalogRecord::m_confidence)
                ->Field("OperationalRisk", &CatalogRecord::m_operationalRisk)
                ->Field("ValidationState", &CatalogRecord::m_validationState)
                ->Field("StalenessState", &CatalogRecord::m_stalenessState)
                ->Field("AllowedUsages", &CatalogRecord::m_allowedUsages)
                ->Field("ForbiddenUsages", &CatalogRecord::m_forbiddenUsages)
                ->Field("EvidenceIds", &CatalogRecord::m_evidenceIds)
                ->Field("MissingRefs", &CatalogRecord::m_missingRefs)
                ->Field("ConflictRefs", &CatalogRecord::m_conflictRefs)
                ->Field("Tags", &CatalogRecord::m_tags)
                ->Field("CreatedAt", &CatalogRecord::m_createdAt)
                ->Field("UpdatedAt", &CatalogRecord::m_updatedAt)
                ->Field("SupersededByRecordId", &CatalogRecord::m_supersededByRecordId);
        }
    }

    bool CatalogRecord::IsSynthetic() const
    {
        return m_identityKind == "synthetic";
    }

    bool CatalogRecord::IsBlocked() const
    {
        return !m_missingRefs.empty()
            || !m_conflictRefs.empty()
            || !m_forbiddenUsages.empty()
            || m_stalenessState == "potentially_stale"
            || m_stalenessState == "stale"
            || !m_supersededByRecordId.empty();
    }

    void CatalogRelationship::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogRelationship>()
                ->Version(2)
                ->Field("RelationshipId", &CatalogRelationship::m_relationshipId)
                ->Field("FromRecordId", &CatalogRelationship::m_fromRecordId)
                ->Field("ToRecordId", &CatalogRelationship::m_toRecordId)
                ->Field("TargetSubjectRef", &CatalogRelationship::m_targetSubjectRef)
                ->Field("RelationshipKind", &CatalogRelationship::m_relationshipKind)
                ->Field("EvidenceIds", &CatalogRelationship::m_evidenceIds)
                ->Field("ResearchStage", &CatalogRelationship::m_researchStage)
                ->Field("Confidence", &CatalogRelationship::m_confidence)
                ->Field("OperationalRisk", &CatalogRelationship::m_operationalRisk)
                ->Field("ValidationState", &CatalogRelationship::m_validationState)
                ->Field("StalenessState", &CatalogRelationship::m_stalenessState)
                ->Field("AllowedUsages", &CatalogRelationship::m_allowedUsages)
                ->Field("ForbiddenUsages", &CatalogRelationship::m_forbiddenUsages)
                ->Field("MissingRefs", &CatalogRelationship::m_missingRefs)
                ->Field("ConflictRefs", &CatalogRelationship::m_conflictRefs)
                ->Field("Attributes", &CatalogRelationship::m_attributes)
                ->Field("CreatedAt", &CatalogRelationship::m_createdAt)
                ->Field("UpdatedAt", &CatalogRelationship::m_updatedAt)
                ->Field("SupersededByRelationshipId", &CatalogRelationship::m_supersededByRelationshipId);
        }
    }

    bool CatalogRelationship::IsBlocked() const
    {
        return !m_missingRefs.empty()
            || !m_conflictRefs.empty()
            || !m_forbiddenUsages.empty()
            || m_stalenessState == "potentially_stale"
            || m_stalenessState == "stale"
            || !m_supersededByRelationshipId.empty();
    }

    void CatalogValidationEvent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogValidationEvent>()
                ->Version(2)
                ->Field("ValidationId", &CatalogValidationEvent::m_validationId)
                ->Field("SubjectKind", &CatalogValidationEvent::m_subjectKind)
                ->Field("SubjectId", &CatalogValidationEvent::m_subjectId)
                ->Field("RecordId", &CatalogValidationEvent::m_recordId)
                ->Field("State", &CatalogValidationEvent::m_state)
                ->Field("Method", &CatalogValidationEvent::m_method)
                ->Field("Validator", &CatalogValidationEvent::m_validator)
                ->Field("CheckedAt", &CatalogValidationEvent::m_checkedAt)
                ->Field("ProfileId", &CatalogValidationEvent::m_profileId)
                ->Field("GameVersion", &CatalogValidationEvent::m_gameVersion)
                ->Field("Branch", &CatalogValidationEvent::m_branch)
                ->Field("EvidenceIds", &CatalogValidationEvent::m_evidenceIds)
                ->Field("Notes", &CatalogValidationEvent::m_notes);
        }
    }

    AZStd::string CatalogValidationEvent::GetSubjectKind() const
    {
        return m_subjectKind.empty() ? AZStd::string("record") : m_subjectKind;
    }

    AZStd::string CatalogValidationEvent::GetSubjectId() const
    {
        return m_subjectId.empty() ? m_recordId : m_subjectId;
    }

    void CatalogGovernanceEvent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogGovernanceEvent>()
                ->Version(1)
                ->Field("EventId", &CatalogGovernanceEvent::m_eventId)
                ->Field("SubjectKind", &CatalogGovernanceEvent::m_subjectKind)
                ->Field("SubjectId", &CatalogGovernanceEvent::m_subjectId)
                ->Field("Axis", &CatalogGovernanceEvent::m_axis)
                ->Field("PreviousValue", &CatalogGovernanceEvent::m_previousValue)
                ->Field("NewValue", &CatalogGovernanceEvent::m_newValue)
                ->Field("Usage", &CatalogGovernanceEvent::m_usage)
                ->Field("EvidenceIds", &CatalogGovernanceEvent::m_evidenceIds)
                ->Field("ValidationIds", &CatalogGovernanceEvent::m_validationIds)
                ->Field("Reviewer", &CatalogGovernanceEvent::m_reviewer)
                ->Field("DecidedAt", &CatalogGovernanceEvent::m_decidedAt)
                ->Field("Notes", &CatalogGovernanceEvent::m_notes);
        }
    }

    void CatalogDocument::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogDocument>()
                ->Version(3)
                ->Field("SchemaVersion", &CatalogDocument::m_schemaVersion)
                ->Field("WorkspaceId", &CatalogDocument::m_workspaceId)
                ->Field("ProfileId", &CatalogDocument::m_profileId)
                ->Field("GameVersion", &CatalogDocument::m_gameVersion)
                ->Field("Branch", &CatalogDocument::m_branch)
                ->Field("Records", &CatalogDocument::m_records)
                ->Field("Relationships", &CatalogDocument::m_relationships)
                ->Field("ValidationHistory", &CatalogDocument::m_validationHistory)
                ->Field("GovernanceHistory", &CatalogDocument::m_governanceHistory)
                ->Field("EconomyItems", &CatalogDocument::m_economyItems)
                ->Field("EconomyRecipes", &CatalogDocument::m_economyRecipes)
                ->Field("RecipeIngredients", &CatalogDocument::m_recipeIngredients)
                ->Field("RecipeOutputs", &CatalogDocument::m_recipeOutputs);
        }
    }

    bool CatalogDocument::UsesSupportedSchema() const
    {
        return m_schemaVersion == 1;
    }

    void BlockerRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BlockerRecord>()
                ->Version(1)
                ->Field("BlockerId", &BlockerRecord::m_blockerId)
                ->Field("Severity", &BlockerRecord::m_severity)
                ->Field("Area", &BlockerRecord::m_area)
                ->Field("SubjectRef", &BlockerRecord::m_subjectRef)
                ->Field("Reason", &BlockerRecord::m_reason)
                ->Field("Status", &BlockerRecord::m_status)
                ->Field("AffectedUsages", &BlockerRecord::m_affectedUsages);
        }
    }

    void DomainCoverage::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DomainCoverage>()
                ->Version(1)
                ->Field("Domain", &DomainCoverage::m_domain)
                ->Field("RecordCount", &DomainCoverage::m_recordCount)
                ->Field("BlockedRecordCount", &DomainCoverage::m_blockedRecordCount);
        }
    }

    void FoundationSnapshot::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<FoundationSnapshot>()
                ->Version(7)
                ->Field("WorkspaceName", &FoundationSnapshot::m_workspaceName)
                ->Field("WorkspaceFilePath", &FoundationSnapshot::m_workspaceFilePath)
                ->Field("ActiveGameProfile", &FoundationSnapshot::m_activeGameProfile)
                ->Field("GameVersion", &FoundationSnapshot::m_gameVersion)
                ->Field("Branch", &FoundationSnapshot::m_branch)
                ->Field("RuntimeTarget", &FoundationSnapshot::m_runtimeTarget)
                ->Field("UnityVersion", &FoundationSnapshot::m_unityVersion)
                ->Field("BepInExVersion", &FoundationSnapshot::m_bepInExVersion)
                ->Field("ActivePackId", &FoundationSnapshot::m_activePackId)
                ->Field("ActivePackName", &FoundationSnapshot::m_activePackName)
                ->Field("ActivePackVersion", &FoundationSnapshot::m_activePackVersion)
                ->Field("ActivePackFilePath", &FoundationSnapshot::m_activePackFilePath)
                ->Field("GameProfileCount", &FoundationSnapshot::m_gameProfileCount)
                ->Field("PackCount", &FoundationSnapshot::m_packCount)
                ->Field("SourceCount", &FoundationSnapshot::m_sourceCount)
                ->Field("EvidenceCount", &FoundationSnapshot::m_evidenceCount)
                ->Field("ImportErrorCount", &FoundationSnapshot::m_importErrorCount)
                ->Field("ImportWarningCount", &FoundationSnapshot::m_importWarningCount)
                ->Field("CatalogRecordCount", &FoundationSnapshot::m_catalogRecordCount)
                ->Field("CatalogRelationshipCount", &FoundationSnapshot::m_catalogRelationshipCount)
                ->Field("CatalogValidationCount", &FoundationSnapshot::m_catalogValidationCount)
                ->Field("CatalogGovernanceCount", &FoundationSnapshot::m_catalogGovernanceCount)
                ->Field("EconomyItemCount", &FoundationSnapshot::m_economyItemCount)
                ->Field("EconomyRecipeCount", &FoundationSnapshot::m_economyRecipeCount)
                ->Field("RecipeIngredientCount", &FoundationSnapshot::m_recipeIngredientCount)
                ->Field("RecipeOutputCount", &FoundationSnapshot::m_recipeOutputCount)
                ->Field("StaleCatalogSubjectCount", &FoundationSnapshot::m_staleCatalogSubjectCount)
                ->Field("AllowedUsageCount", &FoundationSnapshot::m_allowedUsageCount)
                ->Field("ForbiddenUsageCount", &FoundationSnapshot::m_forbiddenUsageCount)
                ->Field("OpenBlockerCount", &FoundationSnapshot::m_openBlockerCount)
                ->Field("DomainCoverage", &FoundationSnapshot::m_domainCoverage)
                ->Field("ImportIssues", &FoundationSnapshot::m_importIssues)
                ->Field("Blockers", &FoundationSnapshot::m_blockers);
        }
    }
} // namespace TaintedGrailModdingSDK
