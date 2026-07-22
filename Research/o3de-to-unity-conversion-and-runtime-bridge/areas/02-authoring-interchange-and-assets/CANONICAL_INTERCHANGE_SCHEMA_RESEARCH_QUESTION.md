# Canonical Interchange Schema and Identity Research Question

Status: focused research question; no implementation authority

Repository baseline: `8fb3f0a729e4be4e513ba896ba52708a73d03eae` (`main`), observed 22 July 2026

Parent synthesis: `../../FOA_SDK_PORTING_METHODS_RESEARCH.md`

## Research question

What is the minimum versioned canonical interchange schema and identity model that preserves FOA-SDK domain semantics, stable identity, native bindings, transformations, provenance, losses, and round-trip evidence across O3DE, Blender, and Unity without granting native-host or runtime authority?

## Why this is the next question

The architecture and ownership review has selected a canonical file-backed interchange package, with shared domain documents and format-specific payloads, as the primary research method. The next unresolved dependency is the exact schema and identity contract required before Blender, O3DE, or Unity qualification can be designed precisely.

This work maps to Gate 5 research. It does not satisfy or authorize Gate 5 implementation.

## Required subquestions

### Package and versioning

- What top-level fields are mandatory in `manifest.tginterchange.json`?
- Which fields are closed, extensible, or namespaced?
- How are schema version, producer profile, consumer profile, feature set, and toolchain lock represented?
- What is the rejection and migration policy for unknown, future, deprecated, or partially supported versions?
- Which values participate in the canonical package fingerprint?

### Identity model

- What are the exact syntax, scope, and lifecycle rules for `PackageId`, `AssetId`, `RevisionFingerprint`, and `BindingId`?
- How are rename, move, duplicate, fork, merge, split, replacement, and one-to-many mappings represented?
- How are O3DE asset IDs, Unity GUIDs, Blender object IDs, source paths, and display names recorded without becoming canonical identity?
- How are deleted, unresolved, stale, conflicting, and superseded bindings represented?
- Which identity changes create a new revision, a new asset, or only a new native binding?

### Domain-document binding

- How are Road Atlas, Avalon AI, actor, troop, relationship, governance, and future domain documents bound to scene/asset payloads?
- Which domain fields remain engine-neutral?
- Which native-only component data is represented as a binding, extension, opaque payload, loss, or blocker?
- How are one domain record to many native objects and many domain records to one payload represented?

### Spatial and temporal basis

- What is the canonical forward axis?
- What vector convention, matrix storage order, multiplication order, row/column convention, and mapping direction are used?
- How are units, handedness, up axis, pivots, hierarchy, negative scale, non-uniform scale, and shear represented?
- How are time units, frame rates, sample rates, interpolation, root motion, and event timing represented?
- Which conversions are exact and which require declared tolerances?

### Payload inventory

- Which payload classes are permitted in schema 1?
- How are media type, relative path, size, SHA-256, semantic role, dependencies, and optional compression recorded?
- How are meshes, textures, materials, skeletons, animation clips, colliders, metadata, and domain documents referenced?
- How are unlisted payloads, duplicate paths, case collisions, traversal, symlink escapes, and external references rejected?
- When may an external source-native reference remain provenance-only, and when must it become a contained fingerprinted dependency?

### Transformations and losses

- What is the canonical transformation record?
- What reason-code, severity, subject, source, destination, and reversibility fields are required?
- How are unsupported, approximated, resampled, renamed, merged, split, dropped, host-native-only, and non-reproducible features represented?
- How are expected losses distinguished from unexpected validation failures?
- What makes a loss ledger complete enough to support round-trip acceptance?

### Determinism and equivalence

- Which outputs require byte identity?
- Which require canonical-document identity?
- Which permit normalized semantic equivalence?
- Which permit numeric tolerance, and how is tolerance declared?
- Which require visual or behavioral review?
- How are nondeterministic host-generated values isolated from canonical fingerprints?

### Provenance and legal state

- Which authorship, source, commit, licence, attribution, modification, redistribution, and review fields are mandatory?
- How is `NOASSERTION` represented and propagated?
- Which legal states block import, export, package publication, or redistribution?
- How are user-supplied, project-owned, synthetic, and prohibited proprietary inputs distinguished?

### Validation and security

- What pure validation rules belong in Core?
- What workspace/profile/evidence/path checks belong in Framework?
- What validation remains host-specific?
- How are malformed JSON, unknown fields, oversized values, hostile paths, duplicate identities, stale locks, fingerprint mismatches, partial packages, and inconsistent loss ledgers rejected?
- What canonical error and blocker codes are required?

### Round-trip evidence

- What evidence binds source package, host operation, result package, native binding changes, transformations, losses, and comparison outcome?
- How are `byte_identical`, `canonical_identical`, `within_tolerance`, `loss_reported`, and `not_reproducible` represented?
- What fixture evidence is required before a mapping claim becomes `fixture-verified`?

## Required sources

Use, in order:

1. current FOA-SDK normative architecture, Gate 0, interchange design, research registers, and current contracts;
2. exact pinned O3DE source and official documentation;
3. official Blender source/documentation for selected candidate formats and identity behavior;
4. official Unity documentation for asset metadata, GUIDs, importers, serialization boundaries, and packages;
5. primary format specifications for any proposed payload container;
6. community material only as clearly classified secondary observation.

Every GitHub-backed statement requires a full-commit permalink and scoped claim. Mutable branch URLs and conversation-local citation identifiers are not acceptable evidence.

## Required option comparison

Compare at least:

- one monolithic manifest;
- core manifest plus typed domain documents;
- core manifest plus namespaced extensions;
- host-native manifests linked by a minimal index;
- content-addressed payload graph versus ordered payload inventory.

Reject or defer options using identity preservation, migration, validation, determinism, security, licensing, round-trip evidence, and gate ownership—not implementation convenience alone.

## Required deliverables

1. proposed schema 1 field inventory with required, optional, and forbidden fields;
2. canonical serialization and fingerprint rules;
3. identity and native-binding state machine;
4. spatial and temporal basis decision record;
5. payload class and dependency model;
6. transformation and loss taxonomy;
7. provenance/licensing state model;
8. determinism and equivalence classes;
9. pure validation and error-code inventory;
10. schema evolution and migration rules;
11. positive and negative fixture matrix;
12. proposed `SOURCE_REGISTER.md` additions;
13. proposed `CLAIM_REGISTER.md` additions;
14. contradictions and unresolved decisions;
15. recommendation for the following focused research question.

## Acceptance criteria

The research is ready for maintainer review only when:

- all field proposals have an owner and gate;
- identity does not depend on paths, display names, Unity GUIDs, Blender names, or O3DE-native IDs alone;
- native bindings can be unresolved, stale, superseded, and one-to-many without identity collapse;
- coordinate and matrix conventions are explicit rather than inferred from engine names;
- losses are typed, ordered, attributable, and complete;
- canonical fingerprints exclude or normalize host-generated nondeterminism explicitly;
- schema evolution fails closed for unsupported future versions;
- `NOASSERTION` and prohibited-content states block unsafe progression;
- every fixture is synthetic or clearly redistributable;
- no implementation code, provider execution, Unity mutation, runtime build, deployment, or evidence promotion is introduced.

## Out of scope

- process-supervisor implementation;
- Blender or Unity execution;
- O3DE Asset Processor publication;
- proprietary FoA project access;
- target-game content loading;
- runtime adapter build or packaging;
- deployment, game launch, mutation, save access, signing, or publication.

## Permanent non-authority statement

This focused research question and its later answer do not authorize a schema implementation or any operational capability.
