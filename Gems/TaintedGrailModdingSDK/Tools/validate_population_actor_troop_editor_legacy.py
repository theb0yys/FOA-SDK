#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the unit-6 population action-lane and Actor/Troop Editor contract."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Iterable


class PopulationEditorContractError(RuntimeError):
    """Raised when the unit-6 population editor boundary is incomplete or unsafe."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise PopulationEditorContractError(
            f"Required population editor file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise PopulationEditorContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise PopulationEditorContractError(
                f"{label} contains forbidden fragment {fragment!r}."
            )


def require_order(text: str, fragments: Iterable[str], label: str) -> None:
    position = -1
    for fragment in fragments:
        next_position = text.find(fragment, position + 1)
        if next_position < 0:
            raise PopulationEditorContractError(
                f"{label} is missing ordered fragment {fragment!r}."
            )
        position = next_position


def section_between(text: str, start_marker: str, end_marker: str, label: str) -> str:
    start = text.find(start_marker)
    end = text.find(end_marker, start + len(start_marker))
    if start < 0 or end < 0:
        raise PopulationEditorContractError(
            f"{label} is missing its bounded decision section."
        )
    return text[start:end]


def validate_population_actor_troop_editor(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = read_text(
        code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    )
    require_fragments(
        core_manifest,
        (
            "Source/PopulationActionLaneService.cpp",
            "Source/PopulationActionLaneService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        ("Source/ActorTroopEditorWidget.cpp", "Source/ActorTroopEditorWidget.h"),
        "Editor manifest",
    )
    require_fragments(
        test_manifest,
        ("Tests/PopulationActionLaneServiceTests.cpp",),
        "Catalog test manifest",
    )

    service_header = read_text(source_root / "PopulationActionLaneService.h")
    service_source = read_text(source_root / "PopulationActionLaneService.cpp")
    service = service_header + "\n" + service_source
    require_fragments(
        service,
        (
            "enum class PopulationActionLane",
            "enum class PopulationActionLaneState",
            "struct PopulationActionLaneDecision",
            "m_blockerIds",
            "m_reasons",
            "class PopulationActionLaneService final",
            "BuildActionLaneMatrix",
            "const AZStd::string& resolvedWorkspaceRoot",
            "resolvedWorkspaceRoot.empty()",
            "catalog.ValidateIntegrity(",
            "record.m_forbiddenUsages",
            "record.m_allowedUsages",
            "CollectBlockers",
            'blocker.m_status == "open"',
            "SortUnique(decision.m_blockerIds)",
            "SortUnique(decision.m_reasons)",
            'return "unavailable"',
        ),
        "Population action-lane service",
    )
    require_order(
        service_source,
        (
            "PopulationActionLane::Display,",
            "PopulationActionLane::AuthorProfile,",
            "PopulationActionLane::ComposeTroop,",
            "PopulationActionLane::Planning,",
            "PopulationActionLane::SpawnCandidate,",
            "PopulationActionLane::RuntimeSpawn,",
            "PopulationActionLane::SaveMutation,",
        ),
        "Fixed population action-lane order",
    )
    require_fragments(
        service_source,
        (
            "if (lane == PopulationActionLane::RuntimeSpawn)",
            "if (lane == PopulationActionLane::SaveMutation)",
            "decision.m_state = PopulationActionLaneState::Unavailable",
            "Spawn candidacy requires a validated, current,",
            "unresolved-free, non-superseded catalog record.",
            "Catalog governance explicitly forbids this population action lane.",
            "The selected population record has no complete",
            "persisted typed profile or troop composition.",
        ),
        "Fail-closed population lane derivation",
    )
    require_order(
        service_source,
        (
            "if (Contains(record.m_forbiddenUsages, laneName))",
            "Contains(record.m_allowedUsages, laneName)",
        ),
        "Forbidden-before-allowed population lane precedence",
    )
    runtime_section = section_between(
        service_source,
        "if (lane == PopulationActionLane::RuntimeSpawn)",
        "if (lane == PopulationActionLane::SaveMutation)",
        "Runtime-spawn lane",
    )
    save_section = section_between(
        service_source,
        "if (lane == PopulationActionLane::SaveMutation)",
        "if (lane == PopulationActionLane::ComposeTroop",
        "Save-mutation lane",
    )
    for section, label in (
        (runtime_section, "Runtime-spawn lane"),
        (save_section, "Save-mutation lane"),
    ):
        require_fragments(
            section,
            ("PopulationActionLaneState::Unavailable", "return decision;"),
            label,
        )
        reject_fragments(
            section,
            (
                "PopulationActionLaneState::Allowed",
                "PopulationActionLaneState::Unset",
                "PopulationActionLaneState::Blocked",
                "PopulationActionLaneState::Forbidden",
                "PopulationActionLaneState::NotApplicable",
            ),
            label,
        )
    reject_fragments(
        service,
        (
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            '"FoundationService.h"',
            "Upsert",
            "SaveCatalog",
            "ApplyCatalogGovernanceDecision",
            "ApplyCatalogValidationDecision",
            "QProcess",
            "ProcessLaunchInfo",
            "BepInEx",
            "HarmonyLib",
        ),
        "Core population action-lane service",
    )

    widget_header = read_text(source_root / "ActorTroopEditorWidget.h")
    widget_source = read_text(source_root / "ActorTroopEditorWidget.cpp")
    widget = widget_header + "\n" + widget_source
    require_fragments(
        widget,
        (
            "class ActorTroopEditorWidget final",
            "FoundationNotificationBus::Handler",
            "m_actorFilter",
            "m_troopFilter",
            "m_actorTemplateRecord",
            "m_actorTemplateSubject",
            "m_troopLeaderRecord",
            "m_troopLeaderSubject",
            "m_memberActorRecord",
            "m_memberActorSubject",
            "m_actorEvidenceDetails",
            "m_troopEvidenceDetails",
            "m_memberEvidenceDetails",
            "m_actorGovernanceSummary",
            "m_actorBlockerSummary",
            "m_actorRelationshipSummary",
            "m_troopGovernanceSummary",
            "m_troopBlockerSummary",
            "m_troopRelationshipSummary",
            "m_actorActionLanes",
            "m_troopActionLanes",
            "m_draftMembers",
            "StageMember",
            "SaveActorProfile",
            "SaveTroopDefinition",
            "RevertActorProfile",
            "RevertTroopDefinition",
            "closeEvent(QCloseEvent* event)",
            "m_actorDirty",
            "m_troopDirty",
            "m_memberEditorDirty",
            "m_foundationRefreshPending",
            "HasDirtyDrafts",
            "HandleActorRecordChange",
            "HandleTroopRecordChange",
            "HandleMemberSelectionChange",
            "ApplyPendingFoundationRefresh",
            "QAbstractItemView::NoEditTriggers",
            "ConfigureTable(m_actorActionLanes)",
            "ConfigureTable(m_troopActionLanes)",
            "FoundationNotificationBus::Handler::BusConnect",
            "FoundationNotificationBus::Handler::BusDisconnect",
            "PopulationActionLaneService",
            "BuildActionLaneMatrix(",
            "GetWorkspaceRootPath()",
            "UpsertPopulationActorProfile(",
            "UpsertPopulationTroopDefinition(",
            "setAccessibleName",
            "setTabOrder",
            "Actor filter refresh is deferred because an actor,",
            "Troop filter refresh is deferred because an actor,",
            "Member selection was not changed because the member editor",
        ),
        "Actor/Troop Editor widget",
    )

    close_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::closeEvent(QCloseEvent* event)",
        "void ActorTroopEditorWidget::OnFoundationChanged()",
        "Actor/Troop dirty close guard",
    )
    require_fragments(
        close_section,
        ("HasDirtyDrafts()", "QMessageBox::Discard", "event->ignore();"),
        "Actor/Troop dirty close guard",
    )
    foundation_change_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::OnFoundationChanged()",
        "void ActorTroopEditorWidget::RefreshAll()",
        "Foundation dirty-draft refresh guard",
    )
    pending_refresh_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::ApplyPendingFoundationRefresh()",
        "void ActorTroopEditorWidget::MarkActorDirty()",
        "Pending Foundation refresh guard",
    )
    require_order(
        pending_refresh_section,
        ("HasDirtyDrafts()", "return;", "m_foundationRefreshPending = false;", "RefreshAll();"),
        "Pending Foundation refresh guard",
    )
    actor_filter_section = section_between(
        widget_source,
        "            m_actorFilter,",
        "            m_troopFilter,",
        "Actor filter dirty-draft guard",
    )
    troop_filter_section = section_between(
        widget_source,
        "            m_troopFilter,",
        "            m_actorRecord,",
        "Troop filter dirty-draft guard",
    )
    require_order(
        actor_filter_section,
        ("HasDirtyDrafts()", "return;", "RefreshRecordChoices();", "LoadCurrentActor();"),
        "Actor filter dirty-draft guard",
    )
    require_order(
        troop_filter_section,
        ("HasDirtyDrafts()", "return;", "RefreshRecordChoices();", "LoadCurrentTroop();"),
        "Troop filter dirty-draft guard",
    )
    require_order(
        foundation_change_section,
        (
            "if (HasDirtyDrafts())",
            "m_foundationRefreshPending = true;",
            "return;",
            "RefreshAll();",
        ),
        "Foundation dirty-draft refresh guard",
    )
    actor_selection_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::HandleActorRecordChange()",
        "void ActorTroopEditorWidget::HandleTroopRecordChange()",
        "Actor dirty selection guard",
    )
    troop_selection_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::HandleTroopRecordChange()",
        "void ActorTroopEditorWidget::HandleMemberSelectionChange()",
        "Troop dirty selection guard",
    )
    member_selection_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::HandleMemberSelectionChange()",
        "void ActorTroopEditorWidget::RefreshRecordChoices()",
        "Member editor dirty selection guard",
    )
    for section, dirty_fragment, label in (
        (actor_selection_section, "m_actorDirty", "Actor dirty selection guard"),
        (troop_selection_section, "m_troopDirty", "Troop dirty selection guard"),
        (
            member_selection_section,
            "m_memberEditorDirty",
            "Member editor dirty selection guard",
        ),
    ):
        require_order(
            section,
            (dirty_fragment, "return;"),
            label,
        )
    require_fragments(
        actor_selection_section,
        ("const QSignalBlocker blocker",),
        "Actor record selection restoration",
    )
    require_fragments(
        troop_selection_section,
        ("const QSignalBlocker blocker",),
        "Troop record selection restoration",
    )
    require_order(
        member_selection_section,
        ("m_memberEditorDirty", "RefreshMemberTable();", "return;", "LoadSelectedMember();"),
        "Member editor dirty selection guard",
    )

    actor_save_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::SaveActorProfile()",
        "void ActorTroopEditorWidget::SaveTroopDefinition()",
        "Actor save refresh boundary",
    )
    troop_save_section = section_between(
        widget_source,
        "void ActorTroopEditorWidget::SaveTroopDefinition()",
        "void ActorTroopEditorWidget::RevertActorProfile()",
        "Troop save refresh boundary",
    )
    require_order(
        actor_save_section,
        (
            "m_actorDirty = false;",
            "m_foundationRefreshPending = true;",
            "ApplyPendingFoundationRefresh();",
        ),
        "Actor save refresh boundary",
    )
    require_order(
        troop_save_section,
        (
            "if (m_memberEditorDirty && !StageMember())",
            "m_troopDirty = false;",
            "m_memberEditorDirty = false;",
            "m_foundationRefreshPending = true;",
            "ApplyPendingFoundationRefresh();",
        ),
        "Troop save refresh boundary",
    )
    reject_fragments(
        actor_save_section + troop_save_section,
        ("RefreshAll();", "LoadCurrentActor();", "LoadCurrentTroop();"),
        "Cross-tab save draft preservation",
    )
    reject_fragments(
        actor_save_section,
        ("m_troopDirty = false;", "m_memberEditorDirty = false;"),
        "Actor save cross-tab dirty-state preservation",
    )
    reject_fragments(
        troop_save_section,
        ("m_actorDirty = false;",),
        "Troop save cross-tab dirty-state preservation",
    )
    reject_fragments(
        widget,
        (
            "UpsertPopulationTroopProfile(",
            "UpsertPopulationTroopMember(",
            "SaveCatalog(",
            "CatalogPersistenceService",
            "ApplyCatalogGovernanceDecision",
            "ApplyCatalogValidationDecision",
            "QFile",
            "QSaveFile",
            "QProcess",
            "ProcessLaunchInfo",
            "BepInEx",
            "HarmonyLib",
            "SpawnLocation(",
            ".erase(",
        ),
        "Actor/Troop Editor widget",
    )

    lifecycle = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        lifecycle,
        (
            '#include "ActorTroopEditorWidget.h"',
            'ActorTroopEditorViewPaneName = "Tainted Grail Actor and Troop Editor"',
            "RegisterViewPane<ActorTroopEditorWidget>",
            "UnregisterViewPane(ActorTroopEditorViewPaneName)",
            "TaintedGrailModdingSDK.ActorTroopEditor",
        ),
        "Actor/Troop Editor lifecycle",
    )

    tests = read_text(tests_root / "PopulationActionLaneServiceTests.cpp")
    require_fragments(
        tests,
        (
            "ClosedVocabularyUsesDeterministicContractValues",
            "FixedLaneOrderAndHardUnavailableRuntimeLanes",
            "AuthoringRequiresResolvedWorkspacePackAndExactEvidence",
            "TroopCompositionPlanningAndSpawnCandidacyFailClosed",
            "SpawnCandidateRejectsUnvalidatedStaleMissingConflictedAndSupersededRecords",
            "ForbiddenAndRelevantBlockersTakePrecedenceDeterministically",
            "UnrelatedBlockersDoNotPoisonSubjectAndInputsRemainUnchanged",
            "MissingOrWrongSelectionFailsClosed",
            "recordCountBefore",
            "blockersBefore",
            "sourceCountBefore",
            "evidenceCountBefore",
        ),
        "Compiled population action-lane tests",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md")
    require_fragments(
        design,
        (
            "6. **Complete** \u2014 immutable population action-lane derivation",
            "7. **Complete** \u2014 deterministic synthetic population fixture",
            "9. **Active acceptance gate** \u2014 exact-head O3DE configure/build",
            "independently tracked actor, troop, and unstaged-member drafts",
            "does not claim that compiled tests have run in an exact-head configured build",
            "Windows UI evidence exists",
        ),
        "Actor/Troop implementation status",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_population_actor_troop_editor(repo_root)
    except (OSError, PopulationEditorContractError) as exc:
        print(f"Tainted Grail population editor validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail population editor unit 6 passed: fixed fail-closed Core lanes, "
        "thin Actor/Troop authoring UI, lifecycle wiring, compiled-test source coverage, "
        "and runtime separation are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
