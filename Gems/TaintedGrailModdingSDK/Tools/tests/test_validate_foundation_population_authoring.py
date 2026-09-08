#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from validate_foundation import validate_population_authoring  # noqa: E402


class PopulationAuthoringValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.gem_root = Path(self._temporary.name) / "TaintedGrailModdingSDK"
        self.source_root = self.gem_root / "Code/Source"
        self._write_valid_contract()

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def _write(self, name: str, content: str) -> None:
        path = self.source_root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _replace(self, name: str, old: str, new: str) -> None:
        path = self.source_root / name
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def _write_valid_contract(self) -> None:
        self._write(
            "PopulationAuthoringService.h",
            "struct PopulationTroopDefinition {};\n"
            "class PopulationAuthoringService final\n"
            "BuildActorProfileCandidate\n"
            "BuildTroopDefinitionCandidate\n"
            "BuildTroopProfileCandidate\n"
            "BuildTroopMemberCandidate\n",
        )
        self._write(
            "PopulationAuthoringService.cpp",
            "ValidateAuthoringContext\n"
            "!IsStableContractId(workspace.m_workspaceId)\n"
            "PackTargetsProfile\n"
            "pack.m_targetGameVersion.empty()\n"
            "pack.m_targetBranch.empty()\n"
            "pack.m_targetGameVersion == profile.m_gameVersion\n"
            "pack.m_targetBranch == profile.m_branch\n"
            "ValidateAuthoredRecord\n"
            "EvidenceIsCompleteAndBound\n"
            "ValidateEvidence\n"
            "candidate.ValidateIntegrity(\n"
            '"population-troop-member:" + member.m_linkId\n'
            "ValidateMemberLinkOwnership\n"
            "existing.m_linkId == member.m_linkId\n"
            "existing.m_troopRecordId != member.m_troopRecordId\n"
            "link identity cannot move\n"
            "PopulationAuthoringService::BuildTroopDefinitionCandidate(\n"
            "ValidateAuthoringContext(\n"
            "definition.m_members.empty()\n"
            "for (const PopulationTroopMember& member : definition.m_members)\n"
            "member.m_troopRecordId\n"
            "!= definition.m_profile.m_recordId\n"
            "ValidateMemberLinkOwnership(member, catalog, validationError)\n"
            "memberIds.push_back(member.m_linkId)\n"
            "AZStd::adjacent_find(memberIds.begin(), memberIds.end())\n"
            "ValidateEvidence(\n"
            "definition.m_profile.m_evidenceIds\n"
            "for (const PopulationTroopMember& member : definition.m_members)\n"
            "ValidateEvidence(\n"
            "member.m_evidenceIds\n"
            "CatalogDocument document = catalog.BuildDocument(workspace, profile)\n"
            "for (const auto& id : definition.m_removedMemberIds)\n"
            "AZStd::find(removedIds.begin(), removedIds.end(), id)\n"
            "AZStd::find(memberIds.begin(), memberIds.end(), id)\n"
            "found == document.m_troopMembers.end()\n"
            "found->m_troopRecordId != definition.m_profile.m_recordId\n"
            "document.m_troopMembers.erase(found);\n"
            "removedIds.push_back(id);\n"
            "bool replacedProfile = false;\n"
            "for (PopulationTroopProfile& existing : document.m_troopProfiles)\n"
            "existing.m_recordId == definition.m_profile.m_recordId\n"
            "existing = definition.m_profile\n"
            "document.m_troopProfiles.push_back(definition.m_profile)\n"
            "for (const PopulationTroopMember& member : definition.m_members)\n"
            "for (PopulationTroopMember& existing : document.m_troopMembers)\n"
            "existing.m_linkId == member.m_linkId\n"
            "existing = member\n"
            "document.m_troopMembers.push_back(member)\n"
            "candidate.ReplaceFromBoundDocument(\n"
            "return AZ::Success(AZStd::move(candidate))\n"
            "PopulationAuthoringService::BuildTroopProfileCandidate(\n"
            "A new troop must be authored atomically with its first member\n"
            "PopulationAuthoringService::BuildTroopMemberCandidate(\n"
            "ValidateAuthoringContext(\n"
            "ValidateAuthoredRecord(\n"
            "ValidateMemberLinkOwnership(\n"
            "ValidateEvidence(\n"
            "CatalogDatabase candidate = catalog\n"
            "candidate.UpsertPopulationTroopMember(\n"
            "return ValidateCandidate(\n",
        )
        self._write(
            "FoundationPopulationService.cpp",
            "ResolvePopulationAuthoringContext\n"
            "GetActivePack()\n"
            "resolvedWorkspaceRoot.empty()\n"
            "workspaceRoot = resolvedWorkspaceRoot\n"
            "FoundationService::UpsertPopulationActorProfile(\n"
            "m_populationAuthoring.BuildActorProfileCandidate(\n"
            "PersistCatalogCandidate(candidate.GetValue(), error)\n"
            "FoundationService::UpsertPopulationTroopDefinition(\n"
            "m_populationAuthoring.BuildTroopDefinitionCandidate(\n"
            "PersistCatalogCandidate(candidate.GetValue(), error)\n"
            "FoundationService::UpsertPopulationTroopProfile(\n"
            "m_populationAuthoring.BuildTroopProfileCandidate(\n"
            "PersistCatalogCandidate(candidate.GetValue(), error)\n"
            "FoundationService::UpsertPopulationTroopMember(\n"
            "m_populationAuthoring.BuildTroopMemberCandidate(\n"
            "PersistCatalogCandidate(candidate.GetValue(), error)\n",
        )
        self._write(
            "FoundationModels.cpp",
            "Class<FoundationSnapshot>()\n->Version(8)\n"
            '"PopulationActorProfileCount"\n'
            '"PopulationTroopProfileCount"\n'
            '"PopulationTroopMemberCount"\n'
            "m_populationActorProfileCount\n"
            "m_populationTroopProfileCount\n"
            "m_populationTroopMemberCount\n",
        )
        self._write(
            "FoundationService.cpp",
            "m_catalog.GetPopulationActorProfiles().size()\n"
            "m_catalog.GetPopulationTroopProfiles().size()\n"
            "m_catalog.GetPopulationTroopMembers().size()\n",
        )
        self._write(
            "FoundationCatalogService.cpp",
            "FoundationService::PersistCatalogCandidate(\n"
            "m_catalogTransaction.Commit(\n"
            "m_catalog = AZStd::move(committed.m_catalog)\n"
            "RefreshSnapshot();\n",
        )

    def test_valid_contract_passes(self) -> None:
        validate_population_authoring(self.gem_root)

    def test_missing_fail_closed_pack_target_is_rejected(self) -> None:
        self._replace(
            "PopulationAuthoringService.cpp",
            "pack.m_targetGameVersion.empty()",
            "pack.target-version-check-removed",
        )
        with self.assertRaisesRegex(RuntimeError, "targetGameVersion"):
            validate_population_authoring(self.gem_root)

    def test_non_atomic_troop_definition_is_rejected(self) -> None:
        self._replace(
            "PopulationAuthoringService.cpp",
            "candidate.ReplaceFromBoundDocument(\n",
            "candidate.ReplaceFromBoundDocument(\n"
            "document.m_troopMembers.erase(\n",
        )
        with self.assertRaisesRegex(RuntimeError, "must not remove"):
            validate_population_authoring(self.gem_root)

    def test_explicit_removal_requires_exact_troop_ownership(self) -> None:
        self._replace("PopulationAuthoringService.cpp",
            "found->m_troopRecordId != definition.m_profile.m_recordId", "ownership-guard-removed")
        with self.assertRaisesRegex(RuntimeError, "exact ownership"):
            validate_population_authoring(self.gem_root)

    def test_incomplete_troop_definition_guards_are_rejected(self) -> None:
        self._replace(
            "PopulationAuthoringService.cpp",
            "definition.m_members.empty()",
            "definition.empty-member-guard-removed",
        )
        with self.assertRaisesRegex(RuntimeError, "definition.m_members.empty"):
            validate_population_authoring(self.gem_root)

    def test_unvalidated_workspace_root_fallback_is_rejected(self) -> None:
        self._replace(
            "FoundationPopulationService.cpp",
            "workspaceRoot = resolvedWorkspaceRoot\n",
            "workspaceRoot = workspace.m_rootPath\n",
        )
        with self.assertRaisesRegex(RuntimeError, "workspaceRoot = resolvedWorkspaceRoot"):
            validate_population_authoring(self.gem_root)

    def test_member_link_owner_change_guard_is_required(self) -> None:
        self._replace(
            "PopulationAuthoringService.cpp",
            "existing.m_troopRecordId != member.m_troopRecordId",
            "existing.member-owner-change-check-removed",
        )
        with self.assertRaisesRegex(RuntimeError, "troopRecordId"):
            validate_population_authoring(self.gem_root)

    def test_direct_foundation_publication_is_rejected(self) -> None:
        self._replace(
            "FoundationPopulationService.cpp",
            "ResolvePopulationAuthoringContext\n",
            "ResolvePopulationAuthoringContext\nm_catalog = candidate\n",
        )
        with self.assertRaisesRegex(RuntimeError, "direct authority"):
            validate_population_authoring(self.gem_root)

    def test_duplicate_command_persistence_is_rejected(self) -> None:
        persisted = "PersistCatalogCandidate(candidate.GetValue(), error)\n"
        self._replace(
            "FoundationPopulationService.cpp",
            persisted + "FoundationService::UpsertPopulationTroopDefinition(",
            persisted + persisted + "FoundationService::UpsertPopulationTroopDefinition(",
        )
        with self.assertRaisesRegex(RuntimeError, "persist exactly once"):
            validate_population_authoring(self.gem_root)

    def test_authoring_side_notification_is_rejected(self) -> None:
        self._replace(
            "FoundationPopulationService.cpp",
            "ResolvePopulationAuthoringContext\n",
            "ResolvePopulationAuthoringContext\nFoundationNotificationBus\n",
        )
        with self.assertRaisesRegex(RuntimeError, "direct authority"):
            validate_population_authoring(self.gem_root)

    def test_runtime_process_coupling_is_rejected(self) -> None:
        self._replace(
            "PopulationAuthoringService.cpp",
            "ValidateAuthoringContext\n",
            "ValidateAuthoringContext\nQProcess\n",
        )
        with self.assertRaisesRegex(RuntimeError, "forbidden authority"):
            validate_population_authoring(self.gem_root)

    def test_multiple_central_refreshes_are_rejected(self) -> None:
        self._replace(
            "FoundationCatalogService.cpp",
            "RefreshSnapshot();\n",
            "RefreshSnapshot();\nRefreshSnapshot();\n",
        )
        with self.assertRaisesRegex(RuntimeError, "one Foundation refresh"):
            validate_population_authoring(self.gem_root)


if __name__ == "__main__":
    unittest.main()
