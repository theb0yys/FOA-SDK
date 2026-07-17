# Tainted Grail Modding SDK

`TaintedGrailModdingSDK` is the editor-side foundation for a governed modding editor and SDK targeting **Tainted Grail: The Fall of Avalon**.

O3DE is the authoring host. Tainted Grail remains a separate Unity runtime target. This Gem must not assume that FoA content is native O3DE project content or that an authored record is automatically safe to execute in the game.

## Implemented foundation

The Gem now provides the first practical editor milestone:

1. **Workspace and game-profile model** — workspace identity, root path, active FoA profile, exact game version, branch, Unity/BepInEx versions, install paths, diagnostics paths, and DLC scopes.
2. **Pack manifest model** — stable pack ownership, semantic version, target build/branch, dependencies, incompatibilities, save impact, and a fail-closed runtime-action switch.
3. **Source and evidence registry** — unique source intake, evidence-to-source validation, source lookup, and subject evidence queries.
4. **Catalog database and query service** — pack-owned knowledge rows, exact native reference lookup, filtered queries, upsert behavior, and domain coverage totals.
5. **Validation and blocker service** — workspace, game-profile, pack, source, evidence, catalog, exact-reference, and missing-reference checks that fail closed.
6. **Foundation Status and Coverage dock window** — a dockable O3DE tool showing workspace state, game profile, counts, catalog coverage, and open blockers.

The dock window is available under **Tools → Tainted Grail SDK → Tainted Grail SDK Status**.

The current database is in-memory and deliberately starts without invented game facts. Until a workspace, exact game profile, pack manifest, sources, evidence, and catalog rows are supplied, the status window reports those omissions as explicit blockers.

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

The check validates Gem metadata, engine discovery, source-manifest completeness, required host-tool variants, all six first-milestone pieces, and the absence of FoA runtime integration. It does not replace a full O3DE configure and compile.

## Next development slice

The next slice will make the foundation editable and durable:

- workspace and game-profile configuration UI;
- pack creation/opening UI;
- JSON persistence for workspace, manifests, registries, blockers, and catalog records;
- source/evidence import commands;
- catalog search and record inspection;
- automatic status refresh when foundation data changes.

No status displayed by the editor grants runtime permission by itself.
