/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationModels.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <cctype>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsSemVerNumericPart(AZStd::string_view value)
        {
            if (value.empty())
            {
                return false;
            }
            if (value.size() > 1 && value.front() == '0')
            {
                return false;
            }
            for (char character : value)
            {
                if (!std::isdigit(static_cast<unsigned char>(character)))
                {
                    return false;
                }
            }
            return true;
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
        return !m_profileId.empty() && !m_installPath.empty() && !m_gameVersion.empty() && !m_branch.empty()
            && !m_runtimeTarget.empty() && !m_managedAssembliesPath.empty();
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
                ->Version(2)
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
        return HasValidPackId() && !m_ownerId.empty() && HasValidSemanticVersion();
    }

    bool PackManifest::HasValidPackId() const
    {
        if (m_packId.empty() || m_packId.front() == '.' || m_packId.back() == '.' || m_packId.find('.') == AZStd::string::npos)
        {
            return false;
        }

        char previous = '\0';
        for (char character : m_packId)
        {
            const bool valid = (character >= 'a' && character <= 'z') || std::isdigit(static_cast<unsigned char>(character))
                || character == '.' || character == '-' || character == '_';
            if (!valid || (character == '.' && previous == '.'))
            {
                return false;
            }
            previous = character;
        }
        return true;
    }

    bool PackManifest::HasValidSemanticVersion() const
    {
        if (m_version.empty())
        {
            return false;
        }

        const size_t suffixPosition = m_version.find_first_of("-+");
        const AZStd::string_view core(m_version.data(), suffixPosition == AZStd::string::npos ? m_version.size() : suffixPosition);
        const size_t firstDot = core.find('.');
        const size_t secondDot = firstDot == AZStd::string_view::npos ? AZStd::string_view::npos : core.find('.', firstDot + 1);
        if (firstDot == AZStd::string_view::npos || secondDot == AZStd::string_view::npos || core.find('.', secondDot + 1) != AZStd::string_view::npos)
        {
            return false;
        }

        return IsSemVerNumericPart(core.substr(0, firstDot))
            && IsSemVerNumericPart(core.substr(firstDot + 1, secondDot - firstDot - 1))
            && IsSemVerNumericPart(core.substr(secondDot + 1));
    }

    void SourceRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SourceRecord>()
                ->Version(1)
                ->Field("SourceId", &SourceRecord::m_sourceId)
                ->Field("Title", &SourceRecord::m_title)
                ->Field("SourceKind", &SourceRecord::m_sourceKind)
                ->Field("Locator", &SourceRecord::m_locator)
                ->Field("Fingerprint", &SourceRecord::m_fingerprint)
                ->Field("GameVersion", &SourceRecord::m_gameVersion)
                ->Field("Branch", &SourceRecord::m_branch)
                ->Field("ToolName", &SourceRecord::m_toolName)
                ->Field("ToolVersion", &SourceRecord::m_toolVersion)
                ->Field("CapturedAt", &SourceRecord::m_capturedAt)
                ->Field("Limitations", &SourceRecord::m_limitations);
        }
    }

    void EvidenceRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EvidenceRecord>()
                ->Version(1)
                ->Field("EvidenceId", &EvidenceRecord::m_evidenceId)
                ->Field("SourceId", &EvidenceRecord::m_sourceId)
                ->Field("SubjectRef", &EvidenceRecord::m_subjectRef)
                ->Field("Claim", &EvidenceRecord::m_claim)
                ->Field("EvidenceKind", &EvidenceRecord::m_evidenceKind)
                ->Field("Confidence", &EvidenceRecord::m_confidence)
                ->Field("Locator", &EvidenceRecord::m_locator);
        }
    }

    void CatalogRecord::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CatalogRecord>()
                ->Version(1)
                ->Field("RecordId", &CatalogRecord::m_recordId)
                ->Field("Domain", &CatalogRecord::m_domain)
                ->Field("RecordKind", &CatalogRecord::m_recordKind)
                ->Field("SubjectRef", &CatalogRecord::m_subjectRef)
                ->Field("NativeRefExact", &CatalogRecord::m_nativeRefExact)
                ->Field("IdentityKind", &CatalogRecord::m_identityKind)
                ->Field("DisplayName", &CatalogRecord::m_displayName)
                ->Field("ResearchStage", &CatalogRecord::m_researchStage)
                ->Field("ValidationState", &CatalogRecord::m_validationState)
                ->Field("AllowedUsages", &CatalogRecord::m_allowedUsages)
                ->Field("ForbiddenUsages", &CatalogRecord::m_forbiddenUsages)
                ->Field("EvidenceIds", &CatalogRecord::m_evidenceIds)
                ->Field("MissingRefs", &CatalogRecord::m_missingRefs)
                ->Field("Tags", &CatalogRecord::m_tags);
        }
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
                ->Version(2)
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
                ->Field("CatalogRecordCount", &FoundationSnapshot::m_catalogRecordCount)
                ->Field("OpenBlockerCount", &FoundationSnapshot::m_openBlockerCount)
                ->Field("DomainCoverage", &FoundationSnapshot::m_domainCoverage)
                ->Field("Blockers", &FoundationSnapshot::m_blockers);
        }
    }
} // namespace TaintedGrailModdingSDK
