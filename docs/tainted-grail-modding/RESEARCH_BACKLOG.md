# Tainted Grail Modding Research Backlog

## Purpose

This backlog records the questions that must be answered before the handbook can claim complete, current, or supported coverage. An unanswered row remains visible; it is not filled with an invented identifier, inferred signature, or compatibility claim.

Priority meanings:

- `P0` — required before publishing a safe first-mod path;
- `P1` — required for broad system and hook coverage;
- `P2` — advanced authoring or ecosystem completeness;
- `P3` — useful long-term depth.

## Runtime and environment

| ID | Priority | Question | Required evidence | Completion result |
|---|---:|---|---|---|
| `TG-HB-001` | P0 | What exact public game versions and branches currently expose Mono and IL2CPP routes? | exact installation/profile observations, branch identity, game fingerprint | reviewed version profiles |
| `TG-HB-002` | P0 | Which BepInEx builds, framework versions, target frameworks, and dependency sets are valid for each route? | exact loader/framework/package evidence and clean load results | runtime setup matrix |
| `TG-HB-003` | P0 | What is the supported local development-reference strategy without redistributing game or loader binaries? | build experiments and legal review | first-mod build guide |
| `TG-HB-004` | P0 | What deployment, removal, rollback, and log locations apply to each route? | exact target inventory and controlled lifecycle evidence | safe local test procedure |
| `TG-HB-005` | P1 | Which game patches or branches invalidate existing runtime assumptions? | profile comparison and staleness review | compatibility history |

## Mod lifecycle and packaging

| ID | Priority | Question | Required evidence | Completion result |
|---|---:|---|---|---|
| `TG-HB-010` | P0 | What is the smallest project-owned example that builds and loads on each supported route? | clean build, package, load, unload, and log evidence | reviewed starter examples |
| `TG-HB-011` | P0 | What plugin identity, dependency, config, logging, and version conventions should the handbook require? | existing ecosystem review plus collision tests | identity and scaffold standard |
| `TG-HB-012` | P0 | What exact package layouts are accepted by current loaders and mod distribution channels? | local lifecycle tests and redistribution review | packaging guide |
| `TG-HB-013` | P1 | How should upgrades, config migrations, renamed GUIDs, and removed patches behave? | migration fixtures and rollback tests | maintenance guide |
| `TG-HB-014` | P1 | Which release claims require live-game evidence versus static or compiled evidence? | validation model reconciliation | mod validation matrix |

## Hook catalogue foundation

| ID | Priority | Question | Required evidence | Completion result |
|---|---:|---|---|---|
| `TG-HB-020` | P0 | Which upstream patch and reflection targets exist at the pinned source commit? | complete source inventory | candidate hook register |
| `TG-HB-021` | P0 | Which candidates can be resolved against exact current game profiles? | local assembly/member inspection with concise permitted evidence | signature-verified hooks |
| `TG-HB-022` | P1 | Which hooks load, execute, and clean up safely? | controlled runtime tests | behavior-validated hooks |
| `TG-HB-023` | P1 | Which targets collide across existing mods or common frameworks? | patch-owner/load-order inventory and combined tests | collision and ordering register |
| `TG-HB-024` | P1 | Which hooks are stable enough for examples, and which require adapters or reflection guards? | version comparison and failure tests | supported-pattern classification |
| `TG-HB-025` | P2 | Which direct patches should be replaced by higher-level ExtensionAPI or service contracts? | architectural review | migration recommendations |

## Game-system coverage

| ID | Priority | Domain | Initial research target |
|---|---:|---|---|
| `TG-HB-030` | P1 | bootstrap/lifecycle | game startup, scene load/unload, pause, shutdown, service availability |
| `TG-HB-031` | P1 | player/input | player identity, input routing, interaction, inventory and state access |
| `TG-HB-032` | P1 | actors/troops | runtime actor lifecycle, templates, equipment, factions, uniqueness, cleanup |
| `TG-HB-033` | P1 | combat/progression | damage, health, status, skills, perks, equipment, rewards |
| `TG-HB-034` | P1 | items/economy | item definitions, recipes, crafting, vendors, loot, rewards, acquisition |
| `TG-HB-035` | P1 | narrative/state | quests, dialogue, events, objectives, state keys, consequences, saves |
| `TG-HB-036` | P1 | spawns/navigation | spawners, encounters, patrols, nav, despawn, replay and cleanup |
| `TG-HB-037` | P1 | world/map/roads | scenes, locations, anchors, travel, map pins, roads and intersections |
| `TG-HB-038` | P1 | UI/localization | HUD, menus, prompts, settings, icons, accessibility, text and localization |
| `TG-HB-039` | P2 | audio/VFX/animation | events, sources, effect spawning, animation, materials, asset lifetime |
| `TG-HB-040` | P1 | AI | blackboards, goals, actions, planning, senses, combat and navigation handoff |
| `TG-HB-041` | P1 | persistence/config | save boundaries, mod state, config migration, recovery and rollback |
| `TG-HB-042` | P2 | interoperability | shared frameworks, dependency discovery, load order, IPC/events and diagnostics |

Each domain must produce an index, entity/relationship summary, candidate hook set, exact profile scope, known risks, SDK authoring surfaces, and an explicit missing-evidence list.

## FOA-SDK integration

| ID | Priority | Question | Completion result |
|---|---:|---|---|
| `TG-HB-050` | P0 | How does a mod author move from handbook research into catalog/source/evidence intake? | end-to-end evidence tutorial |
| `TG-HB-051` | P1 | Which ExtensionAPI queries and candidate-evidence calls are ready for public examples? | reviewed API usage examples |
| `TG-HB-052` | P1 | Which authoring plugins have usable editor vertical slices versus contracts only? | capability/status matrix |
| `TG-HB-053` | P1 | How does deterministic interchange reach Unity-owned payloads without direct editor-to-game mutation? | conversion tutorial linked to current gates |
| `TG-HB-054` | P1 | How are runtime adapter plans, build manifests, staging previews, confirmation, execution results, and verification related? | adapter lifecycle guide |
| `TG-HB-055` | P2 | How should the Suite Wizard expose handbook packages, examples, and documentation without making them executable authority? | reviewed installer package design |

## Legal, privacy, and source provenance

| ID | Priority | Question | Completion result |
|---|---:|---|---|
| `TG-HB-060` | P0 | What source text and code may be adapted from the pinned repository with no root licence assertion? | explicit rights/licence decision or clean-room summaries | migration permission matrix |
| `TG-HB-061` | P0 | What game-derived facts, screenshots, snippets, and diagnostic evidence may be committed? | legal/content/privacy review | evidence handling guide |
| `TG-HB-062` | P1 | Which third-party assets, libraries, templates, and examples have exact redistribution terms? | dependency/source register | legal reference index |
| `TG-HB-063` | P1 | How are private paths, usernames, saves, tokens, logs, and crash data redacted? | mutation tests and sample redaction receipts | diagnostics privacy guide |

## Documentation quality and completeness

| ID | Priority | Question | Completion result |
|---|---:|---|---|
| `TG-HB-070` | P0 | Can a new author complete setup, build, load, validate, remove, and package a minimal mod using only the handbook? | clean-machine walkthrough | first-mod acceptance receipt |
| `TG-HB-071` | P1 | Does every source-map item have a disposition and every published claim have provenance/profile metadata? | automated documentation audit | coverage report |
| `TG-HB-072` | P1 | Are broken links, stale profiles, duplicate hook IDs, unsupported examples, and missing backlinks detected automatically? | documentation validators | mandatory handbook gate |
| `TG-HB-073` | P2 | Are system pages and tutorials accessible, searchable, and understandable without hidden jargon? | accessibility/editorial review | public documentation quality gate |

## Research workflow

A backlog item advances through:

```text
open
→ scoped brief
→ source inventory
→ evidence captured
→ claims reconciled
→ draft documentation or contract
→ exact-profile validation
→ review
→ completed or blocked
```

Blocked items retain their evidence requirements and reason. Completed items must update the relevant handbook page, source map, version profile, hook/system register, and backlinks in the same change.