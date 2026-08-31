# Highmap Importer Zero-Configuration Design Baseline

Observation date: 31 August 2026

Status: preserved design context

This document preserves the user-facing Highmap Importer design established before the terrain-source research
sequence. It is a product-design baseline, not an implementation permit, native-format claim, runtime claim, or
accepted architecture change.

## Product objective

The ordinary user must not reconstruct FOA or SDK directory layouts, internal asset identities, manifests,
output roots, coordinate systems, or importer metadata.

The Highmap service presents exactly two primary actions:

```text
Highmap Importer

Edit Vanilla Map
Import New Map
```

No directory selection, environment-path wizard, or long metadata form is part of the normal path.

## Edit Vanilla Map

Expected interaction:

```text
Edit Vanilla Map
    -> select a human-readable map
    -> SDK resolves the source and technical metadata
    -> SDK creates or reuses a workspace-owned editable revision
    -> editor opens the map
```

The initial public map catalogue is expected to present:

- Horns of the South;
- Cuanacht;
- Forlorn Swords;
- Sanctuary of Sarras.

Display names and aliases do not automatically constitute exact native asset identity. The SDK must maintain the
separation between public display identity, source-scoped references, and exact native/source-object identity.

## Import New Map

Expected interaction:

```text
Import New Map
    -> choose or drop one supported local source
    -> optionally confirm a human-readable map name when it cannot be derived cleanly
    -> Import
    -> open editable revision
```

The SDK should determine internally, where the source permits:

- source type;
- image dimensions;
- encoding;
- operation identity and timestamp;
- map/project identity;
- workspace output location;
- canonical document paths;
- canonical tile layout;
- revision metadata;
- editor registration;
- validation and cleanup.

Technical values must not be moved into the UI merely because the provider has not proved them.

## SDK-owned environment discovery

The importer consumes SDK environment state. It does not ask the user to rediscover it.

Conceptually:

```text
SDK installation
    -> active workspace
    -> active FOA profile
    -> game/runtime profile metadata
    -> authoring/project paths
    -> importer-owned staging and output roots
```

Failure to discover the environment is an exceptional setup/repair state. The normal Highmap workflow does not
begin with multiple path pickers.

## Non-destructive vanilla editing

A vanilla source is never edited in place.

Required ownership transition:

```text
read-only vanilla/source observation
    -> canonical workspace-owned revision
    -> user edits workspace revision
    -> later reviewed output/deployment path
```

Reset, reimport, revision history, and source immutability depend on preserving this boundary.

## Transactional import

The user-visible operation is simple, while the internal operation remains staged and fail-closed:

```text
select source
    -> inspect
    -> validate
    -> resolve metadata
    -> canonicalise
    -> stage
    -> validate complete result
    -> atomically publish workspace revision
    -> open
```

Cancellation or failure must leave no published partial revision. Incomplete staging must be cleaned up within
the reviewed workspace boundary.

## No importer wizard

The design explicitly rejects a normal workflow that asks for fields such as:

```text
game directory
source-data directory
output directory
map ID
world ID
width
height
bit depth
byte order
sample spacing
minimum height
maximum height
coordinate handedness
up axis
forward axis
row orientation
sample semantics
source-to-canonical matrix
bundle or object ID
manifest path
```

If a source provider cannot resolve a consequential field, that source remains unsupported or blocked. The
uncertainty is not transferred to the ordinary user.

## Service responsibility split

```text
User
    chooses the map/source

Highmap UI
    presents two actions, map catalogue, progress, blockers, and recent revisions

Highmap coordinator/provider
    resolves source identity, source format, map metadata, transforms, and canonicalisation

TerrainHeightmapDocumentV1 backend
    validates, tiles, stages, fingerprints, publishes, and preserves revision/provenance state
```

## Design principle

> The SDK knows the environment. The provider knows the source format. The user chooses the map.

## Research dependency

This design deliberately precedes and survives the terrain-source research. Research determines which provider
can satisfy the zero-configuration contract; it does not justify replacing unresolved provider metadata with user
questions.
