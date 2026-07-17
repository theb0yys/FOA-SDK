# Tainted Grail Modding Editor and SDK Roadmap

This roadmap describes capability order, not promised dates. Work is promoted when architecture, evidence, validation, documentation, and build quality are sufficient.

## Guiding rule

Every authoring feature must use the shared workspace, pack ownership, source/evidence, catalog, validation, risk, and permission infrastructure. Domain tools must not create private identity systems or bypass blockers.

## Phase 0 — Project foundation

Status: implemented, continuing hardening.

- O3DE editor Gem and host-tool registration.
- Editor-only product boundary.
- Focused repository validation.
- Full public README, user and contributor guides, architecture, quality, design-review, pre-commit review, PR review, security, conduct, governance, privacy, legal/content, accessibility, support, release, roadmap, changelog, issue forms, PR template, and CODEOWNERS.
- Two-branch development model.

Exit criteria:

- Gem is discoverable and structurally validated.
- Public project rules and contribution workflow are documented and CI-enforced.
- No FoA runtime integration exists in the editor layer.

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

Status: implemented on the active development line, pending integration and hardening.

- SHA-256 fingerprinting.
- Exact profile/build binding.
- Structured JSON and CSV extraction.
- Generic artifact intake without inferred evidence.
- Durable source/evidence documents.
- Import and schema issues.

## Phase 4 — Canonical catalog browser and record inspector

Status: implemented on the active development line, pending integration and hardening.

- Durable `Catalog/catalog.tgcatalog.json` document bound to workspace/profile/version/branch.
- Stable canonical records, first-class relationships, and validation history.
- Search by exact native reference, GUID, record ID, subject, alias, domain, kind, pack, confidence, validation, permission/prohibition, evidence, blocker state, and supersession.
- Inspector for identity, ownership, evidence, relationships, versions, validation history, permissions, conflicts, missing references, and blockers.
- Evidence-backed claim promotion into reviewed-but-unvalidated records.
- Native exact-ref uniqueness and synthetic pack ownership.
- No display-name identity merging.
- Transactional save-before-publish lifecycle.

Exit criteria:

- Records survive workspace reload.
- Exact-reference search is deterministic.
- Evidence linkage is visible and validated.
- Promotion requires pack ownership where applicable, exact profile-matched evidence, and explicit review state.
- Promotion cannot grant allowed usage and adds `no_unvalidated_runtime_use`.

## Phase 5 — Validation, maturity, risk, and permission engine

Status: next active slice.

- Independent maturity, confidence, operational risk, validation, permission, and prohibition states.
- Usage-specific permission decisions.
- Staleness, supersession, and compatibility evaluation.
- Validation history and proof artifacts.
- Queryable missing references and conflicts.
- Review and authoring surfaces for permission decisions without runtime execution.

## Phase 6 — Domain authoring tools

### Economy and content

- Item and Recipe Editor.
- Crafting station, ingredient, output, vendor, loot, and reward relationships.
- Separate existing-content and custom-content lanes.

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
- fail-closed validation;
- editor/runtime separation;
- durable schema versioning;
- legal redistribution boundaries;
- accessibility and usable error reporting;
- public documentation and tests.

## Not roadmap commitments

This document does not promise release dates, support every game patch, guarantee compatibility with private mods, or authorise runtime actions that have not passed the required validation and permission gates.
