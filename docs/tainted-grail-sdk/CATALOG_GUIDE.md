# Canonical Catalog Browser and Record Inspector

## Purpose

The canonical catalog is the shared, durable query surface for evidence-backed FoA knowledge and pack-owned custom records.

It is not a cache of display names and it is not a runtime permission list. It preserves exact identity, evidence, relationships, validation history, blockers, and usage constraints.

Open the tool from:

**Tools → Tainted Grail SDK → Tainted Grail Catalog Browser**

## Prerequisites

Before using the catalog:

1. configure and save a workspace;
2. configure an exact active FoA game profile;
3. create or open a pack when promoting synthetic/custom records;
4. import source artifacts;
5. extract or enter evidence records.

The catalog document is bound to:

- workspace ID;
- active profile ID;
- exact game version;
- branch.

A document bound to another workspace or game profile is rejected.

## Durable location

The catalog is stored at:

```text
Catalog/catalog.tgcatalog.json
```

The file contains:

- canonical records;
- first-class relationships;
- validation history;
- document-level workspace and game-profile binding.

The in-memory catalog is published only after a successful save.

## Search

The browser supports free-text and exact filters.

### Free-text search

Free text searches:

- record ID;
- owner pack ID;
- domain;
- record kind;
- subject reference;
- exact native reference or GUID;
- display name;
- aliases;
- source-scoped references;
- tags.

Text search helps discovery. It does not determine identity or merge records.

### Exact filters

Available filters include:

- record ID;
- exact native reference or GUID;
- subject reference;
- evidence ID;
- owner pack;
- domain;
- record kind;
- identity kind;
- confidence;
- validation state;
- permission or prohibition;
- blocked-only state;
- superseded-record inclusion.

Results are ordered by stable record ID.

### Display-name duplicates

Two records may share a display name. They remain distinct when their stable record IDs or exact identities differ.

Never assume that two rows represent the same subject because their labels look alike.

## Record inspector

Selecting a row shows:

- record ID, identity kind, domain, and kind;
- owner pack for synthetic records;
- subject reference;
- exact native reference or GUID;
- maturity/research stage;
- confidence;
- operational risk;
- validation state;
- supersession state;
- aliases, source-scoped refs, and tags;
- allowed usages and prohibitions;
- missing references and conflicts;
- linked evidence details;
- typed relationships;
- validation history;
- record-specific blockers.

## Identity kinds

### Native

Represents a game-owned subject.

Requirements:

- exact native reference or GUID;
- no custom pack ownership;
- linked evidence.

### Synthetic

Represents project-created or custom content.

Requirements:

- existing owner pack;
- no borrowed native reference;
- linked evidence.

### Composite

Represents a reviewed subject assembled from multiple identities or components. The composition must be explicit and evidence-backed.

### Source-scoped

Represents a subject that is currently meaningful only inside a source or evidence context. It must not be treated as a globally reconciled native identity.

## Promoting evidence into a reviewed record

The promotion form creates a new canonical record from one existing evidence record.

Required inputs:

- evidence ID;
- new stable record ID;
- domain;
- record kind;
- subject reference;
- identity kind.

The subject reference must exactly match the selected evidence record.

Additional inputs include:

- owner pack for synthetic records;
- exact native reference for native records;
- display name;
- aliases;
- research stage;
- confidence;
- operational risk;
- prohibitions;
- tags.

### What promotion does

Promotion:

- verifies the evidence exists;
- verifies evidence matches the active profile/version/branch;
- verifies identity-kind requirements;
- verifies the selected owner pack exists for synthetic records;
- rejects duplicate record IDs;
- rejects duplicate exact native references;
- creates a `reviewed` or `reconciled` record;
- sets validation state to `unvalidated`;
- links the selected evidence;
- adds `no_unvalidated_runtime_use` when absent;
- saves the catalog transactionally.

### What promotion does not do

Promotion does not:

- grant an allowed usage;
- validate spawning, mutation, save use, or deployment;
- merge another record with the same display name;
- create relationships automatically;
- reconcile conflicting evidence automatically;
- author gameplay content;
- call FoA runtime APIs.

## Relationships

Relationships are first-class records with:

- stable relationship ID;
- source record ID;
- target record ID or unresolved target subject reference;
- relationship kind;
- evidence IDs;
- validation state;
- attributes.

Canonical relationships require evidence. Relationships to a known target use the target record ID. A target subject reference may be used while reconciliation remains incomplete.

Examples include:

- contains;
- located-in;
- upgrades-to;
- member-of;
- ingredient-of;
- output-of;
- route-step;
- controlled-by;
- authority-over;
- spawns-at;
- depends-on;
- supersedes.

Relationship authoring UI is expanded with later domain tools; the current inspector displays relationships stored in the catalog document.

## Validation history

A validation event records:

- stable validation ID;
- record ID;
- state;
- method;
- validator;
- check time;
- profile ID and game version;
- evidence IDs;
- notes.

Validation events are append-only history. Changing a record's current validation state should not erase how earlier results were obtained.

A validation event bound to another active profile/version produces a blocker.

## Blockers

Catalog blockers include:

- missing exact native reference;
- missing or unknown pack owner;
- no evidence;
- evidence missing from the active registry;
- unvalidated record;
- allowed usage declared before validated state;
- unresolved conflicts;
- unresolved missing references;
- missing supersession target;
- relationship evidence missing;
- unvalidated relationship;
- validation history bound to another profile/version;
- validation history referencing missing evidence.

A warning or blocker must be resolved through evidence, identity, validation, or correction—not deleted solely to make the catalog appear complete.

## Permissions and prohibitions

The catalog stores allowed usages and forbidden usages separately.

Promotion cannot create allowed usages. Usage permission belongs to the later validation and permission workflow.

Common prohibitions include:

- `no_spawn`;
- `no_mutate`;
- `no_crime_use`;
- `no_unique_reuse`;
- `no_story_use`;
- `no_save_write`;
- `no_density_stack`;
- `no_unvalidated_runtime_use`.

A prohibition remains effective even when the record is visible or searchable.

## Supersession

A superseded record remains durable for auditability. Normal search hides superseded records unless **Include superseded** is enabled.

The superseding record ID must exist. Supersession does not silently delete evidence, validation history, or relationships.

## Save and reload

Use **Save Catalog** to persist the current canonical document.

Use **Reload Catalog** to reload and validate the workspace document. Reload rejects:

- unsupported schema versions;
- mismatched workspace ID;
- mismatched active profile ID;
- mismatched game version or branch;
- duplicate record IDs;
- duplicate exact native refs;
- invalid synthetic ownership;
- records or relationships without required evidence;
- relationships with missing record targets;
- validation events for missing records.

## Safe workflow

1. Import a source artifact.
2. Review its limitations and import issues.
3. Inspect the extracted evidence.
4. Reconcile the subject reference and exact identity.
5. Create a reviewed record through promotion.
6. Resolve missing refs and conflicts.
7. Add and review relationships.
8. Perform usage-specific validation.
9. Record validation history.
10. Grant only the permissions proven by the validation.
11. Produce a handoff for a separate adapter when authorised.

## Troubleshooting

### Promotion says the evidence is outside the active scope

Open the workspace/profile that matches the evidence's profile ID, game version, and branch. Do not rebind old evidence by editing its JSON manually.

### Native promotion requires an exact reference

Supply the exact game-facing reference or GUID supported by the evidence. A display name or guessed identifier is insufficient.

### Synthetic promotion requires a pack

Create or open the owning pack and select its stable pack ID. Custom records cannot borrow native IDs.

### Duplicate exact native ref

Inspect the existing record. Decide whether the new evidence should be linked through a reviewed update, conflict, relationship, or supersession process. Do not create a second canonical owner for the same exact native reference.

### Catalog file is rejected after switching profiles

The catalog is deliberately scoped to an exact profile/version/branch. Switch back to the matching profile or create a separate compatible catalog after reviewed migration/reconciliation.

### Record is visible but blocked for runtime use

Visibility is not permission. Complete the required validation and permission process; do not remove `no_unvalidated_runtime_use` without proof.
