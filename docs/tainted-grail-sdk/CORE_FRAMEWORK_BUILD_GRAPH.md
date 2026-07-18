# Core and Framework Build-Graph Contract

## Status

Accepted correction contract for Slice 5. This slice decomposes the existing editor-side implementation into real build targets without changing durable schemas, user-facing behavior, runtime permissions, deployment, game launch, or save behavior.

## Targets

### `TaintedGrailModdingSDK.Core.Static`

Owns shared domain state and pure services:

- workspace, pack, source, evidence, catalog, governance, and economy models;
- catalog database and transaction logic;
- governance, promotion, validation, blocker, and economy authoring services;
- the source/evidence registry.

Core depends publicly on `AZ::AzCore`. Core must not depend on Framework, Editor, Qt, AzToolsFramework, runtime adapters, deployment, or game APIs.

### `TaintedGrailModdingSDK.Framework.Static`

Owns editor-side orchestration and persistence:

- `FoundationService` composition and domain command implementations;
- workspace candidate loading and atomic publication support;
- workspace schema, canonical path, pack, catalog, source/evidence, and workspace persistence;
- source import and the Foundation notification contract.

Framework depends publicly on Core and privately on the host-tool facilities needed by persistence and editor-side orchestration. Framework must not own widgets, the Editor module, or the Editor system component.

### `TaintedGrailModdingSDK.Editor`

Remains the Tool Gem module and owns only composition and presentation:

- Qt widgets;
- the Editor system component;
- the Gem Editor module.

Editor depends privately on Framework. It does not recompile Core or Framework sources.

## Tests

Tests link Framework and receive Core transitively. Test manifests contain test translation units only. Production `.cpp` files are compiled exactly once by Core, Framework, or Editor; tests do not compile private copies of production implementation files.

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

The test arrow means the test target links Framework; it does not reverse the production dependency direction.

## Source-location policy

This correction changes build ownership, not include paths or public C++ namespaces. Existing files remain under `Code/Source` so the split does not create an unrelated filesystem migration. The CMake manifests are the authoritative ownership map.

## Enforcement

`validate_core_framework_build_graph.py` fails when:

- a production `.cpp` is unowned or owned by more than one target;
- a test manifest recompiles production sources;
- Core owns persistence, path, import, orchestration, UI, or Editor files;
- Core includes Qt, AzToolsFramework, or a Framework-owned header;
- Framework owns UI or Editor-module files;
- Editor owns non-composition production sources;
- CMake reverses or bypasses the Core → Framework → Editor direction;
- Tool or Builder aliases expose internal static targets;
- the obsolete path-policy Editor manifest returns.

## Runtime boundary

No runtime adapter is added. No FoA, Unity, BepInEx, Harmony, deployment, injection, telemetry, game launch, or save mutation code is introduced or authorized. Core and Framework remain host-tool implementation targets inside the existing Tool Gem.

## Rollback

Revert the implementing pull request. No durable documents or user state require migration because the slice changes build ownership only.
