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
11. **Capability execution is shared and version-forward** — existing inert V1 planning/evidence contracts stay inert; future side effects use immutable preview/execute pairs and one shared Build -> Package -> Deploy -> Launch -> Verify spine.

The detailed execution-plane authority is [Capability Execution Contract and Shared Production Spine](CAPABILITY_EXECUTION_CONTRACT.md).

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
- build, package, report, and handoff generation;
- future capability-execution orchestration and durable artifact/receipt repositories after separately authorised implementation.

Pure planning, preview, assessment, and evidence services do not gain side effects merely because an execution plane exists.

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

### 5. ExternalToolchain host and providers

`Gems/ExternalToolchain` owns generic provider descriptors, registration, layered configuration, bounded discovery, and read-only diagnostics.

A future separately reviewed execution API may own generic process supervision. Provider discovery, enabled configuration, compatibility, qualification, policy, human authorisation, execution outcome, and evidence promotion remain distinct.

Provider Gems own native tool translation. They do not select deployment destinations, grant runtime authority, or promote evidence.

### 6. Capability execution plane

The capability execution plane is additive. It consumes immutable control-plane output and produces receipts.

It owns, after separately authorised implementation:

- semantic capability requests;
- per-phase provider binding and deterministic resolution;
- independent support, qualification, environment, policy, and authorisation decisions;
- immutable phase plans and fingerprints;
- shared Build -> Package -> Deploy -> Launch -> Verify orchestration;
- artifact ownership, custody, target preimage, idempotency, and concurrency;
- execution, cleanup, cancellation, and preplanned rollback receipts;
- candidate-evidence handoff to existing assessment and reconciliation services.

It does not move side effects into Core preview/evidence services. It does not collapse domain ownership. Domain systems may own native materialisation and expected observations only.

### 7. FoA runtime adapters

Separate components that may, after explicit validation, policy, exact-plan authorisation, and provider qualification:

- call BepInEx, Harmony, Unity, or FoA APIs;
- grant or register content;
- spawn actors or encounters;
- bind routes or assets;
- mutate vendors, loot, rewards, quests, or state;
- persist data and perform migrations;
- clean up and roll back.

Adapters consume reviewed immutable work orders or phase plans and return runtime observations as execution receipts and candidate evidence. They do not grant their own permission or promotion authority.

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
reviewed work order / handoff / immutable phase plan
  ↓
separate reviewed provider or runtime adapter
  ↓
execution receipt and independent observation
  ↓
candidate source and evidence
  ↓
assessment and reconciliation
  ↓
human promotion when required
```

No arrow may be skipped merely because a value appears plausible.

For side-effecting capability work, the production phase sequence is:

```text
optional domain materialisation
  ↓
Build
  ↓
Package
  ↓
Deploy
  ↓
Launch
  ↓
Verify
  ↓
assessment
  ↓
reconciliation
```

## Capability execution boundary

The existing capability, build-manifest, package-preview, staging/deployment-preview, work-order, result, verifier, reconciliation, and release metadata services remain the control plane.

Future executors must:

- consume exact canonical planner output;
- bind execution to the exact plan fingerprint;
- resolve providers separately for every phase;
- revalidate environment and target preimages before side effects;
- record artifact identity, ownership, producer, custody, digest, and lifecycle;
- implement idempotency and concurrency control;
- plan rollback before execution;
- preserve result, candidate evidence, assessment, reconciliation, and promotion as separate stages.

Existing V1 `BuildAllowed`, `ExecutionAllowed`, `DeploymentAllowed`, mutation, launch, signing, publication, and similar flags remain false where required. They are not activation switches. Executable contracts version forward.

A domain editor or provider must not create its own private build, package, deployment, launch, rollback, receipt, or evidence-promotion system.

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

It should remain an orchestration boundary. Parsing, persistence, validation, domain logic, capability planning, execution policy, and provider invocation belong in dedicated services.

## Workspace model

A workspace defines the authoring boundary and exact game context.

It owns:

- workspace ID and roots;
- output, staging, and deployment paths;
- one or more game profiles;
- active profile selection;
- pack and source document locations;
- future catalog, content, build, report, artifact, execution, and receipt locations.

A game profile records the exact FoA build context that produced evidence or receives later output.

Machine-specific executable observations and secrets do not become shared workspace truth. Provider configuration and execution plans bind their exact fingerprints separately.

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
- runtime-action flag fixed to disabled in editor-owned V1 documents.

Every synthetic catalog record and produced artifact must reference a pack owner where applicable.

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

Execution receipts may project candidate source/evidence documents only through reviewed evidence services. Receipt acceptance is not evidence promotion.

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
- staleness and supersession;
- capability support;
- provider qualification;
- environment readiness;
- execution policy;
- exact-plan authorisation;
- execution outcome;
- verification assessment;
- release decision.

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

Permission is tied to a use, version, evidence set, and validation result. Provider discovery, plan readiness, execution success, or receipt acceptance does not grant permission automatically.

## Persistence architecture

Persistence services own all durable document reads and writes.

Rules:

- documents include schema versions;
- writes stay inside approved workspace roots;
- multi-document operations are transactional where practical;
- load errors are preserved and surfaced;
- breaking schema changes require migration support or explicit rejection;
- UI classes do not write files directly;
- runtime deployment is never performed by document persistence services;
- artifacts, backups, installed ownership, execution attempts, and receipts use dedicated repositories with exact fingerprints;
- secrets are referenced through opaque handles and do not enter canonical plans or logs.

Current suffixes:

- `*.tgworkspace.json`;
- `*.tgpack.json`;
- `source.tgsource.json`;
- `evidence.tgevidence.json`.

Catalog document suffixes are introduced with the catalog milestone. Future execution schemas require separately reviewed versioning and migration decisions.

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
- call FoA runtime APIs;
- launch providers;
- build, package, deploy, roll back, sign, publish, or promote evidence directly.

## Error and blocker model

Errors explain operation failure. Blockers explain why a state or usage remains unavailable.

Good messages identify:

- affected subject;
- exact missing or invalid requirement;
- affected usage;
- corrective action when known.

Capability-execution diagnostics also identify the phase, plan fingerprint, provider binding, artifact or target path, expected and observed fingerprints, policy/authorisation state, rollback state, and whether retry requires a new preview.

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

A provider implementation or external executable is not trusted merely because it is located or registered.

## Architecture changes

A change to an invariant, layer boundary, durable identity, schema, provider execution boundary, artifact-ownership model, deployment behavior, rollback behavior, receipt/evidence semantics, or shared production spine requires:

- design issue or architecture decision record;
- threat and failure analysis;
- migration plan;
- tests;
- documentation updates;
- maintainer approval before implementation and before merge.

The process route is defined by `.codex/workflows/foa_capability_execution_contract.md`.
