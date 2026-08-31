# Highmap Importer SDK Accommodation Assessment

Observation date: 31 August 2026

Repository baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

Evidence lane: repository/static inspection

Repository mutation during assessment: none

This report preserves the repository assessment performed before DR-TH-001. It evaluates how much of the
zero-configuration Highmap design was already implemented or structurally accommodated by FOA-SDK.

## Executive result

The SDK already contained a substantial canonical terrain-import backend, but did not yet provide the intended
end-user Highmap workflow.

```text
End-user Highmap experience:
    FAILED / not implemented

Backend terrain foundation:
    PARTIAL and substantial
```

The correct implementation direction is to build a higher-level Highmap coordinator and user interface over the
existing terrain document/import services rather than replacing them.

## Design-fit matrix

| Requirement | State at baseline | Repository observation |
| --- | --- | --- |
| SDK service exposes Highmap Importer | `FAILED` | TerrainAuthoring existed as a Tool Gem contract, but its initial shell explicitly registered no visible pane and marked commands unavailable in the shell. |
| Edit Vanilla Map | `FAILED` | No terrain-specific vanilla-map discovery, source binding, editable-copy, or open workflow was found. |
| Import New Map | `PARTIAL` | Raw and image heightmap import backends existed. |
| No user-entered directories | `PARTIAL` | Workspace/profile detection and path policy existed, but full zero-configuration machine/game discovery was not established. |
| Automatic heightmap interpretation | `PARTIAL` | Image dimensions and format could be inspected; consequential world/coordinate metadata remained explicit low-level request fields. |
| Non-destructive editing | `PARTIAL / strong foundation` | Workspace-owned revision, save, revert, undo, redo, containment, and no-game-write authority structures existed. |
| Transactional import | `PASSED` | Pending staging, cleanup, immutable-source checks, validation, atomic publication, and no-overwrite behavior existed. |
| Human-readable vanilla map catalogue | `FAILED` | No terrain-specific vanilla catalogue/provider integration was found. |
| Map thumbnails/preview | `FAILED at shell` | Terrain preview projection and visible pane were disabled in the initial shell contract. |
| Automatically open imported map | `FAILED at shell` | OpenDocument was represented as a command contract but not available in the shell. |
| No large configuration wizard | `NOT_APPLICABLE` | No completed TerrainAuthoring UI existed to evaluate. |

## Existing canonical terrain contract

The repository already defined:

```text
foa.terrain-heightmap
schema version 1
```

through `TerrainHeightmapDocumentV1`.

The document carries:

- stable document identity;
- map identity and public aliases;
- active game/profile binding;
- source binding and fingerprints;
- width, height, and metric sample spacing;
- sample encoding;
- minimum and maximum terrain elevation;
- handedness, axes, row orientation, sample semantics, and 4x4 source transform;
- deterministic canonical tile inventory;
- provenance;
- revision lineage;
- local payload state;
- explicit authority flags.

This is the correct low-level contract for preserving source interpretation. It also means a provider must prove
those fields rather than invent them.

## Existing import backends

The repository exposed:

```text
ImportRawHeightmapToWorkspace(...)
ImportImageHeightmapToWorkspace(...)
```

Supported local source routes at the observed baseline included:

```text
16-bit PNG
16-bit TIFF
RAW / U16 / R16 with explicit sidecar metadata
```

The low-level request structures expected internal caller data such as:

```text
workspaceRoot
inputPath
sidecarPath where applicable
MapIdentity
ProfileBinding
Grid metadata
VerticalMapping
CoordinateSpace
operationId
createdAtUtc
importer identity/version
```

These fields are appropriate internal contracts. They are not appropriate normal user questions.

## Existing deterministic behavior

The image route already demonstrated an important zero-configuration pattern:

- inspect the selected image;
- derive its width and height;
- validate optional caller-supplied dimensions when present;
- canonicalise byte order and source-kind classification;
- hash the source;
- generate canonical tiles;
- publish a validated workspace revision.

The Highmap coordinator should extend this pattern by constructing the remaining request fields from the active
workspace/profile, map catalogue, provider contract, and generated operation metadata.

## Transaction and containment behavior

The raw importer already implemented a staged operation resembling:

```text
validate request
    -> resolve canonical workspace root
    -> resolve immutable source snapshot
    -> validate/parse metadata
    -> verify exact payload size
    -> hash source
    -> reset contained pending staging
    -> generate canonical tiles
    -> verify source unchanged
    -> build and validate document
    -> promote pending state to staging
    -> atomically write manifest
    -> write source observation
    -> atomically rename completed revision into published workspace root
```

Failure paths removed contained staging state rather than knowingly publishing partial terrain revisions.

The importer also refused to overwrite an existing published revision for the same operation.

This substantially satisfied the transactional design requirement.

## Existing revision and authority model

TerrainAuthoring command contracts included:

```text
ImportLocalHeightmap
ValidateCandidate
OpenDocument
SaveRevision
RevertRevision
UndoEdit
RedoEdit
```

The command model distinguished operations that write workspace revisions from read/validation operations.

Authority defaults remained local-only and denied:

```text
runtime use
deployment
publication
packaging
game writes
evidence promotion
direct FOA install scan
external process execution
```

That structure aligns with non-destructive vanilla editing: source observations remain read-only and user edits
belong to workspace-owned revisions.

## Existing setup/profile support

`LocalSetupDetectionService` could:

- establish default workspace/profile identities;
- derive output, staging, deployment, diagnostics, and extraction paths from a workspace root;
- validate candidate FOA installation paths;
- recognise likely installations through executable/data-directory indicators;
- populate active game-profile fields when a valid candidate was supplied;
- distinguish Mono and IL2CPP profile concepts.

The service accepted bounded hints, including candidate install paths. The assessment did not establish full
automatic Steam/registry/machine discovery.

Therefore environment discovery was classified `PARTIAL`, not complete.

## TerrainAuthoring shell limitation

The initial TerrainAuthoring service contract declared only `ReadActiveProfile` capability and kept:

```text
visible pane registered = false
preview projection enabled = false
asset processor projection enabled = false
commands available in shell = false
```

It also prohibited direct FOA install scanning and external-process behavior.

This explains why the repository contained backend contracts without the intended two-action Highmap interface.

## Missing coordinator layer

The central missing software layer was:

```text
Highmap Import Coordinator / Vanilla Source Provider
```

Expected responsibilities:

- obtain active workspace automatically;
- obtain active game profile automatically;
- generate operation IDs and timestamps;
- derive local map IDs/display names;
- identify source format;
- resolve map/source object identity;
- determine dimensions and encoding;
- determine metric spacing and vertical range;
- determine coordinate basis and source transform;
- create low-level import request;
- register/open resulting revision;
- expose one coherent progress/failure state to the UI.

## Vanilla route gap

The largest missing feature was not generic tile writing. It was:

```text
FOA map identity
    -> authoritative vanilla terrain source
    -> lawful source observation
    -> deterministic source-format provider
    -> workspace-owned canonical terrain
```

At the observed baseline, TerrainAuthoring intentionally could not scan or extract the FOA installation directly.
No proven terrain-source binding existed for the four campaign maps.

That gap became the subject of DR-TH-001 and later research.

## Assessment conclusion

Existing components that should be retained:

```text
TerrainHeightmapDocumentV1
raw/image canonicalisation
validation and fingerprints
contained staging
atomic publish
revision lineage
package guards
active profile model
path policy
```

Components that remained to be designed/implemented after source research:

```text
Highmap coordinator
vanilla terrain provider
map catalogue integration
two-action TerrainAuthoring pane
map selection and recent revisions
preview/open/edit command routing
provider blocker/recovery state
```

Final assessment:

```text
Canonical terrain backend: PASSED
Transactional import foundation: PASSED
Workspace/revision foundation: PASSED
Environment discovery: PARTIAL
New local map UX: FAILED
Vanilla map source/provider: FAILED
Two-action Highmap UI: FAILED
```
