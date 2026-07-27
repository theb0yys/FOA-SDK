---
name: foa-contract-persistence-compatibility-gates
description: Use whenever FOA-SDK work touches public APIs, schemas, manifests, stable IDs, commands, JSON or XML, persistence, serialization, canonical interchange, migration, configuration, dependencies, package layout, installer state, runtime-adapter contracts, or consumer boundaries. It prevents silent compatibility breakage and unsupported migration claims.
---

# FOA-SDK Contract and Persistence Compatibility Gates

Use after impact classification whenever a changed or nearby surface is public, persisted, serialized, imported, exported, packaged, installed, or consumed across a system boundary.

## First Actions

1. Identify every public or persisted surface touched or nearby.
2. Identify producers, consumers, readers, writers, serializers, importers, exporters, loaders, adapters, installers, and migration paths.
3. Compare old and new shape, IDs, defaults, null behavior, ordering, version fields, provenance, permissions, and exact-reference assumptions.
4. Classify compatibility as unchanged, intentionally changed, migrated, blocked, or unknown.
5. Record required consumer and migration proof before editing.

## Research

Read:

- owner contracts and public API documents
- schemas and manifests
- persistence, serialization, and canonical-interchange code
- migration and rejection rules
- configuration and dependency authorities
- package and installer contracts
- runtime-adapter contracts and target-profile evidence
- existing producer, consumer, malformed-input, and round-trip tests
- release and rollback gates

Implementation shape does not authorise a breaking change by itself.

## Compatibility Surfaces

Treat these as governed compatibility surfaces:

- public C++ or scripting APIs
- ExtensionAPI contracts
- stable type IDs, record IDs, GUIDs, and exact native references
- JSON, XML, schema, and manifest fields
- workspace, pack, evidence, catalog, claim, validation, and permission records
- canonical O3DE-to-Unity handoff formats
- conversion result formats
- installer product, upgrade, repair, and uninstall state
- Mono and IL2CPP adapter contracts
- configuration keys and defaults
- dependency versions and O3DE lock identity
- package layout, output names, and external destinations

## Required Comparison

For every governed surface, compare:

- field and member presence
- type, range, enum, and nullability
- defaults and omitted-value behavior
- identity and normalization rules
- ordering and deterministic serialization
- schema or contract version
- reader and writer compatibility
- downgrade, rejection, and migration behavior
- provenance, permission, and evidence semantics
- package and loader assumptions

## Persistence and Migration

A persisted or serialized change requires explicit authority for one of:

- backward-compatible read/write behavior
- deterministic migration
- explicit rejection with actionable diagnostics
- new-workspace or new-pack requirement
- unsupported downgrade policy

Do not promise migration, save compatibility, installer upgrade compatibility, or adapter compatibility without executed proof.

## Hard Stops

Stop when:

- compatibility impact is unknown
- a public or persisted shape changes without migration or explicit rejection authority
- producers or consumers are unidentified
- old readers or writers would silently misinterpret the new shape
- downgrade, rollback, or new-workspace policy is absent
- exact native references are normalized, reconstructed, or replaced by display names
- configuration, dependency, package, installer, or adapter changes bypass their release gates
- Mono and IL2CPP assumptions are collapsed into one path

## Validation

Require the applicable proof:

- schema or contract comparison
- producer and consumer boundary tests
- serialization and canonical-interchange round trips
- malformed, missing, stale, and future-version input tests
- deterministic ordering tests
- migration or explicit-rejection tests
- static package and manifest assertions
- installer upgrade, repair, rollback, or uninstall tests
- loader or adapter proof for the exact target profile when required

Report compatibility as complete, partial, blocked, or unknown. Missing consumer or migration proof cannot be reported as success.

## Runtime Proof

Static compatibility review is not Fall of Avalon runtime proof. O3DE compilation is not Unity conversion proof, installer proof, adapter proof, save proof, or exact-install runtime proof. State `runtime sign-off not performed` unless the required exact loader or adapter lane actually ran.
