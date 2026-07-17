# Tainted Grail Modding SDK

`TaintedGrailModdingSDK` is the editor-side foundation for a governed modding editor and SDK targeting **Tainted Grail: The Fall of Avalon**.

O3DE is the authoring host. Tainted Grail remains a separate Unity runtime target. This Gem must not assume that FoA content is native O3DE project content or that an authored record is automatically safe to execute in the game.

## Implemented foundation

The Gem provides the first practical editor milestone:

1. **Workspace and game-profile management** — editable workspace identity, root/output/staging/deployment paths, active FoA profile, exact game version and branch, Mono/IL2CPP target, Unity/BepInEx versions, install and managed-assembly paths, diagnostic/extracted-data paths, DLC scopes, and JSON save/open support.
2. **Pack manifest model** — stable pack ownership, semantic version, target build/branch, dependencies, incompatibilities, save impact, and a fail-closed runtime-action switch.
3. **Source and evidence registry** — unique source intake, evidence-to-source validation, source lookup, and subject evidence queries.
4. **Catalog database and query service** — pack-owned knowledge rows, exact native reference lookup, filtered queries, upsert behavior, and domain coverage totals.
5. **Validation and blocker service** — workspace, game-profile, pipeline-path, pack, source, evidence, catalog, exact-reference, and missing-reference checks that fail closed.
6. **Foundation Status and Coverage dock window** — a dockable O3DE tool showing workspace state, game profile, counts, catalog coverage, and open blockers.

The dock window is available under **Tools → Tainted Grail SDK → Tainted Grail SDK Status**.

## Workspace documents

The status window can apply an in-memory configuration, save it as a `*.tgworkspace.json` document, and reopen it later. A workspace document contains only editor-owned configuration. It does not alter the FoA installation, game files, BepInEx configuration, or saves.

Workspace changes automatically refresh the profile summary and blocker tables. Missing exact build information, Mono loader information, research paths, or output/staging/deployment paths remains visible as an explicit blocker. Source intake must remain blocked until the active profile identifies the target build and evidence locations.

The catalog, pack, source, and evidence stores are still in-memory and deliberately start without invented game facts.

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

The check validates Gem metadata, engine discovery, source-manifest completeness, required host-tool variants, workspace editing and JSON persistence, all six foundation pieces, and the absence of FoA runtime integration. It does not replace a full O3DE configure and compile.

## Next development slice

The next slice is mod and content-pack project management:

- create/open pack UI;
- stable pack ID, owner, and semantic version validation;
- compatible game/Core/adapter versions;
- dependencies, required mods, and incompatibilities;
- save-impact, asset, localisation, build, and release declarations;
- durable pack-manifest JSON inside the active workspace.

No status displayed by the editor grants runtime permission by itself.
