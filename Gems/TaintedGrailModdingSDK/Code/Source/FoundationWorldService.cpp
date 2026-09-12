/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"
#include "WorldPlanningService.h"
#include <AzCore/std/algorithm.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString WQ(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string WA(const QString& value)
        { const auto bytes = value.toUtf8(); return {bytes.constData(), static_cast<size_t>(bytes.size())}; }
        AZStd::string WorldId(const char* prefix)
        { return WA(QString::fromUtf8(prefix) + QUuid::createUuid().toString(QUuid::WithoutBraces)); }
        bool WorldFail(AZStd::string* error, const char* message) { if (error) { *error = message; } return false; }
        QJsonObject WorldIntent(const AZStd::string& subject, const QJsonObject& values)
        {
            return {{"evidence_id", WQ(WorldId("evidence.authored.world."))}, {"subject_ref", WQ(subject)},
                {"kind", "world"}, {"confidence", "documented"},
                {"claim", "User-authored world plan: " + QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact))}};
        }
    }
    bool FoundationService::CreateWorldPlace(const AZStd::string& kind, const AZStd::string& name,
        const AZStd::string& parentId, AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear(); WorldPlaceProfile place;
        place.m_recordId = WorldId("custom.world-place."); place.m_parentRecordId = parentId;
        if (!CommitAuthoredWorld(place, {}, kind, name, true, true, error)) { return false; }
        recordId = place.m_recordId; return true;
    }
    bool FoundationService::SaveWorldPlace(const WorldPlaceProfile& place, const AZStd::string& name, AZStd::string* error)
    {
        const auto* record = m_catalog.FindByRecordId(place.m_recordId);
        return CommitAuthoredWorld(place, {}, record ? record->m_recordKind : AZStd::string{}, name, true, false, error);
    }
    bool FoundationService::CreateWorldPath(const AZStd::string& kind, const AZStd::string& name,
        const AZStd::string& sceneId, AZStd::string& recordId, AZStd::string* error)
    {
        recordId.clear(); WorldPathDefinition path;
        path.m_profile.m_recordId = WorldId("custom.world-path."); path.m_profile.m_sceneRecordId = sceneId;
        if (!CommitAuthoredWorld({}, path, kind, name, false, true, error)) { return false; }
        recordId = path.m_profile.m_recordId; return true;
    }
    bool FoundationService::SaveWorldPath(const WorldPathDefinition& path, const AZStd::string& name, AZStd::string* error)
    {
        const auto* record = m_catalog.FindByRecordId(path.m_profile.m_recordId);
        return CommitAuthoredWorld({}, path, record ? record->m_recordKind : AZStd::string{}, name, false, false, error);
    }
    bool FoundationService::CommitAuthoredWorld(WorldPlaceProfile place, WorldPathDefinition path,
        const AZStd::string& kind, const AZStd::string& name, bool isPlace, bool creating, AZStd::string* error)
    {
        const auto* profile = m_workspace.FindActiveGameProfile(); const auto* pack = GetActivePack();
        if (!profile || !profile->IsConfigured() || !pack || GetActivePackFilePath().empty()
            || !pack->HasStableIdentity() || !pack->UsesSupportedSchema() || pack->m_runtimeActionsEnabled
            || pack->m_targetBranch != profile->m_branch || (pack->m_targetGameVersion != profile->m_gameVersion
                && AZStd::find(pack->m_compatibleGameVersions.begin(), pack->m_compatibleGameVersions.end(), profile->m_gameVersion)
                    == pack->m_compatibleGameVersions.end()))
        { return WorldFail(error, "Save a compatible authoring mod before creating or editing world places and paths."); }
        const auto trimmedName = WQ(name).trimmed();
        if (trimmedName.isEmpty() || !WorldPlanningService::IsSingleLine(name, 512, false))
        { return WorldFail(error, "Enter a single-line name up to 512 bytes."); }
        const auto id = isPlace ? place.m_recordId : path.m_profile.m_recordId;
        const auto* existing = m_catalog.FindByRecordId(id);
        const bool hasProfile = isPlace ? m_catalog.FindWorldPlace(id) != nullptr : m_catalog.FindWorldPath(id) != nullptr;
        if ((creating && existing) || (!creating && (!existing || !hasProfile || existing->m_domain != "world"
            || existing->m_recordKind != kind || !existing->IsSynthetic() || existing->m_ownerPackId != pack->m_packId)))
        { return WorldFail(error, "Select a world definition owned by the active mod."); }
        if (isPlace)
        {
            const auto valid = WorldPlanningService::ValidatePlace(place, kind, m_catalog);
            if (!valid.IsSuccess()) { if (error) { *error = valid.GetError(); } return false; }
        }
        else
        {
            const auto valid = WorldPlanningService::Analyze(path, kind, m_catalog);
            if (!valid.IsValid()) { if (error) { *error = valid.m_errors.front(); } return false; }
        }
        const AZStd::string subject = creating ? "pack:" + pack->m_packId + "/" + id : existing->m_subjectRef;
        QJsonArray rows, nodeIds, edgeIds;
        for (auto& node : path.m_nodes)
        {
            const auto row = WorldIntent("world-node:" + node.m_nodeId,
                {{"node_id", WQ(node.m_nodeId)}, {"path_record_id", WQ(id)},
                    {"location_record_id", WQ(node.m_locationRecordId)}, {"notes", WQ(node.m_notes)}});
            node.m_evidenceIds = {WA(row.value("evidence_id").toString())}; rows.append(row); nodeIds.append(WQ(node.m_nodeId));
        }
        for (auto& edge : path.m_edges)
        {
            const auto row = WorldIntent("world-edge:" + edge.m_edgeId,
                {{"edge_id", WQ(edge.m_edgeId)}, {"path_record_id", WQ(id)}, {"from_node_id", WQ(edge.m_fromNodeId)},
                    {"to_node_id", WQ(edge.m_toNodeId)}, {"travel_mode", WQ(edge.m_travelMode)},
                    {"bidirectional", edge.m_bidirectional}, {"travel_cost", edge.m_travelCost}, {"notes", WQ(edge.m_notes)}});
            edge.m_evidenceIds = {WA(row.value("evidence_id").toString())}; rows.append(row); edgeIds.append(WQ(edge.m_edgeId));
        }
        const QJsonObject values = isPlace
            ? QJsonObject{{"record_id", WQ(id)}, {"name", trimmedName}, {"kind", WQ(kind)},
                {"description", WQ(place.m_description)}, {"parent_record_id", WQ(place.m_parentRecordId)},
                {"has_position", place.m_hasPosition}, {"x", place.m_x}, {"z", place.m_z}, {"coordinate_basis", "scene-local-plan-units"}}
            : QJsonObject{{"record_id", WQ(id)}, {"name", trimmedName}, {"kind", WQ(kind)},
                {"description", WQ(path.m_profile.m_description)}, {"scene_record_id", WQ(path.m_profile.m_sceneRecordId)},
                {"road_record_id", WQ(path.m_profile.m_roadRecordId)}, {"travel_constraints", WQ(path.m_profile.m_travelConstraints)},
                {"node_ids", nodeIds}, {"edge_ids", edgeIds}};
        const auto row = WorldIntent(subject, values);
        const auto evidenceId = WA(row.value("evidence_id").toString()); rows.append(row);
        if (isPlace) { place.m_evidenceIds = {evidenceId}; } else { path.m_profile.m_evidenceIds = {evidenceId}; }
        SourceEvidenceRegistry registry = m_sourceRegistry; SourceImportResult imported;
        if (!PrepareAuthoredPopulationEvidence(WA(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact))),
                registry, imported, error)) { return false; }
        CatalogDatabase candidate = m_catalog;
        if (creating)
        {
            CatalogPromotionRequest request;
            request.m_recordId = id; request.m_domain = "world"; request.m_recordKind = kind;
            request.m_subjectRef = subject; request.m_identityKind = "synthetic"; request.m_ownerPackId = pack->m_packId;
            request.m_displayName = WA(trimmedName); request.m_evidenceId = evidenceId;
            request.m_confidence = "documented"; request.m_researchStage = "S1";
            const auto record = m_catalogPromotion.BuildReviewedRecord(request, m_workspace, m_packs, registry);
            if (!record.IsSuccess()) { if (error) { *error = record.GetError(); } return false; }
            if (!candidate.InsertNew(record.GetValue(), error)) { return false; }
        }
        else
        {
            CatalogRecord record = *existing; record.m_displayName = WA(trimmedName);
            if (!candidate.Upsert(record, error)) { return false; }
        }
        if (isPlace) { if (!candidate.UpsertWorldPlace(place, error)) { return false; } }
        else if (!candidate.ReplaceWorldPath(path, error)) { return false; }
        return CommitPopulationIntake(candidate, AZStd::move(registry), imported, error);
    }
}
