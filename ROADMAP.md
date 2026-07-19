# Tainted Grail Modding Editor and SDK Roadmap

This roadmap describes capability order, not promised dates. Work advances only when architecture, evidence, validation, documentation, tests, and build quality are sufficient.

## Guiding rule

Every authoring feature uses the shared workspace, pack ownership, source/evidence, canonical catalog, validation, risk, staleness, permission, prohibition, and blocker infrastructure. Domain tools do not create private identity systems, grant their own permissions, or bypass blockers.

## Phase 0 — Project foundation

Status: implemented, continuing hardening.

- O3DE editor Gem and host-tool registration.
- Editor-only product boundary.
- Focused repository validation and public governance/documentation.
- Two-branch development model: `main` and `foa-development`.

## Phase 1 — Workspace and exact game profile

Status: implemented, continuing hardening.

- Workspace identity and canonical paths.
- Exact FoA version, branch, runtime target, Unity/BepInEx versions, assemblies, diagnostics, and extracted-data locations.
- Durable workspace schema 1 with legacy migration and atomic all-or-nothing publication.
- Blockers for incomplete identity and unsafe paths.

## Phase 2 — Mod and content-pack projects

Status: implemented, continuing hardening.

- Pack-owned stable identities and semantic versions.
- Core, adapter, game, mod, DLC, and incompatibility declarations.
- Save-impact, content, asset, localisation, build, and release declarations.
- Durable pack manifests with runtime actions disabled.

## Phase 3 — Source and evidence intake

Status: implemented, continuing hardening.

- SHA-256 fingerprinting and exact profile/build binding.
- Structured JSON/CSV extraction and bounded generic-artifact intake.
- Durable source/evidence documents and import issues.
- No automatic catalog promotion, validation, or permission.

## Phase 4 — Canonical catalog browser and record inspector

Status: implemented, continuing hardening.

- Durable `Catalog/catalog.tgcatalog.json` bound to workspace/profile/version/branch.
- Stable records, first-class relationships, validation history, and governance history.
- Exact-reference/GUID, identity, pack, evidence, governance, blocker, and supersession search.
- Reviewed claim promotion without display-name identity merging.
- Native exact-reference uniqueness, synthetic pack ownership, and save-before-publish transactions.

## Phase 5 — Validation, maturity, risk, and permission engine

Status: implemented, continuing hardening.

- Independent maturity, confidence, operational risk, validation, staleness, permission, prohibition, and supersession states.
- Usage-specific allow, forbid, and clear decisions.
- Validation and governance history bound to exact subjects, profiles, evidence, reviewers, and proof.
- Stale, failed, blocked, and superseded subjects lose allowed usage.
- Dedicated **Tainted Grail Catalog Governance** pane and rollback/persistence hardening tests.

## Phase 6 — Domain authoring tools

Status: active development.

All domain tools use canonical IDs, source/evidence provenance, governance decisions, blockers, and the shared catalog transaction boundary.

### Economy and content

#### Item and Recipe Editor

Status: implemented, continuing hardening and evidence coverage.

- Typed item/recipe profiles, stations, ingredients, outputs, by-products, unlocks, and acquisition relationships.
- Separate native, synthetic, append, registration, vendor/loot, reward, asset, and localisation lanes.
- Read-only action matrices and station/learnability evidence rows.
- Transactional catalog persistence, exact evidence checks, and fail-closed blockers.
- No item or recipe authoring command invokes FoA runtime code.

#### Economy acquisition coverage dashboard

Status: implemented, continuing hardening and Windows UI verification.

- Pure Core vendor, loot, reward, learnability, and crafting coverage.
- Deterministic `covered`, `partial`, `blocked`, and `missing` states.
- Exact relationship/evidence/governance/blocker handling and input non-mutation.
- Read-only Editor dashboard with no permission, persistence, deployment, launch, or save action.

#### Economy cross-pack duplicate report

Status: implemented, continuing hardening and Windows UI verification.

- Exact case-sensitive cross-pack grouping by economy subject reference and recipe duplicate key.
- Deterministic `review_required`, `partial`, and `blocked` health.
- No display-name, alias, localisation, tag, asset-path, case-folded, or fuzzy matching.
- Read-only review candidates only: no merge, rejection, winner selection, or runtime action.

#### Remaining economy work

- Extend typed planning contracts as later domain tools become implemented.
- Keep runtime behavior in separately reviewed adapters.

### Actors and population

- Actor and Troop Editor.
- Spawn and Encounter Editor.
- Templates, identities, pools, routes, lifecycle, uniqueness, density, cleanup, and rollback research.

### World and societies

- World, Road, and Route Editor.
- Faction and Authority Editor.
- Regions, scenes, locations, roads, factions, cultures, territory, authority, and dispositions.

### Narrative and state

- Quest and State Inspector.
- State keys, decisions, consequences, overlays, and separate read/mutation permissions.

### Assets and localisation

- Asset and Localisation Manager.
- Addresses, bundles, source/cooked assets, localisation keys, evidence, and compatibility.

## Phase 7 — FoA adapter contracts

Status: active development.

The runtime-result evidence envelope remains an observation-only return contract and never authorises another execution.

### Adapter capability contract foundation

Status: implemented, continuing hardening and Windows UI verification.

- Typed lowercase namespaced adapter IDs, strict semantic versions, exact `Mono`/`IL2CPP` targets, and eleven capabilities.
- Exact adapter/capability evidence and same-subject catalog permission/validation proof.
- Deterministic `unsupported`, `version_mismatch`, `permission_missing`, `proof_missing`, and `supported` states.
- Transient registry and read-only **Tainted Grail Adapter Capability Matrix**.
- No runtime adapter implementation, BepInEx/Harmony execution, process access, deployment, game launch, save mutation, or work-order execution.

### Deterministic work-order plan generation

Status: implemented, continuing hardening and Windows UI verification.

- Whole-plan refusal unless all eleven capability rows are `supported`.
- Independent exact identity, permission, proof, payload, relationship, target, and blocker rebuilding.
- Stable plan/step IDs, deterministic order, canonical JSON, and `ExecutionAllowed: false`.
- Read-only **Tainted Grail Adapter Work-Order Plans** pane.
- No plan persistence, export, dispatch, adapter call, code generation, deployment, launch, telemetry, save mutation, or execution.

### Runtime-result evidence envelope

Status: implemented, continuing hardening and Windows UI verification.

- Typed attempted-plan and attempted-step identities bound to exact canonical work-order plans.
- Typed `not_attempted`, `succeeded`, `failed`, and `skipped` outcomes; failures; cleanup/rollback; safe logs; and SHA-256 fingerprints.
- Candidate source/evidence documents returned by value without automatic registration, persistence, validation, or permission.
- Transient `AdapterRuntimeResultRegistry` and read-only **Tainted Grail Adapter Runtime Result Evidence** pane.
- No actual FoA adapter implementation or execution path is introduced or authorised by this contract.

## Phase 8 — Build, package, deploy, and test

Status: active development.

### Reproducible adapter build manifests

Status: implemented, continuing hardening and Windows UI verification.

- Pure Core reproducible build definitions bound to exact plans, packs, adapters, profiles, source/O3DE revisions, toolchains, materials, dependencies, BepInEx metadata, and expected outputs.
- Safe package-relative paths, SHA-256 material fingerprints, redistribution gates, deterministic canonical JSON, and `BuildAllowed: false`.
- Deterministic `plan_mismatch`, `toolchain_unresolved`, `input_missing`, `fingerprint_missing`, `path_invalid`, `redistribution_blocked`, and `ready` states.
- Read-only **Tainted Grail Adapter Build Manifests** pane.
- No compilation, packaging, deployment, launch, or execution is introduced or authorised.

### Deterministic package-assembly preview

Status: implemented, continuing hardening and Windows UI verification.

- Compare one reviewed build manifest with one project-owned staging inventory.
- Require exact manifest/review/inventory/pack/fingerprint/package-root binding.
- Derive a sorted package layout with exact staging paths, package paths, roles, media types, byte sizes, ownership, redistribution state, and output digests.
- Emit explicit omissions for missing manifest outputs and collisions for multiple entries targeting the same output.
- Fail closed for unreviewed manifests, untrusted inventory, undeclared outputs, missing fingerprints, unsafe paths, collisions, and redistribution blockers.
- Deterministic `manifest_not_ready`, `manifest_unreviewed`, `inventory_binding_mismatch`, `inventory_untrusted`, `output_missing`, `fingerprint_missing`, `path_invalid`, `collision`, `redistribution_blocked`, and `ready` states.
- Canonical JSON with `AssemblyAllowed: false`, `ArchiveAllowed: false`, and `DeploymentAllowed: false`.
- Transient registry and read-only **Tainted Grail Package Assembly Preview** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and thirteen-pane Windows UI coverage.
- No file copying, archive creation, deployment, launch, or execution is implemented or authorised.

### Deterministic staging and deployment preview

Status: implemented, continuing hardening and Windows UI verification.

- Compare one exact ready package layout with one accepted evidence-backed declared target inventory.
- Require exact package-preview ID/fingerprint, pack, target root, target inventory ID/fingerprint, and review binding.
- Derive deterministic additions, replacements, removals, unchanged paths, exact conflicts, backup requirements, and inverse rollback steps.
- Preserve previous/desired fingerprints, byte sizes, roles, media types, project ownership, managed/replaceable/removable state, and target entry identities.
- Fail closed for non-ready packages, unreviewed targets, binding drift, untrusted entries, unsafe target/backup paths, ownership/type/multiplicity conflicts, incomplete backups, and incomplete rollback.
- Deterministic `package_not_ready`, `target_unreviewed`, `inventory_binding_mismatch`, `inventory_untrusted`, `path_invalid`, `conflict`, `backup_incomplete`, `rollback_incomplete`, and `ready` states.
- Canonical JSON with `StagingMutationAllowed: false`, `DeploymentMutationAllowed: false`, `RollbackExecutionAllowed: false`, and `LaunchAllowed: false`.
- Transient registry and read-only **Tainted Grail Staging and Deployment Preview** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and fourteen-pane Windows UI coverage.
- **No copying, deletion, deployment, launch, or execution** is implemented or authorised.

Exit criteria:

- a non-ready package layout or unreviewed target inventory cannot produce a ready deployment preview;
- exact package, target, fingerprint, ownership, role, media-type, and path bindings fail closed;
- additions, replacements, removals, conflicts, backups, and rollback are explicit and deterministic;
- a replacement or removal cannot be described without an exact current fingerprint and contained backup path;
- every addition, replacement, and removal has one typed inverse rollback step;
- equivalent inputs produce byte-identical canonical JSON;
- preview generation does not mutate package or target metadata;
- all staging, deployment, rollback-execution, and launch permissions remain false, including for `ready`;
- the Editor pane remains non-editable and exposes no registration, copy, delete, backup, restore, deploy, launch, or execute action;
- the Windows manual UI checklist includes all fourteen panes and the default zero-staging/deployment-input state.

### Next ordered slice — explicit confirmation and deployment work-order contract

Bind one exact ready staging/deployment preview to a typed named reviewer, confirmation scope, expiry, maintenance window, preflight evidence, and operator-facing action checklist. Every execution flag remains false. The contract must add no copy, delete, backup, restore, deployment, launch, or adapter invocation.

Controlled pipeline:

```text
validate → generate → build → package → deploy → launch → capture → attach evidence
```

Remaining Phase 8 work includes controlled package assembly, trusted filesystem inventory and hashing, explicit confirmation, actual backup/restore implementations, compatibility reports, log/diagnostic capture, release archives, checksums, and separately reviewed runtime adapters.

## Phase 9 — Ecosystem and automation

- Importer plugin SDK and domain-tool extensions.
- Headless validation and packaging.
- Public schema packages, CI fixtures, compatibility matrices, and migration tooling.
- Documentation and examples for third-party adapters.

## Cross-cutting requirements

Every phase preserves exact identity, pack ownership, evidence provenance, independent governance axes, fail-closed validation and permission, editor/runtime separation, durable schema versioning, legal redistribution boundaries, accessibility, usable errors, public documentation, and executable tests.

## Not roadmap commitments

This document does not promise release dates, support every game patch, guarantee compatibility with private mods, or authorise runtime actions that have not passed the required validation and permission gates.
