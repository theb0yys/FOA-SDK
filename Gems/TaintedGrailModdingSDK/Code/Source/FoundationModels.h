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
        bool HasValidPackId() const;
        bool HasValidSemanticVersion() const;

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

    struct SourceRecord
    {
        AZ_TYPE_INFO(SourceRecord, "{2569A76D-E028-4FF4-8619-265B354813A4}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_sourceId;
        AZStd::string m_title;
        AZStd::string m_sourceKind;
        AZStd::string m_locator;
        AZStd::string m_fingerprint;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_toolName;
        AZStd::string m_toolVersion;
        AZStd::string m_capturedAt;
        AZStd::string m_limitations;
    };

    struct EvidenceRecord
    {
        AZ_TYPE_INFO(EvidenceRecord, "{850C10EB-36D8-4383-AC42-F598D587554A}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_evidenceId;
        AZStd::string m_sourceId;
        AZStd::string m_subjectRef;
        AZStd::string m_claim;
        AZStd::string m_evidenceKind;
        AZStd::string m_confidence;
        AZStd::string m_locator;
    };

    struct CatalogRecord
    {
        AZ_TYPE_INFO(CatalogRecord, "{F5A2CD52-8D57-42F4-B34D-13F1A48E64E1}");

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_recordId;
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_nativeRefExact;
        AZStd::string m_identityKind;
        AZStd::string m_displayName;
        AZStd::string m_researchStage;
        AZStd::string m_validationState;
        AZStd::vector<AZStd::string> m_allowedUsages;
        AZStd::vector<AZStd::string> m_forbiddenUsages;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_missingRefs;
        AZStd::vector<AZStd::string> m_tags;
    };

    struct CatalogQuery
    {
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_nativeRefExact;
        bool m_blockedOnly = false;
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
        AZ::u64 m_catalogRecordCount = 0;
        AZ::u64 m_openBlockerCount = 0;
        AZStd::vector<DomainCoverage> m_domainCoverage;
        AZStd::vector<BlockerRecord> m_blockers;
    };
} // namespace TaintedGrailModdingSDK
