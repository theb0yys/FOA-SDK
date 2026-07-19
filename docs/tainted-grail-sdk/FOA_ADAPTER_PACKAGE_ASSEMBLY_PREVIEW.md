# FoA Adapter Package Assembly Preview Contract

## Status

Implemented as Correction Slice 12, the second Phase 8 contract slice. It compares one reviewed build manifest with one project-owned staging inventory and derives a deterministic package preview. It performs no package mutation.

## Purpose

Slice 11 defines the source, toolchain, dependency, material, and expected-output contract for an adapter build. Slice 12 answers the next bounded question:

> Given a reviewed build manifest and a project-owned staging inventory, what exact package layout would be assembled, which output digests would be carried forward, and which omissions, collisions, trust failures, or redistribution blockers prevent the preview from being ready?

The answer is transient, read-only, and fail-closed. It is not an archive, deployment, release artefact, or proof that a build ran.

## Reviewed manifest binding

Every preview request contains the exact `AdapterBuildManifest` plus a typed review:

- stable review ID;
- exact manifest ID;
- lowercase `sha256:<64 hex>` manifest fingerprint;
- `unknown`, `accepted`, or `rejected` decision;
- named reviewer;
- one or more review evidence IDs;
- optional notes.

Only `accepted` reviews can reach a ready preview. The manifest itself must already be `ready`, contain canonical JSON and expected outputs, and retain `BuildAllowed: false`.

The manifest fingerprint is externally supplied review metadata. Slice 12 validates its shape and exact agreement across the review and inventory; it does not open or hash a manifest file.

## Project-owned staging inventory

`AdapterStagingInventory` is transient metadata with:

- format version;
- stable inventory ID;
- exact manifest ID and fingerprint;
- exact pack ID;
- exact package root;
- staged inventory entries.

Every entry records:

- stable entry ID;
- safe relative staging path;
- intended package path;
- manifest role and media type;
- lowercase SHA-256 output fingerprint;
- byte size;
- explicit project-owned state;
- explicit redistribution state;
- package-inclusion state.

The service does not scan a staging directory or inspect file bytes. The inventory must be supplied by a separately reviewed producer. An included entry that is not project-owned fails closed.

## Derived package layout

For every expected build-manifest output, the service requires exactly one included staging entry at the same exact package path.

A layout entry preserves:

- staging inventory entry ID;
- staging path;
- package path;
- role;
- media type;
- output digest;
- byte size;
- project-owned state;
- redistribution state.

The output digest is the exact SHA-256 fingerprint declared by the staging inventory. Slice 12 does not recompute or reinterpret it.

Layout rows are sorted by package path and staging path. Canonical JSON uses fixed property order, JSON escaping, locale-independent integer formatting, and deterministic ordering for layout rows, omissions, collisions, and blockers.

## Omissions

An omission is emitted when an expected manifest output has no included staging entry. It records:

- expected package path;
- expected role;
- reason.

An entry excluded by policy is treated as absent for package assembly. The preview does not silently drop required output.

## Collisions

A collision is emitted when more than one included staging entry targets the same expected package path. It records the exact package path and sorted inventory entry IDs.

The service does not choose a winner, compare timestamps, or infer that equal names or digests make entries interchangeable.

## Trust and redistribution blockers

The preview fails closed for:

- incomplete or duplicate inventory entry identities;
- included outputs not declared by the reviewed manifest;
- role or media-type drift;
- non-project-owned included entries;
- absent or malformed output digests;
- unsafe staging paths;
- package paths outside the reviewed package root;
- non-redistributable outputs;
- manifest, review, inventory, pack, fingerprint, or package-root binding drift.

Non-redistributable content may not enter the derived package layout. A package preview cannot weaken the redistribution decision already declared by the build manifest.

## Status precedence

Every preview receives exactly one deterministic status:

1. `manifest_not_ready`;
2. `manifest_unreviewed`;
3. `inventory_binding_mismatch`;
4. `inventory_untrusted`;
5. `output_missing`;
6. `fingerprint_missing`;
7. `path_invalid`;
8. `collision`;
9. `redistribution_blocked`;
10. `ready`.

Earlier structural failures cannot be hidden by later layout details.

## Prohibited operations

Every preview and every status retains:

```text
AssemblyAllowed: false
ArchiveAllowed: false
DeploymentAllowed: false
```

A `ready` preview means only that the compared metadata yields a complete, deterministic, redistributable layout.

**No file copying, archive creation, deployment, launch, or execution** is implemented or authorised. Slice 12 also adds no filesystem scan, package writer, checksum calculator, dependency download, staging mutation, release upload, FoA process access, BepInEx/Harmony loading, telemetry, or save mutation.

## Transient registry and Editor view

`AdapterPackageAssemblyPreviewRegistry` stores complete preview requests in transient Core memory. It has no loader, file picker, persistence service, or registration UI and is cleared on Editor shutdown.

**Tainted Grail Package Assembly Preview** is read-only. It displays:

- preview, manifest, inventory, pack, adapter, and plan identities;
- status and package root;
- derived package layout and output digests;
- omissions;
- collisions;
- redistribution and trust blockers;
- canonical JSON.

The normal Developer Preview state contains zero registered package-preview inputs.

## Testing and enforcement

Production-linked tests cover typed vocabularies, duplicate inventory rejection, ready previews, status precedence, review and binding drift, omissions, missing digests, unsafe paths, collisions, project ownership, redistribution, deterministic canonical output, and complete input non-mutation.

The focused validator rejects filesystem and process access, copy/archive/deploy code, automatic registration, mutable Editor controls, missing ownership/redistribution/path gates, incomplete tests, and missing public contracts.

## Next ordered slice

The next Phase 8 slice is a deterministic staging and deployment preview. It should compare one ready package layout with a declared target inventory and derive additions, replacements, removals, conflicts, backup requirements, and rollback steps without copying or deleting files, mutating deployment directories, launching FoA, or executing an adapter.

## Rollback

Revert the implementing pull request. Preview requests and derived layouts are transient, and no durable workspace, pack, catalog, source/evidence, adapter, work-order, runtime-result, build-manifest, or package schema requires migration.
