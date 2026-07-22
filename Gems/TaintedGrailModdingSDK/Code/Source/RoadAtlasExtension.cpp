/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "RoadAtlasExtension.h"

#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/conversions.h>

#include <cmath>
#include <cstdio>

namespace TaintedGrailModdingSDK::RoadAtlasExtension
{
    namespace
    {
        constexpr size_t MaximumElementCount = 16384;
        constexpr size_t MaximumGeometryPointCount = 65536;

        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
        {
            if ((!allowEmpty && value.empty()) || value.size() > maximumLength)
            {
                return false;
            }
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte < 0x20 || byte == 0x7f)
                {
                    return false;
                }
            }
            return true;
        }

        void AddIssue(
            ValidationResult& result,
            const AZStd::string& elementId,
            AZStd::string code,
            AZStd::string message)
        {
            result.m_issues.push_back(
                ValidationIssue{ elementId, AZStd::move(code), AZStd::move(message) });
        }

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool HasDuplicates(AZStd::vector<AZStd::string> values)
        {
            AZStd::sort(values.begin(), values.end());
            return AZStd::adjacent_find(values.begin(), values.end()) != values.end();
        }

        AZStd::string PrimaryRef(const Element& element)
        {
            switch (element.m_kind)
            {
            case ElementKind::Road:
                return element.m_roadRef;
            case ElementKind::Name:
                return element.m_nameRef;
            case ElementKind::Junction:
                return element.m_junctionRef;
            case ElementKind::Segment:
                return element.m_segmentRef;
            case ElementKind::Anchor:
                return element.m_anchorRef;
            case ElementKind::Connector:
                return element.m_connectorRef;
            default:
                return {};
            }
        }

        bool RequirementsSatisfied(const Element& element)
        {
            for (const EvidenceRequirement& requirement : element.m_evidenceRequirements)
            {
                if (requirement.m_required
                    && (!requirement.m_satisfied || requirement.m_evidenceIds.empty()))
                {
                    return false;
                }
            }
            return true;
        }

        KnowledgePack BuildKnowledgePack()
        {
            KnowledgePack pack;
            pack.m_extensionId = "extension.avalon-core-road-atlas";
            pack.m_version = "0.8.4";
            pack.m_upstreamRepository =
                "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods";
            pack.m_upstreamCommit =
                "d7e740e7f167b73152b53409e483dab07d80d048";
            pack.m_licenseExpression = "NOASSERTION";
            pack.m_sources = {
                { "mods/avalon-core/src/World/RoadAtlasSchemaContracts.cs",
                  "dbb26b698f063e92da6f7cf76a5eea9b930e85ec" },
                { "mods/avalon-core/src/World/RoadAtlasInventoryNameContracts.cs",
                  "060af3cd9885c58d29c3d66221c824813be8673f" },
                { "mods/avalon-core/src/World/RoadAtlasExactGeometryContracts.cs",
                  "db752d4fd1131a7c6c6a159f6cd91a10a604c471" },
                { "mods/avalon-core/src/World/RoadAtlasConnectivityContracts.cs",
                  "b6629505296a6e3deb575110d67891d84c3167dc" },
                { "mods/avalon-core/src/World/RoadAtlasAnchorDestinationContracts.cs",
                  "a900c9f6906cd5bc01af2e24a2fb1baea007985c" },
                { "mods/avalon-core/src/World/RoadAtlasStaticRoadRuleContracts.cs",
                  "76e42bf18515e4009f61a736131deca0672562c1" },
                { "mods/avalon-core/src/World/RoadAtlasSnapshotContracts.cs",
                  "8d23ac6f099da1dda4481ca618d7e7f2a1590141" },
            };
            pack.m_gateOrder = {
                "inventory-and-names",
                "exact-geometry",
                "junction-connectivity",
                "anchors-and-destinations",
                "static-road-rules",
                "supported-region-coverage",
            };
            pack.m_knownLimitations = {
                "The upstream repository has no root licence declaration.",
                "Reviewed upstream road payloads are not copied by this contract port.",
                "Planning readiness is evidence-bound and does not permit runtime movement.",
                "Scene mutation, spawning, path execution, and game calls remain adapter-only.",
            };
            return pack;
        }

        void AppendString(AZStd::string& output, const AZStd::string& value)
        {
            output += AZStd::to_string(value.size());
            output.push_back(':');
            output += value;
            output.push_back('|');
        }

        void AppendDouble(AZStd::string& output, double value)
        {
            char buffer[64] = {};
            const int count = std::snprintf(buffer, sizeof(buffer), "%.17g", value);
            if (count > 0)
            {
                output.append(buffer, static_cast<size_t>(count));
            }
            output.push_back('|');
        }
    } // namespace

    const KnowledgePack& GetCanonicalKnowledgePack()
    {
        static const KnowledgePack pack = BuildKnowledgePack();
        return pack;
    }

    ValidationResult ValidateSnapshot(const Snapshot& snapshot, const GameProfile& profile)
    {
        ValidationResult result;
        if (snapshot.m_schemaVersion != 1
            || !IsStableContractId(snapshot.m_snapshotId)
            || !profile.IsConfigured()
            || snapshot.m_profileId != profile.m_profileId
            || snapshot.m_gameVersion != profile.m_gameVersion
            || snapshot.m_branch != profile.m_branch
            || snapshot.m_runtimeTarget != profile.m_runtimeTarget
            || !IsSha256Fingerprint(snapshot.m_sourceFingerprint)
            || !IsStrictUtcTimestamp(snapshot.m_capturedAtUtc)
            || !snapshot.m_planningOnly
            || snapshot.m_runtimeMutationAllowed)
        {
            AddIssue(
                result, snapshot.m_snapshotId, "snapshot.invalid",
                "Road Atlas snapshot must be schema 1, exact-profile bound, planning-only, and inert.");
            return result;
        }
        if (snapshot.m_elements.empty()
            || snapshot.m_elements.size() > MaximumElementCount)
        {
            AddIssue(
                result, snapshot.m_snapshotId, "snapshot.element-count",
                "Road Atlas snapshot element count is empty or exceeds the contract bound.");
            return result;
        }

        AZStd::vector<AZStd::string> elementIds;
        AZStd::vector<AZStd::string> segmentRefs;
        size_t totalGeometryPoints = 0;
        for (const Element& element : snapshot.m_elements)
        {
            elementIds.push_back(element.m_elementId);
            if (element.m_kind == ElementKind::Segment && !element.m_segmentRef.empty())
            {
                segmentRefs.push_back(element.m_segmentRef);
            }
        }
        if (HasDuplicates(elementIds))
        {
            AddIssue(
                result, snapshot.m_snapshotId, "element.duplicate-id",
                "Road Atlas element identities must be unique.");
        }
        if (HasDuplicates(segmentRefs))
        {
            AddIssue(
                result, snapshot.m_snapshotId, "segment.duplicate-ref",
                "Road Atlas segment references must be unique.");
        }

        for (const Element& element : snapshot.m_elements)
        {
            switch (element.m_kind)
            {
            case ElementKind::Road:
                ++result.m_roadCount;
                break;
            case ElementKind::Name:
                ++result.m_nameCount;
                break;
            case ElementKind::Segment:
                ++result.m_segmentCount;
                break;
            case ElementKind::Junction:
                ++result.m_junctionCount;
                break;
            case ElementKind::Anchor:
                ++result.m_anchorCount;
                break;
            case ElementKind::Connector:
                ++result.m_connectorCount;
                break;
            default:
                break;
            }

            if (!IsStableContractId(element.m_elementId)
                || !IsStableContractId(element.m_ownerPackId)
                || element.m_kind == ElementKind::Unknown
                || element.m_promotionState == PromotionState::Unknown
                || !IsBoundedText(element.m_displayName, 512)
                || !IsBoundedText(element.m_regionRef, 512)
                || !IsBoundedText(element.m_sceneRef, 512)
                || !IsBoundedText(PrimaryRef(element), 1024)
                || element.m_runtimeMutationAllowed)
            {
                AddIssue(
                    result, element.m_elementId, "element.invalid",
                    "Road Atlas element identity, primary reference, scope, or authority is invalid.");
            }
            if (HasDuplicates(element.m_connectedSegmentRefs)
                || HasDuplicates(element.m_tags))
            {
                AddIssue(
                    result, element.m_elementId, "element.duplicate-values",
                    "Road Atlas connected refs and tags must be unique.");
            }
            for (const AZStd::string& segmentRef : element.m_connectedSegmentRefs)
            {
                if (!Contains(segmentRefs, segmentRef))
                {
                    AddIssue(
                        result, element.m_elementId, "element.missing-segment",
                        "Road Atlas connectivity references an unknown segment.");
                }
            }
            if (!element.m_fromElementRef.empty()
                && !Contains(elementIds, element.m_fromElementRef))
            {
                AddIssue(
                    result, element.m_elementId, "element.missing-from",
                    "Road Atlas from-element reference is unresolved.");
            }
            if (!element.m_toElementRef.empty()
                && !Contains(elementIds, element.m_toElementRef))
            {
                AddIssue(
                    result, element.m_elementId, "element.missing-to",
                    "Road Atlas to-element reference is unresolved.");
            }
            if (element.m_kind == ElementKind::Segment
                && element.m_promotionState == PromotionState::ApprovedForPlanning
                && element.m_geometry.size() < 2)
            {
                AddIssue(
                    result, element.m_elementId, "segment.geometry-missing",
                    "Planning-approved segments require at least two exact geometry points.");
            }
            totalGeometryPoints += element.m_geometry.size();
            for (const Coordinate& point : element.m_geometry)
            {
                if (!std::isfinite(point.m_x) || !std::isfinite(point.m_y)
                    || !std::isfinite(point.m_z)
                    || !IsBoundedText(point.m_pointRef, 512))
                {
                    AddIssue(
                        result, element.m_elementId, "segment.geometry-invalid",
                        "Road Atlas geometry points require finite coordinates and stable source refs.");
                }
            }

            AZStd::vector<AZStd::string> requirementIds;
            for (const EvidenceRequirement& requirement : element.m_evidenceRequirements)
            {
                requirementIds.push_back(requirement.m_requirementId);
                if (!IsStableContractId(requirement.m_requirementId)
                    || requirement.m_kind == EvidenceRequirementKind::Unknown
                    || (requirement.m_satisfied && requirement.m_evidenceIds.empty())
                    || HasDuplicates(requirement.m_evidenceIds)
                    || AZStd::any_of(
                        requirement.m_evidenceIds.begin(),
                        requirement.m_evidenceIds.end(),
                        [](const AZStd::string& evidenceId)
                        {
                            return !IsStableContractId(evidenceId);
                        }))
                {
                    AddIssue(
                        result, element.m_elementId, "evidence.invalid",
                        "Road Atlas evidence requirements are incomplete or duplicated.");
                }
            }
            if (HasDuplicates(requirementIds)
                || (element.m_promotionState == PromotionState::ApprovedForPlanning
                    && !RequirementsSatisfied(element)))
            {
                AddIssue(
                    result, element.m_elementId, "evidence.requirements-unsatisfied",
                    "Planning-approved elements require unique, satisfied evidence requirements.");
            }
        }
        if (totalGeometryPoints > MaximumGeometryPointCount)
        {
            AddIssue(
                result, snapshot.m_snapshotId, "snapshot.geometry-count",
                "Road Atlas geometry point count exceeds the contract bound.");
        }

        result.m_accepted = result.m_issues.empty();
        result.m_canUseForPlanning = result.m_accepted;
        if (result.m_accepted)
        {
            result.m_canonicalFingerprint = CalculateSnapshotFingerprint(snapshot);
        }
        return result;
    }

    AZStd::string BuildCanonicalSnapshot(const Snapshot& snapshot)
    {
        AZStd::string output = "road-atlas-snapshot-v1|";
        output += AZStd::to_string(snapshot.m_schemaVersion);
        output.push_back('|');
        AppendString(output, snapshot.m_snapshotId);
        AppendString(output, snapshot.m_profileId);
        AppendString(output, snapshot.m_gameVersion);
        AppendString(output, snapshot.m_branch);
        AppendString(output, snapshot.m_runtimeTarget);
        AppendString(output, snapshot.m_sourceFingerprint);
        AppendString(output, snapshot.m_capturedAtUtc);

        AZStd::vector<const Element*> elements;
        for (const Element& element : snapshot.m_elements)
        {
            elements.push_back(&element);
        }
        AZStd::sort(
            elements.begin(), elements.end(),
            [](const Element* left, const Element* right)
            {
                return left->m_elementId < right->m_elementId;
            });
        output += AZStd::to_string(elements.size());
        output.push_back('|');
        for (const Element* element : elements)
        {
            AppendString(output, element->m_elementId);
            AppendString(output, element->m_ownerPackId);
            output += AZStd::to_string(static_cast<int>(element->m_kind));
            output.push_back('|');
            output += AZStd::to_string(static_cast<int>(element->m_promotionState));
            output.push_back('|');
            AppendString(output, element->m_displayName);
            AppendString(output, element->m_regionRef);
            AppendString(output, element->m_sceneRef);
            AppendString(output, element->m_roadRef);
            AppendString(output, element->m_nameRef);
            AppendString(output, element->m_segmentRef);
            AppendString(output, element->m_junctionRef);
            AppendString(output, element->m_anchorRef);
            AppendString(output, element->m_connectorRef);
            AppendString(output, element->m_fromElementRef);
            AppendString(output, element->m_toElementRef);
            AppendString(output, element->m_worldNodeRef);
            AppendString(output, element->m_coordinateRef);
            AZStd::vector<AZStd::string> connected = element->m_connectedSegmentRefs;
            AZStd::sort(connected.begin(), connected.end());
            for (const AZStd::string& segmentRef : connected)
            {
                AppendString(output, segmentRef);
            }
            output += AZStd::to_string(element->m_geometry.size());
            output.push_back('|');
            for (const Coordinate& point : element->m_geometry)
            {
                AppendDouble(output, point.m_x);
                AppendDouble(output, point.m_y);
                AppendDouble(output, point.m_z);
                AppendString(output, point.m_pointRef);
            }
            AZStd::vector<const EvidenceRequirement*> requirements;
            for (const EvidenceRequirement& requirement : element->m_evidenceRequirements)
            {
                requirements.push_back(&requirement);
            }
            AZStd::sort(
                requirements.begin(), requirements.end(),
                [](const EvidenceRequirement* left, const EvidenceRequirement* right)
                {
                    return left->m_requirementId < right->m_requirementId;
                });
            for (const EvidenceRequirement* requirement : requirements)
            {
                AppendString(output, requirement->m_requirementId);
                output += AZStd::to_string(static_cast<int>(requirement->m_kind));
                output.push_back('|');
                output += requirement->m_required ? "required|" : "optional|";
                output += requirement->m_satisfied ? "satisfied|" : "unsatisfied|";
                AZStd::vector<AZStd::string> evidenceIds = requirement->m_evidenceIds;
                AZStd::sort(evidenceIds.begin(), evidenceIds.end());
                for (const AZStd::string& evidenceId : evidenceIds)
                {
                    AppendString(output, evidenceId);
                }
                AppendString(output, requirement->m_notes);
            }
            AZStd::vector<AZStd::string> tags = element->m_tags;
            AZStd::sort(tags.begin(), tags.end());
            for (const AZStd::string& tag : tags)
            {
                AppendString(output, tag);
            }
            AppendString(output, element->m_notes);
            output += element->m_runtimeMutationAllowed ? "element-runtime|" : "element-inert|";
        }
        output += snapshot.m_planningOnly ? "planning|" : "not-planning|";
        output += snapshot.m_runtimeMutationAllowed ? "runtime|" : "inert|";
        return output;
    }

    AZStd::string CalculateSnapshotFingerprint(const Snapshot& snapshot)
    {
        return CalculateCanonicalSha256(BuildCanonicalSnapshot(snapshot));
    }
} // namespace TaintedGrailModdingSDK::RoadAtlasExtension
