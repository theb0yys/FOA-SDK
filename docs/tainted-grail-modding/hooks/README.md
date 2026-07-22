# Tainted Grail Hook Catalogue

This folder will contain exact, version-bound records for reviewed game extension points, patch targets, events, services, reflection targets, data handoffs, and adapter boundaries.

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

## Evidence boundary

Hook records may contain concise type/member/signature facts and small permitted excerpts needed to identify the target. They must not contain wholesale decompiled source, game assemblies, private paths, unredacted runtime dumps, or unsupported ownership claims.

## Catalogue work order

The first extraction pass will:

1. inventory every `HarmonyPatch`, patch registration, reflection lookup, event subscription, manager/service lookup, and runtime adapter handoff in the pinned source repository;
2. group duplicates by intended target and profile;
3. assign stable hook IDs;
4. record candidate evidence and uncertainty;
5. reconcile targets against current exact game profiles;
6. connect reviewed hooks to system pages, examples, and compatibility records.

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