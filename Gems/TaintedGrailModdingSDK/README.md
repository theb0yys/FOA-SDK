# Tainted Grail Modding SDK

`TaintedGrailModdingSDK` is the editor-side foundation for a governed modding editor and SDK targeting **Tainted Grail: The Fall of Avalon**.

O3DE is the authoring host. Tainted Grail remains a separate Unity runtime target. This Gem must not assume that FoA content is native O3DE project content or that an authored record is automatically safe to execute in the game.

## Implemented foundation

The Gem provides the first practical editor milestone:

1. **Workspace and game-profile management** — editable workspace identity, root/output/staging/deployment paths, active FoA profile, exact game version and branch, Mono/IL2CPP target, Unity/BepInEx versions, install and managed-assembly paths, diagnostic/extracted-data paths, DLC scopes, and JSON save/open support.
2. **Mod and content-pack project management** — create/apply/open/save workflows for namespaced pack identity, owner, semantic version, game/Core/adapter compatibility, dependencies, required mods, incompatibilities, save impact, content definitions, assets, localisation, build configuration, release channel, and durable pack-manifest JSON.
3. **Source and evidence registry** — unique source intake, evidence-to-source validation, source lookup, and subject evidence queries.
4. **Catalog database and query service** — pack-owned knowledge rows, exact native reference lookup, filtered queries, upsert behavior, and domain coverage totals.
5. **Validation and blocker service** — workspace, game-profile, pipeline-path, pack identity, compatibility, dependency-conflict, save-impact, resource, catalog, exact-reference, and missing-reference checks that fail closed.
6. **Foundation Status and Coverage dock window** — a dockable O3DE tool showing workspace state, game profile, counts, catalog coverage, and open blockers.

The editor tools are available under **Tools → Tainted Grail SDK**:

- **Tainted Grail SDK Status**
- **Tainted Grail Pack Manager**

## Workspace documents

The status window can apply an in-memory configuration, save it as a `*.tgworkspace.json` document, and reopen it later. A workspace document contains only editor-owned configuration. It does not alter the FoA installation, game files, BepInEx configuration, or saves.

Workspace changes automatically refresh the profile summary and blocker tables. Missing exact build information, Mono loader information, research paths, or output/staging/deployment paths remains visible as an explicit blocker. Source intake must remain blocked until the active profile identifies the target build and evidence locations.

## Pack manifests

The pack manager can create a new draft, apply it to the active workspace, save it as a `*.tgpack.json` document, and reopen it later. Saved manifests are required to remain inside the active workspace root.

A pack ID must be a lowercase namespaced identifier such as `owner.pack-name`; the owner is explicit and the version uses semantic versioning. The manifest separately declares:

- primary and compatible FoA versions and branch;
- required Avalon Core and FoA adapter versions;
- pack dependencies, required mods, and incompatibilities;
- save impact;
- content definitions, asset paths, and localisation paths;
- build configuration and release channel.

The manager always writes `RuntimeActionsEnabled` as false. Runtime execution is not a pack-authoring option.

The catalog, source, and evidence stores are still in-memory and deliberately start without invented game facts.

## Product boundary

The SDK owns authoring and review surfaces for:

- pack identity and dependencies;
- exact native references and project-owned identities;
- source and evidence registration;
- game-knowledge records and relationships;
- missing references, conflicts, and stale records;
- validation runs and usage-specific permissions;
- content definitions and downstream execution handoffs.

FoA-facing adapters remain separate components. They must own native game calls, BepInEx/Harmony integration, runtime loading, persistence, cleanup, and rollback after their own validation gates pass.

## Validation

Run the foundation contract check from the repository root:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
```

The check validates Gem metadata, engine discovery, source-manifest completeness, required host-tool variants, workspace and pack editing, JSON persistence, compatibility and ownership fields, all six foundation pieces, and the absence of FoA runtime integration. It does not replace a full O3DE configure and compile.

## Next development slice

The next slice is source and evidence intake:

- importer contracts for diagnostic, dump, observation, decompilation-note, log, screenshot, CSV, JSON, and Avalon Core source-set inputs;
- source fingerprinting and exact game-profile binding;
- evidence extraction and source linkage;
- import validation, limitations, and schema-error reporting;
- durable source and evidence JSON inside the active workspace;
- source/evidence views in the Foundation Status and Coverage window.

No status displayed by the editor grants runtime permission by itself.
