# FoA Adapter Staging and Deployment Preview Contract

## Status

Implemented as Correction Slice 13, the third bounded Phase 8 preview. It compares one exact ready package layout with one accepted declared target inventory and derives a deterministic deployment and rollback description.

`StagingMutationAllowed`, `DeploymentMutationAllowed`, `RollbackExecutionAllowed`, and `LaunchAllowed` are always `false`.

## Purpose

Slice 11 defines a reproducible build manifest. Slice 12 derives a ready package layout from a reviewed manifest and project-owned staging inventory. Slice 13 answers the next question without touching a filesystem:

> What additions, replacements, removals, unchanged paths, conflicts, backups, and inverse rollback steps would be required to reconcile the reviewed package layout with one exact declared target inventory?

A `ready` preview describes metadata only. It does not prove that the target files exist, that a backup has been written, or that a deployment has occurred.

## Exact package binding

Every request contains one `AdapterPackageAssemblyPreview` with:

- `ready` status;
- non-empty canonical JSON;
- exact package-preview ID;
- lowercase SHA-256 package-preview fingerprint supplied by the reviewed caller;
- exact pack and package root;
- a non-empty deterministic package layout;
- valid output digests for every layout entry;
- `AssemblyAllowed`, `ArchiveAllowed`, and `DeploymentAllowed` all false.

Any drift or non-ready package produces `package_not_ready` before target review or inventory checks.

## Declared target inventory

The transient target inventory binds to:

- stable inventory ID and format version;
- lowercase SHA-256 inventory fingerprint;
- exact package-preview ID and fingerprint;
- exact pack ID;
- exact target root equal to the reviewed package root;
- a separate safe backup root.

Each target entry declares:

- stable entry ID;
- exact safe relative target path;
- role and media type;
- current lowercase SHA-256 fingerprint;
- byte size;
- owner pack ID;
- project-owned state;
- managed state;
- replaceable state;
- removable state.

The service does not discover, open, hash, stat, copy, or delete a target file. It validates caller-supplied metadata only.

## Target review

The inventory requires an accepted typed review bound to its exact ID and fingerprint. The review records:

- stable review ID;
- exact inventory ID and fingerprint;
- `unknown`, `accepted`, or `rejected` decision;
- named reviewer;
- one or more evidence IDs;
- notes.

A missing, rejected, mismatched, anonymous, or evidence-free review produces `target_unreviewed`.

## Derived change classes

Every reviewed package path receives one deterministic classification:

- `add` — no target entry exists at the exact package path;
- `replace` — one managed project-owned entry for the same pack exists, its role/media type match, its fingerprint differs, and it is replaceable;
- `unchanged` — the target fingerprint already equals the reviewed output digest;
- `conflict` — multiplicity, ownership, management, role, media type, or replaceability prevents a safe exact decision.

A managed project-owned target entry for the same pack that is absent from the package layout becomes `remove` only when it is explicitly removable.

The preview never selects a winner among duplicate target entries, compares timestamps, guesses ownership, or treats names and matching byte sizes as equivalent.

## Additions, replacements, removals, and unchanged paths

Each derived change preserves:

- stable change ID and typed change kind;
- exact package and target entry IDs where applicable;
- exact package and target paths;
- role and media type;
- previous and desired fingerprints;
- previous and desired byte sizes;
- backup requirement;
- deterministic reason.

Collections are sorted by target path and change ID before canonical serialization.

## Conflicts

Conflicts are explicit and fail closed. They include:

- multiple target entries occupying one exact package path;
- a target owned by another pack;
- a non-project-owned or unmanaged target;
- role or media-type drift;
- a changed target that is not replaceable;
- an obsolete managed target that is not removable;
- an unmanaged or differently owned entry beneath the reviewed package root that is absent from the new layout.

Conflict rows preserve every involved target entry ID. The service does not choose or mutate a winner.

## Backup requirements

Every replacement and removal requires a deterministic backup description containing:

- backup ID;
- target entry ID and target path;
- backup path beneath the separate backup root;
- exact pre-change fingerprint;
- byte size;
- reason.

A missing current fingerprint or unsafe backup path produces `backup_incomplete`. The contract does not create the backup.

## Rollback steps

Every addition, replacement, and removal requires exactly one inverse step:

- `remove_added` — remove a hypothetical new entry;
- `restore_replaced` — restore exact pre-replacement bytes from the declared backup;
- `restore_removed` — restore exact removed bytes from the declared backup.

Rollback steps use deterministic one-based sequence numbers and preserve target paths, backup paths, expected deployed fingerprints, restore fingerprints, and reasons.

The rollback order is the inverse of the described deployment categories: additions are removed first, then replacements are restored, then removals are restored. Missing inverse steps produce `rollback_incomplete`.

## Status precedence

Every preview receives exactly one status:

1. `package_not_ready`;
2. `target_unreviewed`;
3. `inventory_binding_mismatch`;
4. `inventory_untrusted`;
5. `path_invalid`;
6. `conflict`;
7. `backup_incomplete`;
8. `rollback_incomplete`;
9. `ready`.

Earlier identity and trust failures cannot be hidden by a later layout or rollback detail.

## Canonical representation

Equivalent reviewed inputs produce byte-identical canonical JSON with:

- fixed property order;
- sorted additions, replacements, removals, unchanged rows, conflicts, backups, rollback steps, and blockers;
- JSON escaping;
- locale-independent integer formatting;
- exact package and target fingerprints;
- all mutation and launch permissions false.

The preview is transient and has no durable document schema.

## Read-only Editor pane

**Tainted Grail Staging and Deployment Preview** displays:

- preview, package-preview, target-inventory, and pack identities;
- status, target root, and backup root;
- additions;
- replacements;
- removals and unchanged paths;
- conflicts;
- backup requirements;
- rollback steps;
- blockers and canonical JSON.

The pane is non-editable. The normal Developer Preview state contains zero registered staging/deployment-preview inputs.

## No copying, deletion, deployment, launch, or execution

Slice 13 adds no filesystem scan, hashing, copy, move, replace, delete, backup, restore, archive, staging mutation, deployment-directory mutation, FoA launch, BepInEx/Harmony loading, telemetry, save mutation, or adapter execution.

The transient registry accepts explicit metadata requests only. The pane contains no registration, import, save, export, copy, delete, backup, rollback, deploy, launch, or execute control.

## Tests and enforcement

Production-linked tests cover:

- strict review, status, change, and rollback vocabularies;
- duplicate request rejection;
- ready add/replace/remove/unchanged derivation;
- status precedence;
- exact review and inventory binding;
- foreign ownership and duplicate-target conflicts;
- backup and rollback incompleteness;
- target and backup path containment;
- exact inverse rollback sequencing;
- canonical determinism;
- complete input non-mutation.

The focused validator rejects filesystem or process mutation, missing backup/rollback gates, mutable Editor controls, incomplete tests, and missing CI, roadmap, release, architecture, user, or fourteen-pane Windows acceptance contracts.

## Next ordered slice

Correction Slice 14 is a typed explicit-confirmation and deployment work-order contract. It should bind one exact ready staging/deployment preview to a named reviewer, expiry, confirmation scope, maintenance window, preflight evidence, and operator-facing action checklist while keeping every execution flag false and adding no copy, delete, backup, restore, deploy, launch, or adapter call.

## Rollback

Revert the implementing pull request. Target inventories, reviews, previews, backups, and rollback descriptions are transient metadata, so no workspace, pack, catalog, source/evidence, adapter, work-order, runtime-result, build-manifest, staging, package, deployment, or rollback schema requires migration.
