# User Guide

## Overview

The Tainted Grail Modding Editor and SDK is an O3DE-hosted authoring environment for governed FoA mod projects. It records exact game-build context, pack ownership, source provenance, evidence, blockers, and eventually canonical game knowledge and authored content.

The current project is pre-alpha. It does not yet provide production runtime deployment or complete domain authoring tools.

## Before you begin

You need:

- a source build environment supported by the O3DE revision in this repository;
- this repository checked out locally;
- Git LFS configured for the O3DE source tree;
- a legitimate FoA installation when configuring a game profile;
- writable workspace, output, staging, and deployment directories;
- legally distributable data for any committed examples or fixtures.

Do not place your workspace inside the game installation. Keep authoring data, generated output, staging, and deployment locations separate.

## Build and open the editor

Build the O3DE Editor using the standard source-build process for your platform. The `TaintedGrailModdingSDK` Gem is registered with the engine and is available to host-tool builds.

Run the repository contract validator before diagnosing a build problem:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
```

After launching the O3DE Editor, open **Tools → Tainted Grail SDK**.

Current tools:

- **Tainted Grail SDK Status**
- **Tainted Grail Pack Manager**
- **Tainted Grail Source Intake**

## Recommended workflow

Use the tools in this order:

1. configure and save a workspace;
2. configure an exact game profile;
3. create and save a pack manifest;
4. import sources and evidence;
5. review blockers and compatibility;
6. use later catalog and authoring tools only after their required evidence and permissions exist.

## Workspace and game profile

Open **Tainted Grail SDK Status**.

### Workspace fields

Configure:

- workspace ID;
- display name;
- workspace root;
- output directory;
- staging directory;
- deployment directory.

The directories have separate purposes:

- **workspace root** stores project-controlled JSON documents and authoring data;
- **output** stores generated build products;
- **staging** stores package layouts before deployment;
- **deployment** is the explicit destination considered by later deployment tooling.

Using distinct directories reduces accidental overwrite and makes review and rollback possible.

### Game-profile fields

Configure:

- stable profile ID;
- profile display name;
- FoA installation path;
- exact game version;
- game branch;
- runtime target: `Mono` or `IL2CPP`;
- Unity version;
- BepInEx version for Mono profiles;
- managed assemblies path;
- BepInEx plugin path for Mono profiles;
- diagnostics path;
- extracted-data path;
- installed DLC and content scopes.

Imported evidence is bound to the active profile ID, game version, branch, and runtime target. Changing profiles does not automatically re-authorise older evidence.

### Apply and save

- **Apply Configuration** updates the in-memory workspace.
- **Save Workspace As** creates a `*.tgworkspace.json` document.
- **Save Workspace** updates the current document.
- **Open Workspace** loads an existing document and reloads its persisted source/evidence records.

A workspace document contains editor configuration only. It does not edit the game installation, BepInEx configuration, or saves.

## Pack manager

Open **Tainted Grail Pack Manager** after configuring the workspace and active game profile.

### Identity and ownership

A pack requires:

- lowercase namespaced pack ID, such as `owner.pack-name`;
- display name;
- owner ID;
- semantic version, such as `0.1.0`.

Custom records created later must be owned by a pack. Do not reuse a native game identity for custom content.

### Compatibility

Declare:

- primary game version;
- target branch;
- additional compatible game versions;
- required Avalon Core version;
- required FoA adapter version;
- DLC and content scopes.

The blocker engine compares the active game profile with the pack's declared compatibility.

### Dependencies and conflicts

Enter one value per line for:

- pack dependencies;
- required external mods;
- incompatibilities.

A dependency or required mod must not also be listed as incompatible.

### Save impact

Choose one:

- `none` — no persistent save effect is expected;
- `compatible` — persistent data is expected to remain compatible;
- `migration` — a versioned migration is required;
- `destructive` — removal or change may lose persistent state;
- `unknown` — impact has not been established and release remains blocked.

The declaration is a planning and validation input, not proof by itself.

### Content and release declarations

Declare content-definition, asset, and localisation paths relative to the workspace or pack layout. Also declare a build configuration and release channel.

### Apply and save

- **New Pack** clears the active draft.
- **Apply** validates identity and publishes the pack to the in-memory foundation state.
- **Save As** writes a `*.tgpack.json` manifest inside the workspace root.
- **Save** updates the active manifest.
- **Open** loads an existing manifest.

Pack manifests outside the active workspace root are rejected. Runtime actions are always written as disabled.

## Source and evidence intake

Open **Tainted Grail Source Intake** after the active workspace and game profile are fully configured.

### Supported source kinds

- template diagnostics;
- item and recipe dumps;
- scene and world observations;
- decompilation notes;
- runtime logs;
- screenshots;
- CSV registers;
- JSON registers;
- Avalon Core source sets.

### Importer contracts

#### Structured JSON

Accepts:

- a root JSON array of evidence objects;
- an object containing an `evidence` array;
- one evidence-shaped root object.

Required fields:

```json
{
  "subject_ref": "subject:example",
  "claim": "Evidence-backed statement"
}
```

Optional fields:

- `evidence_id`;
- `kind`;
- `confidence`;
- `locator`.

#### Structured CSV

Required columns:

```text
subject_ref,claim
```

Optional columns:

```text
evidence_id,kind,confidence,locator
```

#### Generic artifact

Fingerprints and registers the file without creating evidence. Use this for opaque dumps, logs, screenshots, notes, and unsupported structured formats. Manual evidence extraction remains required.

### Source metadata

Provide, when known:

- source kind;
- title;
- tool name and version;
- capture time;
- limitations;
- preferred importer contract.

The importer computes a SHA-256 fingerprint and deterministic source ID. The same fingerprint cannot be imported twice for the same game profile.

### Durable output

Each successful intake writes:

```text
Sources/<source-id>/source.tgsource.json
Sources/<source-id>/evidence.tgevidence.json
```

The in-memory registry is updated only after both documents save successfully.

### Import issues

Malformed inputs, missing fields, duplicate evidence IDs, unsupported schemas, size limits, profile mismatches, and document mismatches are preserved as structured import issues.

An imported source may exist with warnings or errors. That does not make its evidence reviewed or usable for runtime action.

## Foundation status and blockers

The status window is the shared health view for:

- active workspace and game profile;
- active pack;
- source, evidence, catalog, and blocker counts;
- domain coverage;
- missing paths and compatibility;
- import and schema problems;
- unresolved identity, evidence, or relationship requirements.

A blocker explains why a use is unavailable. Fix the underlying configuration or evidence; do not remove blockers merely to make the interface appear complete.

## Workspace file layout

A typical workspace evolves toward:

```text
MyWorkspace/
├── workspace.tgworkspace.json
├── Packs/
│   └── owner.pack-name/
│       └── pack.tgpack.json
├── Sources/
│   └── source.profile.fingerprint/
│       ├── source.tgsource.json
│       └── evidence.tgevidence.json
├── Catalog/
├── Content/
├── Assets/
├── Localisation/
├── Build/
└── Reports/
```

Catalog and later authoring layouts are introduced by their respective milestones.

## Safe-use rules

- Back up workspaces and saves before testing any future adapter or deployment feature.
- Never treat a display name as an identity.
- Keep exact native references unchanged.
- Do not reuse native IDs for custom records.
- Do not share copyrighted game assets or proprietary source material.
- Review generated output before deployment.
- Treat warnings and unknown save impact as unresolved work.
- Do not assume an imported claim is true because parsing succeeded.

## Troubleshooting

### The TG SDK menu is missing

- confirm the Gem is present in `engine.json` external subdirectories;
- confirm you built a host-tool target;
- run the foundation validator;
- inspect Editor logs for Gem module load errors.

### Workspace save fails

- verify the target directory is writable;
- avoid the game installation and protected system directories;
- confirm the file suffix and parent path are valid;
- review the error message for serialization or path details.

### Pack save fails

- use a lowercase namespaced pack ID;
- provide owner and semantic version;
- keep the manifest inside the workspace root;
- ensure runtime actions are disabled.

### Source import fails

- configure an exact active game profile;
- verify the source file exists and is readable;
- choose the correct importer contract;
- inspect schema issues in the intake window;
- use generic artifact intake when the data is not evidence-shaped.

### A source imported but no evidence appeared

This is expected for generic artifacts and for structured files that do not contain the required evidence shape. The source remains registered for manual review.

## Getting help

Follow [SUPPORT.md](../../SUPPORT.md). Security issues follow [SECURITY.md](../../SECURITY.md).
