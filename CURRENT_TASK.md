# Current Task

Status: Faction and Authority Editor implementation and local authoring validation complete; maintainer review pending.

Goal: create and edit local factions and cultures, assign saved actors/troops, describe leadership and jurisdiction, edit directed faction relationships, and save/reopen without losing links.

Classification: Significant. Primary owner: content-pack-authoring. Supporting owners: catalog-and-identity, schemas-and-persistence, workspace-and-packs and ui-framework.

Delivered: typed society profiles and first-class faction links, exact ownership/evidence, catalog schema 4 with schema-1/2/3 input migration and verified backups, Foundation transactions, a registered Editor pane, guides and compiled/live acceptance.

Out of scope: native faction discovery or runtime changes, game relationships/membership, world editing, game/save writes, deployment, installer, engine changes and other feature work.

Validation on 2026-09-10:
- PASSED L0/L1: full static validation, 827 Python tests discovered (818 passed, nine explicit platform skips), all enabled pinned source-policy checks, and 30-pane lifecycle inventory.
- PASSED L2: exact pinned O3DE configure and changed SDK targets; Catalog tests 445 passed with two explicit Windows symlink-fixture skips; CanonicalInterchange tests 39 passed.
- PASSED L3: seven running Editor workflow groups cover Town Guard/Bandits, culture editing, membership/leadership, directed hostility, jurisdiction, updates/removal, rejected input, dirty drafts, failed writes, workspace reopening and further edits.
- PASSED L3 presentation: all three tabs visually reviewed; floating dock verified at 860x640 logical pixels with scrolling, keyboard focus and persistent Save/Revert controls.
- PASSED measured performance: maximum sampled UI timer gap 2.172 seconds against a three-second budget, using an isolated input catalog with 887 actors, 3,914 items, 356 recipes and one encounter. This is not a maximum-catalog benchmark.
- PASSED protected-data audit: original authoring catalog hash unchanged; source diff excludes private observations, personal paths, screenshots and build outputs.
- NOT_APPLICABLE: game/runtime, deployment, installer and release validation. Runtime sign-off not performed.

Build evidence reuses unchanged outputs from the same external engine pin while compiling the changed SDK targets from this branch. It is not a full clean engine build claim. Logs, test XML, screenshots and the machine-readable evidence pack remain outside Git.

Branch: codex/faction-authority-editor from encounter commit 9f79f18ea758eec1e55ab365e805a45c9058f956. Encounter PR #252 remains an unmerged prerequisite; the faction review keeps that dependency explicit and its own diff separate.

Design: docs/tainted-grail-sdk/FACTION_AUTHORITY_EDITOR_DESIGN.md.

Next action: maintainer review. The prerequisite must be integrated before the faction change reaches main. No merge or follow-on milestone is authorized by this task record.
