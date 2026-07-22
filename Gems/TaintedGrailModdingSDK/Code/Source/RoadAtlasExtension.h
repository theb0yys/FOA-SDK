/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::RoadAtlasExtension
{
    enum class ElementKind
    {
        Unknown,
        Road,
        Name,
        Junction,
        Segment,
        Anchor,
        Connector,
    };

    enum class PromotionState
    {
        Unknown,
        Raw,
        Candidate,
        Mapped,
        Linked,
        Confirmed,
        ApprovedForPlanning,
        Blocked,
    };

    enum class EvidenceRequirementKind
    {
        Unknown,
        NameAuthority,
        SourceRegister,
        RuntimeCoordinate,
        SceneReference,
        SegmentGeometry,
        JunctionConnectivity,
        AnchorVisibility,
        ConnectorEndpoint,
        ReviewerApproval,
    };

    struct SourceBinding
    {
        AZStd::string m_path;
        AZStd::string m_gitBlobSha1;
    };

    struct KnowledgePack
    {
        AZStd::string m_extensionId;
        AZStd::string m_version;
        AZStd::string m_upstreamRepository;
        AZStd::string m_upstreamCommit;
        AZStd::string m_licenseExpression;
        AZStd::vector<SourceBinding> m_sources;
        AZStd::vector<AZStd::string> m_gateOrder;
        AZStd::vector<AZStd::string> m_knownLimitations;
    };

    struct Coordinate
    {
        double m_x = 0.0;
        double m_y = 0.0;
        double m_z = 0.0;
        AZStd::string m_pointRef;
    };

    struct EvidenceRequirement
    {
        AZStd::string m_requirementId;
        EvidenceRequirementKind m_kind = EvidenceRequirementKind::Unknown;
        bool m_required = false;
        bool m_satisfied = false;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_notes;
    };

    struct Element
    {
        AZStd::string m_elementId;
        AZStd::string m_ownerPackId;
        ElementKind m_kind = ElementKind::Unknown;
        PromotionState m_promotionState = PromotionState::Unknown;
        AZStd::string m_displayName;
        AZStd::string m_regionRef;
        AZStd::string m_sceneRef;
        AZStd::string m_roadRef;
        AZStd::string m_nameRef;
        AZStd::string m_segmentRef;
        AZStd::string m_junctionRef;
        AZStd::string m_anchorRef;
        AZStd::string m_connectorRef;
        AZStd::string m_fromElementRef;
        AZStd::string m_toElementRef;
        AZStd::string m_worldNodeRef;
        AZStd::string m_coordinateRef;
        AZStd::vector<AZStd::string> m_connectedSegmentRefs;
        AZStd::vector<Coordinate> m_geometry;
        AZStd::vector<EvidenceRequirement> m_evidenceRequirements;
        AZStd::vector<AZStd::string> m_tags;
        AZStd::string m_notes;
        bool m_runtimeMutationAllowed = false;
    };

    struct Snapshot
    {
        AZ::u32 m_schemaVersion = 1;
        AZStd::string m_snapshotId;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_sourceFingerprint;
        AZStd::string m_capturedAtUtc;
        AZStd::vector<Element> m_elements;
        bool m_planningOnly = true;
        bool m_runtimeMutationAllowed = false;
    };

    //! Sanitized exact-profile identity accepted from the public ExtensionAPI.
    struct ProfileBinding
    {
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
    };

    struct ValidationIssue
    {
        AZStd::string m_elementId;
        AZStd::string m_code;
        AZStd::string m_message;
    };

    struct ValidationResult
    {
        bool m_accepted = false;
        bool m_canUseForPlanning = false;
        AZ::u64 m_roadCount = 0;
        AZ::u64 m_nameCount = 0;
        AZ::u64 m_segmentCount = 0;
        AZ::u64 m_junctionCount = 0;
        AZ::u64 m_anchorCount = 0;
        AZ::u64 m_connectorCount = 0;
        AZStd::vector<ValidationIssue> m_issues;
        AZStd::string m_canonicalFingerprint;
        bool m_runtimeMutationAllowed = false;
    };

    const KnowledgePack& GetCanonicalKnowledgePack();

    ValidationResult ValidateSnapshot(
        const Snapshot& snapshot,
        const GameProfile& profile);

    ValidationResult ValidateSnapshot(
        const Snapshot& snapshot,
        const ProfileBinding& profile);

    AZStd::string BuildCanonicalSnapshot(const Snapshot& snapshot);
    AZStd::string CalculateSnapshotFingerprint(const Snapshot& snapshot);
} // namespace TaintedGrailModdingSDK::RoadAtlasExtension
