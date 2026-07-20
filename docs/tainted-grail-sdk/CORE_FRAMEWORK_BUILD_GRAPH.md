# Core and Framework Build-Graph Contract

## Status

Accepted correction contract for Slice 5 and extended by Slices 6–15. The build graph decomposes editor-side implementation into real targets without changing durable schemas, runtime permissions, deployment, game launch, or save behavior.

## Targets

### `TaintedGrailModdingSDK.Core.Static`

Owns shared domain state and services that are free of Qt and host-tool dependencies:

- workspace, pack, source, evidence, catalog, governance, and economy models;
- catalog database, transactions, governance types, blockers, and validation;
- immutable economy acquisition coverage and cross-pack duplicate analysis;
- typed transient adapter declarations, semantic-version compatibility, capability and proof evaluation;
- deterministic execution-prohibited work-order planning;
- runtime-result contract validation and candidate source/evidence return;
- reproducible adapter build-manifest generation;
- deterministic package-assembly preview derivation from reviewed metadata;
- deterministic staging/deployment preview derivation with changes, conflicts, backups, and rollback;
- deterministic deployment confirmation/work-order derivation with exact scope, expiry, window, preflight, step, and checklist contracts;
- deployment execution-result verification and candidate evidence return with no executor or automatic evidence promotion;
- the source/evidence registry.

Core depends publicly on `AZ::AzCore`. Core must not depend on Framework, Editor, Qt, AzToolsFramework, runtime adapters, deployment, or game APIs.

### `TaintedGrailModdingSDK.Framework.Static`

Owns editor-side orchestration and services that require host-tool or Qt facilities:

- `FoundationService` composition and domain commands;
- governance, promotion, and economy authoring services with Qt-backed timestamps or normalisation;
- atomic workspace candidate loading;
- workspace schema, canonical path, pack, catalog, source/evidence, and workspace persistence;
- source import and the Foundation notification contract.

Framework depends publicly on Core and privately on host-tool facilities. Framework must not own widgets, the Editor module, or the Editor system component.

### `TaintedGrailModdingSDK.Editor`

Owns only composition and presentation:

- Qt widgets, including the read-only economy acquisition, duplicate-report, adapter-capability, and work-order-plan panes;
- runtime-result, build-manifest, package-assembly-preview, staging/deployment-preview, deployment-confirmation/work-order, and deployment-execution-result-evidence panes;
- the Editor system component;
- the Gem Editor module.

Editor depends privately on Framework and does not recompile Core or Framework sources.

## Tests

Tests link Framework and receive Core transitively. Test manifests contain tests and included test fragments only. Every production `.cpp` has exactly one production owner.

## Dependency direction

```text
AZ::AzCore
    ↓
Core.Static
    ↓
Framework.Static
    ↓
Editor

Framework.Static
    ↓
Catalog.Tests
```

Tests link Framework; they do not reverse the production dependency direction.

## Source-location and transient-state policy

Existing files remain under `Code/Source`; CMake manifests are the authoritative ownership map. Qt-dependent services remain Framework-owned until their host dependencies are removed under separate review.

Adapter declarations, work-order plans, runtime-result envelopes, build manifests, package-preview requests, staging/deployment-preview requests, confirmations, maintenance windows, preflight records, deployment work orders, operator checklists, deployment execution-result envelopes, and returned candidate documents are transient Core state. They have no durable schema, persistence service, filesystem loader, process access, or runtime implementation. Editor shutdown clears the transient registries.

Work-order planning reads immutable governed inputs and returns plans/refusals by value with `ExecutionAllowed=false`.

Runtime-result contracts validate externally supplied metadata against exact plans and return candidate evidence by value without registering evidence or changing governance.

Build manifests bind exact plans to caller-supplied toolchain, material, dependency, output, path, and redistribution declarations. `BuildAllowed=false` for every status.

Package previews compare an accepted evidence-backed manifest review with a project-owned staging inventory and derive layout rows, output digests, omissions, collisions, trust failures, and redistribution blockers. `AssemblyAllowed=false`, `ArchiveAllowed=false`, and `DeploymentAllowed=false` for every status. The service does not scan staging or touch files.

Staging/deployment previews compare one exact ready package layout with one reviewed target inventory and derive additions, replacements, removals, unchanged paths, conflicts, backup requirements, and typed inverse rollback steps. `StagingMutationAllowed=false`, `DeploymentMutationAllowed=false`, `RollbackExecutionAllowed=false`, and `LaunchAllowed=false` for every status. The service does not inspect or mutate a target directory.

Deployment confirmation/work-order derivation binds one exact ready staging/deployment preview to a named evidence-backed reviewer decision, typed scope, explicit UTC issue/expiry/evaluation values, a reviewed maintenance window, typed preflight evidence, exact non-executable step coverage, and a pending operator checklist. `ExecutionAllowed=false`, `CopyAllowed=false`, `DeleteAllowed=false`, `BackupAllowed=false`, `RestoreAllowed=false`, `DeploymentAllowed=false`, and `LaunchAllowed=false` for every status. The service does not consult a host clock, record acknowledgement, or invoke any operation.

Deployment execution-result contracts validate metadata from a separately reviewed executor against one exact `review_ready` work order. They preserve attempted step identities, backup/restore outcomes, target-verification states, deployed fingerprints, rollback results, failures, and safe log references, then return candidate source/evidence documents by value. Contract acceptance remains separate from execution success. The service adds no executor or automatic evidence promotion and never registers, persists, validates, permits, or publishes returned evidence.

## Enforcement

`validate_core_framework_build_graph.py` rejects duplicate or missing production ownership, production code in test manifests, reverse dependencies, Core Qt/AzToolsFramework/Framework-header coupling, UI in Framework, non-composition production code in Editor, exposed internal aliases, and obsolete manifests.

Feature-specific validators additionally enforce:

- exact case-sensitive economy signals, deterministic aggregation, non-mutation, and non-editable views;
- typed adapter identity/capability/version/proof gates and no runtime behavior;
- all-capability work-order refusal, exact payload rebuilding, canonical JSON, and `ExecutionAllowed=false`;
- exact runtime-result bindings, typed outcomes/recovery/logs, candidate evidence-only output, and no execution path;
- exact build-manifest plan/toolchain/material/output/redistribution bindings and `BuildAllowed=false`;
- accepted manifest review, project-owned staging inventory, exact package layout/digests, explicit omissions/collisions, path and redistribution gates, `AssemblyAllowed=false`, and a non-editable package preview;
- exact package/target review binding, ownership and management gates, add/replace/remove/unchanged classification, conflicts, backup and rollback completeness, deterministic canonical output, `DeploymentMutationAllowed=false`, and a non-editable staging/deployment preview;
- exact deployment confirmation/work-order binding, typed scope and expiry, reviewed maintenance window, required preflight kinds, exact step/checklist coverage, deterministic canonical output, `ExecutionAllowed=false`, and a non-editable deployment work-order pane;
- exact reviewed work-order and executor binding, attempted-step/backup/verification/rollback/failure/log contracts, candidate evidence-only return, non-mutation, no executor invocation, no automatic evidence promotion, and a non-editable deployment execution-result pane.
- exact ready release-artifact and reviewed external assembler/checksummer binding, per-content checksum observations, archive-result identity/fingerprint, failures, safe diagnostics, candidate evidence-only return, non-mutation, and a non-editable release assembly-result pane.

## Runtime boundary

No runtime adapter is added. No FoA, Unity, BepInEx, Harmony, compiler invocation, executor invocation, file copy, replacement, deletion, backup, restore, package assembly, archive creation, deployment, injection, telemetry, game launch, save mutation, work-order execution, acknowledgement, automatic evidence promotion, or independent result-verification code is introduced or authorised. Core and Framework remain host-tool implementation targets inside the existing Tool Gem.

## Rollback

Revert the implementing pull request. No durable documents or user state require migration because these slices add transient or derived analysis, planning, evidence candidates, build definitions, package previews, staging/deployment previews, confirmations, preflight records, work orders, checklists, and deployment execution-result envelopes only.
