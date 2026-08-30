# FOA-QUEST-001 QuestDefinition Contract Gate

## Decision

Status: Accepted for the first quest/mission layer slice.

Accepted by: current task requester, treated as maintainer authority for this Codex task.

Accepted UTC: 2026-07-31T18:27:17Z

Accepted branch: `codex/designed-installer-wizard`

Accepted base revision: `54e72121e6da6a46f5196a8a07e2ce3873503c52`

Authorized document path: `Research/quest-mission-layer/gates/FOA_QUEST_001_QUEST_DEFINITION_CONTRACT_GATE.md`

This gate approves a contract-only first slice for the mission layer. The slice is limited to QuestDefinition contracts, stable IDs, schema validation, deterministic canonical JSON, malformed-input tests, and explicit no-runtime/no-editor-mutation boundaries.

Runtime adapters, O3DE authoring UI, Script Canvas authoring nodes, Unity conversion, game launch, save mutation, deployment, and exact Fall of Avalon runtime proof remain later gates.

## Authority Map

```yaml
gate_id: FOA-QUEST-001
gate_name: QuestDefinition Contract Gate
quest_definition_contract_gate_accepted: true
future_contract_code_slice_authorized_under_this_gate: true
quest_definition_v1_contract_authorized: true
stable_quest_id_vocabulary_authorized: true
schema_validation_authorized: true
deterministic_canonical_json_authorized: true
malformed_input_tests_authorized: true
synthetic_fixtures_authorized: true
runtime_state_authorized: false
runtime_adapter_authorized: false
o3de_authoring_ui_authorized: false
script_canvas_authoring_authorized: false
unity_conversion_authorized: false
foa_game_launch_authorized: false
foa_save_mutation_authorized: false
deployment_authorized: false
protected_game_data_authorized: false
fall_of_avalon_runtime_signoff_performed: false
```

Passing this gate is not runtime sign-off. It does not prove compatibility with native Fall of Avalon quests, saves, journal, dialogue, inventory, combat, scenes, spawning, AI, or time systems.

## Governing Requirements

- O3DE is the authoring host. Fall of Avalon runtime integration must remain separated behind reviewed adapters.
- Quest definitions are inert, engine-neutral data. A QuestDefinition document cannot grant runtime permission, editor mutation permission, save mutation permission, deployment permission, or asset access.
- Durable JSON formats require explicit schema versions, exact IDs, deterministic serialization, compatibility policy, and fail-closed handling for unknown or future versions.
- Missing proof fails closed. The first contract slice must not invent native FoA hook points, native quest IDs, save structures, journal structures, or runtime capability claims.
- The first slice must use synthetic fixtures only. It must not include proprietary game assets, private paths, decompiled source, runtime logs, save files, or extracted content.
- O3DE UI, Script Canvas facades, authoring panes, runtime adapters, installer behavior, and exact-install runtime proof are separate evidence lanes and must not substitute for this contract gate.

## Authorized Scope

The next implementation slice may add a pure contract surface for a versioned quest-definition document. It may include only inert model types, schema-validation support, deterministic canonical JSON support, synthetic fixtures, and malformed-input tests needed to prove the contract behavior.

The intended first contract is:

```text
schema id: foa.quest-definition
schema version: 1
document kind: QuestDefinition
suggested file form: *.tgquest.json
authority: inert contract data only
```

The first slice may define stable IDs for quest-contract identity, including:

- `QuestId`
- `QuestPhaseId`
- `QuestObjectiveId`
- `QuestTransitionId`
- `QuestRoleId`
- `QuestConditionId`
- `QuestActionId`
- `QuestOutcomeId`
- `QuestTextKey`

IDs must be stable, case-normalized where the contract specifies, deterministic under canonicalization, and separate from display text. Display names, localized strings, file names, raw entity IDs, Unity instance IDs, O3DE entity IDs, native pointers, absolute paths, and discovered runtime identifiers must not be accepted as stable quest IDs.

## Minimum QuestDefinition V1 Shape

The implementation may refine exact names during code review, but the contract must cover these conceptual fields before promotion:

- Schema identity and schema version.
- Quest identity and content version.
- Owning pack or module identity.
- Human-facing display/localization references.
- Quest lifecycle declaration.
- Actor or object roles by stable role ID.
- Phases by stable phase ID.
- Objectives by stable objective ID.
- Transitions by stable transition ID.
- Conditions and actions by registered stable IDs.
- Outcomes and terminal states.
- Binding requirements expressed as logical requirements, not resolved runtime objects.
- Compatibility and authority metadata.

The document must not contain executable code, script bodies, native method names treated as callable authority, runtime object handles, raw save paths, absolute filesystem paths, or adapter-specific payloads.

## Deterministic Canonical JSON Rules

The canonical QuestDefinition representation must be deterministic from semantic input. The first implementation slice must establish and test rules for:

- Stable object member ordering.
- Stable ordering for set-like arrays where order is not semantically meaningful.
- Preserving authored order only where the schema declares order meaningful.
- Normalized IDs before fingerprinting.
- Stable numeric, boolean, null, and string representation.
- Stable escaping behavior.
- No timestamps, machine paths, random values, GUID generation at canonicalization time, or host-specific values in canonical output.
- Excluding any computed fingerprint field from its own fingerprint input.

Two semantically identical QuestDefinition documents must produce byte-identical canonical JSON and identical fingerprints. Two semantically different documents must not be treated as equivalent by canonicalization.

## Required Validation Behavior

The validator must fail closed on malformed or ambiguous input. The first implementation slice must include malformed-input tests for at least:

- Missing required schema fields.
- Unknown or future schema version.
- Unknown public fields unless the schema explicitly reserves an extension block.
- Duplicate IDs inside and across the relevant quest-definition namespaces.
- IDs derived from display text instead of stable identifiers.
- Invalid ID casing, whitespace, separators, or namespace form.
- Transition references to unknown phases, objectives, conditions, actions, or outcomes.
- Missing entry phase or missing terminal outcome.
- Ambiguous transitions with the same trigger and priority where no deterministic tie-break policy exists.
- Cycles without an explicit repeat or loop policy.
- Unknown condition/action registration IDs.
- Unbounded recursion, excessive nesting, excessive counts, or oversized strings.
- Absolute, relative escape, UNC, URI, or private filesystem paths.
- Raw O3DE entity IDs, Unity instance IDs, native pointers, or extracted native object identifiers.
- Any authority flag or payload that attempts to enable runtime execution, editor mutation, save mutation, game launch, deployment, or asset extraction.

Validation must report stable issue codes suitable for tests. It must not repair documents silently, mutate the editor, scan the project, launch external tools, call O3DE Editor services, call Unity, or inspect a Fall of Avalon install.

## Explicit Boundaries

This gate does not authorize:

- `QuestRuntimeState`.
- Runtime save/load behavior.
- Runtime mutation, runtime event dispatch, runtime execution, or runtime capability grants.
- FoA Mono, IL2CPP, BepInEx, Harmony, or native adapter code.
- O3DE editor components, Qt panes, menus, Script Canvas nodes, Asset Processor rules, prefab generation, entity mutation, or authoring UI.
- Unity editor packages, Unity `.meta` files, Unity scene/prefab/material changes, or Unity batch conversion.
- Quest package deployment, installer changes, mod packaging, or game install writes.
- Native FoA quest, journal, dialogue, inventory, combat, scene, AI, time, spawn, or save integration claims.
- A proof quest fixture, title, or content. This contract-only gate does not select a proof-quest name; naming and scope require a later proof-quest gate.

## Owner Lanes

Primary owner lane for the first implementation slice: Foundation/Core contract and schema validation.

Supporting lanes:

- Canonical interchange may review canonical JSON and fingerprint rules, but package handoff is not authorized by this gate.
- Catalog and identity may review stable ID shape, but catalog mutation or catalog indexing is not authorized by this gate.
- Documentation may record the public contract after implementation, but this gate does not authorize broad documentation restructuring.

Deferred owner lanes:

- O3DE host/editor tooling.
- World authoring UI.
- Unity bridge and conversion.
- Runtime adapter contracts.
- Installer, packaging, deployment, and exact-install runtime proof.

## Compatibility And Migration

QuestDefinition V1 is a new contract. There is no backward migration requirement for legacy quest files in this gate.

Future schema versions require a separate reviewed compatibility or migration gate. Unknown future versions must fail closed until a reviewed migration path exists.

The implementation must not downgrade, opportunistically repair, or silently reinterpret invalid documents. Any repair, migration, import, or conversion behavior is a later gate.

## Evidence Required For The Implementation PR

A later implementation PR under this gate must provide:

- Authority evidence citing this gate and the controlling repository documents.
- A protected-file audit showing no protected Tales from the Age of Men overhaul files, proprietary game files, saves, extracted assets, runtime logs, or private paths were touched.
- Schema and model tests for valid QuestDefinition V1 documents.
- Malformed-input tests covering the required validation behavior above.
- Deterministic canonical JSON tests proving byte-stable output for semantically identical input.
- Negative tests proving runtime/editor mutation payloads are rejected.
- A compatibility note for unknown and future schema versions.
- A statement that no Fall of Avalon runtime sign-off was performed.

## Drift And Stop Conditions

Stop and request a new gate if the implementation needs to:

- Add runtime state, runtime execution, save handling, adapter code, or game-install writes.
- Add O3DE editor UI, Script Canvas nodes, entity mutation, prefab/material/asset generation, or Asset Processor behavior.
- Add Unity project files, Unity conversion, or Unity-facing authoring packages.
- Use proprietary Fall of Avalon data, extracted game files, save files, runtime logs, or private paths.
- Select, rename, or include a proof quest fixture or proof quest content.
- Change public schema compatibility, migration behavior, persistence rules, release policy, installer behavior, or protected external data policy.
- Depend on undocumented native FoA hook points or unverified runtime capabilities.

## Source Register

- `AGENTS.md`
- `README.md`
- `GOVERNANCE.md`
- `CONTRIBUTING.md`
- `docs/protected-files-policy.md`
- `docs/systems/SYSTEM_INDEX.md`
- `CURRENT_TASK.md`
- `DECISIONS.md`
- `.codex/workflows/foa_research_first_process_stack.md`
- `.codex/workflows/foa_sdk_test_gates.md`
- `.codex/workflows/foa_professional_code_performance_gate.md`
- `.codex/workflows/foa_artifact_deploy_gate.md`
- `.codex/checklists/deep_review.md`
- `.codex/checklists/evidence_pack_template.json`
- `Research/README.md`
- `docs/tainted-grail-sdk/README.md`
- `docs/tainted-grail-sdk/ARCHITECTURE.md`
- `docs/tainted-grail-sdk/DATA_FORMATS.md`
- `docs/tainted-grail-sdk/SDK_FUNCTIONAL_EXPANSION_PROGRAM.md`
- `ROADMAP.md`
- `docs/tainted-grail-modding/README.md`
- `docs/tainted-grail-modding/process/README.md`
- `docs/tainted-grail-modding/runtime/VERIFIED_PROFILES.md`
- `C:\Users\kane0\.codex\attachments\3efcaa48-87f1-4b60-bdb1-59462e91b573\pasted-text.txt`
- `C:\Users\kane0\.codex\attachments\0b3d6dca-30ab-46b3-8082-98d125f286d9\pasted-text.txt`
