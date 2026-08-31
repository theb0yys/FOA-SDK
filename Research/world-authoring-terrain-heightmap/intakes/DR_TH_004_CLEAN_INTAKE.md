# DR-TH-004 Clean Intake

Research ID: `DR-TH-004`

Research date: 31 August 2026

Deep Research execution: `PASSED`

Research conclusion: `INSUFFICIENT_EVIDENCE`

Overall research state: `PARTIAL`

Public/web source-binding lane: exhausted for the present question

New decompilation: `NOT_RUN`

Serialized commercial scene inspection: `NOT_RUN`

Runtime validation: `NOT_RUN`

Private installation inspection: `NOT_RUN`

Repository implementation authority created: none

## Per-map disposition

```text
CampaignMap_HOS:
    INSUFFICIENT_EVIDENCE

CampaignMap_Cuanacht:
    INSUFFIC_EVIDENCE

CampaignMap_Forlorn:
    INSUFFICIENT_EVIDENCE

CampaignMap_Sarras:
    INSUFFICIENT_EVIDENCE
```

No map currently has an evidence-backed `TERRAINDATA_BASE_CONFIRMED`, `MESH_BASE_CONFIRMED`, `MIXED_BASE_CONFIRMED`, or `NO_CONTINUOUS_HEIGHTFIELD_SOURCE` result. Lack of confirmation is not evidence of absence.

## Accepted source-binding context

### Map identity

The four source-scoped campaign identities are established sufficiently for catalogue and discovery:

```text
CampaignMap_HOS
CampaignMap_Cuanacht
CampaignMap_Forlorn
CampaignMap_Sarras
```

They are not terrain-object identities.

### DepthTextures

The authoritative-terrain hypothesis remains closed. Supplied static evidence identifies the consumer chain as wetness, precipitation, and VFX support. Any later DepthTextures work concerns regeneration of a derived product, not the editable terrain source.

### GroundBounds

The following relation is supported by the supplied static report:

```text
TopDownDepthTexturesLoadingManager
    -> GroundBounds.CalculateGameBounds()
    -> Bounds used for world X/Z extent and vertical camera range
```

The defining `GroundBounds` type, assembly, fields, method body, and backing source remain unavailable.

Current classification:

```text
GROUNDBOUNDS_SOURCE_UNKNOWN
```

### TerrainHeightRemapper

Project-specific Terrain awareness remains supported:

```text
Awaken.TG.EditorOnly.TerrainHeightRemapper

CurrentLow = Terrain.transform.position.y
CurrentHigh = Terrain.transform.position.y + Terrain.terrainData.size.y
```

This proves FOA code directly works with `Terrain` and `TerrainData`. It does not establish a CampaignMap binding, caller, editor tool, serialized Terrain assignment, or heightmap mutation path.

Current classification:

```text
USAGE_UNKNOWN
```

### Medusa

The source/product distinction remains:

```text
Unity LODGroup + MeshRenderer source objects
    -> scene build processing
    -> Medusa derived runtime data
```

Medusa confirms mesh-authored static landscape participation. `medusa.arch` remains rejected as the editable authoring source. Exact source-object selection and continuous-ground membership remain unknown.

### Addressables

FOA Addressables use and the package bundle surface are established. The required semantic join remains unavailable:

```text
CampaignMap or terrain key
    -> PrimaryKey / RuntimeKey
    -> InternalId / GUID
    -> TerrainData or base-ground mesh
    -> dependencies and bundle
```

Hash-like bundle names alone provide no terrain identity.

## Remaining gaps

### 1. `GroundBounds` definition

Required evidence:

```text
defining assembly
full namespace/type
CalculateGameBounds() body
fields/config references
direct callees
```

The critical question is whether bounds are derived from Terrain, colliders, renderers, an authored bounds object, GameConstants, or another world system.

### 2. `TerrainHeightRemapper` caller graph

Required evidence:

```text
all fields and methods
serialized Terrain reference
constructors and callers
custom inspectors/windows/menu commands
scene/map filters
TerrainData read/write operations
```

A campaign-specific caller or serialized Terrain assignment would be decisive.

### 3. Campaign scene identity

For each map:

```text
exact .unity scene path
scene GUID/file ID
build index
Addressables scene key
additive-scene dependencies
SubScene/entity-scene relationships
world initialization owner
```

Only the source-scoped map keys are currently known.

### 4. Terrain component inventory

For each map:

```text
Terrain count
Terrain GameObject IDs
component object/file IDs
transform position/rotation/scale
TerrainCollider references
neighbour references
```

Missing public metadata must remain `UNKNOWN`, not zero.

### 5. TerrainData identity and metadata

For every campaign-bound TerrainData:

```text
asset path
GUID/file ID/object ID
Addressables key and bundle dependency
heightmapResolution
heightmapScale
size
holesResolution
```

A single durable chain such as `CampaignMap_Cuanacht -> Terrain -> TerrainData GUID/fileID` would resolve most native Highmap fields.

### 6. Exact mesh-base inventory

If base elevation is mesh-authored, required evidence is:

```text
continuous-ground GameObjects
mesh GUID/file IDs
MeshFilter/MeshRenderer/MeshCollider
LODGroup
world transforms and bounds
scene/chunk ownership
```

Evidence must distinguish continuous ground from cliffs, caves, overhangs, rocks, structures, and decorative landscape.

### 7. Medusa source-selection implementation

Still unknown:

```text
process-scene type
marker components
tags/layers/static rules
inclusion/exclusion predicates
scene filters
source object identifiers
archive entry identifiers
source-object -> archive-entry join
```

### 8. Addressables semantic mapping

Required evidence:

```text
CampaignMap or terrain semantic key
    -> IResourceLocation.PrimaryKey
    -> InternalId/GUID
    -> ResourceType
    -> dependencies
    -> bundle
```

### 9. Serialized scene metadata

The decisive metadata-only inventory would contain:

```text
GameObjects
Transforms
Terrain/TerrainCollider
TerrainData refs
MeshFilter/MeshRenderer/MeshCollider
LODGroup
Medusa/custom markers
file IDs/object IDs/GUIDs
```

No proprietary geometry or texture payloads need to be preserved in the repository.

## Blocked Highmap fields

For every campaign map:

| Terrain document input | State |
| --- | --- |
| catalogue/map identity | resolved |
| authoritative source kind | `UNKNOWN` |
| exact source-object identity | `UNKNOWN` |
| source width/height | `UNKNOWN` |
| sample spacing | `UNKNOWN` |
| minimum/maximum elevation | `UNKNOWN` |
| source transform | `UNKNOWN` |
| tile count/topology | `UNKNOWN` |
| neighbour relationships | `UNKNOWN` |
| row orientation | `UNKNOWN` |
| sample semantics | `UNKNOWN` |
| source-to-canonical transform | `UNKNOWN` |
| final source provenance | `UNKNOWN` |

The canonical output format being known does not resolve native source metadata.

## Evidence direction

Another broad public Deep Research pass is unlikely to improve the result. The next useful evidence must be a bounded static record, preferably in this order:

```text
1. GroundBounds definition and method body
2. TerrainHeightRemapper full type and callers
3. complete Terrain/TerrainData/TerrainCollider reference sweep
4. CampaignMap scene-loading/dependency types
5. Medusa source-selection/build types
6. Addressables semantic key construction
7. lawfully available serialized CampaignMap metadata
```

The terminal evidence required is one of:

```text
CampaignMap_X
    -> Terrain component
    -> TerrainData GUID/file ID
```

or:

```text
CampaignMap_X
    -> exact continuous-ground mesh objects
    -> mesh IDs + transforms + colliders
```

Until one of those joins is obtained:

```text
Deterministic vanilla Highmap provider:
    BLOCKED

Zero-configuration Highmap UX:
    UNAFFECTED

User-entered technical source metadata:
    REJECTED
```

## Authority boundary

This clean intake is research context only. It creates no source-extraction, implementation, runtime, deployment, publication, packaging, or evidence-promotion authority.
