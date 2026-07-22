# Canonical Interchange Schema and Identity Deep Research — Preserved Intake

Status: preserved research input; not an accepted decision

Source title: `Canonical Interchange Schema and Identity Research`

Source observation: ChatGPT Deep Research artifact created 22 July 2026

Original proposed repository path:

```text
Research/o3de-to-unity-conversion-and-runtime-bridge/
areas/02-authoring-interchange-and-assets/
CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md
```

## Preservation note

This file preserves the substantive Deep Research result as a normalized repository intake record. The rendered
Deep Research artifact remains the source-of-record presentation. This intake copy removes conversation-local
citation tokens and preserves the report's proposals, option comparison, field groups, state models, source
proposals, contradictions, and recommended next question for repository reconciliation.

Nothing in this preserved input is implementation authority. Statements become accepted only when reconciled
against the current repository, primary sources, source register, claim register, and controlling gate documents.

## Research question addressed

What is the minimum versioned canonical interchange schema and identity model that preserves FOA-SDK domain
semantics, stable identity, native bindings, transformations, provenance, losses, and round-trip evidence across
O3DE, Blender, and Unity without granting native-host or runtime authority?

## Executive finding from the report

The report recommended one authoritative `manifest.tginterchange.json` that indexes contained payloads, typed
engine-neutral domain documents, exact native binding ledgers, ordered transformations, ordered losses,
provenance/licensing state, and evidence or fingerprint observations.

Its selected shape was:

```text
one canonical manifest
+ typed engine-neutral domain documents
+ contained format-specific payloads
+ exact native bindings
+ bounded namespaced extensions
```

The report rejected a host-native authority model because Unity GUIDs, Blender names, O3DE labels, native paths,
and importer output must remain observations or bindings rather than canonical identity. It also rejected a
content-addressed graph as the first Schema-1 authority because that model was judged too complex for the minimum
Gate 5 contract.

## Baseline asserted by the report

The report observed repository `main` at:

```text
195f5c15f0c59d47bd54e661b37d7457af9d1d95
```

It treated the O3DE lock as:

```text
repository: https://github.com/o3de/o3de.git
commit: 68683f23fb747380d3efa2424bd5f30242e9c5a2
engine version: 2.7.0
```

The report itself warned that its repository observations needed later reconciliation before implementation.

## Architecture option comparison preserved from the report

| Option | Report finding | Intake disposition |
| --- | --- | --- |
| Monolithic manifest | Simple authority but too rigid and too large for domain growth | rejected |
| Core manifest plus typed domain documents | Best preservation of engine-neutral domain semantics | selected |
| Core manifest plus namespaced extensions | Useful when namespace admission fails closed | selected with constraints |
| Host-native indexes with a minimal canonical index | Native formats would become de facto authority | rejected |
| Content-addressed payload graph | Strong integrity but excessive initial complexity | deferred |
| Ordered payload inventory | Easier review and deterministic validation for Schema 1 | selected |

## Proposed package boundary preserved from the report

The report proposed a contained directory or deterministic archive whose authority rests on exactly one file:

```text
manifest.tginterchange.json
```

It proposed externally calculated package fingerprints rather than a self-referential fingerprint field in the
manifest.

The proposed top-level groups were:

- `schema_version` and `schema_profile`;
- `package_kind`, `package_id`, and `pack_id`;
- human-readable package metadata;
- exact producer and intended-consumer identity;
- exact toolchain lock;
- typed domain documents;
- logical asset records;
- canonical-to-native bindings;
- contained payload inventory;
- ordered transformations;
- ordered losses;
- provenance and licensing records;
- validation evidence references;
- reviewed namespaced extensions.

The report proposed forbidding absolute paths, game-install paths, proprietary Unity-project paths, executable or
script-entry fields, runtime-target selection, deployment targets, save-mutation controls, and a self-referential
package fingerprint.

## Proposed identity model preserved from the report

The report proposed four primary identities:

- `PackageId` — one interchange-package lineage;
- `AssetId` — one project-owned logical asset;
- `RevisionFingerprint` — one normalized semantic revision;
- `BindingId` — one canonical-to-native mapping.

It proposed these lifecycle rules:

- rename and move preserve `AssetId`;
- semantic changes create a new `RevisionFingerprint`;
- duplicate or fork requires an explicit lineage decision and usually a new `AssetId`;
- merge and split require typed mappings, transformations, and loss records;
- a changed Unity GUID, O3DE source identity, Blender object locator, or native path changes the binding rather
  than silently changing canonical identity;
- bindings can be unresolved, active, stale, conflicted, retired, or superseded.

The report used parent asset references to represent lineage and recommended preserving supersession history.

## Domain-document binding preserved from the report

The report kept Road Atlas, Avalon AI, actor, troop, relationship, governance, and future domain records as
separate engine-neutral documents. It proposed binding those documents to native scene and asset payloads through
canonical document IDs, asset IDs, and binding records.

Native-only data was to be classified as one of:

- an exact native binding;
- a reviewed namespaced extension;
- a contained opaque payload;
- a typed transformation;
- a typed loss;
- a blocker.

## Spatial and temporal proposal preserved from the report

The report selected the O3DE authoring physical basis in part:

```text
right-handed
Z-up
metres
seconds
```

It did not select an exact forward axis, vector convention, matrix storage order, or multiplication order. It
required every matrix to state its mapping direction and required numeric tolerances to be explicit.

The report recommended a later focused spatial-basis question before fixture-ready host qualification.

## Payload and path model preserved from the report

The report proposed payload records containing:

- stable payload ID;
- contained relative path;
- semantic role;
- media type;
- exact byte size;
- lowercase SHA-256 observation;
- optional compression declaration;
- declared payload dependencies.

It proposed rejecting:

- absolute paths;
- traversal and dot segments;
- symlink escapes;
- Windows path aliases and reserved-name conflicts;
- case-colliding paths;
- duplicate paths;
- undeclared payloads;
- missing payloads;
- digest mismatches;
- undeclared external dependencies.

Source-native references could remain provenance-only, but could not satisfy a material payload dependency unless
contained and fingerprinted.

## Transformation and loss taxonomy preserved from the report

Each transformation was proposed to record an explicit sequence, operation code, subject, parameters, source,
destination, and evidence references.

The proposed minimum loss classes were:

- `unsupported-feature`;
- `approximation`;
- `resampled`;
- `rebaked`;
- `renamed-native`;
- `merged`;
- `split`;
- `dropped`;
- `opaque-native-reference`;
- `non-reproducible`.

Losses were to preserve severity, source feature, result representation, reversibility, tolerance, and evidence.
Successful import or export was explicitly not enough to claim losslessness.

## Determinism classes preserved from the report

The report distinguished:

- `byte_identical`;
- `canonical_document_identical`;
- `payload_set_identical`;
- `semantic_equivalent_within_tolerance`;
- `reviewed_visual_or_behavioural_match`;
- `non_reproducible_but_loss_accounted`.

It proposed separating semantic revision fingerprints from qualification evidence and host-generated values.

## Provenance and licensing proposal preserved from the report

The report proposed per-subject provenance and legal state covering:

- project-owned, synthetic, user-supplied, local-reference, and upstream-reference sources;
- authorship and source locators;
- source fingerprints;
- SPDX expression or `NOASSERTION`;
- attribution and notice requirements;
- modification state;
- redistribution state;
- reviewer and evidence bindings.

It stated that `NOASSERTION` must not be treated as redistribution permission and should block publication-sensitive
progression.

## Validation ownership preserved from the report

The report divided validation as follows:

- Core — schema shape, identity, intrinsic references, canonical form, paths, fingerprints, transformations,
  losses, provenance, licensing, and deterministic state;
- Framework — workspace/profile context, evidence binding, staging, candidate-before-publish behavior, and
  qualification records;
- host-specific providers — native identifier syntax, importer/exporter observations, native lock behavior, and
  host-specific compatibility evidence.

It proposed a synthetic positive and negative fixture matrix for canonical documents, identity transitions,
paths, payloads, transforms, losses, legal blockers, and cross-host round trips.

## Migration proposal preserved from the report

The report proposed failing closed for unsupported future versions and preserving source packages during explicit
migration. Its per-field tables marked changes as additive, explicit, breaking, or revision-producing, but it did
not provide a complete package-level migration state machine.

## Proposed register material preserved from the report

The report proposed new repository sources, O3DE documentation, Unity metadata/package-lock documentation, and
Blender coordinate-system documentation. Its proposed repository source IDs began at `SRC-REPO-019`, and its
proposed Unity IDs reused `SRC-UNITY-010`; those IDs collided with entries already present by the time of
reconciliation.

The proposed claims included:

- one authoritative manifest with typed documents and payloads;
- native identifiers are bindings, not canonical identity;
- package self-fingerprints should be forbidden;
- the physical basis is right-handed Z-up metres seconds;
- the exact forward axis and matrix convention remained unresolved;
- `NOASSERTION` is not redistribution permission;
- Unity GUID retention belongs in the binding and lock model;
- host-native index architectures are incompatible with the exact-identity invariant;
- Schema 1 grants no execution or runtime authority.

## Contradictions and unresolved points preserved from the report

The report identified:

- a stale repository baseline;
- source-register and claim-register drift;
- tension between “deterministic FBX-plus-sidecar” wording and the broader manifest-and-payload design;
- unresolved forward-axis and matrix conventions;
- unselected exact Blender and Unity locks;
- no fixture evidence for cross-host compatibility.

## Recommended next question preserved from the report

The report recommended researching the exact spatial basis, matrix convention, numeric-tolerance model, and
fixture lock required to classify O3DE, Blender, and Unity Editor test-project round trips.

## Permanent non-authority statement

This preserved intake does not authorize a schema implementation, migration code, provider, process launch,
Blender or Unity execution, O3DE source-root mutation, runtime adapter, game inspection, deployment, save access,
evidence promotion, signing, or publication.
