/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ModPackagePlan.h"
#include "ResearchContractValidation.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/std/functional.h>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        using Ids = AZStd::set<AZStd::string>;
        template<class T, class Predicate> void Keep(AZStd::vector<T>& rows, Predicate accept)
        { rows.erase(AZStd::remove_if(rows.begin(), rows.end(), [&](const T& row) { return !accept(row); }), rows.end()); }
        template<class T> void ResetReview(T& row)
        {
            row.m_allowedUsages.clear();
            row.m_researchStage = "unknown"; row.m_confidence = "unknown";
            row.m_operationalRisk = "unknown"; row.m_validationState = "unvalidated"; row.m_stalenessState = "unknown";
        }
    }
    AZ::Outcome<ModPackageSelection, AZStd::string> ModPackagePlan::Select(
        const WorkspaceModel& workspace, const AZStd::vector<PackManifest>& packs, const AZStd::string& selected,
        const CatalogDatabase& catalog, const SourceEvidenceRegistry& registry)
    {
        const auto* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured() || packs.size() > 512 || catalog.GetRecords().size() > 65536)
        { return AZ::Failure(AZStd::string("Select a configured workspace within the package limits.")); }
        AZStd::map<AZStd::string, const PackManifest*> available;
        for (const auto& pack : packs)
        {
            if (!available.emplace(pack.m_packId, &pack).second)
            { return AZ::Failure(AZStd::string("Duplicate workspace pack identity: ") + pack.m_packId); }
        }
        ModPackageSelection result;
        Ids included, visiting;
        AZStd::string error;
        AZStd::function<bool(const AZStd::string&)> visit = [&](const AZStd::string& id)
        {
            if (included.count(id)) { return true; }
            if (!visiting.insert(id).second) { error = "Dependency cycle at " + id; return false; }
            const auto found = available.find(id);
            if (found == available.end()) { error = "Missing workspace pack dependency: " + id; return false; }
            const auto& pack = *found->second;
            if (!pack.HasStableIdentity() || !pack.UsesSupportedSchema() || pack.m_runtimeActionsEnabled
                || !pack.HasAllowedPackagePaths(&error) || pack.m_targetBranch != profile->m_branch
                || (pack.m_targetGameVersion != profile->m_gameVersion && AZStd::find(pack.m_compatibleGameVersions.begin(),
                    pack.m_compatibleGameVersions.end(), profile->m_gameVersion) == pack.m_compatibleGameVersions.end()))
            { error = "Invalid or incompatible authoring pack: " + id + ". " + error; return false; }
            if (pack.m_dependencies.size() > 512) { error = "Too many pack dependencies: " + id; return false; }
            Ids dependencies;
            for (const auto& dependency : pack.m_dependencies)
            {
                if (!IsStableContractId(dependency) || !dependencies.insert(dependency).second)
                { error = "Dependencies must be distinct exact pack IDs: " + id; return false; }
                if (!visit(dependency)) { return false; }
            }
            visiting.erase(id); included.insert(id); return true;
        };
        if (!IsStableContractId(selected)) { return AZ::Failure(AZStd::string("Select a saved mod before previewing a package.")); }
        if (!visit(selected)) { return AZ::Failure(error); }
        for (const auto& id : included)
        {
            auto pack = *available[id];
            for (const auto& conflict : pack.m_incompatibilities)
            {
                if (included.count(conflict)) { return AZ::Failure(AZStd::string("Conflicting included packs: ") + id + " and " + conflict); }
            }
            AZStd::sort(pack.m_dependencies.begin(), pack.m_dependencies.end());
            result.m_packs.push_back(AZStd::move(pack));
        }
        for (const auto& id : included)
        {
            const auto& pack = *available[id];
            if (pack.m_dependencies.size() > 512 || pack.m_incompatibilities.size() > 512 || pack.m_requiredMods.size() > 512)
            { return AZ::Failure(AZStd::string("Pack dependency declarations exceed the package limits: ") + id); }
        }
        auto document = catalog.BuildDocument(workspace, *profile);
        document.m_validationHistory.clear(); document.m_governanceHistory.clear();
        Ids owned;
        for (const auto& record : document.m_records)
        {
            if (record.IsSynthetic() && included.count(record.m_ownerPackId))
            {
                if (!record.m_missingRefs.empty() || !record.m_conflictRefs.empty() || !record.m_supersededByRecordId.empty())
                { return AZ::Failure(AZStd::string("Resolve this authored record's missing/conflicting/superseded references: ") + record.m_recordId); }
                owned.insert(record.m_recordId);
            }
        }
        Keep(document.m_economyItems, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_economyRecipes, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_actorProfiles, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_troopProfiles, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_encounterDefinitions, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_cultureProfiles, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_factionProfiles, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_worldPlaces, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_worldPaths, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_questProfiles, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_projectAssets, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_localisationEntries, [&](const auto& row) { return owned.count(row.m_recordId); });
        Keep(document.m_recipeIngredients, [&](const auto& row) { return owned.count(row.m_recipeRecordId); });
        Keep(document.m_recipeOutputs, [&](const auto& row) { return owned.count(row.m_recipeRecordId); });
        Keep(document.m_troopMembers, [&](const auto& row) { return owned.count(row.m_troopRecordId); });
        Keep(document.m_factionLinks, [&](const auto& row) { return owned.count(row.m_factionRecordId); });
        Keep(document.m_worldPathNodes, [&](const auto& row) { return owned.count(row.m_pathRecordId); });
        Keep(document.m_worldPathEdges, [&](const auto& row) { return owned.count(row.m_pathRecordId); });

        Keep(document.m_relationships, [&](const auto& row) { return owned.count(row.m_fromRecordId); });
        Keep(document.m_presentationBindings, [&](const auto& row) { return included.count(row.m_ownerPackId); });
        Ids references;
        auto ref = [&](const AZStd::string& id) { if (!id.empty()) { references.insert(id); } };
        for (const auto& r : document.m_relationships) { ref(r.m_toRecordId); }
        for (const auto& r : document.m_economyRecipes) { for (const auto& id : r.m_stationRecordIds) { ref(id); } }
        for (const auto& r : document.m_recipeIngredients) { ref(r.m_itemRecordId); }
        for (const auto& r : document.m_recipeOutputs) { ref(r.m_itemRecordId); }
        for (const auto& r : document.m_actorProfiles) { ref(r.m_templateRecordId); }
        for (const auto& r : document.m_troopProfiles) { ref(r.m_leaderActorRecordId); }
        for (const auto& r : document.m_troopMembers) { ref(r.m_actorRecordId); }
        for (const auto& r : document.m_encounterDefinitions)
        { ref(r.m_placementRecordId); for (const auto& e : r.m_entries) { ref(e.m_targetRecordId); } }
        for (const auto& r : document.m_factionProfiles) { ref(r.m_cultureRecordId); }
        for (const auto& r : document.m_factionLinks) { ref(r.m_targetRecordId); }
        for (const auto& r : document.m_worldPlaces) { ref(r.m_parentRecordId); }
        for (const auto& r : document.m_worldPaths) { ref(r.m_sceneRecordId); ref(r.m_roadRecordId); }
        for (const auto& r : document.m_worldPathNodes) { ref(r.m_locationRecordId); }
        for (const auto& r : document.m_questProfiles) { for (const auto& b : r.m_bindings) { ref(b.m_recordId); } }
        for (const auto& r : document.m_presentationBindings) { ref(r.m_targetRecordId); ref(r.m_valueRecordId); }
        for (const auto& id : references)
        {
            const auto* record = catalog.FindByRecordId(id);
            if (!record) { return AZ::Failure(AZStd::string("Missing referenced record: ") + id); }
            if (record->IsSynthetic() && !owned.count(id))
            { return AZ::Failure(AZStd::string("Add the owning pack as a dependency for referenced record: ") + id); }
        }
        Keep(document.m_records, [&](const auto& r) { return owned.count(r.m_recordId) || references.count(r.m_recordId); });
        for (auto& r : document.m_records)
        {
            ResetReview(r);
            r.m_aliases.clear(); r.m_sourceScopedRefs.clear(); r.m_createdAt.clear(); r.m_updatedAt.clear();
            if (!r.IsSynthetic())
            {
                r.m_displayName = r.m_recordId;
                r.m_tags = {"package-native-reference"};
                r.m_missingRefs.clear(); r.m_conflictRefs.clear(); r.m_supersededByRecordId.clear();
            }
        }
        for (auto& r : document.m_relationships)
        {
            if (!r.m_missingRefs.empty() || !r.m_conflictRefs.empty() || !r.m_supersededByRelationshipId.empty())
            { return AZ::Failure(AZStd::string("Resolve relationship blockers before export: ") + r.m_relationshipId); }
            ResetReview(r);
        }
        Ids evidenceIds;
        auto evidence = [&](const auto& rows)
        { for (const auto& row : rows) { evidenceIds.insert(row.m_evidenceIds.begin(), row.m_evidenceIds.end()); } };
        evidence(document.m_economyItems);
        evidence(document.m_economyRecipes);
        evidence(document.m_actorProfiles);
        evidence(document.m_troopProfiles);
        evidence(document.m_encounterDefinitions);
        evidence(document.m_cultureProfiles);
        evidence(document.m_factionProfiles);
        evidence(document.m_worldPlaces);
        evidence(document.m_worldPaths);
        evidence(document.m_questProfiles);
        evidence(document.m_projectAssets);
        evidence(document.m_localisationEntries);
        evidence(document.m_recipeIngredients);
        evidence(document.m_recipeOutputs);
        evidence(document.m_troopMembers);
        evidence(document.m_factionLinks);
        evidence(document.m_worldPathNodes);
        evidence(document.m_worldPathEdges);
        evidence(document.m_relationships);
        evidence(document.m_records);
        evidence(document.m_presentationBindings);

        AZStd::map<AZStd::string, SourceRecord> sources;
        AZStd::vector<EvidenceRecord> evidenceRows;
        for (const auto& id : evidenceIds)
        {
            const auto* input = registry.FindEvidence(id);
            const auto* origin = input ? registry.FindSource(input->m_sourceId) : nullptr;
            if (!input || !origin) { return AZ::Failure(AZStd::string("Missing authoring evidence: ") + id); }
            auto row = *input;
            row.m_locator = "package:" + row.m_sourceId;
            row.m_recordPath = row.m_evidenceId;
            if (row.m_claim.find("User-authored ") != 0)
            { row.m_claim = "Portable authoring reference; not native runtime proof."; }
            row.m_confidence = "unknown";
            evidenceRows.push_back(AZStd::move(row));
            if (!sources.count(origin->m_sourceId))
            {
                auto source = *origin;
                source.m_title = "Portable authoring intent";
                source.m_sourceKind = "authoring-package-metadata";
                source.m_locator = "package:" + source.m_sourceId;
                source.m_limitations = "Imported metadata only. Source payloads, runtime validation and permissions are not included.";
                source.m_toolName = "FOA-SDK authoring package"; source.m_toolVersion = "1.0.0";
                source.m_importerId = "foa.package.metadata"; source.m_importerVersion = "1.0.0";
                source.m_mediaType = "application/json"; source.m_byteSize = 0;
                sources.emplace(source.m_sourceId, AZStd::move(source));
            }
        }
        for (const auto& pair : sources)
        { if (!result.m_evidence.RegisterSource(pair.second, &error)) { return AZ::Failure(error); } }
        for (const auto& row : evidenceRows)
        { if (!result.m_evidence.RegisterEvidence(row, &error)) { return AZ::Failure(error); } }
        CatalogDatabase verified;
        if (!verified.ReplaceFromBoundDocument(document, workspace, *profile, result.m_evidence, &error))
        { return AZ::Failure(AZStd::string("Package content is not self-contained: ") + error); }
        result.m_catalog = verified.BuildDocument(workspace, *profile);
        result.m_warnings.push_back("Native definition edits without pack ownership and arbitrary manifest files are not included.");
        result.m_warnings.push_back("Package contains editable authoring data. Native references need local game intake before runtime use.");
        result.m_warnings.push_back("Catalog review/permission history is excluded. Imported content requires fresh review.");
        for (const auto& pack : result.m_packs)
        { if (!pack.m_requiredMods.empty()) { result.m_warnings.push_back("Runtime mod requirements are declared only for " + pack.m_packId); } }
        return AZ::Success(AZStd::move(result));
    }
}
