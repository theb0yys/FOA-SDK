# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows Keep a Changelog principles. Version numbers will follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe reviewed capability slices before a tag exists.

## Unreleased

### Added

- Repository-owned `run_local_validation.py` entry point for Python unit tests, contract validators, temporary fixture/diagnostics verification, tracked-path hygiene, O3DE source policy, and optional compiled catalog tests.
- CI/runner policy validator, negative regression tests, and public manual-validation documentation covering unavailable Actions, self-hosted runner isolation, registration-token handling, and restoration gates.
- Pure-Core `AdapterDeploymentExecutionEvidenceService` and typed execution-result contracts for exact reviewed work-order binding, separately reviewed executor metadata, attempted steps, backup/restore outcomes, deployed fingerprints, target verification, rollback, failures, safe logs, and candidate evidence return.
- Transient `AdapterDeploymentExecutionResultRegistry` and read-only **Tainted Grail Deployment Execution Result Evidence** pane; no executor is invoked and there is no automatic evidence promotion.
- Deployment execution-result production-linked C++ tests, focused validator and negative tests, workflow definition, public Phase 8 documentation, and sixteen-pane Windows manual UI coverage.
- Pure-Core `AdapterDeploymentWorkOrderService` for exact ready-preview binding, typed named confirmation, confirmation scope, expiry, UTC maintenance windows, required preflight evidence, deterministic non-executable work-order steps, and an operator-facing checklist.
- Transient `AdapterDeploymentWorkOrderRegistry` and read-only **Tainted Grail Deployment Confirmation and Work Orders** pane with canonical JSON, pending acknowledgements, and `ExecutionAllowed`, copy, delete, backup, restore, deployment, and launch permissions permanently false.
- Deployment confirmation/work-order production-linked C++ tests, focused validator and negative tests, workflow definition, public Phase 8 documentation, and fifteen-pane Windows manual UI coverage.
- Pure-Core `AdapterStagingDeploymentPreviewService` for exact ready-package and reviewed target-inventory binding, deterministic additions/replacements/removals/unchanged paths, ownership and type conflicts, backup requirements, and typed inverse rollback steps.
- Transient `AdapterStagingDeploymentPreviewRegistry` and read-only **Tainted Grail Staging and Deployment Preview** pane with canonical JSON and `StagingMutationAllowed`, `DeploymentMutationAllowed`, `RollbackExecutionAllowed`, and `LaunchAllowed` permanently false.
- Staging/deployment-preview production-linked C++ tests, focused validator and negative tests, workflow definition, public Phase 8 documentation, and fourteen-pane Windows manual UI coverage.
- Pure-Core `AdapterPackageAssemblyPreviewService` for exact reviewed-manifest and staging-inventory binding, project ownership, deterministic package layout and output digests, explicit omissions/collisions, path containment, and redistribution blockers.
- Transient `AdapterPackageAssemblyPreviewRegistry` and read-only **Tainted Grail Package Assembly Preview** pane with canonical JSON and `AssemblyAllowed`, `ArchiveAllowed`, and `DeploymentAllowed` permanently false.
- Package-preview production-linked C++ tests, focused validator and negative tests, workflow definition, public Phase 8 documentation, and thirteen-pane Windows manual UI coverage.
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
- O3DE host-tool Gem registration, workspace/pack/source/evidence/catalog/governance/economy tools, focused validators, and manual workflow definitions.
- Developer Preview command layer, dedicated Editor project/entry, deterministic synthetic fixture, service-level persistence smoke, controlled Editor launch, redacted diagnostics, and project-owned duplicate-review companion.
- Windows manual UI checklist covering all TG SDK panes, normal scaling, keyboard traversal, synthetic data, save/close/reopen, actionable failures, and runtime boundaries.
- **Screenshot-evidence initializer**, recorder, PNG attachment helper, attestation, and exact-commit verifier.
- Screenshot checks for **PNG integrity**, dimensions, limits, SHA-256, required coverage, traversal/symlink rejection, privacy review, and no-runtime attestations.
- Public user, contributor, architecture, data-format, governance, security, privacy, accessibility, release, roadmap, changelog, and support documentation.

### Changed

- Automatic pull-request and push triggers are suspended because exact-head jobs could not acquire GitHub-hosted runners; the TG SDK, Editor-entry, and repository-hygiene workflows are manual-only.
- The inherited full-engine `AR` workflow and generic upstream `Validation` workflow are removed from this fork so queued checks are not misrepresented as TG SDK test evidence.
- Exact local validation evidence is the development merge gate until automatic Actions are safely restored; **no automated per-commit test result is claimed**.
- Phase 8 now includes typed deployment execution-result and verification envelopes; a deterministic post-deployment verification and release-blocker report is the next ordered slice.
- Focused validators enforce exact work-order and reviewed-executor binding, typed attempted steps, backup/restore outcomes, target verification, rollback, same-subject failures/logs, candidate evidence-only return, non-mutation, and the no-executor boundary.
- The Windows manual UI checklist now covers all sixteen panes and the default zero-deployment-execution-result-envelope state.
- Phase 8 retains confirmation/work-order, staging/deployment, package-assembly, and reproducible-build contracts as prerequisite handoffs; structural result acceptance never authorises execution or evidence promotion.
- Phase 7 retains typed adapter capabilities, deterministic work-order plans, and runtime-result evidence returns as separate fail-closed contracts.
- Production implementation files compile exactly once under Core, Framework, or Editor, while tests link production libraries.
- Workspace loading publishes only complete validated candidates, and durable workspace persistence emits explicit schema-1 JSON.
- Catalog governance remains typed, append-only, save-before-publish, evidence-backed, and separate from validation.
- Runtime execution remains disabled across editor-owned workspace, catalog, economy, adapter, planning, result-evidence, build-manifest, package-preview, staging/deployment-preview, deployment-work-order, and deployment-execution-result workflows.

### Security

- Public pull requests must not execute on a general-purpose self-hosted runner; any future self-hosted design requires disposable isolation, no secrets or personal files, narrow labels, restricted triggers, and explicit operator ownership.
- Runner registration tokens exposed in screenshots, messages, logs, or shell history are treated as compromised and must be abandoned and regenerated.
- Deployment execution-result envelopes are transient executor-supplied metadata. Exact work-order, step, backup, verification, rollback, failure, and safe-log bindings fail closed before candidate evidence is returned.
- Contract-valid failed execution remains distinct from successful deployment; no result is automatically promoted into source/evidence, validation, permission, release, or another execution.
- Deployment confirmations, maintenance windows, preflight records, work orders, and operator checklists are transient metadata only. Every execution, copy, delete, backup, restore, deployment, and launch permission remains false, and checklist acknowledgements are never recorded automatically.
- Exact preview fingerprints, named reviewers, typed scope, confirmation expiry, reviewed UTC windows, evidence-backed preflight kinds, and complete change/backup/rollback coverage fail closed before `review_ready`.
- No filesystem scan, file copy/replace/delete, backup/restore, archive writer, package assembler, deployment mutation, FoA launch, BepInEx/Harmony load, telemetry, save mutation, or adapter execution is added.
- Staging/deployment previews are transient metadata only. Target inventories require exact review, ownership, management, fingerprint, path, replacement/removal, backup, and rollback declarations; every mutation and launch permission remains false.
- Package previews are transient metadata only. Included entries must be project-owned, declared by the reviewed manifest, safely contained, fingerprinted, and redistributable; all assembly/archive/deployment permissions remain false.
- Build manifests remain transient definitions with exact source/O3DE revisions, toolchain declarations, fingerprints, safe package paths, redistribution decisions, and `BuildAllowed: false`.
- Runtime-result envelopes return candidate evidence only; they do not automatically register, persist, promote, validate, permit, dispatch, execute, deploy, launch, or mutate saves.
- Work-order plans are immutable descriptions with `ExecutionAllowed: false` at plan and step level.
- Adapter compatibility is fail-closed; `supported` never authorises execution.
- Economy analyses are immutable and cannot grant permission, mutate acquisition state, merge duplicates, reject packs, or select winners.
- Failed workspace candidates cannot publish partial state or redirect canonical pack containment.
- Developer Preview fixture, launch, diagnostics, and screenshot tooling perform no automatic upload or runtime mutation and reject unsafe paths, secrets, and unreviewed outputs.
- Public contribution rules require design review, DCO, focused/compiled tests, resolved review threads, and maintainer approval.

### Known limitations

- Automatic GitHub Actions are unavailable in the current repository/account state; manual workflow definitions remain present, but full local and compiled validation must be run and recorded from a real checkout.
- Executor review, step outcomes, fingerprints, backup/restore results, verification observations, rollback outcomes, failures, and logs are caller-supplied metadata; no executor, trusted target scanner, independent verifier, or automatic evidence promotion exists.
- An `accepted` execution-result envelope proves contract shape only, not deployment success, safety, target existence, backup integrity, rollback correctness, or release readiness.
- Confirmations, timestamps, maintenance-window evidence, preflight results, and reviewer identities are caller-supplied metadata; no trusted clock, identity provider, independent preflight runner, acknowledgement system, or deployment executor exists.
- A `review_ready` work order does not prove that files exist, checks were independently reproduced, backup capacity exists, rollback was tested, or an operator acknowledged the checklist.
- Deployment target inventories, fingerprints, ownership flags, backup paths, and review evidence are caller-supplied metadata; no trusted target scanner, hashing, backup writer, restore engine, or deployer exists.
- A `ready` staging/deployment preview does not prove target files exist, backups were made, rollback was tested, or deployment is safe to execute.
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
