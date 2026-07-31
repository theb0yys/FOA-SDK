# FOA-QUEST-002 QuestBindingManifest Contract Gate

## Decision

Status: proposed gate record requested in the current task. This gate becomes effective only after maintainer review and merge of the pull request.

Requested timestamp: 2026-07-31T22:07:07Z

Working branch: `codex/quest-binding-manifest-contract-gate`

Base revision: `2aed8c7c3601badb78218d84323876b510e0a82e`

Gate record path: `Research/quest-mission-layer/gates/FOA_QUEST_002_QUEST_BINDING_MANIFEST_CONTRACT_GATE.md`

This gate creates the focused authority for the next quest/mission-layer contract step. It does not implement the contract. After this gate is reviewed and merged, a later implementation PR may add the Core-only `QuestBindingManifest` contract, schema validation, deterministic canonical JSON, fingerprinting, and malformed-input tests inside the boundaries below.

## Authority Map

The following map describes the authority this gate would provide after maintainer PR acceptance. It is not runtime, editor, save, deployment, catalog-mutation, evidence-promotion, or permission-grant authority.

```yaml
gate_id: FOA-QUEST-002
gate_name: QuestBindingManifest Contract Gate
gate_document_only_slice_authorized: true
future_contract_code_slice_authorized_after_gate_merge: true
quest_binding_manifest_v1_contract_authorized: true
stable_binding_ids_authorized: true
catalog_reference_contracts_authorized: true
evidence_reference_contracts_authorized: true
permission_reference_contracts_authorized: true
deterministic_canonical_json_authorized: true
canonical_fingerprint_authorized: true
fail_closed_schema_validation_authorized: true
malformed_input_tests_authorized: true
synthetic_fixtures_authorized: true
quest_definition_v1_contract_change_authorized: false
quest_runtime_state_authorized: false
runtime_resolution_authorized: false
runtime_adapter_authorized: false
o3de_authoring_ui_authorized: false
script_canvas_authoring_authorized: false
unity_conversion_authorized: false
foa_game_launch_authorized: false
foa_save_mutation_authorized: false
deployment_authorized: false
catalog_mutation_authorized: false
evidence_promotion_authorized: false
permission_grant_authorized: false
protected_game_data_authorized: false
proof_quest_content_authorized: false
fall_of_avalon_runtime_signoff_performed: false
```

## Owning Systems

Primary owner: Foundation/Core contract surface, covering `schemas-and-persistence`, `catalog-and-identity`, `validation-and-evidence`, `permissions-and-risk`, and `canonical-interchange`.

Supporting owners: documentation and test harness for the future implementation PR.

Deferred owners: O3DE editor UI, Script Canvas, Unity conversion, runtime adapters, installer, deployment review, exact-install runtime evidence, and proof quest content.

## Contract Purpose

`QuestBindingManifest` is the inert bridge between an engine-neutral `QuestDefinition` and reviewed project knowledge. It records which reviewed catalog, evidence, and permission references satisfy a quest definition's declared binding requirements.

The manifest is not a resolver, importer, launcher, editor save service, deployment tool, runtime adapter, or permission grant. It must not perform catalog mutation, evidence promotion, permission decisions, game discovery, Unity conversion, FoA launch, save access, or runtime execution.

## Authorized Future Contract Shape

A later contract-only implementation may define schema version 1 with these bounded surfaces:

- schema identity: `foa.quest-binding-manifest`;
- schema version: `1`;
- candidate standalone suffix: `*.tgquestbindings.json` for contract fixtures and package documents only;
- stable `manifest_id`, `quest_id`, `owner_pack_id`, `owner_module_id`, and `content_version` fields;
- an exact `quest_definition_fingerprint` binding to the upstream `QuestDefinition` canonical payload;
- immutable catalog context references such as `catalog_id`, `catalog_fingerprint`, `profile_id`, `game_version`, `branch`, and `runtime_target` when supplied by a reviewed caller;
- `role_bindings` connecting declared quest roles and binding requirements to catalog references;
- `location_bindings`, `anchor_bindings`, `item_bindings`, `reward_bindings`, `dialogue_bindings`, `journal_bindings`, and other logical resource bindings only as stable contract rows;
- `evidence_refs` that bind each row to evidence IDs, source IDs, source fingerprints, and profile/build context;
- `permission_refs` that point to reviewed usage-specific permission decisions or requirements;
- explicit `authority` flags that remain false for runtime, editor mutation, save mutation, deployment, catalog mutation, evidence promotion, permission grant, and asset extraction;
- optional `manifest_fingerprint`, excluded from its own canonical projection.

The implementation may validate by value against immutable reference snapshots supplied by tests or callers. It must not query live services, repair catalog state, promote evidence, grant permissions, or write workspace documents in this gate.

## Stable ID Rules

- Binding IDs are stable contract identities, not display names, native object references, paths, URIs, GUIDs, engine handles, Script Canvas node IDs, Unity instance IDs, or O3DE entity IDs.
- Binding IDs are namespaced and immutable after publication.
- Deleted binding IDs remain reserved for compatibility.
- Display text, authoring layout, graph positions, and editor selection state are not identity.
- Duplicate IDs fail validation within their declared scope.
- A binding row that references a `QuestDefinition` role, binding requirement, action, condition, objective, or outcome must point to an ID declared by the supplied quest definition snapshot.

## Reference Contracts

Catalog references must point to reviewed catalog records by stable record ID and, where available, immutable catalog fingerprint/profile context. A catalog reference must not embed raw O3DE, Unity, or FoA runtime handles in V1.

Evidence references must point to evidence IDs and source fingerprints. Evidence is support for a claim; it is not a reviewed catalog record and does not grant permission.

Permission references must point to reviewed usage-specific governance or validation decisions, or explicitly record the missing decision as a blocker. A permission reference is never an allow flag by itself. Missing, stale, mismatched, unsupported, or unresolved permission status fails closed for the affected binding.

## Canonical JSON and Fingerprint Requirements

The future implementation must produce deterministic canonical JSON:

- fixed top-level field order;
- stable object field order;
- sorted set-like arrays on canonical copies;
- duplicate values rejected before canonicalization;
- lowercase `sha256:<64-hex-digits>` fingerprints over the canonical JSON projection;
- `manifest_fingerprint` excluded from its own projection;
- embedded upstream quest fingerprints included and checked exactly;
- no normalization of stable IDs, exact fingerprints, or native catalog references beyond rules explicitly defined by the schema.

Semantically equivalent manifests must serialize to byte-identical canonical JSON. Semantically different manifests must produce different canonical payloads and fingerprints.

## Required Fail-Closed Validation

A future implementation PR under this gate must include malformed-input and negative tests that reject at least:

- invalid JSON, empty JSON, non-object roots, excessive nesting, excessive document size, oversized strings, and excessive row counts;
- missing `schema`, unsupported schema versions, malformed version values, unknown public fields, wrong field types, and missing required fields;
- invalid stable IDs, duplicate IDs, display names as IDs, absolute paths, path traversal, URIs, GUID-like values, O3DE entity IDs, Unity instance IDs, and other native handles in public ID fields;
- missing or mismatched `quest_id` and `quest_definition_fingerprint`;
- bindings that reference undeclared roles or binding requirements from the supplied quest definition snapshot;
- binding rows whose subject kind or usage conflicts with the corresponding quest definition binding requirement;
- missing catalog references where a binding requires a reviewed catalog subject;
- evidence references with missing IDs, mismatched source fingerprints, mismatched profile/build context, or duplicate evidence IDs;
- permission references with missing decisions, stale decisions, mismatched usage, mismatched subject, denied status, or unresolved status;
- mutually exclusive bindings for a unique role, duplicate required bindings, and conflicting fallback policies;
- any true authority flag for runtime execution, editor mutation, save mutation, deployment, catalog mutation, evidence promotion, permission grant, or asset extraction;
- any attempt to treat parser success, catalog reference presence, evidence presence, or permission-reference presence as Fall of Avalon runtime proof.

Validation must return deterministic issue ordering. Missing proof is a blocker, not a warning that can be ignored for compilation or runtime use.

## Explicit Non-Goals

This gate does not authorize:

- changes to `QuestDefinitionV1`;
- `QuestRuntimeState`, quest instances, lifecycle execution, event dispatch, action ledgers, schedules, save/load, or migration of active saves;
- runtime adapter contracts, Mono or IL2CPP code, BepInEx, Harmony, Unity APIs, FoA APIs, or game launch;
- O3DE authoring UI, components, panes, Script Canvas nodes, Graph Canvas tools, assets, prefabs, spawnables, or editor persistence;
- Unity conversion, compiled quest packages, package assembly, installer changes, deployment previews, rollback, signing, publication, or release work;
- catalog writes, evidence promotion, permission grants, or automatic review decisions;
- protected game files, proprietary assets, private paths, saves, extracted commercial content, or exact-install runtime evidence;
- selection of a proof quest title, story content, fixtures, or mission sample.

## Deferred Research Conflict

The supplied quest research uses both `The Silent Cart` and `The Missing Cart` as notice-board proof candidates. That naming and fixture-content conflict is deferred. This gate resolves only the `QuestBindingManifest` contract boundary and does not bake either title, story premise, sample data, or proof quest route into the first binding-manifest slice.

## Compatibility and Migration

This gate introduces no repository implementation and no durable workspace migration. The future implementation must treat V1 as a new contract surface and reject unknown future versions unless a later migration gate authorizes migration. Existing `QuestDefinition` canonical output, catalog documents, evidence documents, workspace files, pack manifests, adapter contracts, and installer state must remain unchanged unless a later PR explicitly authorizes and validates those changes.

If the future implementation introduces a public document suffix or fields, the same PR must update the public data-format documentation and include compatibility notes. Static-only validation must not be described as an O3DE host build, Unity conversion, adapter proof, deployment proof, save proof, or Fall of Avalon runtime sign-off.

## Evidence Required For The Future Implementation PR

The implementation PR that consumes this gate must provide:

- authority citation to this gate and the merged `QuestDefinition` contract surface;
- protected-file audit showing no protected game data, private paths, saves, generated engine output, or proprietary assets were committed;
- schema/model tests for valid minimal and valid complete manifests;
- canonical JSON and fingerprint determinism tests;
- malformed-input tests covering the fail-closed cases above;
- reference-contract tests for catalog, evidence, and permission references using synthetic fixtures only;
- no-mutation assertions proving the contract does not query live services, mutate catalog state, promote evidence, grant permissions, save editor documents, deploy files, or launch FoA;
- compatibility review for producers, consumers, versioning, rejection behavior, and migration status;
- focused static validation and the applicable Core-only compiled tests for the exact reviewed head;
- explicit statement that Fall of Avalon runtime sign-off was not performed unless separate exact-install evidence is captured under a later runtime gate.

## Stop Conditions

Stop and produce a Deep Research Brief instead of implementing if the next slice requires any of the following before a separate gate exists:

- runtime state or save semantics;
- native FoA quest/state binding semantics;
- O3DE UI or Script Canvas authoring behavior;
- Unity conversion or compiled package layout;
- runtime adapter capability execution;
- catalog mutation, evidence promotion, permission grant, or deployment authority;
- proof quest naming, story content, fixture data, or real game-data bindings;
- migration of an existing durable schema;
- protected external data or exact-install runtime proof.

## Sources Considered

Controlling repository authority:

- `AGENTS.md`;
- `README.md`;
- `GOVERNANCE.md`;
- `CONTRIBUTING.md`;
- `CURRENT_TASK.md`;
- `DECISIONS.md`;
- `docs/protected-files-policy.md`;
- `docs/systems/SYSTEM_INDEX.md`;
- `docs/tainted-grail-sdk/README.md`;
- `docs/tainted-grail-sdk/ARCHITECTURE.md`;
- `docs/tainted-grail-sdk/DATA_FORMATS.md`;
- `docs/tainted-grail-sdk/CODE_QUALITY.md`;
- `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`;
- `docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md`;
- `Research/README.md`;
- `.codex/workflows/foa_sdk_development_process.md`;
- `.codex/workflows/foa_research_first_process_stack.md`;
- `.codex/checklists/deep_review.md`;
- `.codex/checklists/evidence_pack_template.json`;
- `.github/PULL_REQUEST_TEMPLATE.md`;
- `.github/CODEOWNERS`.

Supporting current-state context:

- merged `QuestDefinition` Core contract and tests in `Gems/TaintedGrailModdingSDK/Code/Source/QuestDefinitionContract.*` and `Gems/TaintedGrailModdingSDK/Code/Tests/QuestDefinitionContractTests.cpp`;
- task-supplied `Portable Companion Quest Framework for Tainted Grail: The Fall of Avalon` research brief;
- task-supplied `FOA Quest SDK Architecture Research` brief.

## Next Researched Stop

After this gate is reviewed and merged, the next researched stop is a contract-only implementation PR for `QuestBindingManifestV1` in Core with schema validation, deterministic canonical JSON, canonical fingerprinting, malformed-input tests, synthetic reference-contract fixtures, and explicit no-runtime/editor/save/deployment mutation boundaries.
