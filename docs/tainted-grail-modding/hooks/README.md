# Tainted Grail Hook Catalogue

This folder contains exact, version-bound records for reviewed game extension points, patch targets, events, services, reflection targets, data handoffs, and adapter boundaries.

A hook record is documentation and evidence. It is not permission to patch a game installation or execute runtime code.

## Hook classes

- `lifecycle` — startup, scene, pause, shutdown, save, or service availability;
- `method-patch` — Harmony prefix, postfix, finalizer, or transpiler target;
- `event` — subscribed event or callback surface;
- `service` — resolved singleton, registry, manager, or interface;
- `reflection` — guarded member/type lookup where no stable public surface exists;
- `data` — reviewed read/write data structure or serialization handoff;
- `ui` — view, controller, input, HUD, menu, prompt, or localization surface;
- `asset` — addressable/resource/material/audio/VFX/animation loading surface;
- `adapter` — Mono, IL2CPP, BepInEx, Unity, or SDK boundary;
- `extension-api` — FOA-SDK public ExtensionAPI surface.

## Required record fields

```yaml
hook_id: tg.hook.<domain>.<stable-name>
title: human-readable title
status: candidate | observed | signature-verified | load-verified | behavior-verified | compatibility-reviewed | stale | blocked | deprecated
hook_class: lifecycle | method-patch | event | service | reflection | data | ui | asset | adapter | extension-api
domain: exact system domain
purpose: concise behavior or observation goal
profile:
  game_version: exact or unknown
  branch: exact or unknown
  runtime: Mono | IL2CPP | editor-only | engine-neutral
  loader: exact or not-applicable
  framework: exact or not-applicable
target:
  assembly: exact or unknown
  namespace: exact or unknown
  type: exact or unknown
  member: exact or unknown
  signature: exact or unknown
patch:
  style: prefix | postfix | finalizer | transpiler | subscribe | resolve | reflect | query | handoff
  ordering: known constraints
  cleanup: unpatch, unsubscribe, release, or not-applicable
context:
  lifecycle_point: exact point
  thread: known thread or unknown
  frequency: one-shot, event, frame, hot-path, or unknown
  inputs: reviewed inputs
  outputs: reviewed outputs
  side_effects: known effects
safety:
  nullability: guards
  failure_mode: fail-open | fail-closed | diagnostics-only
  save_risk: none | read | write | unknown
  performance_risk: low | medium | high | unknown
  story_risk: none | bounded | high | unknown
compatibility:
  known_conflicts: hook IDs, mod IDs, or unknown
  load_order: constraints
  version_stability: evidence-backed assessment
evidence:
  source_repository: owner/name
  source_commit: 40-character SHA
  source_paths: exact paths
  evidence_ids: canonical IDs
validation:
  static: result
  target_resolution: result
  load: result
  behavior: result
  combined_mods: result
permissions:
  allowed: explicit operations
  prohibited: explicit operations
limitations:
  - unresolved limitation
```

## Promotion rules

1. A patch occurrence in source creates only a `candidate` record.
2. An exact type/member match against the intended profile permits `signature-verified`.
3. Successful plugin load and patch registration permits `load-verified`.
4. Controlled evidence of intended behavior permits `behavior-verified`.
5. Combined-mod/load-order testing and reviewed risks permit `compatibility-reviewed`.
6. A new game patch does not inherit an old status. The hook remains bound to its original profile until reverified.
7. A helper or controller does not inherit safety, cleanup, or validation from its caller. Configuration gates and `try/catch` wrappers are source facts, not promotion evidence.

## Evidence boundary

Hook records may contain concise type/member/signature facts and small permitted excerpts needed to identify the target. They must not contain wholesale decompiled source, game assemblies, private paths, unredacted runtime dumps, or unsupported ownership claims.

## Catalogue work order

The extraction programme will:

1. inventory every `HarmonyPatch`, patch registration, reflection lookup, event subscription, manager/service lookup, and runtime adapter handoff in the pinned source repository;
2. group duplicates by intended target and profile;
3. assign stable hook IDs;
4. record candidate evidence and uncertainty;
5. reconcile targets against current exact game profiles;
6. connect reviewed hooks to system pages, examples, and compatibility records.

## Completed semantic batches

### Batch 001

[Semantic Hook Batch 001](BATCH_001.md) dispositions seven selected source files into ten candidates, one source-system fact, one decomposition blocker, and one explicit rejection.

Candidate records:

- [NPC non-critical death completion](records/ACTORS_NPC_DEATH.md)
- [Readable steal completion](records/CRIME_READABLE_STEAL.md)
- [Cloud-service EndSave family](records/PERSISTENCE_CLOUD_END_SAVE.md)
- [Menu initialization and options-button lookup](records/UI_MENU_INITIALIZE.md)

Other outcomes:

- [Avalon Mounts aggregation blocker](blockers/AVALON_MOUNTS_PLUGIN.md)
- [Template ExamplePatch rejection](rejections/TEMPLATE_EXAMPLE_PATCH.md)
- [Economy patch registrar fact](../systems/items-economy/PATCH_REGISTRATION_FACT.md)

### Batch 002

[Semantic Hook Batch 002](BATCH_002.md) decomposes Avalon Mounts and processes the first eight Tainted Economy registrar callers.

Candidate records:

- [Seven Avalon Mounts runtime targets](records/MOUNTS_RUNTIME_TARGETS.md)
- [Crafting and recipe UI](records/ECONOMY_CRAFTING_RECIPE.md)
- [Harvest yield](records/ECONOMY_HARVEST.md)
- [Reward and wealth routes](records/ECONOMY_REWARDS.md)
- [Service-cost getters](records/ECONOMY_SERVICE_COSTS.md)
- [Container loot](records/ECONOMY_CONTAINER_LOOT.md)
- [Vendor price](records/ECONOMY_VENDOR_PRICE.md)

Other outcomes:

- [Avalon Mounts lifecycle/frame/compatibility facts](../systems/mounts/AVALON_MOUNTS_RUNTIME_FACTS.md)
- [Avalon Mounts diagnostics/privacy blocker](blockers/AVALON_MOUNTS_DIAGNOSTICS_PRIVACY.md)
- [Resolved aggregate blocker history](blockers/AVALON_MOUNTS_PLUGIN.md)
- [Processed economy registrar fact](../systems/items-economy/PATCH_REGISTRATION_FACT.md)

Batch 002 adds 27 source-only candidate records and promotes none.

### Batch 003

[Semantic Hook Batch 003](BATCH_003.md) reviews thirteen downstream economy helper files and the two Avalon Mounts frame-ticked controllers without inheriting safety or validation from their callers.

Candidate adapter records:

- [Avalon Companions dialogue registration and cleanup](records/MOUNTS_AVALON_COMPANIONS_DIALOGUE_BRIDGE.md)

Canonical source facts:

- [Economy helper semantics](../systems/items-economy/HELPER_SEMANTICS_FACTS.md)
- [Resolved mount frame-controller behavior](../systems/mounts/AVALON_MOUNTS_RUNTIME_FACTS.md)

Research blockers:

- [Economy reflection, pricing, quantity, protection, and reward mutation](blockers/ECONOMY_HELPER_SAFETY.md)
- [Wolf mount seat frame controller and native contract ownership](blockers/WOLF_MOUNT_SEAT_CONTROLLER.md)

Batch 003 dispositions fifteen selected files into two source-only adapter candidates, seven canonical helper facts, and seven blockers. It promotes none.

## Initial domain indexes

Planned hook indexes:

- bootstrap and lifecycle;
- player and input;
- actors and troops;
- combat and progression;
- items, crafting, vendors, loot, and rewards;
- quests, dialogue, events, and world state;
- spawns, encounters, navigation, and cleanup;
- world, scenes, map, travel, and roads;
- UI, HUD, menus, prompts, and localization;
- audio, VFX, animation, materials, and assets;
- AI and blackboards;
- saves, persistence, configuration, and migration;
- frameworks, dependencies, compatibility, and diagnostics.

No domain is considered covered merely because one historical mod patched it.
