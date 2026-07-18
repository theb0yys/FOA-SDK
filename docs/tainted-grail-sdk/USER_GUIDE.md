# User Guide

## Overview

The Tainted Grail Modding Editor and SDK is an O3DE-hosted authoring environment for governed FoA mod projects. It records exact game-build context, pack ownership, source provenance, evidence, canonical identities, independent governance decisions, permissions, prohibitions, and blockers.

The current project is pre-alpha. It does not provide production runtime deployment or complete domain authoring tools.

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

Run the repository validators first:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
python Gems/TaintedGrailModdingSDK/Tools/validate_catalog_tests.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_contracts.py
```

After launching the O3DE Editor, open **Tools → Tainted Grail SDK**.

Current tools:

- **Tainted Grail SDK Status**
- **Tainted Grail Pack Manager**
- **Tainted Grail Source Intake**
- **Tainted Grail Catalog Browser**
- **Tainted Grail Catalog Governance**
- **Tainted Grail Item and Recipe Editor**
- **Tainted Grail Economy Acquisition Coverage**
- **Tainted Grail Economy Cross-Pack Duplicates**
- **Tainted Grail Adapter Capability Matrix**

## Recommended workflow

Use the tools in this order:

1. configure and save a workspace;
2. configure an exact game profile;
3. create and save a pack manifest;
4. import sources and evidence;
5. promote evidence into canonical records;
6. inspect records, relationships, and blockers;
7. review maturity, confidence, risk, and staleness independently;
8. record validation proof;
9. allow or forbid one named usage lane explicitly;
10. review economy acquisition coverage and exact cross-pack duplicate candidates;
11. review adapter capability, version, permission, and proof readiness;
12. review the shared status window before downstream planning.

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

The roots have separate purposes:

- **workspace** stores governed documents and authoring data;
- **output** stores generated products;
- **staging** stores package layouts before deployment;
- **deployment** is the explicit destination considered by later tools.

### Game-profile fields

Configure:

- stable profile ID and name;
- FoA installation path;
- exact game version and branch;
- `Mono` or `IL2CPP` runtime target;
- Unity version;
- BepInEx version and plugin path for Mono profiles;
- managed assemblies;
- diagnostics and extracted-data locations;
- installed DLC/content scopes.

Imported evidence, catalogs, validation events, governance decisions, and adapter evidence are bound to the exact active profile. Changing profiles does not re-authorise older data.

### Apply and save

- **Apply Configuration** updates in-memory state.
- **Save Workspace As** creates a `*.tgworkspace.json` document.
- **Save Workspace** updates the active document.
- **Open Workspace** loads configuration, source/evidence documents, and the canonical catalog.

## Pack manager

Open **Tainted Grail Pack Manager** after configuring the workspace and profile.

A pack requires:

- lowercase namespaced pack ID such as `owner.pack-name`;
- explicit owner ID;
- display name;
- semantic version;
- exact game/Core/adapter compatibility;
- dependencies, required mods, and incompatibilities;
- save-impact declaration;
- content, asset, and localisation declarations;
- build configuration and release channel.

`RequiredAdapterVersion` is the pack’s minimum adapter contract version. Slice 8 interprets it through a conservative semantic-version policy: the declared adapter must be at least that version, use the same major version, and use the same minor version while the major version is zero.

Synthetic catalog records require an existing pack owner. Do not reuse native game identities for custom content.

Pack manifests must remain inside the workspace and always persist runtime actions as disabled.

## Source and evidence intake

Open **Tainted Grail Source Intake** after the workspace and profile are complete.

Supported source kinds include diagnostics, item/recipe dumps, world observations, decompilation notes, runtime logs, screenshots, CSV/JSON registers, and Avalon Core source sets.

Importer contracts:

- `tg.structured-json` extracts evidence-shaped JSON;
- `tg.structured-csv` extracts rows containing `subject_ref` and `claim`;
- `tg.generic-artifact` fingerprints opaque data without inventing evidence.

Each successful intake writes:

```text
Sources/<source-id>/source.tgsource.json
Sources/<source-id>/evidence.tgevidence.json
```

The importer computes a SHA-256 fingerprint and deterministic source ID. The same fingerprint cannot be imported twice for one profile. The in-memory registry is published only after both documents save.

Parsing success does not make a claim reviewed or permitted.

Adapter identity evidence uses the exact subject:

```text
adapter:<adapter-id>
```

Adapter capability evidence uses:

```text
adapter:<adapter-id>:capability:<capability>
```

These evidence records still require the normal exact source fingerprint, profile, game version, branch, and runtime-target bindings.

## Canonical catalog browser

Open **Tainted Grail Catalog Browser**.

The catalog is stored at:

```text
Catalog/catalog.tgcatalog.json
```

It is bound to workspace ID, profile ID, exact game version, and branch.

### Search

Search by:

- record ID;
- exact native ref or GUID;
- subject reference;
- display name or alias;
- domain and kind;
- owner pack;
- identity kind;
- maturity and confidence;
- operational risk;
- validation and staleness;
- permission/prohibition;
- evidence ID;
- blocked state;
- supersession.

Display-name search is discovery only. Records are never merged because labels match.

### Inspect

Selecting a record shows:

- stable identity and ownership;
- exact refs and aliases;
- evidence details;
- relationships;
- current governance states;
- validation and governance history;
- permissions and prohibitions;
- missing refs, conflicts, supersession, and blockers.

### Promote evidence

Promotion requires an existing profile-matched evidence ID and a new stable record ID.

Native records require an exact native ref and cannot claim custom pack ownership. Synthetic records require an existing owner pack and cannot borrow a native ref.

Promotion creates a reviewed-but-`unvalidated`, staleness-`unknown` record. It adds `no_unvalidated_runtime_use` and cannot create an allowed usage.

## Catalog governance

Open **Tainted Grail Catalog Governance** after selecting or creating canonical subjects.

The pane supports both records and relationships.

### Current states

It displays:

- maturity;
- confidence;
- operational risk;
- validation;
- staleness;
- allowed usages;
- prohibitions;
- supersession;
- validation history;
- governance history.

### Governance decisions

Select an axis:

- `maturity`;
- `confidence`;
- `operational_risk`;
- `staleness`;
- `permission`;
- `supersession`.

Provide evidence IDs, a named reviewer, and notes. Permission grants also require validation proof IDs.

### Validation decisions

Provide:

- validation state;
- method or test protocol;
- evidence IDs;
- named validator;
- notes.

Validation is bound to the active exact profile/build and is recorded as append-only history.

**Validation does not grant permission.**

### Allow a usage

To allow one named usage, the subject must be:

- `validated`;
- `current`;
- free of missing refs and conflicts;
- not superseded.

The decision also requires evidence and at least one `validated` proof event for the same subject.

Phase 7 formalises the economy adapter-facing usage names:

- `existing_item_grant`;
- `existing_recipe_learn`;
- `runtime_recipe_append`;
- `custom_item_registration`;
- `custom_recipe_registration`;
- `vendor_or_loot_injection`;
- `quest_or_contract_reward_injection`.

### Forbid a usage

A `forbid` decision removes the usage from the allowed list and creates an explicit prohibition. A prohibition remains meaningful even when the subject is visible and searchable.

### Staleness and supersession

Marking a subject `potentially_stale` or `stale` removes all allowed usages. Returning it to `current` does not restore old permissions.

Supersession requires a different existing replacement ID. The old subject becomes stale, loses allowed usage, and remains durable for auditability.

See [Governance Engine Guide](GOVERNANCE_ENGINE_GUIDE.md) for the full state vocabulary and transition rules.

## Item and recipe authoring

Open **Tainted Grail Item and Recipe Editor** after canonical economy records and evidence exist.

The pane authors typed item and recipe profiles on existing canonical identities. It also manages ingredient, output, by-product, station, unlock, and acquisition relationship data through the shared catalog transaction boundary.

The lane matrices and station/learnability evidence rows are read-only. The pane cannot grant governance permission and does not invoke FoA runtime behavior.

## Economy acquisition coverage

Open **Tainted Grail Economy Acquisition Coverage** to inspect read-only vendor, loot, reward, learnability, and crafting coverage.

The view derives applicable lanes from canonical relationships, exact evidence bindings, governance state, and open blockers. `covered`, `partial`, `blocked`, and `missing` describe research coverage only; they do not grant permission or prove runtime behavior.

## Economy cross-pack duplicate report

Open **Tainted Grail Economy Cross-Pack Duplicates** to review authoring-time duplicate candidates across owner packs.

The report uses only:

- exact subject references for pack-owned economy records of the same kind;
- exact recipe duplicate keys for pack-owned recipes;
- at least two distinct owner packs.

Matching is case-sensitive and never uses display-name similarity, aliases, localisation text, tags, asset paths, or fuzzy inference.

A `review_required` group has current validated candidates with usable exact-subject evidence and no open blockers. `partial` means at least one candidate is not yet current/validated or lacks its typed profile. `blocked` means evidence, governance, references, supersession, or blockers fail closed.

The report does not merge records, reject a pack, choose a winner, author governance, or prove a runtime conflict. Review the exact canonical records and evidence before making any separate catalog decision.

## Adapter capability matrix

Open **Tainted Grail Adapter Capability Matrix** to inspect Phase 7 readiness across packs, typed adapter declarations, the active runtime target, catalog permissions, validation proof, and adapter evidence.

Slice 8 defines these capabilities:

- `item_grant`;
- `recipe_learn`;
- `recipe_append`;
- `custom_item_registration`;
- `custom_recipe_registration`;
- `vendor_mutation`;
- `loot_mutation`;
- `reward_mutation`;
- `persistence`;
- `cleanup`;
- `rollback`.

The matrix statuses are:

- `unsupported` — no declaration is registered, the capability is undeclared, or the active runtime target is not declared;
- `version_mismatch` — the declaration fails the pack’s semantic-version requirement;
- `permission_missing` — no eligible pack-owned subject has the mapped allowed usage;
- `proof_missing` — the allowed usage exists without a valid same-subject permission/validation chain, or adapter identity/capability evidence is invalid;
- `supported` — declaration, runtime, version, permission, and proof gates pass.

The evaluation order is support, version, permission, proof, then `supported`. A more fundamental failure therefore cannot be hidden by a later proof state.

The adapter declaration registry is transient and cleared when the Editor shuts down. There is no adapter declaration document or registration control in Slice 8. With no declaration registered, the matrix deliberately shows `unsupported` rows for all eleven capabilities for each pack.

**`supported` means contract readiness only.** It does not load or execute an adapter, generate a work order, deploy files, launch FoA, or mutate a save.

## Foundation status and blockers

The status window reports:

- active workspace/profile and pack;
- source and evidence totals;
- catalog records and relationships;
- validation events and governance decisions;
- stale subjects;
- allowed and prohibited usage counts;
- domain coverage;
- import, compatibility, identity, evidence, governance, permission, and proof blockers.

A blocker explains why a use is unavailable. Fix the underlying data or decision; do not remove validation solely to make the dashboard green.

## Workspace layout

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
│   └── catalog.tgcatalog.json
├── Content/
├── Assets/
├── Localisation/
├── Build/
└── Reports/
```

There is no adapter declaration file in the workspace layout in Slice 8.

## Safe-use rules

- Back up workspaces and saves before testing future adapter/deployment features.
- Never treat a display name as an identity.
- Keep exact native references unchanged.
- Do not reuse native IDs for custom records.
- Do not share copyrighted game assets or proprietary source material.
- Treat unknown risk, staleness, or save impact as unresolved work.
- Do not assume an imported claim is true because parsing succeeded.
- Do not assume a validated claim is permitted for every usage.
- Do not assume clearing staleness restores permission.
- Do not treat a duplicate candidate group as an automatic merge or deletion instruction.
- Do not treat a `supported` adapter row as permission to execute runtime behavior.
- Review generated output before any future deployment.

## Troubleshooting

### TG SDK menu is missing

- confirm the Gem is registered in `engine.json`;
- confirm a host-tool target was built;
- run the focused validators;
- inspect Editor logs for module-load errors.

### Workspace or pack save fails

- verify the destination is writable and inside the expected workspace boundary;
- use valid stable IDs and semantic versions;
- keep runtime actions disabled;
- inspect the serialization/path error.

### Source import fails

- configure an exact active profile;
- verify the source exists and is readable;
- choose the correct importer;
- inspect schema issues;
- use generic artifact intake for opaque formats.

### Catalog reload fails

- confirm the catalog belongs to the same workspace/profile/version/branch;
- inspect exact identity and relationship errors;
- check validation/governance history references;
- do not hand-edit allowed permissions without corresponding proof history.

### Permission decision fails

- validate the subject first;
- mark staleness `current` through a reviewed evidence-backed decision;
- resolve missing refs and conflicts;
- provide the validated proof event ID;
- use the same record or relationship subject for the proof;
- provide a named reviewer.

### Duplicate report is empty

- confirm the records are pack-owned economy items or recipes;
- confirm at least two distinct owner packs use the same exact case-sensitive subject reference or recipe duplicate key;
- do not expect display names or fuzzy similarity to create a group;
- confirm typed profiles and exact-subject evidence exist for candidate health details.

### Adapter matrix shows only `unsupported`

- this is the expected fail-closed state when no transient adapter declaration is registered;
- confirm the pack has a non-empty strict semantic `RequiredAdapterVersion`;
- confirm the declaration includes the active `Mono` or `IL2CPP` runtime target;
- confirm the capability is explicitly declared;
- confirm adapter identity and capability evidence use the exact documented subjects and active-profile bindings.

### Adapter matrix shows `permission_missing` or `proof_missing`

- confirm an eligible pack-owned catalog subject exists for the capability;
- confirm the mapped usage is allowed through Catalog Governance;
- confirm the subject remains validated, current, reference-complete, conflict-free, and non-superseded;
- confirm the permission event and its validation IDs target that same record;
- confirm all permission, validation, adapter identity, and capability evidence resolves to active-profile-bound sources.

## Getting help

Follow [SUPPORT.md](../../SUPPORT.md). Security issues follow [SECURITY.md](../../SECURITY.md).
