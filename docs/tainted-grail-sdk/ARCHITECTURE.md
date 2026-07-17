# Architecture

## System purpose

The Tainted Grail Modding Editor and SDK is a governed authoring system. It separates what is known, how it is known, what is safe to do, and which component is allowed to perform an action.

O3DE provides the editor host. FoA remains a separate Unity runtime. The project does not convert FoA into an O3DE game project.

## Core invariants

These are mandatory architectural rules:

1. **Editor/runtime separation** — editor and knowledge services never execute FoA gameplay mutations.
2. **Exact identity** — native references, GUIDs, paths, and source locators are preserved exactly.
3. **Pack ownership** — synthetic and custom records are owned by a stable pack ID.
4. **Display names are not identities** — records are never merged because labels look alike.
5. **Evidence before promotion** — imported evidence remains distinct from claims and reviewed records.
6. **Validation is usage-specific** — safe display does not imply safe spawn, mutation, deployment, or save use.
7. **Missing proof fails closed** — absent or stale evidence produces blockers, not optimistic permission.
8. **Persistence is versioned** — durable documents carry schema versions and require migration for breaking changes.
9. **Runtime actions belong to adapters** — native game calls, patches, persistence, cleanup, and rollback are adapter responsibilities.
10. **Public inputs are untrusted** — imports, manifests, paths, and documents require validation and bounded processing.

## Layer model

### 1. O3DE host layer

Provides:

- editor lifecycle;
- dockable Qt tools;
- reflection and serialization;
- build and module loading;
- automation and asset infrastructure;
- platform abstractions.

The host layer does not define FoA semantics.

### 2. TG SDK editor foundation

Implemented in `Gems/TaintedGrailModdingSDK`.

Owns:

- workspace and game profiles;
- pack manifests;
- source/evidence intake;
- catalog and relationship services;
- validation, blockers, maturity, risk, and permission models;
- editor views and commands;
- persistence and migration services;
- build, package, report, and handoff generation.

### 3. Avalon Core knowledge layer

Conceptual and downstream integration boundary for:

- evidence trust and provenance;
- canonical knowledge contracts;
- read-only queries;
- permissions and prohibitions;
- planning and handoff records.

It remains independent of FoA runtime libraries and mutation code.

### 4. Diagnostic and extraction tools

Produce bounded, read-only observations such as:

- template inventories;
- item and recipe dumps;
- type/member maps;
- scene observations;
- runtime logs;
- schema and object inventories.

Their output becomes source artifacts. A diagnostic result is not automatically a reviewed catalog fact.

### 5. FoA runtime adapters

Separate components that may, after explicit validation and permission:

- call BepInEx, Harmony, Unity, or FoA APIs;
- grant or register content;
- spawn actors or encounters;
- bind routes or assets;
- mutate vendors, loot, rewards, quests, or state;
- persist data and perform migrations;
- clean up and roll back.

Adapters consume reviewed work orders and return runtime proof as new evidence.

## Data flow

```text
raw artifact
  ↓
source document
  ↓
evidence records and import issues
  ↓
claim review
  ↓
canonical catalog records and relationships
  ↓
validation, risk, permission, and prohibitions
  ↓
reviewed work order / handoff
  ↓
separate runtime adapter
  ↓
runtime observation and proof
  ↺ new source and evidence
```

No arrow may be skipped merely because a value appears plausible.

## Foundation service

`FoundationService` coordinates the current editor state.

Responsibilities:

- active workspace and persisted path;
- active pack and persisted path;
- source/evidence registry;
- catalog database;
- import issues;
- validation and blocker snapshot;
- change notifications.

It should remain an orchestration boundary. Parsing, persistence, validation, and domain logic belong in dedicated services.

## Workspace model

A workspace defines the authoring boundary and exact game context.

It owns:

- workspace ID and roots;
- output, staging, and deployment paths;
- one or more game profiles;
- active profile selection;
- pack and source document locations;
- future catalog, content, build, and report locations.

A game profile records the exact FoA build context that produced evidence or receives later output.

## Pack model

A pack is the ownership and release unit.

It defines:

- stable namespaced identity;
- owner;
- version;
- game/Core/adapter compatibility;
- dependencies and conflicts;
- save impact;
- content, asset, and localisation declarations;
- build and release intent;
- runtime-action flag fixed to disabled in editor-owned documents.

Every synthetic catalog record must reference a pack owner.

## Source and evidence model

### Source

A source is an immutable description of an imported artifact:

- source ID;
- kind and locator;
- SHA-256 fingerprint;
- exact profile/build binding;
- tool and importer metadata;
- capture and import times;
- limitations and media type;
- byte size and import status.

### Evidence

Evidence is an attributed observation or statement extracted from one source:

- evidence ID;
- source and fingerprint binding;
- exact profile/build binding;
- subject reference;
- claim text;
- kind, confidence, locator, and record path;
- extraction time.

Evidence is not a canonical record and does not grant permission.

### Import issue

An import issue preserves parsing, schema, identity, compatibility, size, and persistence problems. Issues feed the blocker engine and survive workspace reload.

## Catalog model

The catalog is the canonical query surface for game knowledge and project-owned records.

A catalog record uses:

- stable record ID;
- domain and kind;
- subject reference;
- exact native reference when applicable;
- identity kind;
- display name and aliases;
- owning pack for synthetic content;
- source and evidence links;
- relationships;
- maturity, confidence, risk, validation, permissions, and prohibitions;
- versions, staleness, supersession, conflicts, and missing references.

Relationships are first-class records or typed attributed edges, not unstructured strings when they require independent evidence or validation.

## Validation and permission

Separate dimensions include:

- research stage;
- confidence;
- operational risk;
- validation state and history;
- allowed usages;
- forbidden usages;
- missing references;
- conflicts;
- staleness and supersession.

Examples:

- display-only;
- planning-safe;
- route candidate;
- spawn candidate;
- economy candidate;
- quest read;
- validated runtime use;
- no spawn;
- no mutate;
- no story use;
- no save write.

Permission is tied to a use, version, evidence set, and validation result.

## Persistence architecture

Persistence services own all durable document reads and writes.

Rules:

- documents include schema versions;
- writes stay inside approved workspace roots;
- multi-document operations are transactional where practical;
- load errors are preserved and surfaced;
- breaking schema changes require migration support or explicit rejection;
- UI classes do not write files directly;
- runtime deployment is never performed by document persistence services.

Current suffixes:

- `*.tgworkspace.json`;
- `*.tgpack.json`;
- `source.tgsource.json`;
- `evidence.tgevidence.json`.

Catalog document suffixes are introduced with the catalog milestone.

## UI architecture

Qt widgets should be thin views over services.

A widget may:

- collect user input;
- call a service command;
- display state, errors, and blockers;
- respond to foundation notifications.

A widget should not:

- implement domain identity rules;
- parse complex source formats;
- own canonical state;
- perform unmanaged file writes;
- infer runtime permission;
- call FoA runtime APIs.

## Error and blocker model

Errors explain operation failure. Blockers explain why a state or usage remains unavailable.

Good messages identify:

- affected subject;
- exact missing or invalid requirement;
- affected usage;
- corrective action when known.

Do not hide errors to keep dashboards green.

## Dependency policy

Prefer O3DE, AzCore, AzToolsFramework, Qt, and the C++ standard facilities already available in the host.

A new dependency requires:

- clear necessity;
- licence compatibility;
- provenance and security review;
- supported platforms;
- maintenance plan;
- deterministic build integration;
- removal or migration strategy.

## Architecture changes

A change to an invariant, layer boundary, durable identity, or schema requires:

- design issue or architecture decision record;
- threat and failure analysis;
- migration plan;
- tests;
- documentation updates;
- maintainer approval before implementation and before merge.
