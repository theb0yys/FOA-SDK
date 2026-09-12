/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorldPlanningService.h"
#include "CatalogDatabase.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <cmath>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool WorldKind(const CatalogDatabase& catalog, const AZStd::string& id, const char* kind)
        {
            const auto* record = catalog.FindByRecordId(id);
            return record && record->m_domain == "world" && record->m_recordKind == kind;
        }
        AZ::Outcome<void, AZStd::string> WorldInvalid(const char* text) { return AZ::Failure(AZStd::string(text)); }
    }
    bool WorldPlanningService::IsSingleLine(const AZStd::string& value, size_t maximumBytes, bool emptyAllowed)
    {
        if (value.size() > maximumBytes || (!emptyAllowed && value.empty())) { return false; }
        for (unsigned char c : value) { if (c < 0x20 || c == 0x7f) { return false; } }
        return true;
    }
    AZ::Outcome<void, AZStd::string> WorldPlanningService::ValidatePlace(const WorldPlaceProfile& place,
        const AZStd::string& kind, const CatalogDatabase& catalog)
    {
        if (!IsStableContractId(place.m_recordId) || !IsSingleLine(place.m_description, 2048))
        { return WorldInvalid("A place needs a stable ID and a single-line description up to 2048 bytes."); }
        if (kind != "region" && kind != "scene" && kind != "location")
        { return WorldInvalid("Choose region, scene or location."); }
        if ((kind == "region" && !place.m_parentRecordId.empty()) || (kind != "region"
            && !WorldKind(catalog, place.m_parentRecordId, kind == "scene" ? "region" : "scene")))
        { return WorldInvalid("Regions have no parent; scenes need a saved region and locations need a saved scene."); }
        if (!std::isfinite(place.m_x) || !std::isfinite(place.m_z)
            || std::abs(place.m_x) > 1000000 || std::abs(place.m_z) > 1000000
            || (kind != "location" && place.m_hasPosition)
            || (!place.m_hasPosition && (place.m_x != 0 || place.m_z != 0)))
        { return WorldInvalid("Only locations can have a plan position. Use finite X/Z within +/-1000000, or clear the position."); }
        return AZ::Success();
    }
    AZ::Outcome<void, AZStd::string> WorldPlanningService::ValidatePath(const WorldPathProfile& path,
        const AZStd::string& kind, const CatalogDatabase& catalog)
    {
        if (!IsStableContractId(path.m_recordId) || !IsSingleLine(path.m_description, 2048)
            || !IsSingleLine(path.m_travelConstraints, 2048))
        { return WorldInvalid("A path needs a stable ID and single-line description/constraints up to 2048 bytes."); }
        if ((kind != "road" && kind != "route") || !WorldKind(catalog, path.m_sceneRecordId, "scene"))
        { return WorldInvalid("A road or route needs an exact saved scene."); }
        if (!path.m_roadRecordId.empty())
        {
            const auto* road = catalog.FindWorldPath(path.m_roadRecordId);
            if (kind != "route" || !WorldKind(catalog, path.m_roadRecordId, "road") || !road
                || road->m_sceneRecordId != path.m_sceneRecordId)
            { return WorldInvalid("A route may reference a saved road in the same scene; roads cannot reference other roads."); }
        }
        return AZ::Success();
    }
    WorldPathAnalysis WorldPlanningService::Analyze(const WorldPathDefinition& path, const AZStd::string& kind,
        const CatalogDatabase& catalog)
    {
        WorldPathAnalysis result;
        const auto valid = ValidatePath(path.m_profile, kind, catalog);
        if (!valid.IsSuccess()) { result.m_errors.push_back(valid.GetError()); }
        if (path.m_nodes.size() > 128 || path.m_edges.size() > 256)
        { result.m_errors.push_back("A path supports at most 128 nodes and 256 edges."); return result; }
        AZStd::unordered_map<AZStd::string, const WorldPathNode*> nodes;
        AZStd::unordered_set<AZStd::string> locations, edgeIds, directedPairs;
        AZStd::unordered_map<AZStd::string, AZStd::vector<AZStd::string>> neighbors;
        bool allPositions = !path.m_nodes.empty();
        for (const auto& node : path.m_nodes)
        {
            const auto* place = catalog.FindWorldPlace(node.m_locationRecordId);
            if (!IsStableContractId(node.m_nodeId) || node.m_pathRecordId != path.m_profile.m_recordId
                || !nodes.emplace(node.m_nodeId, &node).second || !IsSingleLine(node.m_notes, 1024))
            { result.m_errors.push_back("Each node needs a distinct stable ID, the owning path and notes up to 1024 bytes."); }
            if (!place || !WorldKind(catalog, node.m_locationRecordId, "location")
                || place->m_parentRecordId != path.m_profile.m_sceneRecordId
                || !locations.insert(node.m_locationRecordId).second)
            { result.m_errors.push_back("Each node must reference a distinct saved location in this path's scene."); }
            if (!place || !place->m_hasPosition) { allPositions = false; }
        }
        for (const auto& edge : path.m_edges)
        {
            if (!IsStableContractId(edge.m_edgeId) || edge.m_pathRecordId != path.m_profile.m_recordId
                || !edgeIds.insert(edge.m_edgeId).second || nodes.count(edge.m_edgeId) || !IsSingleLine(edge.m_notes, 1024))
            { result.m_errors.push_back("Each edge needs a distinct stable ID, the owning path and notes up to 1024 bytes."); }
            if (!nodes.count(edge.m_fromNodeId) || !nodes.count(edge.m_toNodeId) || edge.m_fromNodeId == edge.m_toNodeId)
            { result.m_errors.push_back("Choose two different nodes from this path for each edge."); continue; }
            if ((edge.m_travelMode != "walk" && edge.m_travelMode != "ride" && edge.m_travelMode != "boat")
                || !std::isfinite(edge.m_travelCost) || edge.m_travelCost <= 0 || edge.m_travelCost > 1000000)
            { result.m_errors.push_back("Use walk, ride or boat and a finite planning cost greater than zero and at most 1000000."); }
            const auto forward = edge.m_fromNodeId + "\n" + edge.m_toNodeId;
            const auto reverse = edge.m_toNodeId + "\n" + edge.m_fromNodeId;
            if (!directedPairs.insert(forward).second
                || (edge.m_bidirectional && !directedPairs.insert(reverse).second))
            { result.m_errors.push_back("Connections cannot duplicate an existing direction, including a two-way edge."); }
            neighbors[edge.m_fromNodeId].push_back(edge.m_toNodeId);
            neighbors[edge.m_toNodeId].push_back(edge.m_fromNodeId);
        }
        result.m_usesPlanPositions = allPositions;
        AZStd::unordered_set<AZStd::string> visited;
        for (const auto& node : path.m_nodes)
        {
            if (!visited.insert(node.m_nodeId).second) { continue; }
            ++result.m_components; AZStd::vector<AZStd::string> pending{node.m_nodeId};
            while (!pending.empty())
            {
                const auto current = pending.back(); pending.pop_back();
                for (const auto& neighbor : neighbors[current])
                { if (visited.insert(neighbor).second) { pending.push_back(neighbor); } }
            }
        }
        if (path.m_nodes.empty()) { result.m_warnings.push_back("Add locations as nodes to draw this path."); }
        else if (result.m_components > 1) { result.m_warnings.push_back("The path has disconnected groups of locations."); }
        if (!path.m_nodes.empty() && !allPositions)
        { result.m_warnings.push_back("Schematic topology: some locations have no plan position."); }
        return result;
    }
}
