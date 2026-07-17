/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class ReflectContext;
}

namespace TaintedGrailModdingSDK
{
    struct GameProfile
    {
        AZ_TYPE_INFO(GameProfile, "{DC297C16-E75D-4C9B-99E2-B35985F246C5}");

        static void Reflect(AZ::ReflectContext* context);
        bool IsConfigured() const;

        AZStd::string m_profileId;
        AZStd::string m_displayName;
        AZStd::string m_installPath;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_unityVersion;
        AZStd::string m_bepInExVersion;
        AZStd::string m_managedAssembliesPath;
        AZStd::string m_pluginPath;
        AZStd::string m_diagnosticsPath;
        AZStd::string m_extractedDataPath;
        AZStd::vector<AZStd::string> m_dlcScopes;
    };

    struct WorkspaceModel
    {
        AZ_TYPE_INFO(WorkspaceModel, "{7D79CC9C-E263-46B0-839C-41B8C0EFFB78}");

        static void Reflect(AZ::ReflectContext* context);
        const GameProfile* FindActiveGameProfile() const;

        AZStd::string m_workspaceId;
        AZStd::string m_displayName;
        AZStd::string m_rootPath;
        AZStd::string m_outputPath;
        AZStd::string m_stagingPath;
        AZStd::string m_deploymentPath;
        AZStd::string m_activeGameProfileId;
        AZStd::vector<GameProfile> m_gameProfiles;
    };

    struct PackManifest
    {
        AZ_TYPE_INFO(PackManifest, "{34531983-5855-49A2-9D33-118F6F7E7CB9}");

        static void Reflect(AZ::ReflectContext* context);
        bool HasStableIdentity() const;
        bool UsesSupportedSchema() const;

        AZ::u32 m_schemaVersion = 1;
        AZStd::string m_packId;
        AZStd::string m_displayName;
        AZStd::string m_ownerId;
        AZStd::string m_version;
        AZStd::string m_targetGameVersion;
        AZStd::string m_targetBranch;
        AZStd::vector<AZStd::string> m_compatibleGameVersions;
        AZStd::string m_requiredCoreVersion;
        AZStd::string m_requiredAdapterVersion;
        AZStd::string m_saveImpact;
        AZStd::vector<AZStd::string> m_dlcScopes;
        AZStd::vector<AZStd::string> m_dependencies;
        AZStd::vector<AZStd::string> m_requiredMods;
        AZStd::vector<AZStd::string> m_incompatibilities;
        AZStd::vector<AZStd::string> m_contentDefinitionPaths;
        AZStd::vector<AZStd::string> m_assetPaths;
        AZStd::vector<AZStd::string> m_localisationPaths;
        AZStd::string m_buildConfiguration;
        AZStd::string m_releaseChannel;
        bool m_runtimeActionsEnabled = false;
    };

    struct SourceImporterContract
    {
        AZ_TYPE_INFO(SourceImporterContract, "{84A83DA2-52AA-45EA-AEA0-D01DF3F82D7C}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_importerId;
        AZStd::string m_displayName;
        AZStd::vector<AZStd::string> m_supportedSourceKinds;
        AZStd::vector<AZStd::string> m_supportedExtensions;
        bool m_extractsEvidence = false;
    };

    struct SourceRecord
    {
        AZ_TYPE_INFO(SourceRecord, "{2569A76D-E028-4FF4-8619-265B354813A4}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_sourceId;
        AZStd::string m_title;
        AZStd::string m_sourceKind;
        AZStd::string m_locator;
        AZStd::string m_fingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_toolName;
        AZStd::string m_toolVersion;
        AZStd::string m_importerId;
        AZStd::string m_importerVersion;
        AZStd::string m_capturedAt;
        AZStd::string m_importedAt;
        AZStd::string m_limitations;
        AZStd::string m_mediaType;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_importStatus;
    };

    struct EvidenceRecord
    {
        AZ_TYPE_INFO(EvidenceRecord, "{850C10EB-36D8-4383-AC42-F598D587554A}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_evidenceId;
        AZStd::string m_sourceId;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_subjectRef;
        AZStd::string m_claim;
        AZStd::string m_evidenceKind;
        AZStd::string m_confidence;
        AZStd::string m_locator;
        AZStd::string m_recordPath;
        AZStd::string m_extractedAt;
    };

    struct ImportIssue
    {
        AZ_TYPE_INFO(ImportIssue, "{65080C4B-37BD-4E54-8071-1FD8B23DF117}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_issueId;
        AZStd::string m_severity;
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_locator;
        AZStd::string m_recordPath;
        AZ::s64 m_line = 0;
    };

    struct SourceDocument
    {
        AZ_TYPE_INFO(SourceDocument, "{98E18D12-E262-4404-AF4E-DCA613375B3B}");

        static void Reflect(AZ::ReflectContext* context);
        bool UsesSupportedSchema() const;

        AZ::u32 m_schemaVersion = 1;
        SourceRecord m_source;
        AZStd::vector<ImportIssue> m_issues;
    };

    struct EvidenceDocument
    {
        AZ_TYPE_INFO(EvidenceDocument, "{379F425B-1B32-449F-A045-9832EEAD4AE8}");

        static void Reflect(AZ::ReflectContext* context);
        bool UsesSupportedSchema() const;

        AZ::u32 m_schemaVersion = 1;
        AZStd::string m_sourceId;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::vector<EvidenceRecord> m_evidence;
        AZStd::vector<ImportIssue> m_issues;
    };

    struct SourceImportRequest
    {
        AZStd::string m_inputPath;
        AZStd::string m_sourceKind;
        AZStd::string m_title;
        AZStd::string m_toolName;
        AZStd::string m_toolVersion;
        AZStd::string m_capturedAt;
        AZStd::string m_limitations;
        AZStd::string m_preferredImporterId;
    };

    struct SourceImportResult
    {
        SourceDocument m_sourceDocument;
        EvidenceDocument m_evidenceDocument;

        bool HasErrors() const;
    };

    struct CatalogRecord
    {
        AZ_TYPE_INFO(CatalogRecord, "{F5A2CD52-8D57-42F4-B34D-13F1A48E64E1}");

        static void Reflect(AZ::ReflectContext* context);
        bool IsSynthetic() const;
        bool IsBlocked() const;

        AZStd::string m_recordId;
        AZStd::string m_ownerPackId;
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_nativeRefExact;
        AZStd::string m_identityKind;
        AZStd::string m_displayName;
        AZStd::vector<AZStd::string> m_aliases;
        AZStd::vector<AZStd::string> m_sourceScopedRefs;
        AZStd::string m_researchStage;
        AZStd::string m_confidence;
        AZStd::string m_operationalRisk;
        AZStd::string m_validationState;
        AZStd::string m_stalenessState;
        AZStd::vector<AZStd::string> m_allowedUsages;
        AZStd::vector<AZStd::string> m_forbiddenUsages;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_missingRefs;
        AZStd::vector<AZStd::string> m_conflictRefs;
        AZStd::vector<AZStd::string> m_tags;
        AZStd::string m_createdAt;
        AZStd::string m_updatedAt;
        AZStd::string m_supersededByRecordId;
    };

    struct CatalogRelationship
    {
        AZ_TYPE_INFO(CatalogRelationship, "{B4E770F5-CE9D-4F03-9653-BF10EBE27275}");

        static void Reflect(AZ::ReflectContext* context);
        bool IsBlocked() const;

        AZStd::string m_relationshipId;
        AZStd::string m_fromRecordId;
        AZStd::string m_toRecordId;
        AZStd::string m_targetSubjectRef;
        AZStd::string m_relationshipKind;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_researchStage;
        AZStd::string m_confidence;
        AZStd::string m_operationalRisk;
        AZStd::string m_validationState;
        AZStd::string m_stalenessState;
        AZStd::vector<AZStd::string> m_allowedUsages;
        AZStd::vector<AZStd::string> m_forbiddenUsages;
        AZStd::vector<AZStd::string> m_missingRefs;
        AZStd::vector<AZStd::string> m_conflictRefs;
        AZStd::vector<AZStd::string> m_attributes;
        AZStd::string m_createdAt;
        AZStd::string m_updatedAt;
        AZStd::string m_supersededByRelationshipId;
    };

    struct CatalogValidationEvent
    {
        AZ_TYPE_INFO(CatalogValidationEvent, "{5D896787-D118-4702-B5E3-F02BFA7D3E61}");

        static void Reflect(AZ::ReflectContext* context);
        AZStd::string GetSubjectKind() const;
        AZStd::string GetSubjectId() const;

        AZStd::string m_validationId;
        AZStd::string m_subjectKind;
        AZStd::string m_subjectId;
        AZStd::string m_recordId; // Legacy schema-1 record binding.
        AZStd::string m_state;
        AZStd::string m_method;
        AZStd::string m_validator;
        AZStd::string m_checkedAt;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_notes;
    };

    struct CatalogGovernanceEvent
    {
        AZ_TYPE_INFO(CatalogGovernanceEvent, "{7E46D7D8-1D57-4F39-AC1C-6D9F2CB8A3C1}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_eventId;
        AZStd::string m_subjectKind;
        AZStd::string m_subjectId;
        AZStd::string m_axis;
        AZStd::string m_previousValue;
        AZStd::string m_newValue;
        AZStd::string m_usage;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_validationIds;
        AZStd::string m_reviewer;
        AZStd::string m_decidedAt;
        AZStd::string m_notes;
    };

    struct CatalogDocument
    {
        AZ_TYPE_INFO(CatalogDocument, "{96D425E4-F3CB-4107-8A43-C0EED4A2DFEF}");

        static void Reflect(AZ::ReflectContext* context);
        bool UsesSupportedSchema() const;

        AZ::u32 m_schemaVersion = 1;
        AZStd::string m_workspaceId;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::vector<CatalogRecord> m_records;
        AZStd::vector<CatalogRelationship> m_relationships;
        AZStd::vector<CatalogValidationEvent> m_validationHistory;
        AZStd::vector<CatalogGovernanceEvent> m_governanceHistory;
    };

    struct CatalogPromotionRequest
    {
        AZStd::string m_evidenceId;
        AZStd::string m_recordId;
        AZStd::string m_ownerPackId;
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_nativeRefExact;
        AZStd::string m_identityKind;
        AZStd::string m_displayName;
        AZStd::vector<AZStd::string> m_aliases;
        AZStd::string m_researchStage;
        AZStd::string m_confidence;
        AZStd::string m_operationalRisk;
        AZStd::vector<AZStd::string> m_allowedUsages;
        AZStd::vector<AZStd::string> m_forbiddenUsages;
        AZStd::vector<AZStd::string> m_tags;
    };

    struct CatalogGovernanceRequest
    {
        AZStd::string m_subjectKind;
        AZStd::string m_subjectId;
        AZStd::string m_axis;
        AZStd::string m_value;
        AZStd::string m_usage;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_validationIds;
        AZStd::string m_reviewer;
        AZStd::string m_notes;
    };

    struct CatalogValidationRequest
    {
        AZStd::string m_subjectKind;
        AZStd::string m_subjectId;
        AZStd::string m_state;
        AZStd::string m_method;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_validator;
        AZStd::string m_notes;
    };

    struct CatalogQuery
    {
        AZStd::string m_searchText;
        AZStd::string m_recordId;
        AZStd::string m_ownerPackId;
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_nativeRefExact;
        AZStd::string m_identityKind;
        AZStd::string m_researchStage;
        AZStd::string m_confidence;
        AZStd::string m_operationalRisk;
        AZStd::string m_validationState;
        AZStd::string m_stalenessState;
        AZStd::string m_permission;
        AZStd::string m_evidenceId;
        bool m_blockedOnly = false;
        bool m_includeSuperseded = false;
    };

    struct BlockerRecord
    {
        AZ_TYPE_INFO(BlockerRecord, "{56C682A0-72FD-4B64-B60A-0131EC2636F1}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_blockerId;
        AZStd::string m_severity;
        AZStd::string m_area;
        AZStd::string m_subjectRef;
        AZStd::string m_reason;
        AZStd::string m_status;
        AZStd::vector<AZStd::string> m_affectedUsages;
    };

    struct DomainCoverage
    {
        AZ_TYPE_INFO(DomainCoverage, "{4EA9534A-2E58-4B98-8480-54D6369C5135}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_domain;
        AZ::u64 m_recordCount = 0;
        AZ::u64 m_blockedRecordCount = 0;
    };

    struct FoundationSnapshot
    {
        AZ_TYPE_INFO(FoundationSnapshot, "{59D13AF8-2339-41CD-9F74-15D19353510A}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_workspaceName;
        AZStd::string m_workspaceFilePath;
        AZStd::string m_activeGameProfile;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_unityVersion;
        AZStd::string m_bepInExVersion;
        AZStd::string m_activePackId;
        AZStd::string m_activePackName;
        AZStd::string m_activePackVersion;
        AZStd::string m_activePackFilePath;
        AZ::u64 m_gameProfileCount = 0;
        AZ::u64 m_packCount = 0;
        AZ::u64 m_sourceCount = 0;
        AZ::u64 m_evidenceCount = 0;
        AZ::u64 m_importErrorCount = 0;
        AZ::u64 m_importWarningCount = 0;
        AZ::u64 m_catalogRecordCount = 0;
        AZ::u64 m_catalogRelationshipCount = 0;
        AZ::u64 m_catalogValidationCount = 0;
        AZ::u64 m_catalogGovernanceCount = 0;
        AZ::u64 m_staleCatalogSubjectCount = 0;
        AZ::u64 m_allowedUsageCount = 0;
        AZ::u64 m_forbiddenUsageCount = 0;
        AZ::u64 m_openBlockerCount = 0;
        AZStd::vector<DomainCoverage> m_domainCoverage;
        AZStd::vector<ImportIssue> m_importIssues;
        AZStd::vector<BlockerRecord> m_blockers;
    };
} // namespace TaintedGrailModdingSDK
