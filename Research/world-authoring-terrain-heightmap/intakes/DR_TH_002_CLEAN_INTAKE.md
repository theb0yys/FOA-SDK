# DR-TH-002 Clean Research Intake

Research subject: authoritative campaign terrain / world-geometry source

Preserved input:

- `../inputs/DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md`

Status: `PARTIAL`

System-level representation: `MIXED_REPRESENTATION`

Authoritative editable campaign heightfield: `UNKNOWN`

Implementation authority created: none

Live runtime validation: `NOT_RUN`

Commercial scene/bundle extraction: `NOT_RUN`

## Clean conclusion

DR-TH-002 materially narrows the world-authoring model but does not establish a deterministic native heightfield
source for any of the four campaign maps.

The durable developer sources support a project architecture containing several distinct systems:

- Leshy for vegetation;
- Medusa for fully static mesh-rendered environment content, originally cliffs and later generalized;
- Drake for runtime renderer registration and mesh/material streaming through Addressables;
- scene baking;
- HLOD and other derived/runtime systems.

The supplied static report separately establishes project-specific access to Unity `Terrain` / `TerrainData`
through `Awaken.TG.EditorOnly.TerrainHeightRemapper`.

Taken together, the strongest research classification is:

```text
Campaign world representation:
    MIXED_REPRESENTATION

Mesh-authored static landscape participation:
    source-supported

Unity Terrain / TerrainData awareness somewhere in code/tooling:
    static-report-supported

CampaignMap -> exact Terrain/TerrainData binding:
    unknown

CampaignMap -> exact base-ground mesh binding:
    unknown
```

This is E1 research context. It is not a normative architecture lock and does not authorize a source provider.

## System dispositions

### DepthTextures

Final state: `contradicted` as authoritative terrain.

The accepted DR-TH-001 static intake identifies wetness/precipitation/VFX consumers rather than terrain
construction.

### Leshy

Final state: `contradicted` as authoritative terrain.

Developer material describes Leshy as vegetation streaming/rendering. Its per-map files remain useful as
source-scoped identity/placement evidence only.

### Medusa

State:

```text
Medusa involvement in static landscape meshes:
    source-supported

Medusa authoring input:
    Unity LODGroup + MeshRenderer objects

Medusa runtime product:
    immutable build derivative in StreamingAssets

medusa.arch as editable terrain source:
    contradicted

all continuous base ground owned by Medusa meshes:
    unknown
```

`SRC-DEV-MEDUSA` states that Medusa objects are ordinary Unity render primitives baked using
`IProcessSceneWithReport` into separate StreamingAssets data and that runtime data is immutable for the scene.
The project overview includes static terrain-like assets in Medusa's domain. That establishes material
mesh-landscape participation without identifying the base-ground source per map.

### Drake and HLOD

These are retained as runtime/derived rendering systems. Their existence may expose asset identity or dependency
metadata, but they are not currently qualified as editable terrain authority.

### Unity Terrain / TerrainData

State:

```text
Terrain/TerrainData project awareness:
    static-report-supported

campaign Terrain component inventory:
    unknown

TerrainData asset identities:
    unknown

heightmapResolution / size / heightmapScale:
    unknown

terrain transforms / neighbours:
    unknown
```

If a campaign `TerrainData` binding is later proved, `SRC-UNITY-TERRAINDATA` defines upstream APIs that can
supply most required source-grid and vertical metadata. Generic Unity API capability does not establish a FOA
binding by itself.

### Mesh To Terrain

The third-party tool's documented purpose is source-supported by `SRC-MESH-TO-TERRAIN`. Actual use in FOA
campaign construction remains unknown. The supplied DLL report contains only helper/documentation stubs and does
not establish the original editor workflow.

## Per-map state

| Map | Source-scoped map identity | Exact base-ground owner | Exact TerrainData/mesh source | Highmap readiness |
| --- | --- | --- | --- | --- |
| `CampaignMap_HOS` | strong source/runtime-scene context | `unknown` | `unknown` | `BLOCKED` |
| `CampaignMap_Cuanacht` | strong source/runtime-scene context | `unknown` | `unknown` | `BLOCKED` |
| `CampaignMap_Forlorn` | strong package/source-scoped context | `unknown` | `unknown` | `BLOCKED` |
| `CampaignMap_Sarras` | strong package/source-scoped context | `unknown` | `unknown` | `BLOCKED` |

No map is promoted to `UNITY_TERRAINDATA_AUTHORITATIVE`, `MESH_WORLD_AUTHORITATIVE`, or a qualified custom
provider by this intake.

## Heightfield fitness boundary

The research rejects the assumption that the whole visible/collision world can be represented by one heightmap.
Medusa's documented cliff/static-mesh domain means the campaign world includes geometry that a 2.5D heightfield
cannot intrinsically encode, such as overhangs or multiple vertical surfaces at one horizontal coordinate.

The Highmap Importer's bounded domain should therefore be treated as:

```text
editable continuous base-terrain / heightfield layer
```

separate from:

```text
cliffs
caves and overhangs
rocks and structures
static landscape meshes
vegetation
roads and other world systems
```

The base layer itself remains unidentified per map.

## Remaining blocking fields

The following `TerrainHeightmapDocumentV1` source fields remain unresolved for all four campaign maps:

| Field | State |
| --- | --- |
| authoritative `sourceKind` | `unknown` |
| exact `sourceObjectIdentifier` | `unknown` |
| source width/height or mesh-raster resolution | `unknown` |
| horizontal sample spacing | `unknown` |
| native minimum/maximum elevation | `unknown` |
| terrain world origin | `unknown` |
| terrain tile/chunk topology | `unknown` |
| per-tile/object transforms | `unknown` |
| row orientation | `unknown` |
| sample-position semantics | `unknown` |
| deterministic source-to-canonical transform | `unknown` |

The canonical output may still use the V1 U16 little-endian tiled representation once an authoritative source is
known. Canonical output rules do not resolve unknown native source semantics.

## Highest-value next evidence unit

The next research unit must be a static, per-map component and source-asset inventory—not another broad world
systems survey.

For each `CampaignMap_*`, it must enumerate and bind:

```text
UnityEngine.Terrain
TerrainCollider
TerrainData references
Terrain transforms
TerrainData resolution / size / scale / neighbours

MeshRenderer
MeshFilter
MeshCollider
LODGroup
Medusa marker/filter components
landscape/base-ground object identities and transforms

scene dependencies
SubScene/entity-scene references
Addressables keys/GUIDs where exposed
GroundBounds references and source
```

This scope is defined in:

`../briefs/DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md`

## Product impact

The user-facing target remains:

```text
Highmap Importer

Edit Vanilla Map
Import New Map
```

The user must not be asked to supply TerrainData paths, scene GUIDs, bundle names, dimensions, height ranges,
terrain transforms, tile topology, world origin, or coordinate conversion. Missing source fields keep the
provider blocked.

## Source bindings

Claims in this intake use:

- `SRC-INPUT-DR-TH-002`
- `SRC-INPUT-DR-TH-001-STATIC`
- `SRC-DEV-INTRO`
- `SRC-DEV-MEDUSA`
- `SRC-DEV-DRAKE`
- `SRC-DEV-TECH-UPDATE`
- `SRC-PUB-DEPOT`
- `SRC-UNITY-TERRAINDATA`
- `SRC-MESH-TO-TERRAIN`
- `SRC-REPO-TERRAIN-H`
- `SRC-REPO-WA-TH-001`

See `../SOURCE_REGISTER.md` and `../CLAIM_REGISTER.md`.
