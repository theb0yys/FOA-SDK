# Actor and Troop Architecture and Data Formats

Status: implemented Core, Framework, persistence, Editor, fixture, and validation contracts; runtime population execution is deferred.

## Architectural ownership

### Core

Core owns:

- `PopulationActorProfile`, `PopulationTroopProfile`, and `PopulationTroopMember` models;
- schema-2 parsing, validation, canonical ordering, integrity, and document construction;
- exact actor-template, leader, and member identity binding;
- population evidence-coverage validation;
- immutable action-lane derivation;
- deterministic population fixture validation;
- no Qt, Editor lifecycle, process, runtime, deployment, or save-game integration.

### Framework

Framework owns governed candidate publication:

- canonical workspace-root and active-pack checks;
- exact source/evidence lookup;
- candidate actor-profile and atomic troop-definition construction;
- save-before-publish persistence;
- notification only after durable success;
- no direct widget ownership and no runtime authority.

### Editor

The Editor pane owns presentation and draft orchestration:

- actor/troop filtering and selection;
- typed field editing;
- member staging;
- save, revert, dirty-draft, close, and deferred-refresh behavior;
- evidence, governance, blockers, relationships, and action-lane display;
- no private catalog implementation, permission grant, runtime call, deployment, or save mutation.

## Data flow

```text
project-owned source input
  -> source fingerprint and evidence rows
  -> canonical population records
  -> typed actor/troop/member candidate
  -> exact evidence and integrity validation
  -> durable schema-2 catalog save
  -> atomic published catalog snapshot
  -> Foundation notification
  -> Actor/Troop pane refresh
```

Any failure before the durable save leaves the published snapshot unchanged. Any persistence failure blocks publication and notification.

## Catalog schema

Population authoring requires catalog `SchemaVersion: 2`.

Schema 2 extends the existing catalog document with these top-level arrays:

```json
{
  "ActorProfiles": [],
  "TroopProfiles": [],
  "TroopMembers": []
}
```

Schema-1 input may be migrated by the catalog loader, but durable writes are schema 2. Unknown, malformed, duplicate, or contradictory population rows fail closed.

## Actor profile

Canonical shape:

```json
{
  "RecordId": "example.pack.actor.guard",
  "ActorKind": "npc",
  "Archetype": "guard",
  "MinimumLevel": 1,
  "MaximumLevel": 10,
  "UniqueActor": false,
  "PersistentActor": false,
  "EssentialActor": false,
  "TemplateRecordId": "example.pack.template.guard",
  "TemplateSubjectRef": "subject:example:template:guard",
  "ModelAssetRef": "example.pack.asset.guard.model",
  "PortraitAssetRef": "example.pack.asset.guard.portrait",
  "LocalisationNameRef": "example.pack.loc.guard.name",
  "LocalisationDescriptionRef": "example.pack.loc.guard.description",
  "EvidenceIds": ["example.evidence.actor.guard"],
  "Tags": ["guard"]
}
```

Required invariants:

- `RecordId` resolves to a canonical record of kind `actor`;
- actor kind is typed;
- minimum level is positive and not greater than maximum level;
- resolved template IDs point to a canonical actor-template record;
- an unresolved template is preserved as an exact subject, never inferred from display text;
- every required actor/template subject has exact evidence;
- arrays are canonicalized and stable.

## Troop profile

Canonical shape:

```json
{
  "RecordId": "example.pack.troop.patrol",
  "TroopKind": "patrol",
  "Formation": "column",
  "MinimumSize": 2,
  "MaximumSize": 4,
  "LeaderActorRecordId": "example.pack.actor.guard",
  "LeaderActorSubjectRef": "subject:example:actor:guard",
  "EvidenceIds": ["example.evidence.troop.patrol"],
  "Tags": ["patrol"]
}
```

Required invariants:

- the canonical record kind is `troop`;
- troop kind is typed;
- minimum size is positive and not greater than maximum size;
- the leader resolves to a typed actor profile;
- leader record and exact subject agree;
- evidence covers the troop and required leader subjects.

## Troop member

Canonical shape:

```json
{
  "LinkId": "example.pack.member.patrol-guard",
  "TroopRecordId": "example.pack.troop.patrol",
  "ActorRecordId": "example.pack.actor.guard",
  "ActorSubjectRef": "subject:example:actor:guard",
  "Role": "leader",
  "Required": true,
  "MinimumCount": 1,
  "MaximumCount": 1,
  "Weight": 1.0,
  "Conditions": [],
  "EvidenceIds": ["example.evidence.member.patrol-guard"]
}
```

Required invariants:

- link, troop, and actor IDs are stable;
- troop and actor records resolve to typed profiles;
- actor record and exact subject agree;
- required rows have a positive minimum;
- optional rows may have a zero minimum;
- maximum count is not lower than minimum count;
- weight is finite and non-negative;
- roles are typed;
- conditions and evidence IDs are sorted and unique;
- an exact actor subject appears at most once per troop;
- exactly one leader row matches the troop profile leader;
- aggregate member ranges overlap the troop size range.

## Identity and evidence

Population records use the canonical catalog identity rules:

- native identity requires an exact native reference;
- synthetic identity requires active-pack ownership and no claimed native reference;
- display names, aliases, localisation, tags, and asset paths are never identity keys;
- source/evidence rows remain bound to exact profile, game version, branch, source fingerprint, and subject;
- a collection of evidence IDs is valid only when every required subject is covered.

An unresolved template subject is not permission to guess. It remains visible as unresolved until evidence-backed promotion creates the exact canonical record.

## Governance and action lanes

Population permissions remain usage-specific catalog governance. The editor derives seven immutable lanes from canonical state. The service evaluates prohibitions before allowances and also checks:

- catalog integrity;
- typed profile/composition completeness;
- exact evidence;
- validation state;
- staleness;
- supersession;
- open blockers;
- resolved workspace context.

`runtime_spawn` and `save_mutation` are structurally unavailable, regardless of catalog governance.

## Deterministic serialization

Population collections are serialized in stable order:

- actor profiles by `RecordId`;
- troop profiles by `RecordId`;
- troop members by `LinkId`;
- member conditions, tags, and evidence IDs in canonical sorted order.

Equivalent valid inputs must produce byte-equivalent canonical JSON. Load, save, close, and reopen must preserve the same typed state.

## Fixture format

The deterministic fixture writes six payloads plus `population-preview-fixture.manifest.json`:

- workspace;
- pack;
- source input;
- source document;
- evidence document;
- schema-2 catalog.

The manifest contains the fixture identity, expected semantic state, sorted relative paths, SHA-256 values, and byte sizes. It contains no absolute/private path and no proprietary game content.

## Deferred contracts

This schema intentionally does not define:

- encounter placement;
- spawn points or runtime pools;
- routes and schedules;
- density, lifecycle, cleanup, or rollback execution;
- runtime entity handles;
- save-game ownership;
- Mono or IL2CPP calls.

Those capabilities require separate reviewed schemas and adapters. They must reference these canonical identities rather than replacing them with a second population database.
