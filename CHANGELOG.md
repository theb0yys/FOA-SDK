# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows Keep a Changelog principles. Version numbers will follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe reviewed capability slices before a tag exists.

## Unreleased

### Added

- Pure-Core `AdapterPackageAssemblyPreviewService` for exact reviewed-manifest and staging-inventory binding, project ownership, deterministic package layout and output digests, explicit omissions/collisions, path containment, and redistribution blockers.
- Transient `AdapterPackageAssemblyPreviewRegistry` and read-only **Tainted Grail Package Assembly Preview** pane with canonical JSON and `AssemblyAllowed`, `ArchiveAllowed`, and `DeploymentAllowed` permanently false.
- Package-preview production-linked C++ tests, focused validator and negative tests, CI integration, public Phase 8 documentation, and thirteen-pane Windows manual UI coverage.
- Pure-Core `AdapterBuildManifestService` for exact plan/toolchain/material binding, BepInEx metadata, dependencies, expected outputs, path containment, redistribution gates, and canonical JSON with `BuildAllowed: false`.
- Read-only **Tainted Grail Adapter Build Manifests** pane plus production-linked tests, focused validation, documentation, and twelve-pane acceptance coverage.
- Pure-Core `AdapterRuntimeResultRegistry` and typed runtime-result contracts for exact attempted plan/step identities, outcomes, failures, cleanup, rollback, safe log references, and SHA-256 fingerprints.
- Pure-Core `AdapterRuntimeResultEvidenceService`, which returns deterministic candidate evidence by value and **does not promote validation or permission**.
- Read-only **Tainted Grail Adapter Runtime Result Evidence** pane and eleven-pane acceptance coverage.
- Pure-Core `AdapterWorkOrderPlanningService` for whole-plan compatibility gating, exact reviewed payload rebuilding, stable identities, deterministic refusals, canonical JSON, and execution disabled.
- Read-only **Tainted Grail Adapter Work-Order Plans** pane; **execution remains prohibited**.
- Pure-Core `AdapterContractRegistry` and `AdapterCompatibilityService` with typed identities, semantic versions, exact runtime targets, eleven capabilities, and deterministic `unsupported`, `version_mismatch`, `permission_missing`, `proof_missing`, and `supported` results; there is **no runtime adapter implementation**.
- Read-only **Tainted Grail Adapter Capability Matrix**.
- Pure-Core `EconomyCoverageService` and read-only **Tainted Grail Economy Acquisition Coverage** pane.
- Pure-Core `EconomyDuplicateDetectionService` and read-only **Tainted Grail Economy Cross-Pack Duplicates** pane using exact case-sensitive signals only, with no display-name or fuzzy matching.
- Internal `TaintedGrailModdingSDK.Core.Static` and `Framework.Static` targets with Core → Framework → Editor dependency direction and unique source ownership.
- Durable workspace schema 1, validated schema-0 migration, atomic workspace candidates, canonical path policy, and rollback tests.
- O3DE host-tool Gem registration, workspace/pack/source/evidence/catalog/governance/economy tools, focused validators, and CI.
- Developer Preview command layer, dedicated Editor project/entry, deterministic synthetic fixture, service-level persistence smoke, controlled Editor launch, redacted diagnostics, and project-owned duplicate-review companion.
- Windows manual UI checklist covering all TG SDK panes, normal scaling, keyboard traversal, synthetic data, save/close/reopen, actionable failures, and runtime boundaries.
- **Screenshot-evidence initializer**, recorder, PNG attachment helper, attestation, and exact-commit verifier.
- Screenshot checks for **PNG integrity**, dimensions, limits, SHA-256, required coverage, traversal/symlink rejection, privacy review, and no-runtime attestations.
- Public user, contributor, architecture, data-format, governance, security, privacy, accessibility, release, roadmap, changelog, and support documentation.

### Changed

- Phase 8 now includes deterministic package-assembly preview; deterministic staging and deployment preview is the next ordered slice.
- Focused validators enforce accepted manifest reviews, exact inventory binding, project ownership, expected-output coverage, output digests, omissions, collisions, path containment, redistribution decisions, canonical sorting, non-mutation, and the no-assembly boundary.
- The Windows manual UI checklist now covers all thirteen panes and the default zero-package-preview-input state.
- Phase 8 retains reproducible adapter build manifests as the prerequisite handoff; `ready` never authorises a build.
- Phase 7 retains typed adapter capabilities, deterministic work-order plans, and runtime-result evidence returns as separate fail-closed contracts.
- Production implementation files compile exactly once under Core, Framework, or Editor, while tests link production libraries.
- Workspace loading publishes only complete validated candidates, and durable workspace persistence emits explicit schema-1 JSON.
- Catalog governance remains typed, append-only, save-before-publish, evidence-backed, and separate from validation.
- Runtime execution remains disabled across editor-owned workspace, catalog, economy, adapter, planning, result-evidence, build-manifest, and package-preview workflows.

### Security

- Package previews are transient metadata only. Included entries must be project-owned, declared by the reviewed manifest, safely contained, fingerprinted, and redistributable; all assembly/archive/deployment permissions remain false.
- No filesystem scan, file copy/delete, archive writer, package assembler, deployment, FoA launch, BepInEx/Harmony load, telemetry, save mutation, or adapter execution is added.
- Build manifests remain transient definitions with exact source/O3DE revisions, toolchain declarations, fingerprints, safe package paths, redistribution decisions, and `BuildAllowed: false`.
- Runtime-result envelopes return candidate evidence only; they do not automatically register, persist, promote, validate, permit, dispatch, execute, deploy, launch, or mutate saves.
- Work-order plans are immutable descriptions with `ExecutionAllowed: false` at plan and step level.
- Adapter compatibility is fail-closed; `supported` never authorises execution.
- Economy analyses are immutable and cannot grant permission, mutate acquisition state, merge duplicates, reject packs, or select winners.
- Failed workspace candidates cannot publish partial state or redirect canonical pack containment.
- Developer Preview fixture, launch, diagnostics, and screenshot tooling perform no automatic upload or runtime mutation and reject unsafe paths, secrets, and unreviewed outputs.
- Public contribution rules require design review, DCO, focused/compiled tests, resolved review threads, and maintainer approval.

### Known limitations

- Package-preview inventories and output fingerprints are caller-supplied reviewed metadata; no trusted staging scanner, file hashing, copy, archive, package assembly, or deployment exists.
- A `ready` package preview does not prove files exist or a package was created.
- Build-manifest previews do not resolve/install toolchains, invoke compilers, generate output digests, provenance, SBOMs, or signed artefacts.
- Runtime-result envelopes are transient and add no file loader, transport, automatic import, or live proof capture.
- Adapter declarations and work-order plans are transient and are not dispatched or executed.
- FoA runtime adapters, cleanup/rollback behavior, production deployment, and save integration are not implemented.
- Remaining actor, spawn, world, faction, narrative/state, asset, and localisation authoring tools are not implemented.
- The **actual Windows screenshot pass remains pending**.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
