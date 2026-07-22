# FOA-SDK optional Tool Gem system

## Status

The package layout, manifest contract, deterministic validation, ExtensionAPI lifecycle, host-owned document persistence, project selection, and first two authoring packages are implemented. Exact-head O3DE configure/build, compiled tests, and Windows UI evidence remain required before this head is accepted.

## Architecture

FOA-SDK does not implement a second dynamic loader. Every executable editor plug-in is an ordinary O3DE Tool Gem beneath `Plugins/<Category>/<Package>/Gem` and is selected through `TaintedGrailModdingEditor/project.json`.

Each package owns:

- one schema-1 `plugin.json` at the package root;
- one stable `extension.*` identity and strict semantic version;
- exact category, compatibility, capability, dependency, provenance, licence, and redistribution-review declarations;
- one or more explicit package-local entry points;
- its O3DE module/component activation and deactivation lifecycle;
- any panes and package-specific validation logic.

Repository validation rejects malformed manifests, unsafe paths, missing Tool Gem entry points, unknown capabilities, duplicate identities, missing dependencies, dependency cycles, category drift, target drift, unpinned Git provenance, unlicensed redistribution claims, and project-selection drift.

## Cross-module ExtensionAPI

Optional Tool Gems use `ExtensionRequestBus`. Foundation remains the single handler and delegates to the existing bounded `ExtensionAPI::Service`.

The bus exposes only:

- deterministic register, unregister, and registration-state operations;
- sanitized active-profile copies;
- bounded copied catalog queries;
- governed candidate-evidence submission;
- host-owned extension-document load/save operations.

It exposes no mutable workspace, catalog, source registry, game path, deployment path, runtime host, signing service, publication service, save access, or process execution.

Activation requires `TaintedGrailModdingSDKService`, so Foundation is active before an optional package registers. Deactivation unregisters the pane and extension identity. The canonical Tainted Framework consumer is registered by Foundation itself and cannot be removed while Foundation is active.

## Extension documents

Packages never receive the workspace root. They request a document by extension ID and safe relative path. Foundation resolves the bounded destination below:

```text
Extensions/<extension-id>/<relative-path>
```

Writes are limited to 4 MiB, reject absolute paths, traversal, separators outside the portable contract, and unregistered identities, and publish through `QSaveFile`. Failed writes cancel the candidate and do not replace prior state.

## Available authoring packages

### Road Atlas

`Plugins/Authoring/RoadAtlas` provides the **Tainted Grail Road Atlas Editor**. It creates, loads, parses, validates, saves, and reverts exact-profile schema-1 snapshots. Every document is converted to typed Road Atlas contracts before save. Validation exposes subject-specific issues and the deterministic planning fingerprint.

The package remains planning-only. It cannot mutate scenes, move actors, spawn content, deploy files, launch FoA, or write saves.

### Avalon AI

`Plugins/Authoring/AvalonAI` provides the **Tainted Grail Avalon AI Editor**. It creates, loads, parses, validates, saves, and reverts API 2.0 packages containing typed blackboard keys, goals, conditions, actions, effects, capabilities, interrupt policies, and timeouts. Save requires a valid deterministic inert authoring plan.

The package does not link an AI runtime host, dispatch actions, mutate blackboards, deploy files, launch FoA, or write saves.

## Selection and removal

The dedicated source-built Editor currently selects both available packages. A downstream product can omit either package by removing its Tool Gem path and Gem name from its own project manifest. Package removal does not invalidate the host; its pane and extension registration simply do not exist in that product.

Removing a package does not delete its workspace documents. This preserves recovery and avoids silent data loss. Re-enabling the same compatible package can load those documents through the host boundary.

## Verification

`validate_plugin_packages.py` validates package manifests, entry points, dependencies, compatibility, provenance, and deterministic project selection. `validate_editor_lifecycle.py` inventories all twenty-six panes and checks build ownership, registration/deactivation, unique saved-layout keys, Hub reachability, optional-Gem activation ordering, atomic persistence, and runtime separation.

Python mutation suites exercise malformed manifests, unknown capabilities, dependency cycles, missing entry points, duplicate layout keys, missing Hub routes, missing unregister paths, non-atomic storage, and process leakage. Production-linked C++ tests cover ExtensionAPI revocation/reset and sanitized Road Atlas profile validation.

These checks do not substitute for exact-head O3DE configuration, compilation, compiled CTest execution, or the real Windows twenty-six-pane UI pass.
