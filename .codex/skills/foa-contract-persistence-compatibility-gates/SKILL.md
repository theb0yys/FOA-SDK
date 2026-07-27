---
name: foa-contract-persistence-compatibility-gates
description: Use whenever FOA-SDK work touches public APIs, schemas, manifests, stable IDs, commands, JSON/XML, persistence, serialization, canonical interchange, migration, configuration, dependencies, package layout, or consumer boundaries.
---

# FOA-SDK Contract and Persistence Compatibility Gates

## First Actions

1. Identify every public or persisted surface touched or nearby.
2. Identify producers, consumers, readers, writers, serializers, importers, exporters, loaders, and adapters.
3. Compare old/new shape, IDs, defaults, null behaviour, ordering, versions, provenance, permissions, and exact-reference assumptions.
4. Classify compatibility as unchanged, intentionally changed, migrated, blocked, or unknown.

## Research

Read owner contracts, schemas, manifests, persistence/interchange code, migrations, consumer tests, release gates, and target profile evidence.

## Hard Stops

Stop if impact is unknown; a public or persisted shape changes without migration/authority; consumers are unidentified; downgrade/new-workspace policy is absent; exact native references are normalised; or config/dependency/package changes bypass release gates.

## Validation

Require schema/contract comparison, consumer boundary tests, malformed/degraded input tests, persistence/interchange round trips, migration proof where applicable, static package assertions, and runtime/loader proof when required.

## Runtime Proof

Static compatibility is not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless the exact loader/adapter lane ran.
