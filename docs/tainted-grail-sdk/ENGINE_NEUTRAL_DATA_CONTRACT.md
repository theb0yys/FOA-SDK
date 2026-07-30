# Engine-neutral canonical data contract

## Status

Proposed architecture contract for maintainer review.

## Decision

FOA-SDK canonical information is engine-neutral. O3DE is the primary governed authoring host, but it does not define the canonical format, identity model, or semantics of durable FOA-SDK data.

Every engine or authoring environment integrates through an adapter. O3DE integration belongs in product Gems. Target-game integration belongs in separate runtime adapters. A non-O3DE host must be able to consume, validate, and produce canonical documents without converting from O3DE serialization or loading O3DE libraries.

## Dependency rule

Dependencies point inward:

```text
O3DE Gem adapter ───────┐
Other host adapter ─────┼──> canonical contracts
Runtime adapter ────────┘
```

Canonical contracts never depend on O3DE, AzCore, AzToolsFramework, Qt, Unity, Unreal, Godot, or another engine SDK.

## Canonical contract requirements

Canonical documents and their in-memory domain equivalents may use only declared portable concepts:

- stable namespaced identifiers;
- explicit schema and contract versions;
- UTF-8 strings, booleans, bounded integers, finite decimal values, arrays, maps, and tagged unions;
- explicitly defined vectors, transforms, colours, bounds, references, relationships, provenance, validation, permission, and evidence records;
- URI-like source locators with declared normalization rules;
- content-addressed hashes and declared media types;
- adapter extension payloads namespaced by adapter ID and adapter contract version.

The interchange profile must define deterministic encoding, canonical field ordering where hashing requires it, duplicate-key handling, unknown-field policy, numeric ranges, nullability, defaulting, unit conventions, coordinate conventions, path and URI semantics, and schema migration behavior.

Units, axes, handedness, colour space, coordinate system, path roots, case sensitivity, and reference resolution must never be implicit.

## Prohibited engine leakage

Canonical identity or durable canonical fields must not be represented by:

- `AZ::Uuid`;
- `AZ::EntityId`;
- `AZ::Data::AssetId`;
- O3DE component type IDs or reflected class names;
- O3DE registry paths, aliases, project-relative asset paths, or cache paths;
- Qt object identity or widget state;
- Unity GUIDs, instance IDs, serialized-property paths, or engine object references;
- another engine's native entity, component, asset, scene, package, or reflection identifiers.

An exact native identifier may be preserved as attributed adapter metadata or as an explicitly typed external reference. It does not become the canonical record identity merely because a host uses it.

## Adapter envelopes

Engine-specific metadata must be stored in a reversible, namespaced envelope:

```json
{
  "adapter": "org.o3de.foa-authoring",
  "adapterContractVersion": "1.0.0",
  "payloadSchema": "org.o3de.foa-authoring.asset-binding/1",
  "payload": {}
}
```

Adapter envelopes:

- may retain host-native references needed for lossless round trips;
- must declare ownership and version;
- must be ignorable by consumers that do not implement the adapter;
- must not silently alter canonical identity, provenance, evidence state, permissions, prohibitions, or validation maturity;
- must fail closed when required adapter metadata is missing, stale, incompatible, or ambiguous.

## O3DE Gem adapter responsibilities

An O3DE Gem may:

- reflect canonical contracts into `AZ::SerializeContext`;
- map canonical values to O3DE editor controls and asset infrastructure;
- resolve O3DE assets, entities, components, and paths;
- produce editor diagnostics and previews;
- import and export canonical documents;
- retain reversible O3DE metadata in an adapter envelope.

An O3DE Gem must not:

- redefine canonical identity;
- require O3DE serialization to parse the canonical format;
- persist editor cache state as canonical truth;
- silently promote O3DE-native metadata into reviewed evidence or permission;
- require another engine to reverse-engineer O3DE data before implementing its own adapter.

## Runtime adapter responsibilities

Runtime adapters consume reviewed canonical work orders and produce target-specific payloads, receipts, and evidence. They may reject unsupported capabilities, but they must not require canonical records to be rewritten into an O3DE-defined interchange format first.

Runtime-specific behavior, persistence, cleanup, rollback, native API calls, and compatibility checks remain adapter responsibilities.

## Conformance requirements

The architecture is conformant only when all of the following are demonstrated:

1. Canonical fixtures parse and validate in a test executable or script that does not load O3DE libraries.
2. O3DE round-trip tests prove canonical to O3DE representation to canonical preserves every canonical field.
3. O3DE-only metadata remains isolated in its adapter envelope.
4. At least one mock or non-O3DE adapter consumes the same canonical fixtures without schema translation.
5. Breaking canonical changes have explicit migrations or explicit version rejection.
6. Adapter contract upgrades have compatibility tests and migration behavior.
7. Canonical hashing produces identical results across conforming implementations.

## Review checklist

Reject a durable schema or persisted contract when any answer is yes:

- Does parsing require an engine SDK?
- Is identity represented by an engine-native object or type ID?
- Are paths interpreted using one host's project or asset-root rules?
- Are units, axes, handedness, colour space, or coordinate conventions implicit?
- Would a second engine need to reverse-engineer O3DE serialization to consume the data?
- Can adapter metadata alter canonical identity, provenance, evidence status, validation, permissions, or prohibitions without explicit review?
- Is host cache or editor state being treated as canonical truth?

## Non-goals

This decision does not:

- remove O3DE as the primary governed editor host;
- prohibit O3DE-specific caches, editor state, generated assets, previews, or build products;
- require runtime adapters with materially different capabilities to behave identically;
- claim that target-engine payloads are portable after adaptation.

The portability boundary is the reviewed canonical information and work-order contract, not every generated engine artifact.