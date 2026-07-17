# Tainted Grail Modding Editor and SDK Roadmap

This roadmap describes capability order, not promised dates. Work is promoted when architecture, evidence, validation, documentation, tests, and build quality are sufficient.

## Guiding rule

Every authoring feature must use the shared workspace, pack ownership, source/evidence, canonical catalog, validation, risk, staleness, permission, and prohibition infrastructure. Domain tools must not create private identity systems, grant their own permissions, or bypass blockers.

## Phase 0 — Project foundation

Status: implemented, continuing hardening.

- O3DE editor Gem and host-tool registration.
- Editor-only product boundary.
- Focused repository validation.
- Full public README, user and contributor guides, architecture, quality, design-review, pre-commit review, PR review, security, conduct, governance, privacy, legal/content, accessibility, support, release, roadmap, changelog, issue forms, PR template, and CODEOWNERS.
- Two-branch development model.

## Phase 1 — Workspace and exact game profile

Status: implemented, continuing hardening.

- Workspace identity and paths.
- Exact FoA installation, version, branch, runtime target, Unity version, BepInEx version, assemblies, diagnostics, and extracted-data locations.
- Durable workspace JSON.
- Blockers for incomplete build identity and pipeline paths.

## Phase 2 — Mod and content-pack projects

Status: implemented, continuing hardening.

- Pack-owned stable identities.
- Semantic versions and compatibility.
- Dependencies, required mods, and incompatibilities.
- Save impact and content/resource declarations.
- Durable pack-manifest JSON.

## Phase 3 — Source and evidence intake

Status: implemented, continuing hardening.

- SHA-256 fingerprinting.
- Exact profile/build binding.
- Structured JSON and CSV extraction.
- Generic artifact intake without inferred evidence.
- Durable source/evidence documents.
- Import and schema issues.

## Phase 4 — Canonical catalog browser and record inspector

Status: implemented, continuing hardening.

- Durable `Catalog/catalog.tgcatalog.json` document bound to workspace/profile/version/branch.
- Stable canonical records, first-class relationships, validation history, and governance history.
- Search by exact native reference, GUID, record ID, subject, alias, domain, kind, pack, maturity, confidence, risk, validation, staleness, permission/prohibition, evidence, blocker state, and supersession.
- Inspector for identity, ownership, evidence, relationships, versions, history, permissions, conflicts, missing references, and blockers.
- Evidence-backed claim promotion into reviewed-but-unvalidated, staleness-unknown records.
- Native exact-ref uniqueness and synthetic pack ownership.
- No display-name identity merging.
- Transactional save-before-publish lifecycle.

## Phase 5 — Validation, maturity, risk, and permission engine

Status: implemented, continuing hardening.

- Independent maturity, confidence, operational risk, validation, staleness, permission, and prohibition states for records and relationships.
- Usage-specific allow, forbid, and clear decisions.
- Validation events bound to exact profile, game version, branch, evidence, method, and named validator.
- Permission grants require a separate reviewed decision and validated proof for the same subject.
- Stale and potentially stale subjects automatically lose allowed usages.
- Supersession records a replacement, marks the old subject stale, removes allowed usages, and preserves history.
- Append-only governance history with evidence, validation proof IDs, reviewer, time, and notes.
- Governance blockers for unknown assessments, stale subjects, missing proof, missing history references, and profile mismatch.
- Dedicated **Tainted Grail Catalog Governance** editor pane.
- Unit tests for validation/permission separation, proof requirements, stale-state revocation, relationship governance, and supersession.

## Phase 6 — Domain authoring tools

Status: active development.

All domain tools use canonical IDs, source/evidence provenance, governance decisions, blockers, and the same transactional catalog document.

### Economy and content

#### Item and Recipe Editor

Status: implemented, continuing hardening and evidence coverage.

- Typed item profiles keyed to canonical `economy/item` records.
- Typed recipe profiles keyed to canonical `economy/recipe` records.
- Canonical crafting-station references.
- Ingredient joins with quantity, alternatives, consumption, conditions, and evidence.
- Output and by-product joins with quantity, chance, conditions, and evidence.
- Item flags and references for quest sensitivity, uniqueness, hidden state, localisation, icons, and assets.
- Recipe type, tab, unlock, duplicate key, and persistence description.
- First-class `sold_by`, `dropped_by`, `found_at`, `rewarded_by`, `learned_from`, `granted_by`, and `crafted_at` relationships.
- Separate existing-item, existing-recipe, runtime-append, custom-registration, asset/localisation, vendor/loot, and reward lanes.
- Read-only lane matrices; permission remains owned by Catalog Governance.
- Read-only station visibility and learnability evidence rows derived from exact recipe/station IDs, `crafted_at` and `learned_from` relationships, evidence, governance state, and blockers.
- Economy blockers for missing profiles, evidence, stations, outputs, unresolved joins, quest/unique risks, and identity-incompatible lanes.
- Transactional persistence inside `Catalog/catalog.tgcatalog.json`.
- Unit tests for profile identity, stations, joins, chance/quantity, round-trip persistence, acquisition defaults, lane status, deterministic evidence rows, fail-closed evidence/governance, and non-mutation.

Exit criteria:

- item and recipe profiles survive workspace reload;
- an item record cannot masquerade as a recipe record;
- native and synthetic action lanes remain separate;
- stations, ingredients, outputs, and acquisition relationships preserve canonical identity and evidence;
- profile and join writes reject missing or unrelated evidence;
- action lanes and evidence views remain read-only in the domain tool;
- station and learnability research never implies runtime capability;
- no item or recipe authoring command invokes FoA runtime code.

#### Remaining economy work

- vendor, loot, reward, and acquisition coverage dashboards;
- authoring-time duplicate detection reports across packs;
- work-order generation after adapter contracts exist.

### Actors and population

- Actor and Troop Editor.
- Templates, identities, unit classes, troop trees, pools, and upgrades.
- Spawn and Encounter Editor.
- Anchors, compositions, routes, lifecycle, uniqueness, density, and cleanup.

### World and societies

- World, Road, and Route Editor.
- Regions, scenes, locations, roads, segments, intersections, and routes.
- Faction and Authority Editor.
- Factions, cultures, territory, authority, and dispositions.

### Narrative and state

- Quest and State Inspector.
- State keys, decisions, consequences, overlays, and read/mutation permissions.

### Assets and localisation

- Asset and Localisation Manager.
- Addresses, bundles, source assets, cooked assets, localisation keys, and compatibility.

## Phase 7 — FoA adapter contracts

- Typed handoffs for item grants, recipe learning, recipe append, custom registration, spawns, routes, asset loading, vendors, loot, rewards, quest/state operations, persistence, and rollback.
- Adapter capability and version declarations.
- Work-order generation from reviewed catalog records.
- Runtime proof returned as new evidence, not silently promoted permission.

## Phase 8 — Build, package, deploy, and test

Controlled pipeline:

```text
validate → generate → build → package → deploy → launch → capture → attach evidence
```

- Reproducible build manifests.
- Mod-owned output only.
- Staging and deployment previews.
- Explicit user confirmation.
- Restoration and rollback plans.
- Compatibility reports.
- Log and diagnostic capture.
- Release archives and checksums.

## Phase 9 — Ecosystem and automation

- Importer plugin SDK.
- Domain tool extensions.
- Headless validation and packaging.
- Public schema packages.
- CI fixtures and compatibility matrices.
- Migration tooling.
- Documentation and examples for third-party adapters.

## Cross-cutting requirements

Every phase must preserve:

- exact identity;
- pack ownership;
- evidence provenance;
- independent governance axes;
- fail-closed validation and permission;
- editor/runtime separation;
- durable schema versioning;
- legal redistribution boundaries;
- accessibility and usable error reporting;
- public documentation and executable tests.

## Not roadmap commitments

This document does not promise release dates, support every game patch, guarantee compatibility with private mods, or authorise runtime actions that have not passed the required validation and permission gates.
