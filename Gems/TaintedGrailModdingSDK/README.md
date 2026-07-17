# Tainted Grail Modding SDK

`TaintedGrailModdingSDK` is the editor-side foundation for a governed modding editor and SDK targeting **Tainted Grail: The Fall of Avalon**.

O3DE is the authoring host. Tainted Grail remains a separate Unity runtime target. This Gem must not assume that FoA content is native O3DE project content or that an authored record is automatically safe to execute in the game.

## First development slice

This initial slice establishes:

- an editor-only O3DE Tool Gem;
- engine discovery through `engine.json`;
- a required editor system component;
- a stable service identity for future editor subsystems;
- explicit fail-closed logging that FoA runtime execution is disabled.

It does not yet provide a dock widget, import pipeline, catalog database, runtime adapter, asset converter, packager, or game mutation path.

## Product boundary

The SDK will own authoring and review surfaces for:

- pack identity and dependencies;
- exact native references and project-owned identities;
- source and evidence registration;
- game-knowledge records and relationships;
- missing references, conflicts, and stale records;
- validation runs and usage-specific permissions;
- content definitions and downstream execution handoffs.

FoA-facing adapters remain separate components. They must own native game calls, BepInEx/Harmony integration, runtime loading, persistence, cleanup, and rollback after their own validation gates pass.

## Next development slice

The next editor feature is the **Foundation Status and Coverage** surface. It will expose:

1. registered source counts;
2. record counts by domain and maturity;
3. open missing references and conflicts;
4. validation and permission summaries;
5. a clear distinction between research-only, planning-ready, and runtime-authorised data.

No status displayed by that surface will grant runtime permission by itself.
