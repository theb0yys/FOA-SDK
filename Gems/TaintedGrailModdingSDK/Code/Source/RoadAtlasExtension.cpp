/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "RoadAtlasExtension.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cmath>

namespace TaintedGrailModdingSDK::RoadAtlasExtension
{
    namespace
    {
        constexpr size_t MaximumElementCount = 16384;
        constexpr size_t MaximumGeometryPointCount = 65536;
        constexpr size_t MaximumConnectedSegmentCount = 4096;
        constexpr size_t MaximumTagCount = 256;
        constexpr size_t MaximumEvidenceRequirementCount = 256;
        constexpr size_t MaximumEvidenceIdsPerRequirement = 256;

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

        bool IsValidElementKind(ElementKind kind)
        {
            switch (kind)
            {
            case ElementKind::Road:
            case ElementKind::Name:
            case ElementKind::Junction:
            case ElementKind::Segment:
            case ElementKind::Anchor:
            case ElementKind::Connector:
                return true;
            default:
                return false;
            }
        }

        bool IsValidPromotionState(PromotionState state)
        {
            switch (state)
            {
            case PromotionState::Raw:
            case PromotionState::Candidate:
            case PromotionState::Mapped:
            case PromotionState::Linked:
            case PromotionState::Confirmed:
            case PromotionState::ApprovedForPlanning:
            case PromotionState::Blocked:
                return true;
            default:
                return false;
            }
        }

        bool IsValidEvidenceRequirementKind(EvidenceRequirementKind kind)
        {
            switch (kind)
            {
            case EvidenceRequirementKind::NameAuthority:
            case EvidenceRequirementKind::SourceRegister:
            case EvidenceRequirementKind::RuntimeCoordinate:
            case EvidenceRequirementKind::SceneReference:
            case EvidenceRequirementKind::SegmentGeometry:
            case EvidenceRequirementKind::JunctionConnectivity:
            case EvidenceRequirementKind::AnchorVisibility:
            case EvidenceRequirementKind::ConnectorEndpoint:
            case EvidenceRequirementKind::ReviewerApproval:
                return true;
            default:
                return false;
            }
        }

        bool AreBoundedValues(
            const AZStd::vector<AZStd::string>& values,
            size_t maximumCount,
            size_t maximumLength)
        {
            return values.size() <= maximumCount
                && AZStd::all_of(
                    values.begin(), values.end(),
                    [maximumLength](const AZStd::string& value)
                    {
                        return IsBoundedText(value, maximumLength);
                    });
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
            if (element.m_evidenceRequirements.empty())
            {
                return false;
            }
            bool hasRequiredEvidence = false;
            for (const EvidenceRequirement& requirement : element.m_evidenceRequirements)
            {
                if (!requirement.m_required)
                {
                    continue;
                }
                hasRequiredEvidence = true;
                if (!requirement.m_satisfied || requirement.m_evidenceIds.empty())
                {
                    return false;
                }
            }
            return hasRequiredEvidence;
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
        bool geometryCountExceeded = false;
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
                || !IsValidElementKind(element.m_kind)
                || !IsValidPromotionState(element.m_promotionState)
                || !IsBoundedText(element.m_displayName, 512)
                || !IsBoundedText(element.m_regionRef, 512)
                || !IsBoundedText(element.m_sceneRef, 512)
                || !IsBoundedText(PrimaryRef(element), 1024)
                || !IsBoundedText(element.m_roadRef, 1024, true)
                || !IsBoundedText(element.m_nameRef, 1024, true)
                || !IsBoundedText(element.m_segmentRef, 1024, true)
                || !IsBoundedText(element.m_junctionRef, 1024, true)
                || !IsBoundedText(element.m_anchorRef, 1024, true)
                || !IsBoundedText(element.m_connectorRef, 1024, true)
                || !IsBoundedText(element.m_fromElementRef, 1024, true)
                || !IsBoundedText(element.m_toElementRef, 1024, true)
                || !IsBoundedText(element.m_worldNodeRef, 1024, true)
                || !IsBoundedText(element.m_coordinateRef, 1024, true)
                || !IsBoundedText(element.m_notes, 4096, true)
                || element.m_runtimeMutationAllowed)
            {
                AddIssue(
                    result, element.m_elementId, "element.invalid",
                    "Road Atlas element identity, references, text, state, or authority is invalid.");
            }

            const bool connectedValuesBounded = AreBoundedValues(
                element.m_connectedSegmentRefs,
                MaximumConnectedSegmentCount,
                1024);
            const bool tagsBounded = AreBoundedValues(
                element.m_tags,
                MaximumTagCount,
                256);
            if (!connectedValuesBounded
                || !tagsBounded
                || (connectedValuesBounded
                    && HasDuplicates(element.m_connectedSegmentRefs))
                || (tagsBounded && HasDuplicates(element.m_tags)))
            {
                AddIssue(
                    result, element.m_elementId, "element.duplicate-values",
                    "Road Atlas connected refs and tags must be unique and bounded.");
            }
            if (connectedValuesBounded)
            {
                for (const AZStd::string& segmentRef : element.m_connectedSegmentRefs)
                {
                    if (!Contains(segmentRefs, segmentRef))
                    {
                        AddIssue(
                            result, element.m_elementId, "element.missing-segment",
                            "Road Atlas connectivity references an unknown segment.");
                    }
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

            const bool geometryWithinBound = element.m_geometry.size()
                <= MaximumGeometryPointCount - totalGeometryPoints;
            if (!geometryWithinBound)
            {
                geometryCountExceeded = true;
            }
            else
            {
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
            }

            const bool requirementsWithinBound = element.m_evidenceRequirements.size()
                <= MaximumEvidenceRequirementCount;
            AZStd::vector<AZStd::string> requirementIds;
            if (!requirementsWithinBound)
            {
                AddIssue(
                    result, element.m_elementId, "evidence.requirement-count",
                    "Road Atlas evidence requirement count exceeds the contract bound.");
            }
            else
            {
                for (const EvidenceRequirement& requirement : element.m_evidenceRequirements)
                {
                    requirementIds.push_back(requirement.m_requirementId);
                    if (!IsStableContractId(requirement.m_requirementId)
                        || !IsValidEvidenceRequirementKind(requirement.m_kind)
                        || !IsBoundedText(requirement.m_notes, 4096, true)
                        || requirement.m_evidenceIds.size()
                            > MaximumEvidenceIdsPerRequirement
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
                            "Road Atlas evidence requirements are incomplete, unbounded, or duplicated.");
                    }
                }
                if (HasDuplicates(requirementIds)
                    || (element.m_promotionState == PromotionState::ApprovedForPlanning
                        && !RequirementsSatisfied(element)))
                {
                    AddIssue(
                        result, element.m_elementId, "evidence.requirements-unsatisfied",
                        "Planning-approved elements require a non-empty set of unique, required, satisfied evidence requirements.");
                }
            }
        }
        if (geometryCountExceeded || totalGeometryPoints > MaximumGeometryPointCount)
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
        using namespace DeterministicContractJson;

        AZStd::string output = "{";
        AppendString(output, "schema", "road-atlas-snapshot-v1");
        AppendUnsigned(output, "schema_version", snapshot.m_schemaVersion);
        AppendString(output, "snapshot_id", snapshot.m_snapshotId);
        AppendString(output, "profile_id", snapshot.m_profileId);
        AppendString(output, "game_version", snapshot.m_gameVersion);
        AppendString(output, "branch", snapshot.m_branch);
        AppendString(output, "runtime_target", snapshot.m_runtimeTarget);
        AppendString(output, "source_fingerprint", snapshot.m_sourceFingerprint);
        AppendString(output, "captured_at_utc", snapshot.m_capturedAtUtc);

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
        AppendName(output, "elements");
        output.push_back('[');
        for (size_t elementIndex = 0; elementIndex < elements.size(); ++elementIndex)
        {
            if (elementIndex != 0)
            {
                output.push_back(',');
            }
            const Element& element = *elements[elementIndex];
            output.push_back('{');
            AppendString(output, "element_id", element.m_elementId);
            AppendString(output, "owner_pack_id", element.m_ownerPackId);
            AppendSigned(output, "kind", static_cast<AZ::s64>(element.m_kind));
            AppendSigned(
                output,
                "promotion_state",
                static_cast<AZ::s64>(element.m_promotionState));
            AppendString(output, "display_name", element.m_displayName);
            AppendString(output, "region_ref", element.m_regionRef);
            AppendString(output, "scene_ref", element.m_sceneRef);
            AppendString(output, "road_ref", element.m_roadRef);
            AppendString(output, "name_ref", element.m_nameRef);
            AppendString(output, "segment_ref", element.m_segmentRef);
            AppendString(output, "junction_ref", element.m_junctionRef);
            AppendString(output, "anchor_ref", element.m_anchorRef);
            AppendString(output, "connector_ref", element.m_connectorRef);
            AppendString(output, "from_element_ref", element.m_fromElementRef);
            AppendString(output, "to_element_ref", element.m_toElementRef);
            AppendString(output, "world_node_ref", element.m_worldNodeRef);
            AppendString(output, "coordinate_ref", element.m_coordinateRef);
            AppendSortedStringArray(
                output,
                "connected_segment_refs",
                element.m_connectedSegmentRefs);

            AppendName(output, "geometry");
            output.push_back('[');
            for (size_t pointIndex = 0; pointIndex < element.m_geometry.size(); ++pointIndex)
            {
                if (pointIndex != 0)
                {
                    output.push_back(',');
                }
                const Coordinate& point = element.m_geometry[pointIndex];
                output.push_back('{');
                AppendDouble(output, "x", point.m_x);
                AppendDouble(output, "y", point.m_y);
                AppendDouble(output, "z", point.m_z);
                AppendString(output, "point_ref", point.m_pointRef, false);
                output.push_back('}');
            }
            output += "],";

            AZStd::vector<const EvidenceRequirement*> requirements;
            for (const EvidenceRequirement& requirement : element.m_evidenceRequirements)
            {
                requirements.push_back(&requirement);
            }
            AZStd::sort(
                requirements.begin(), requirements.end(),
                [](const EvidenceRequirement* left, const EvidenceRequirement* right)
                {
                    return left->m_requirementId < right->m_requirementId;
                });
            AppendName(output, "evidence_requirements");
            output.push_back('[');
            for (size_t requirementIndex = 0;
                 requirementIndex < requirements.size();
                 ++requirementIndex)
            {
                if (requirementIndex != 0)
                {
                    output.push_back(',');
                }
                const EvidenceRequirement& requirement = *requirements[requirementIndex];
                output.push_back('{');
                AppendString(output, "requirement_id", requirement.m_requirementId);
                AppendSigned(
                    output,
                    "kind",
                    static_cast<AZ::s64>(requirement.m_kind));
                AppendBool(output, "required", requirement.m_required);
                AppendBool(output, "satisfied", requirement.m_satisfied);
                AppendSortedStringArray(
                    output,
                    "evidence_ids",
                    requirement.m_evidenceIds);
                AppendString(output, "notes", requirement.m_notes, false);
                output.push_back('}');
            }
            output += "],";
            AppendSortedStringArray(output, "tags", element.m_tags);
            AppendString(output, "notes", element.m_notes);
            AppendBool(
                output,
                "runtime_mutation_allowed",
                element.m_runtimeMutationAllowed,
                false);
            output.push_back('}');
        }
        output += "],";
        AppendBool(output, "planning_only", snapshot.m_planningOnly);
        AppendBool(
            output,
            "runtime_mutation_allowed",
            snapshot.m_runtimeMutationAllowed,
            false);
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateSnapshotFingerprint(const Snapshot& snapshot)
    {
        return CalculateCanonicalSha256(BuildCanonicalSnapshot(snapshot));
    }
} // namespace TaintedGrailModdingSDK::RoadAtlasExtension
