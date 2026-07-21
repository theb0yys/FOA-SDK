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

from validate_population_actor_troop_editor import (  # noqa: E402
    PopulationEditorContractError,
    validate_population_actor_troop_editor,
)


class PopulationActorTroopEditorValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.repo_root = Path(self._temporary.name)
        self._write_valid_contract()

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def _write(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _append(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.write_text(path.read_text(encoding="utf-8") + content, encoding="utf-8")

    def _write_valid_contract(self) -> None:
        code = "Gems/TaintedGrailModdingSDK/Code/"
        source = code + "Source/"
        tests = code + "Tests/"
        self._write(
            code + "taintedgrailmoddingsdk_core_files.cmake",
            "Source/PopulationActionLaneService.cpp\n"
            "Source/PopulationActionLaneService.h\n",
        )
        self._write(
            code + "taintedgrailmoddingsdk_editor_files.cmake",
            "Source/ActorTroopEditorWidget.cpp\nSource/ActorTroopEditorWidget.h\n",
        )
        self._write(
            code + "taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "Tests/PopulationActionLaneServiceTests.cpp\n",
        )
        self._write(
            source + "PopulationActionLaneService.h",
            "enum class PopulationActionLane {};\n"
            "enum class PopulationActionLaneState {};\n"
            "struct PopulationActionLaneDecision { m_blockerIds; m_reasons; };\n"
            "class PopulationActionLaneService final { BuildActionLaneMatrix; };\n"
            "const AZStd::string& resolvedWorkspaceRoot;\n",
        )
        self._write(
            source + "PopulationActionLaneService.cpp",
            "PopulationActionLane::Display,\n"
            "PopulationActionLane::AuthorProfile,\n"
            "PopulationActionLane::ComposeTroop,\n"
            "PopulationActionLane::Planning,\n"
            "PopulationActionLane::SpawnCandidate,\n"
            "PopulationActionLane::RuntimeSpawn,\n"
            "PopulationActionLane::SaveMutation,\n"
            "BuildActionLaneMatrix\n"
            "const AZStd::string& resolvedWorkspaceRoot\n"
            "resolvedWorkspaceRoot.empty()\n"
            "catalog.ValidateIntegrity(\n"
            "record.m_forbiddenUsages\nrecord.m_allowedUsages\n"
            "CollectBlockers\n"
            'blocker.m_status == "open"\n'
            "SortUnique(decision.m_blockerIds)\n"
            "SortUnique(decision.m_reasons)\n"
            'return "unavailable"\n'
            "if (lane == PopulationActionLane::RuntimeSpawn)\n"
            "decision.m_state = PopulationActionLaneState::Unavailable\n"
            "return decision;\n"
            "if (lane == PopulationActionLane::SaveMutation)\n"
            "decision.m_state = PopulationActionLaneState::Unavailable\n"
            "return decision;\n"
            "if (lane == PopulationActionLane::ComposeTroop\n"
            "if (Contains(record.m_forbiddenUsages, laneName))\n"
            "Contains(record.m_allowedUsages, laneName)\n"
            "Spawn candidacy requires a validated, current,\n"
            "unresolved-free, non-superseded catalog record.\n"
            "Catalog governance explicitly forbids this population action lane.\n"
            "The selected population record has no complete\n"
            "persisted typed profile or troop composition.\n",
        )
        self._write(
            source + "ActorTroopEditorWidget.h",
            "class ActorTroopEditorWidget final : FoundationNotificationBus::Handler {};\n"
            "m_actorFilter m_troopFilter m_actorTemplateRecord m_actorTemplateSubject\n"
            "m_troopLeaderRecord m_troopLeaderSubject m_memberActorRecord m_memberActorSubject\n"
            "m_actorEvidenceDetails m_troopEvidenceDetails m_memberEvidenceDetails\n"
            "m_actorGovernanceSummary m_actorBlockerSummary m_actorRelationshipSummary\n"
            "m_troopGovernanceSummary m_troopBlockerSummary m_troopRelationshipSummary\n"
            "m_actorActionLanes m_troopActionLanes m_draftMembers\n"
            "StageMember SaveActorProfile SaveTroopDefinition RevertActorProfile RevertTroopDefinition\n"
            "closeEvent(QCloseEvent* event) m_actorDirty m_troopDirty\n"
            "m_memberEditorDirty m_foundationRefreshPending HasDirtyDrafts\n"
            "HandleActorRecordChange HandleTroopRecordChange\n"
            "HandleMemberSelectionChange ApplyPendingFoundationRefresh\n",
        )
        self._write(
            source + "ActorTroopEditorWidget.cpp",
            "QAbstractItemView::NoEditTriggers\n"
            "ConfigureTable(m_actorActionLanes)\n"
            "ConfigureTable(m_troopActionLanes)\n"
            "FoundationNotificationBus::Handler::BusConnect\n"
            "FoundationNotificationBus::Handler::BusDisconnect\n"
            "PopulationActionLaneService BuildActionLaneMatrix( GetWorkspaceRootPath()\n"
            "UpsertPopulationActorProfile( UpsertPopulationTroopDefinition(\n"
            "setAccessibleName setTabOrder\n"
            "Actor filter refresh is deferred because an actor,\n"
            "Troop filter refresh is deferred because an actor,\n"
            "Member selection was not changed because the member editor\n"
            "connect(\n            m_actorFilter,\n"
            "if (HasDirtyDrafts()) { return; }\n"
            "RefreshRecordChoices(); LoadCurrentActor();\n"
            "connect(\n            m_troopFilter,\n"
            "if (HasDirtyDrafts()) { return; }\n"
            "RefreshRecordChoices(); LoadCurrentTroop();\n"
            "connect(\n            m_actorRecord,\n"
            "void ActorTroopEditorWidget::closeEvent(QCloseEvent* event) {\n"
            "if (!HasDirtyDrafts()) { return; }\n"
            "QMessageBox::Discard; event->ignore();\n"
            "}\n"
            "void ActorTroopEditorWidget::OnFoundationChanged() {\n"
            "if (HasDirtyDrafts()) {\n"
            "m_foundationRefreshPending = true; return;\n"
            "}\nRefreshAll();\n}\n"
            "void ActorTroopEditorWidget::RefreshAll() {}\n"
            "void ActorTroopEditorWidget::ApplyPendingFoundationRefresh() {\n"
            "if (HasDirtyDrafts() || !m_foundationRefreshPending) { return; }\n"
            "m_foundationRefreshPending = false; RefreshAll();\n}\n"
            "void ActorTroopEditorWidget::MarkActorDirty() {}\n"
            "void ActorTroopEditorWidget::HandleActorRecordChange() {\n"
            "if (m_actorDirty) { const QSignalBlocker blocker; return; }\n"
            "LoadCurrentActor();\n}\n"
            "void ActorTroopEditorWidget::HandleTroopRecordChange() {\n"
            "if (m_troopDirty || m_memberEditorDirty) {\n"
            "const QSignalBlocker blocker; return;\n}\n"
            "LoadCurrentTroop();\n}\n"
            "void ActorTroopEditorWidget::HandleMemberSelectionChange() {\n"
            "if (m_memberEditorDirty) {\n"
            "RefreshMemberTable(); return;\n}\n"
            "LoadSelectedMember();\n}\n"
            "void ActorTroopEditorWidget::RefreshRecordChoices() {}\n"
            "void ActorTroopEditorWidget::SaveActorProfile() {\n"
            "m_actorDirty = false;\n"
            "m_foundationRefreshPending = true;\n"
            "ApplyPendingFoundationRefresh();\n}\n"
            "void ActorTroopEditorWidget::SaveTroopDefinition() {\n"
            "if (m_memberEditorDirty && !StageMember()) { return; }\n"
            "m_troopDirty = false;\n"
            "m_memberEditorDirty = false;\n"
            "m_foundationRefreshPending = true;\n"
            "ApplyPendingFoundationRefresh();\n}\n"
            "void ActorTroopEditorWidget::RevertActorProfile() {}\n",
        )
        self._write(
            source + "TaintedGrailModdingSDKSystemComponent.cpp",
            '#include "ActorTroopEditorWidget.h"\n'
            'ActorTroopEditorViewPaneName = "Tainted Grail Actor and Troop Editor"\n'
            "RegisterViewPane<ActorTroopEditorWidget>\n"
            "UnregisterViewPane(ActorTroopEditorViewPaneName)\n"
            "TaintedGrailModdingSDK.ActorTroopEditor\n",
        )
        self._write(
            tests + "PopulationActionLaneServiceTests.cpp",
            "ClosedVocabularyUsesDeterministicContractValues\n"
            "FixedLaneOrderAndHardUnavailableRuntimeLanes\n"
            "AuthoringRequiresResolvedWorkspacePackAndExactEvidence\n"
            "TroopCompositionPlanningAndSpawnCandidacyFailClosed\n"
            "SpawnCandidateRejectsUnvalidatedStaleMissingConflictedAndSupersededRecords\n"
            "ForbiddenAndRelevantBlockersTakePrecedenceDeterministically\n"
            "UnrelatedBlockersDoNotPoisonSubjectAndInputsRemainUnchanged\n"
            "MissingOrWrongSelectionFailsClosed\n"
            "recordCountBefore blockersBefore sourceCountBefore evidenceCountBefore\n",
        )
        self._write(
            "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md",
            "6. **Complete** \u2014 immutable population action-lane derivation\n"
            "7. **Next** \u2014 deterministic synthetic population fixture\n"
            "independently tracked actor, troop, and unstaged-member drafts\n"
            "exact-head compiled test run\nWindows UI review\n",
        )

    def test_valid_contract_passes(self) -> None:
        validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_qt_in_core_service(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationActionLaneService.cpp",
            "#include <QFile>\n",
        )
        with self.assertRaisesRegex(PopulationEditorContractError, "forbidden fragment"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_path_policy_resolved_workspace_root(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationActionLaneService.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "resolvedWorkspaceRoot.empty()", "resolved-root-check-removed"
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(PopulationEditorContractError, "resolvedWorkspaceRoot.empty"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_direct_troop_member_write_from_widget(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp",
            "UpsertPopulationTroopMember(\n",
        )
        with self.assertRaisesRegex(PopulationEditorContractError, "forbidden fragment"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_optimistic_runtime_lane(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationActionLaneService.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (lane == PopulationActionLane::RuntimeSpawn)\n"
            "decision.m_state = PopulationActionLaneState::Unavailable",
            "if (lane == PopulationActionLane::RuntimeSpawn)\n"
            "decision.m_state = PopulationActionLaneState::Allowed",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(PopulationEditorContractError, "Runtime-spawn lane"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_non_unavailable_save_lane(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationActionLaneService.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (lane == PopulationActionLane::SaveMutation)\n"
            "decision.m_state = PopulationActionLaneState::Unavailable",
            "if (lane == PopulationActionLane::SaveMutation)\n"
            "decision.m_state = PopulationActionLaneState::Forbidden",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(PopulationEditorContractError, "Save-mutation lane"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_direct_persistence_from_widget(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp",
            "SaveCatalog(\n",
        )
        with self.assertRaisesRegex(PopulationEditorContractError, "forbidden fragment"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_lifecycle_unregistration(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/"
            "TaintedGrailModdingSDKSystemComponent.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "UnregisterViewPane(ActorTroopEditorViewPaneName)",
            "unregister-removed",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(PopulationEditorContractError, "UnregisterViewPane"):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_foundation_refresh_to_preserve_dirty_drafts(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (HasDirtyDrafts()) {\n"
            "m_foundationRefreshPending = true; return;",
            "if (false) {\n"
            "m_foundationRefreshPending = true; return;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Foundation dirty-draft refresh guard"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_cross_tab_destructive_refresh_after_save(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "void ActorTroopEditorWidget::SaveActorProfile() {\n"
            "m_actorDirty = false;",
            "void ActorTroopEditorWidget::SaveActorProfile() {\n"
            "RefreshAll();\n"
            "m_actorDirty = false;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Cross-tab save draft preservation"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_cross_tab_dirty_flag_clear_after_save(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "void ActorTroopEditorWidget::SaveActorProfile() {\n"
            "m_actorDirty = false;",
            "void ActorTroopEditorWidget::SaveActorProfile() {\n"
            "m_troopDirty = false;\n"
            "m_actorDirty = false;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError,
            "Actor save cross-tab dirty-state preservation",
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_pending_refresh_dirty_guard(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (HasDirtyDrafts() || !m_foundationRefreshPending) { return; }",
            "if (!m_foundationRefreshPending) { return; }",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Pending Foundation refresh guard"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_filter_dirty_guard(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "connect(\n            m_actorFilter,\n"
            "if (HasDirtyDrafts()) { return; }",
            "connect(\n            m_actorFilter,\n"
            "if (false) { return; }",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Actor filter dirty-draft guard"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_each_record_selection_blocker(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (m_troopDirty || m_memberEditorDirty) {\n"
            "const QSignalBlocker blocker; return;",
            "if (m_troopDirty || m_memberEditorDirty) {\n"
            "return;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Troop record selection restoration"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_requires_unstaged_member_selection_guard(self) -> None:
        path = self.repo_root / (
            "Gems/TaintedGrailModdingSDK/Code/Source/ActorTroopEditorWidget.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "if (m_memberEditorDirty) {\n"
            "RefreshMemberTable(); return;",
            "if (false) {\n"
            "RefreshMemberTable(); return;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            PopulationEditorContractError, "Member editor dirty selection guard"
        ):
            validate_population_actor_troop_editor(self.repo_root)

    def test_rejects_stale_unit_status(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
        text = path.read_text(encoding="utf-8").replace(
            "6. **Complete**", "6. **Next**"
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(PopulationEditorContractError, r"6\. \*\*Complete"):
            validate_population_actor_troop_editor(self.repo_root)


if __name__ == "__main__":
    unittest.main()
