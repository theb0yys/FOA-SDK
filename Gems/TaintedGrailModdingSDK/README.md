# Tainted Grail Modding SDK

`TaintedGrailModdingSDK` is the editor-side foundation for a governed modding editor and SDK targeting **Tainted Grail: The Fall of Avalon**.

O3DE is the authoring host. Tainted Grail remains a separate Unity runtime target. This Gem must not assume that FoA content is native O3DE project content or that an authored record is automatically safe to execute in the game.

## Implemented foundation

1. **Workspace and game-profile management** — editable workspace identity, root/output/staging/deployment paths, active FoA profile, exact game version and branch, Mono/IL2CPP target, Unity/BepInEx versions, install and managed-assembly paths, diagnostic/extracted-data paths, DLC scopes, and JSON save/open support.
2. **Mod and content-pack project management** — create/apply/open/save workflows for namespaced pack identity, owner, semantic version, game/Core/adapter compatibility, dependencies, required mods, incompatibilities, save impact, content definitions, assets, localisation, build configuration, release channel, and durable pack-manifest JSON.
3. **Source and evidence intake** — importer contracts, SHA-256 fingerprints, exact profile/build binding, JSON/CSV evidence extraction, generic artifact registration without inference, durable source/evidence documents, reload support, and import/schema issue reporting.
4. **Canonical catalog browser and record inspector** — durable records, first-class relationships, validation history, exact-reference/GUID search, evidence and blocker inspection, and evidence-backed promotion into reviewed-but-unvalidated records.
5. **Validation and blocker service** — workspace, profile, pack, source, import, catalog identity, evidence, relationship, conflict, missing-reference, validation, and permission checks that fail closed.
6. **Foundation Status and Coverage window** — shared workspace state, coverage, counts, import issues, and blockers.

## Editor tools

Available under **Tools → Tainted Grail SDK**:

- **Tainted Grail SDK Status**
- **Tainted Grail Pack Manager**
- **Tainted Grail Source Intake**
- **Tainted Grail Catalog Browser**

## Workspace documents

### Workspace

```text
*.tgworkspace.json
```

Contains editor-owned workspace and exact game-profile configuration. It does not alter FoA, BepInEx, game files, or saves.

### Pack manifest

```text
*.tgpack.json
```

A pack ID is lowercase and namespaced, the owner is explicit, and the version is semantic. The manifest declares compatibility, dependencies, save impact, resources, build intent, and release channel. Runtime actions are always persisted as disabled.

### Source and evidence

```text
Sources/<source-id>/source.tgsource.json
Sources/<source-id>/evidence.tgevidence.json
```

Each artifact is SHA-256 fingerprinted and bound to the active profile ID, game version, branch, and runtime target. Structured JSON and CSV may extract evidence. Generic intake never invents evidence.

### Canonical catalog

```text
Catalog/catalog.tgcatalog.json
```

The document is bound to the workspace and active exact game profile. It contains:

- canonical records with stable record IDs;
- exact native refs and GUIDs for native records;
- pack ownership for synthetic records;
- aliases and source-scoped references;
- evidence links;
- first-class relationships;
- maturity, confidence, risk, validation, permissions, and prohibitions;
- conflicts, missing refs, supersession, and validation history.

Catalog records are never merged by display name. Promotion requires an existing evidence ID with the same active game-profile binding. Promotion creates a reviewed, `unvalidated` record, adds `no_unvalidated_runtime_use`, and cannot grant an allowed usage.

## Product boundary

The SDK owns authoring, evidence, canonical knowledge, validation, planning, and handoff surfaces. FoA-facing adapters remain separate components and must own native game calls, BepInEx/Harmony integration, runtime loading, persistence, cleanup, and rollback after their own validation gates pass.

## Public project documentation

The root project includes a full public documentation and governance set:

- project README and user guide;
- architecture and data formats;
- contribution, code-quality, design-review, pre-commit review, and merge rules;
- governance, conduct, security, privacy, legal/content, accessibility, support, roadmap, changelog, release, and maintainer policies;
- CODEOWNERS, pull-request template, and issue forms.

Start at [`docs/tainted-grail-sdk/README.md`](../../docs/tainted-grail-sdk/README.md).

## Validation

Run from the repository root:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
```

The contract check validates public-project governance, Gem registration, source manifests, host-tool variants, workspace and pack editing, importer contracts, source/evidence persistence, canonical catalog persistence and promotion boundaries, editor panes, and absence of FoA runtime integration. It does not replace a full O3DE configure and compile.

## Next development slice

The next slice expands the independent validation, maturity, operational-risk, and permission engine around canonical records and relationships. No displayed or imported status grants runtime permission by itself.
