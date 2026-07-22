# Actor and Troop Editor Design

Status: implemented vertical slice — Core contracts/database, schema-2 migration/persistence, Framework
evidence-bound candidate publication, production-linked population tests, the immutable population action-lane
contract, Actor and Troop Editor lifecycle, deterministic synthetic fixture, full local-validation integration,
and public release-readiness documentation are implemented. Exact-head O3DE configure/build, compiled Catalog
test execution, and the real Windows twenty-four-pane evidence pass remain the active acceptance gate.

Target: first Phase 6 functional-expansion vertical slice

## Accepted implementation boundary

The approved first substantial SDK expansion beyond economy authoring covers typed actor profiles, troop profiles,
troop membership, evidence-bound authoring services, durable catalog persistence, validation and blocker
integration, and a usable **Tainted Grail Actor and Troop Editor**.

Implementation proceeds on `FOA-plug-in-development`. The accepted boundary includes the catalog schema migration, Core
and Framework services, Editor workflow, tests, validators, deterministic synthetic fixtures, and
documentation described here.

It does not authorise actor spawning, AI execution, scene placement, faction mutation, quest behavior, FoA
launch, adapter execution, deployment, save mutation, or proprietary game data.

## User problem

The SDK can govern canonical identities and author typed item and recipe data, but it cannot yet describe the
actors and groups that participate in encounters, factions, routes, quests, rewards, or world population.

Without actor and troop profiles, later domain tools would either duplicate identity fields, use untyped
strings, or build private stores that bypass the catalog. This slice creates the shared population foundation
before spawn, encounter, faction, world, and narrative work begins.

## User outcomes

A mod author following the accepted workflow must be able to:

1. select an existing canonical actor or troop record;
2. inspect its exact identity, pack ownership, evidence, governance, and blockers;
3. create or update one typed actor or troop profile;
4. bind an actor to an optional evidence-backed template identity;
5. define troop size, leader, formation metadata, and typed member rows;
6. use resolved canonical member IDs or explicit unresolved subject references without confusing the two;
7. see read-only action lanes for display, planning, composition, spawn candidacy, runtime spawn, and save
   mutation;
8. save through the catalog transaction boundary;
9. close-equivalent reset and reopen the workspace with equivalent canonical state;
10. receive actionable errors for missing evidence, invalid identity, incompatible record kind, duplicate
    membership, bad counts, unsafe references, or persistence failure.

## Architecture ownership

### Core

Core owns:

- `PopulationActorProfile`, `PopulationTroopProfile`, and `PopulationTroopMember` contracts;
- enum or exact-value parsing and stringification where a closed value set is appropriate;
- intrinsic field validation;
- canonical-record and relationship queries;
- deterministic troop composition and evidence views;
- Catalog schema-2 in-memory storage and integrity checks.

Core does not read or write files and does not depend on Qt or FoA runtime libraries.

Core also owns the immutable population action-lane derivation introduced in unit 6. It derives the fixed
seven-lane matrix from exact published catalog state, the path-policy-resolved authoring context, exact
profile/pack/evidence binding, and applicable open blockers without mutating any input or granting authority.

### Framework

Framework owns:

- evidence-to-subject validation;
- active workspace, profile, and pack checks;
- actor/troop authoring commands;
- candidate `CatalogDatabase` mutation;
- save-before-publish persistence;
- Catalog schema-1 to schema-2 migration;
- Foundation snapshot counts and change notifications.

### Editor

Editor owns one **Tainted Grail Actor and Troop Editor** pane. The widget remains a thin client over Framework
services and cannot write catalog files directly.

### Runtime

No runtime component changes. Actor creation, actor spawning, troop activation, AI behavior, scene placement,
cleanup, rollback, and save persistence remain separately designed adapter responsibilities.

## Canonical identity boundary

An actor or troop profile attaches to exactly one existing `CatalogRecord` by `recordId`.

Accepted canonical record kinds for the first slice are:

- actor profile: domain `population`, kind `actor`;
- troop profile: domain `population`, kind `troop`.

A profile never creates the canonical identity implicitly. Native identities remain exact. Synthetic
identities must remain pack-owned under existing catalog rules. Display names, aliases, localisation text,
template labels, and asset paths never become identity keys.

## Proposed typed contracts

### PopulationActorProfile

Fields:

- `recordId` — exact canonical actor record ID;
- `actorKind` — required closed value such as `npc`, `creature`, `animal`, `construct`, or `other`;
- `archetype` — bounded evidence-backed descriptive classification;
- `templateRecordId` — optional resolved canonical template record;
- `templateSubjectRef` — optional unresolved exact template subject reference;
- `minimumLevel` and `maximumLevel` — optional bounded planning range with minimum not greater than maximum;
- `uniqueActor` — project claim that the actor identity is unique;
- `essentialActor` — project claim that later runtime handling may require essential-state protection;
- `persistentActor` — project claim that later runtime handling may require persistence planning;
- `localisationNameRef` and `localisationDescriptionRef` — optional exact localisation references;
- `portraitAssetRef` and `modelAssetRef` — optional project or reviewed asset references;
- sorted unique `tags`;
- sorted unique `evidenceIds`.

The three lifecycle flags are descriptive planning metadata only. They grant no runtime or save permission.

### PopulationTroopProfile

Fields:

- `recordId` — exact canonical troop record ID;
- `troopKind` — required closed value such as `party`, `patrol`, `encounter_group`, `reinforcement`, or
  `other`;
- `leaderActorRecordId` — optional resolved canonical actor leader;
- `leaderActorSubjectRef` — optional unresolved exact leader subject reference;
- `minimumSize` and `maximumSize` — required positive bounded composition range;
- `formation` — optional bounded planning label;
- sorted unique `tags`;
- sorted unique `evidenceIds`.

If a resolved leader is supplied, the accepted troop composition must contain a member row for that actor.

### PopulationTroopMember

Fields:

- `linkId` — stable namespaced membership identity;
- `troopRecordId` — exact owning troop record;
- `actorRecordId` — optional resolved canonical actor record;
- `actorSubjectRef` — optional unresolved exact actor subject reference;
- `role` — required bounded role such as leader, melee, ranged, support, specialist, or other;
- `minimumCount` and `maximumCount` — positive range with minimum not greater than maximum;
- `weight` — finite non-negative planning weight;
- `required` — whether at least the minimum count is structurally required;
- sorted unique `conditions`;
- sorted unique `evidenceIds`.

Exactly one actor binding form is used when possible. If both a resolved record and subject reference are
present, the subject reference must equal the canonical record's exact subject reference.

## Relationships and later domains

The profile contracts hold intrinsic authoring data. Independently governed associations remain first-class
`CatalogRelationship` records.

Examples reserved for relationship services include:

- actor uses template;
- actor belongs to faction;
- actor carries item;
- actor grants or drops item;
- actor participates in quest;
- troop belongs to faction;
- troop follows route;
- troop appears in encounter.

This slice may present existing relevant relationships read-only, but it does not invent faction, route,
encounter, or quest schemas.

## Evidence rules

Every profile and member row requires evidence IDs.

Evidence must:

- exist in the active `SourceEvidenceRegistry`;
- match the active profile, game version, and branch under existing registry rules;
- refer to the actor, troop, template, leader, or member subject represented by the authored row;
- remain distinct from validation and permission decisions;
- be sorted and unique in durable output.

Evidence for a troop row cannot be borrowed from an unrelated actor merely because display names match.

## Validation and fail-closed behavior

### Actor profile validation

Reject:

- missing or non-actor canonical records;
- unsupported actor kinds;
- empty or oversized required text;
- a minimum level greater than maximum level;
- template IDs that do not resolve to an accepted template or actor-template record;
- a supplied template record and subject reference that disagree;
- duplicate tags or evidence identities;
- missing or wrong-subject evidence;
- synthetic records without pack ownership.

### Troop profile validation

Reject:

- missing or non-troop canonical records;
- unsupported troop kinds;
- zero, excessive, or inverted size ranges;
- leader bindings that are malformed, unresolved without a subject reference, or inconsistent with a resolved
  actor;
- a resolved leader absent from troop membership;
- duplicate tags or evidence identities;
- missing or wrong-subject evidence.

### Membership validation

Reject:

- unstable or duplicate link IDs;
- missing troop identity;
- non-troop owners;
- missing actor identity and subject reference;
- non-actor resolved records;
- resolved and unresolved bindings that disagree;
- duplicate actor membership within one troop unless a later design explicitly authorises distinct keyed
  variants;
- zero, excessive, or inverted count ranges;
- non-finite or negative weights;
- unknown roles;
- duplicate conditions or evidence identities;
- missing or wrong-subject evidence.

## Action lanes

The Editor displays independent read-only lanes rather than a single safe/unsafe result:

- `display`;
- `author_profile`;
- `compose_troop`;
- `planning`;
- `spawn_candidate`;
- `runtime_spawn`;
- `save_mutation`.

Profile authoring and troop composition require the exact authoring evidence and an active pack. Spawn
candidacy additionally depends on relevant validation, permissions, staleness, and blockers. `runtime_spawn`
and `save_mutation` remain unavailable because this slice adds no runtime adapter or save contract.

The UI cannot grant an allowed usage or clear a blocker.

## Durable schema and migration

This slice changes `Catalog/catalog.tgcatalog.json` from schema 1 to schema 2.

Schema 2 adds:

- `ActorProfiles`;
- `TroopProfiles`;
- `TroopMembers`.

Migration behavior:

1. schema 1 loads through the existing fields and compatibility normalization retains that version;
2. actor, troop, and membership collections initialize empty;
3. the loaded candidate remains schema 1 and direct save is refused;
4. the complete candidate is validated against the active workspace, profile, source/evidence registry, and
   catalog integrity rules before it may replace published state;
5. only successful bound replacement and `BuildDocument` produce a schema-2 document;
6. the next successful save writes canonical schema 2;
7. schema versions newer than 2 are rejected with an actionable unsupported-version message;
8. failed migration or persistence never replaces the current published catalog.

No economy profile, relationship, validation, governance, or identity data may be lost or reordered
nondeterministically during migration.

## CatalogDatabase surface

Add methods equivalent to the existing economy profile surface:

- `UpsertPopulationActorProfile`;
- `UpsertPopulationTroopProfile`;
- `UpsertPopulationTroopMember`;
- `FindPopulationActorProfile`;
- `FindPopulationTroopProfile`;
- `FindPopulationMembersForTroop`;
- getters for the three typed collections;
- intrinsic validators and integrity checks;
- schema-2 `BuildDocument` and replacement support.

All collection ordering in persisted output must be deterministic by stable identity.

## Framework authoring surface

Add Foundation commands:

- `UpsertPopulationActorProfile`;
- `UpsertPopulationTroopDefinition` for one atomic troop profile and supplied member upserts;
- `UpsertPopulationTroopProfile`;
- `UpsertPopulationTroopMember`;
- later removal commands only with explicit dependency and rollback validation.

`UpsertPopulationTroopDefinition` is the required bootstrap path for a new troop. A complete catalog cannot
publish a troop profile without at least one typed member, and a member cannot publish without its typed troop
profile. The compound command therefore upserts the troop profile and supplied members in one candidate before
integrity validation, persistence, and publication. Existing members omitted from the request remain unchanged;
this command does not add removal authority. The single-profile and single-member commands remain available
only for updates that independently preserve complete-catalog integrity.

Membership link identity is immutable across owning troops. Updating an existing `linkId` may change that row's
authored fields only while retaining its exact `troopRecordId`; moving a link to another troop is rejected as an
undeclared removal/addition operation.

The active authoring pack must have stable editor-owned identity, keep runtime actions disabled, and target the
active profile's branch and game version (directly or through its compatible-version set). Pack-owned canonical
primary actor or troop record being authored must belong to that active pack; native targets remain ownerless.
Evidence-backed references to template, leader, or member actors may cross pack boundaries and do not transfer
ownership.

Authoring also requires a stable workspace identity and the canonical workspace root previously resolved and
validated by the existing save/load path policy. A merely non-empty root supplied through `SetWorkspace` is not
publication authority.

Each command:

1. validates active workspace, profile, and pack state;
2. validates evidence against allowed subjects;
3. copies the current catalog to a candidate;
4. applies the typed mutation to the candidate;
5. validates complete integrity;
6. persists the complete candidate document;
7. publishes the candidate only after persistence succeeds;
8. emits one foundation change notification.

## Editor workflow

The **Tainted Grail Actor and Troop Editor** should provide:

- actor/troop canonical record filters;
- exact identity, subject, owner, evidence, governance, and blocker summary;
- Actor Profile and Troop Profile forms;
- troop membership table and bounded member editor;
- resolved-record selectors plus explicit unresolved subject-reference fields;
- evidence selection and evidence-detail view;
- read-only relevant relationship view;
- read-only action-lane matrix;
- save and revert controls;
- independently tracked actor, troop, and unstaged-member drafts: selection and shared Foundation refreshes
  cannot discard them, saving one tab preserves the other tab, and closing with unsaved work requires explicit
  discard confirmation;
- clear empty, invalid, blocked, and persistence-failure states.

The pane does not create canonical records, grant permission, invoke an adapter, launch FoA, or expose spawn
or save-mutation controls.

## Accessibility

Required behavior includes:

- keyboard access to record filters, profile fields, member table, evidence selector, save, and revert;
- explicit labels for all controls;
- status text that does not rely on colour alone;
- deterministic tab order;
- readable validation summaries;
- no hover-only evidence or blocker explanation.

## Security, privacy, and legal boundary

The slice must not:

- scan for FoA installations or game files;
- load private assemblies or assets;
- copy game content;
- accept unrestricted absolute paths;
- store credentials or personal paths;
- execute user-supplied scripts;
- launch a process or contact a network service;
- include proprietary actor, troop, image, audio, or model fixtures.

Synthetic tests and preview data use only project-owned `preview.*` identities and invented descriptive
content.

## Failure and rollback

Failures return actionable errors and leave published state unchanged.

Required failure paths include:

- catalog schema mismatch;
- migration failure;
- missing canonical record;
- wrong record kind;
- missing or mismatched evidence;
- invalid template, leader, or member binding;
- duplicate profile or member identity;
- count or weight validation failure;
- integrity failure elsewhere in the candidate catalog;
- persistence failure;
- reload mismatch.

Rollback is reversion of the actor/troop contracts, services, UI, schema-2 migration, tests, validators,
fixtures, and docs. A downgrade path is not implied: a workspace already saved as schema 2 must be preserved
or explicitly rejected by older builds rather than silently truncated.

## Test strategy

### Core compiled tests

Cover:

- actor, troop, and member happy paths;
- exact canonical kind and subject binding;
- synthetic pack ownership;
- template and leader resolution;
- size, level, count, weight, role, and kind bounds;
- duplicate and conflicting bindings;
- deterministic ordering;
- complete-catalog integrity;
- preservation of governance and action authority without population authoring silently granting either;
- input non-mutation.

### Framework and persistence tests

Cover:

- evidence-bound authoring;
- candidate-before-publish behavior;
- persistence failure rollback;
- schema-1 to schema-2 migration;
- schema-2 round trip;
- unsupported future schema rejection;
- save, close-equivalent clear, reopen, and canonical equivalence;
- existing economy data preservation.

### Editor and validator tests

Cover:

- pane and module registration;
- non-editable governance surfaces;
- population-specific action-lane derivation and read-only presentation;
- absence of runtime, process, deployment, and save-mutation APIs;
- required controls and error text;
- fail-closed dirty-draft guards for filters, record/member selection, Foundation notifications, cross-tab
  saves, and pane close;
- deterministic synthetic fixture coverage;
- local-validation integration;
- Windows manual checklist expansion from twenty-two to twenty-three TG SDK panes.

## Implementation sequence

After design approval, implementation proceeds in focused reviewable units:

1. **Complete** — actor/troop contracts, reflection, and Catalog schema-2 model;
2. **Complete** — CatalogDatabase validation, queries, document build/replacement, and integrity;
3. **Complete** — schema-1 migration, schema-2-only writing, persistence round-trip coverage, malformed-input
   rejection, and focused repository validation;
4. **Complete** — Framework evidence-bound authoring, atomic troop-definition bootstrap, candidate validation,
   save-before-publish persistence, Foundation snapshot counts, and single-change notification;
5. **Complete** — Core and Framework positive/negative population-authoring test sources and compiled-target wiring;
6. **Complete** — immutable population action-lane derivation, Actor and Troop Editor pane, and lifecycle
   registration;
7. **Complete** — deterministic synthetic population fixture and full vertical-slice local-validation integration;
8. **Complete** — public user, architecture/data-format, release-readiness, changelog, and twenty-four-pane
   evidence-checklist updates;
9. **Active acceptance gate** — exact-head O3DE configure/build, compiled Catalog tests, and Windows UI evidence.

Completion of units 1–8 establishes the implemented Actor/Troop vertical slice: durable population contracts,
persistence, Framework authoring commands, positive/negative production-linked tests, immutable action lanes,
the registered Editor pane, deterministic project-owned fixture, validator integration, and public documentation.
It does not claim that compiled tests have run in an exact-head configured build or that Windows UI evidence exists.

Unit 9 owns the remaining exact-head host and real Windows twenty-four-pane acceptance evidence.

Every commit receives complete staged-diff self-review, DCO sign-off, relevant focused validation, and
`FOA-plug-in-development` synchronization before and after publication.

## Acceptance criteria

The slice is complete only when:

- actor and troop profiles attach only to exact accepted canonical records;
- every authored profile and membership row is evidence-backed;
- troop composition is deterministic and referentially valid;
- schema-1 catalogs migrate without losing existing data;
- schema-2 catalogs round-trip byte-stably where canonical serialization promises stability;
- failed writes do not publish candidate state;
- save, close-equivalent reset, and reopen preserve equivalent state;
- action lanes remain independent and fail closed;
- the Editor workflow is usable and accessible;
- no runtime, spawn, scene, adapter, deployment, or save operation is introduced;
- focused validators, compiled tests, applicable host build, documentation, and Windows UI coverage pass for
  the exact accepted head.

## Review boundary

Approval authorises this actor/troop authoring vertical slice and the schema-2 migration described above.

It does not authorise spawn or encounter execution, faction mutation, world placement, quest behavior, asset
extraction, runtime adapters, process launch, deployment, save mutation, telemetry, proprietary fixtures, or
later Phase 6 schemas not covered by a focused review.
