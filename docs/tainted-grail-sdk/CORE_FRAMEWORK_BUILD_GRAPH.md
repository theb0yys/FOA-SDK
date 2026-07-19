# Core and Framework Build-Graph Contract

## Status

Accepted correction contract for Slice 5 and extended by Slices 6–12. The build graph decomposes editor-side implementation into real targets without changing durable schemas, runtime permissions, deployment, game launch, or save behavior.

## Targets

### `TaintedGrailModdingSDK.Core.Static`

Owns shared domain state and services that are free of Qt and host-tool dependencies:

- workspace, pack, source, evidence, catalog, governance, and economy models;
- catalog database, transactions, governance types, blockers, and validation;
- immutable economy acquisition coverage and cross-pack duplicate analysis;
- typed transient adapter declarations, semantic-version compatibility, capability and proof evaluation;
- deterministic execution-prohibited work-order planning;
- typed runtime-result validation and candidate evidence return;
- reproducible adapter build-manifest generation;
- deterministic package-assembly preview derivation from reviewed metadata;
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
- runtime-result, build-manifest, and package-assembly-preview panes;
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

Adapter declarations, work-order plans, runtime-result envelopes, build manifests, and package-preview requests are transient Core state. They have no durable schema, persistence service, filesystem loader, process access, or runtime implementation. Editor shutdown clears the transient registries.

Work-order planning reads immutable governed inputs and returns plans/refusals by value with `ExecutionAllowed=false`.

Runtime-result contracts validate externally supplied metadata against exact plans and return candidate evidence by value without registering evidence or changing governance.

Build manifests bind exact plans to caller-supplied toolchain, material, dependency, output, path, and redistribution declarations. `BuildAllowed=false` for every status.

Package previews compare an accepted evidence-backed manifest review with a project-owned staging inventory and derive layout rows, output digests, omissions, collisions, trust failures, and redistribution blockers. `AssemblyAllowed=false`, `ArchiveAllowed=false`, and `DeploymentAllowed=false` for every status. The service does not scan staging or touch files.

## Enforcement

`validate_core_framework_build_graph.py` rejects duplicate or missing production ownership, production code in test manifests, reverse dependencies, Core Qt/AzToolsFramework/Framework-header coupling, UI in Framework, non-composition production code in Editor, exposed internal aliases, and obsolete manifests.

Feature-specific validators additionally enforce:

- exact case-sensitive economy signals, deterministic aggregation, non-mutation, and non-editable views;
- typed adapter identity/capability/version/proof gates and no runtime behavior;
- all-capability work-order refusal, exact payload rebuilding, canonical JSON, and `ExecutionAllowed=false`;
- exact runtime-result bindings, typed outcomes/recovery/logs, candidate evidence-only output, and no execution path;
- exact build-manifest plan/toolchain/material/output/redistribution bindings and `BuildAllowed=false`;
- accepted manifest review, project-owned staging inventory, exact package layout/digests, explicit omissions/collisions, path and redistribution gates, `AssemblyAllowed=false`, and a non-editable package preview.

## Runtime boundary

No runtime adapter is added. No FoA, Unity, BepInEx, Harmony, compiler invocation, file copy, package assembly, archive creation, deployment, injection, telemetry, game launch, save mutation, work-order execution, or result capture code is introduced or authorised. Core and Framework remain host-tool implementation targets inside the existing Tool Gem.

## Rollback

Revert the implementing pull request. No durable documents or user state require migration because these slices add transient or derived analysis, planning, evidence candidates, build definitions, and package previews only.
