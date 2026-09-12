/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"
#include "WorldPlanningService.h"
#include "PopulationEvidenceValidation.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool WorldError(AZStd::string* error, const char* text) { if (error) { *error = text; } return false; }
        bool WorldOwned(const CatalogRecord* record)
        { return record && record->m_domain == "world" && record->IsSynthetic() && !record->m_ownerPackId.empty(); }
        bool WorldEvidence(const AZStd::vector<AZStd::string>& ids)
        {
            if (ids.empty() || ids.size() > 64) { return false; }
            AZStd::unordered_set<AZStd::string> seen;
            for (const auto& id : ids) { if (!IsStableContractId(id) || !seen.insert(id).second) { return false; } }
            return true;
        }
        template<class T> void WorldCanonicalEvidence(T& value)
        { AZStd::sort(value.m_evidenceIds.begin(), value.m_evidenceIds.end()); }
        template<class T> const T* WorldFind(const AZStd::vector<T>& values, const AZStd::string& id)
        {
            const auto it = AZStd::lower_bound(values.begin(), values.end(), id,
                [](const auto& value, const auto& key) { return value.m_recordId < key; });
            return it != values.end() && it->m_recordId == id ? &*it : nullptr;
        }
        template<class T> bool WorldUpsert(AZStd::vector<T>& values, T value, size_t limit, AZStd::string* error)
        {
            auto it = AZStd::lower_bound(values.begin(), values.end(), value.m_recordId,
                [](const auto& current, const auto& key) { return current.m_recordId < key; });
            WorldCanonicalEvidence(value);
            if (it != values.end() && it->m_recordId == value.m_recordId) { *it = AZStd::move(value); return true; }
            if (values.size() >= limit) { return WorldError(error, "The world collection has reached its supported limit."); }
            values.insert(it, AZStd::move(value)); return true;
        }
    }
    const WorldPlaceProfile* CatalogDatabase::FindWorldPlace(const AZStd::string& id) const { return WorldFind(m_worldPlaces, id); }
    const WorldPathProfile* CatalogDatabase::FindWorldPath(const AZStd::string& id) const { return WorldFind(m_worldPaths, id); }
    const AZStd::vector<WorldPlaceProfile>& CatalogDatabase::GetWorldPlaces() const { return m_worldPlaces; }
    const AZStd::vector<WorldPathProfile>& CatalogDatabase::GetWorldPaths() const { return m_worldPaths; }
    const AZStd::vector<WorldPathNode>& CatalogDatabase::GetWorldPathNodes() const { return m_worldPathNodes; }
    const AZStd::vector<WorldPathEdge>& CatalogDatabase::GetWorldPathEdges() const { return m_worldPathEdges; }
    WorldPathDefinition CatalogDatabase::FindWorldPathDefinition(const AZStd::string& id) const
    {
        WorldPathDefinition result;
        if (const auto* profile = FindWorldPath(id)) { result.m_profile = *profile; }
        for (const auto& node : m_worldPathNodes) { if (node.m_pathRecordId == id) { result.m_nodes.push_back(node); } }
        for (const auto& edge : m_worldPathEdges) { if (edge.m_pathRecordId == id) { result.m_edges.push_back(edge); } }
        return result;
    }
    bool CatalogDatabase::UpsertWorldPlace(const WorldPlaceProfile& place, AZStd::string* error)
    {
        const auto* record = FindByRecordId(place.m_recordId);
        if (!WorldOwned(record) || !WorldEvidence(place.m_evidenceIds))
        { return WorldError(error, "World places require pack ownership and exact authoring evidence."); }
        const auto valid = WorldPlanningService::ValidatePlace(place, record->m_recordKind, *this);
        if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        return WorldUpsert(m_worldPlaces, place, 5000, error);
    }
    bool CatalogDatabase::UpsertWorldPathProfile(const WorldPathProfile& path, AZStd::string* error)
    {
        const auto* record = FindByRecordId(path.m_recordId);
        if (!WorldOwned(record) || !WorldEvidence(path.m_evidenceIds))
        { return WorldError(error, "World paths require pack ownership and exact authoring evidence."); }
        const auto valid = WorldPlanningService::ValidatePath(path, record->m_recordKind, *this);
        if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        return WorldUpsert(m_worldPaths, path, 1000, error);
    }
    bool CatalogDatabase::ReplaceWorldPath(const WorldPathDefinition& path, AZStd::string* error)
    {
        const auto* record = FindByRecordId(path.m_profile.m_recordId);
        if (!WorldOwned(record)) { return WorldError(error, "Choose a pack-owned world path."); }
        const auto result = WorldPlanningService::Analyze(path, record->m_recordKind, *this);
        if (!result.IsValid()) { if (error) { *error = result.m_errors.front(); } return false; }
        AZStd::unordered_set<AZStd::string> incoming;
        for (const auto& node : path.m_nodes)
        {
            if (!WorldEvidence(node.m_evidenceIds) || FindByRecordId(node.m_nodeId)) { return WorldError(error, "Nodes require distinct IDs and bounded exact evidence."); }
            incoming.insert(node.m_nodeId);
        }
        for (const auto& edge : path.m_edges)
        {
            if (!WorldEvidence(edge.m_evidenceIds) || FindByRecordId(edge.m_edgeId)) { return WorldError(error, "Edges require distinct IDs and bounded exact evidence."); }
            incoming.insert(edge.m_edgeId);
        }
        size_t replacedNodes = 0, replacedEdges = 0;
        for (const auto& node : m_worldPathNodes)
        {
            const bool same = node.m_pathRecordId == path.m_profile.m_recordId;
            replacedNodes += same ? 1 : 0;
            if (incoming.count(node.m_nodeId) && (!same
                || AZStd::find_if(path.m_nodes.begin(), path.m_nodes.end(), [&node](const auto& n) { return n.m_nodeId == node.m_nodeId; }) == path.m_nodes.end()))
            { return WorldError(error, "Node IDs cannot move between paths or become edge IDs."); }
        }
        for (const auto& edge : m_worldPathEdges)
        {
            const bool same = edge.m_pathRecordId == path.m_profile.m_recordId;
            replacedEdges += same ? 1 : 0;
            if (incoming.count(edge.m_edgeId) && (!same
                || AZStd::find_if(path.m_edges.begin(), path.m_edges.end(), [&edge](const auto& e) { return e.m_edgeId == edge.m_edgeId; }) == path.m_edges.end()))
            { return WorldError(error, "Edge IDs cannot move between paths or become node IDs."); }
        }
        if (m_worldPathNodes.size() - replacedNodes + path.m_nodes.size() > 10000
            || m_worldPathEdges.size() - replacedEdges + path.m_edges.size() > 20000)
        { return WorldError(error, "The catalog supports at most 10000 path nodes and 20000 path edges."); }
        if (!UpsertWorldPathProfile(path.m_profile, error)) { return false; }
        const auto owner = [&path](const auto& value) { return value.m_pathRecordId == path.m_profile.m_recordId; };
        m_worldPathNodes.erase(AZStd::remove_if(m_worldPathNodes.begin(), m_worldPathNodes.end(), owner), m_worldPathNodes.end());
        m_worldPathEdges.erase(AZStd::remove_if(m_worldPathEdges.begin(), m_worldPathEdges.end(), owner), m_worldPathEdges.end());
        m_worldPathNodes.insert(m_worldPathNodes.end(), path.m_nodes.begin(), path.m_nodes.end());
        m_worldPathEdges.insert(m_worldPathEdges.end(), path.m_edges.begin(), path.m_edges.end());
        for (auto& node : m_worldPathNodes) { WorldCanonicalEvidence(node); }
        for (auto& edge : m_worldPathEdges) { WorldCanonicalEvidence(edge); }
        AZStd::sort(m_worldPathNodes.begin(), m_worldPathNodes.end(), [](const auto& a, const auto& b) { return a.m_nodeId < b.m_nodeId; });
        AZStd::sort(m_worldPathEdges.begin(), m_worldPathEdges.end(), [](const auto& a, const auto& b) { return a.m_edgeId < b.m_edgeId; });
        return true;
    }
    bool CatalogDatabase::LoadWorldCollections(const CatalogDocument& document, AZStd::string* error)
    {
        if (document.m_worldPlaces.size() > 5000 || document.m_worldPaths.size() > 1000
            || document.m_worldPathNodes.size() > 10000 || document.m_worldPathEdges.size() > 20000)
        { return WorldError(error, "World collections exceed their supported bounds."); }
        AZStd::unordered_set<AZStd::string> ids;
        for (const auto& place : document.m_worldPlaces)
        {
            if (!ids.insert(place.m_recordId).second) { return WorldError(error, "World place IDs must be unique."); }
            if (!UpsertWorldPlace(place, error)) { return false; }
        }
        for (const auto& path : document.m_worldPaths)
        { if (!ids.insert(path.m_recordId).second) { return WorldError(error, "World profile IDs must be unique."); } }
        // All canonical identities are already loaded; roads must precede routes that reference them.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (const auto& path : document.m_worldPaths)
            {
                const auto* record = FindByRecordId(path.m_recordId);
                if (!record) { return WorldError(error, "A path's canonical record is missing."); }
                if ((record->m_recordKind == "road") == (pass == 0))
                { if (!UpsertWorldPathProfile(path, error)) { return false; } }
            }
        }
        AZStd::unordered_map<AZStd::string, WorldPathDefinition> groups;
        for (const auto& path : m_worldPaths) { groups[path.m_recordId].m_profile = path; }
        for (auto node : document.m_worldPathNodes)
        {
            if (!ids.insert(node.m_nodeId).second || FindByRecordId(node.m_nodeId) || !FindWorldPath(node.m_pathRecordId) || !WorldEvidence(node.m_evidenceIds))
            { return WorldError(error, "Path nodes contain duplicate IDs, missing owners or invalid evidence."); }
            WorldCanonicalEvidence(node); groups[node.m_pathRecordId].m_nodes.push_back(AZStd::move(node));
        }
        for (auto edge : document.m_worldPathEdges)
        {
            if (!ids.insert(edge.m_edgeId).second || FindByRecordId(edge.m_edgeId) || !FindWorldPath(edge.m_pathRecordId) || !WorldEvidence(edge.m_evidenceIds))
            { return WorldError(error, "Path edges contain duplicate IDs, missing owners or invalid evidence."); }
            WorldCanonicalEvidence(edge); groups[edge.m_pathRecordId].m_edges.push_back(AZStd::move(edge));
        }
        for (const auto& [id, path] : groups)
        {
            const auto analysis = WorldPlanningService::Analyze(path, FindByRecordId(id)->m_recordKind, *this);
            if (!analysis.IsValid()) { if (error) { *error = analysis.m_errors.front(); } return false; }
            m_worldPathNodes.insert(m_worldPathNodes.end(), path.m_nodes.begin(), path.m_nodes.end());
            m_worldPathEdges.insert(m_worldPathEdges.end(), path.m_edges.begin(), path.m_edges.end());
        }
        AZStd::sort(m_worldPathNodes.begin(), m_worldPathNodes.end(), [](const auto& a, const auto& b) { return a.m_nodeId < b.m_nodeId; });
        AZStd::sort(m_worldPathEdges.begin(), m_worldPathEdges.end(), [](const auto& a, const auto& b) { return a.m_edgeId < b.m_edgeId; });
        return true;
    }
    bool CatalogDatabase::ValidateWorldIntegrity(const GameProfile& profile, const SourceEvidenceRegistry& registry, AZStd::string* error) const
    {
        const auto coverage = [this, &profile, &registry, error](AZStd::vector<AZStd::string> evidence,
            AZStd::vector<AZStd::string> subjects, const AZStd::vector<AZStd::string>& records)
        {
            if (!WorldEvidence(evidence)) { return WorldError(error, "World authoring evidence is missing or invalid."); }
            AZStd::string intentError;
            if (!ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "World intent", intentError))
            { if (error) { *error = intentError; } return false; }
            for (const auto& id : records)
            {
                const auto* record = FindByRecordId(id);
                if (record)
                {
                    AppendUniquePopulationRequiredSubject(subjects, record->m_subjectRef);
                    AppendUniquePopulationEvidenceIds(evidence, record->m_evidenceIds);
                }
            }
            AZStd::string message;
            if (!ValidatePopulationEvidenceCoverage(evidence, subjects, profile, registry, "World authoring", message))
            { if (error) { *error = message; } return false; }
            return true;
        };
        for (const auto& place : m_worldPlaces)
        {
            const auto* record = FindByRecordId(place.m_recordId);
            if (!WorldOwned(record)) { return WorldError(error, "World place ownership is invalid."); }
            const auto valid = WorldPlanningService::ValidatePlace(place, record->m_recordKind, *this);
            if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
            if (!coverage(place.m_evidenceIds, {record->m_subjectRef}, {place.m_recordId, place.m_parentRecordId})) { return false; }
        }
        AZStd::unordered_map<AZStd::string, WorldPathDefinition> groups;
        for (const auto& path : m_worldPaths) { groups[path.m_recordId].m_profile = path; }
        for (const auto& node : m_worldPathNodes)
        {
            if (!FindWorldPath(node.m_pathRecordId)) { return WorldError(error, "Path node owner is missing."); }
            groups[node.m_pathRecordId].m_nodes.push_back(node);
            if (!coverage(node.m_evidenceIds, {"world-node:" + node.m_nodeId}, {node.m_pathRecordId, node.m_locationRecordId})) { return false; }
        }
        for (const auto& edge : m_worldPathEdges)
        {
            if (!FindWorldPath(edge.m_pathRecordId)) { return WorldError(error, "Path edge owner is missing."); }
            groups[edge.m_pathRecordId].m_edges.push_back(edge);
            if (!coverage(edge.m_evidenceIds, {"world-edge:" + edge.m_edgeId}, {edge.m_pathRecordId})) { return false; }
        }
        for (const auto& [id, path] : groups)
        {
            const auto* record = FindByRecordId(id);
            if (!WorldOwned(record)) { return WorldError(error, "World path ownership is invalid."); }
            const auto valid = WorldPlanningService::Analyze(path, record->m_recordKind, *this);
            if (!valid.IsValid()) { if (error) { *error = valid.m_errors.front(); } return false; }
            if (!coverage(path.m_profile.m_evidenceIds, {record->m_subjectRef}, {id, path.m_profile.m_sceneRecordId, path.m_profile.m_roadRecordId})) { return false; }
        }
        return true;
    }
}
