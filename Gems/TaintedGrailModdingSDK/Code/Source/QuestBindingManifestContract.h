/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK
{
    struct QuestDefinitionV1;

    inline constexpr AZ::u32 QuestBindingManifestSchemaVersionV1 = 1;
    inline constexpr const char* QuestBindingManifestSchemaIdV1 = "foa.quest-binding-manifest";

    enum class QuestBindingManifestIssueSeverityV1 : AZ::u8
    {
        Error,
        Blocker,
    };

    struct QuestBindingManifestIssueV1
    {
        QuestBindingManifestIssueSeverityV1 m_severity =
            QuestBindingManifestIssueSeverityV1::Error;
        AZStd::string m_code;
        AZStd::string m_subjectId;
        AZStd::string m_propertyPath;
    };

    struct QuestBindingManifestValidationResultV1
    {
        AZStd::vector<QuestBindingManifestIssueV1> m_issues;

        bool IsValid() const;
        bool IsBlocked() const;
    };

    struct QuestBindingManifestCatalogReferenceV1
    {
        AZStd::string m_catalogRecordId;
        AZStd::string m_domain;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_catalogFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
    };

    struct QuestBindingManifestEvidenceReferenceV1
    {
        AZStd::string m_evidenceId;
        AZStd::string m_sourceId;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
    };

    struct QuestBindingManifestPermissionReferenceV1
    {
        AZStd::string m_permissionId;
        AZStd::string m_subjectKind;
        AZStd::string m_subjectId;
        AZStd::string m_usage;
        AZStd::string m_decision;
        AZStd::string m_validationId;
    };

    struct QuestBindingManifestBindingV1
    {
        AZStd::string m_bindingId;
        AZStd::string m_bindingKind;
        AZStd::string m_requirementId;
        AZStd::string m_roleId;
        AZStd::string m_subjectKind;
        AZStd::string m_usage;
        QuestBindingManifestCatalogReferenceV1 m_catalogRef;
        AZStd::vector<QuestBindingManifestEvidenceReferenceV1> m_evidenceRefs;
        AZStd::vector<QuestBindingManifestPermissionReferenceV1> m_permissionRefs;
        AZStd::string m_fallbackPolicy;
        bool m_unique = true;
    };

    struct QuestBindingManifestAuthorityV1
    {
        bool m_runtimeExecutionAllowed = false;
        bool m_editorMutationAllowed = false;
        bool m_saveMutationAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_catalogMutationAllowed = false;
        bool m_evidencePromotionAllowed = false;
        bool m_permissionGrantAllowed = false;
        bool m_assetExtractionAllowed = false;
    };

    struct QuestBindingManifestV1
    {
        AZStd::string m_schema = QuestBindingManifestSchemaIdV1;
        AZ::u32 m_schemaVersion = QuestBindingManifestSchemaVersionV1;
        AZStd::string m_manifestId;
        AZStd::string m_questId;
        AZStd::string m_contentVersion;
        AZStd::string m_ownerPackId;
        AZStd::string m_ownerModuleId;
        AZStd::string m_questDefinitionFingerprint;
        AZStd::string m_catalogId;
        AZStd::string m_catalogFingerprint;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::vector<QuestBindingManifestBindingV1> m_roleBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_locationBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_anchorBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_itemBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_rewardBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_dialogueBindings;
        AZStd::vector<QuestBindingManifestBindingV1> m_journalBindings;
        AZStd::string m_minimumSdkVersion;
        AZStd::vector<AZStd::string> m_compatibilityTags;
        QuestBindingManifestAuthorityV1 m_authority;
        AZStd::string m_manifestFingerprint;
    };

    bool IsQuestBindingManifestStableIdV1(const AZStd::string& value);

    QuestBindingManifestValidationResultV1 ValidateQuestBindingManifestV1(
        const QuestBindingManifestV1& manifest,
        const QuestDefinitionV1* definition = nullptr);

    QuestBindingManifestValidationResultV1 ParseQuestBindingManifestJsonV1(
        AZStd::string_view json,
        QuestBindingManifestV1& manifest,
        const QuestDefinitionV1* definition = nullptr);

    AZStd::string SerializeCanonicalQuestBindingManifestV1(
        const QuestBindingManifestV1& manifest);

    AZStd::string CalculateQuestBindingManifestFingerprintV1(
        const QuestBindingManifestV1& manifest);

    bool QuestBindingManifestFingerprintMatchesV1(
        const QuestBindingManifestV1& manifest);
} // namespace TaintedGrailModdingSDK
