# Game and Modding System Reference

This lane explains the structures a mod author can observe, author against, or extend. It links to canonical catalog records, evidence, SDK contracts, and hook records rather than maintaining a second private game database.

## Planned domain map

| Domain | Required coverage |
|---|---|
| Bootstrap and lifecycle | game startup, service readiness, scene transitions, pause, shutdown, cleanup |
| Player and input | player identity, interaction, input routing, controls, state and inventory boundaries |
| Actors and troops | templates, identities, factions, equipment, ranks, uniqueness, spawning and cleanup |
| Combat and progression | health, damage, status, skills, perks, equipment, rewards and failure risks |
| Items and economy | items, recipes, crafting, stations, vendors, loot, rewards and acquisition relationships |
| Narrative and state | quests, dialogue, objectives, events, state keys, consequences, lockouts and saves |
| Spawns and navigation | spawn definitions, encounters, patrols, anchors, navigation, density and lifecycle |
| World, map and roads | worldspaces, scenes, areas, locations, travel, map pins, roads, routes and intersections |
| UI and localization | HUD, menus, settings, prompts, icons, accessibility, input focus, text and localization |
| Audio, VFX and animation | events, emitters, effect lifetime, animation, materials, resources and asset ownership |
| AI | blackboards, goals, actions, planning, senses, combat and navigation handoffs |
| Persistence and configuration | mod config, state storage, save boundaries, migration, recovery and rollback |
| Interoperability and diagnostics | identity, dependencies, frameworks, load order, patch collisions, logs and support |

## Required page shape

Every domain page must include:

1. player-visible purpose;
2. canonical entities and relationships;
3. lifecycle and ownership;
4. known authoring surfaces in FOA-SDK;
5. known runtime profiles;
6. reviewed hooks and extension points;
7. compatibility, performance, save, story, and cleanup risks;
8. evidence and validation state;
9. examples and procedures;
10. unresolved research and explicit prohibitions.

## Existing canonical owners

Current implemented or contract-level documentation already exists for:

- actor and troop authoring;
- item, recipe, station, economy coverage, and duplicate analysis;
- canonical catalog, evidence, validation, blockers, and governance;
- Tainted Framework and Tainted Interface utilities;
- Road Atlas schemas and topology validation;
- Avalon AI API 2.0 authoring contracts;
- adapter planning, build, staging, deployment, verification, reconciliation, and release metadata.

The system reference summarizes and connects those owners. It does not copy their schemas or imply that contract-only systems can already execute in the target game.

## Imported source-system facts

- [Economy patch registrar](items-economy/PATCH_REGISTRATION_FACT.md) — establishes the pinned upstream registrar's caller-supplied, fail-open registration pattern and links the twenty processed Batch 002 target candidates.
- [Economy helper semantics](items-economy/HELPER_SEMANTICS_FACTS.md) — separates seven dry-run/delegation source facts from the six blocked reflection, pricing, quantity, protection, and story-mutation helpers reviewed in Batch 003.
- [Tainted Economy diagnostic writer](items-economy/DIAGNOSTIC_WRITER_FACT.md) — records the exact 26-field writer, row-count cap, path/write sequence, deterministic containment and publication defects, and the Batch 004 current-source prohibition.
- [Avalon Mounts runtime structure](mounts/AVALON_MOUNTS_RUNTIME_FACTS.md) — separates plug-in lifecycle/cleanup, exact frame-controller behavior, filename-token compatibility detection, the prohibited wolf-seat controller, and the optional Avalon Companions adapter candidates.
- [Avalon Companions API v1](mounts/AVALON_COMPANIONS_API_V1_FACT.md) — verifies the pinned `AvalonCompanions` 0.2.17/API v1 source signatures while keeping built-assembly resolution, behavior, owner isolation, and cleanup unverified.

A source-system fact describes a verified property of the selected source architecture. It is not automatically a fact about the current game runtime. Native type/member facts still require exact-profile evidence and hook promotion.

A fact that a helper is diagnostics-only or delegates through a configuration gate does not prove reflected-member identity, bounded output, mutation safety, rollback, persistence semantics, or combined-mod compatibility.

Batch 004's [`promotion and prohibition register`](../hooks/decisions/BATCH_004_PROMOTION_PROHIBITION.md) is authoritative for the current-source decisions attached to these facts and their related candidate records.

## Coverage rule

A domain is not complete until it has an exact profile, source/evidence inventory, entity and relationship model, hook inventory, compatibility risks, authoring/runtime boundary, validation status, and explicit missing-evidence register.
