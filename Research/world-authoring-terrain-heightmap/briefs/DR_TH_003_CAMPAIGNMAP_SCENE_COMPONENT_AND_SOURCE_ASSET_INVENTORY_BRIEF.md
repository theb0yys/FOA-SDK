# DR-TH-003 Research Brief

## CampaignMap Scene Component and Source-Asset Inventory

Research ID: `DR-TH-003`

Owner system: `world-authoring / TerrainAuthoring`

Target capability: `Highmap Importer -> Edit Vanilla Map`

Research state: `READY`

Implementation authority: none

## 1. Exact question

For each Fall of Avalon campaign scene, enumerate the scene components and source-asset references necessary to
determine whether continuous base-ground elevation is owned by Unity `Terrain`/`TerrainData`, scene-authored mesh
geometry, or a combination of both.

This brief is deliberately limited to:

```text
CampaignMap scene
    -> component inventory
    -> source object/asset references
    -> transforms, bounds, resolution, size, topology
```

It must not expand into another broad survey of FOA world systems.

## 2. Starting state

The accepted research intake currently establishes:

```text
DepthTextures:
    derived wetness / precipitation / VFX
    not authoritative terrain

Leshy:
    vegetation
    not authoritative terrain

Medusa:
    static mesh landscape participation confirmed
    runtime archive is a derivative, not editable source

Unity Terrain / TerrainData:
    project-specific code awareness confirmed by supplied static report
    CampaignMap binding unknown

Campaign world:
    mixed representation at system level

Per-map base-ground source:
    unknown
```

Do not reopen these closed subsystem questions unless direct evidence contradicts the current claim register.

## 3. Maps in scope

Investigate independently:

- `CampaignMap_HOS`
- `CampaignMap_Cuanacht`
- `CampaignMap_Forlorn`
- `CampaignMap_Sarras`

Do not infer one map's component inventory from another map.

## 4. Required evidence lane

The target is a static scene/component/source-reference report. Acceptable evidence may include lawfully obtained:

- Unity scene object/type inventory;
- serialized component metadata;
- asset-reference metadata such as GUID/file ID/object ID;
- Addressables catalogue/dependency metadata;
- static assembly/type/caller evidence needed to interpret named components;
- exact public project/modding metadata where it exposes relevant type or asset identity.

The report must label its actual lane. Static metadata does not prove live runtime loading or persistence.

This brief does not itself authorize the agent to inspect a private installation, extract commercial scenes or
bundles, redistribute proprietary assets, launch the game, mutate files, or commit extracted content. Any such
evidence operation requires separate exact authority and must keep protected material outside the repository.

## 5. Scene identity inventory

For each map, record:

| Field | Required |
| --- | --- |
| public map name | yes |
| `CampaignMap_*` source-scoped scene key | yes |
| exact Unity scene asset/path if lawfully evidenced | yes or `UNKNOWN` |
| scene GUID / file ID / Addressables key | yes or `UNKNOWN` |
| build-scene / Addressable / additive / SubScene status | yes or `UNKNOWN` |
| direct scene dependencies | bounded relevant list |
| evidence locator | exact |

Do not promote a source-scoped scene key into a TerrainData or terrain-object identity.

## 6. Unity Terrain inventory

For every `UnityEngine.Terrain` component found, capture:

```text
map / scene
GameObject name
hierarchy path
component file ID / object ID
enabled state
Transform position
Transform rotation
Transform scale
TerrainData reference
TerrainCollider relationship
neighbour references
material/template references where relevant
rendering/collision flags relevant to source ownership
```

For every referenced `TerrainData`, capture:

```text
asset path if available
asset GUID
file ID / object ID
Addressables key and bundle dependency if exposed
heightmapResolution
heightmapScale
size.x
size.y
size.z
holes resolution where relevant
alphamap/detail/tree metadata only as supporting context
source/importer identity if exposed
```

The report must distinguish values read from serialized asset metadata from values calculated or inferred.

## 7. Terrain topology

If multiple Terrain objects exist, determine:

- tile count;
- transform grid;
- X/Z ordering;
- tile extent;
- shared-edge placement;
- neighbour references;
- gaps or overlaps;
- vertical offsets;
- whether all tiles share one resolution/size/height range;
- whether the set is rectangular or sparse;
- whether map sectors are additive or streamed.

Provide an explicit per-map topology table.

## 8. Mesh/base-ground inventory

For scene objects that may represent the continuous ground or major landscape surface, capture:

```text
GameObject/hierarchy identity
MeshFilter and mesh asset reference
MeshRenderer
MeshCollider or other collider
LODGroup membership
Medusa marker/selection component if present
layer/tag/static flags
local transform
world transform if deterministically derivable
renderer/collider bounds
scene/additive chunk ownership
Addressables or source-asset identity
```

The report must separate:

- likely continuous base-ground surfaces;
- cliffs and vertical landscape pieces;
- caves/overhangs/arches;
- rocks and decorative formations;
- structures;
- vegetation;
- unknown static geometry.

Object names alone are leads, not classification proof. Use components, layers, tags, colliders, bounds, selection
markers, build processors, or other evidence.

## 9. Medusa source selection

Where Medusa-related components or metadata are present, identify:

- exact marker/filter type;
- inclusion/exclusion conditions;
- source `LODGroup`/`MeshRenderer` identities;
- map/scene binding;
- mesh/material references;
- transforms and bounds;
- collider relationship;
- any archive entry/index identity that can be joined back to the scene source object.

The objective is source-object identity, not reconstruction from `medusa.arch` as though the archive were an
editable source.

## 10. GroundBounds

Identify the exact type and source used by:

```text
GroundBounds.CalculateGameBounds()
```

Capture:

- full namespace/type;
- fields and serialized references;
- implementation or exact static pseudocode;
- queried scene objects/components;
- per-map configuration;
- resulting bounds if available;
- whether bounds are derived from Terrain, colliders, authored boxes, scene metadata, or another owner.

GroundBounds may supply world extent/origin metadata, but it must not be treated as terrain ownership without an
explicit source relationship.

## 11. TerrainHeightRemapper

Follow all references/callers of:

```text
Awaken.TG.EditorOnly.TerrainHeightRemapper
```

Capture:

- defining assembly and hash;
- all fields/methods;
- editor menu/tool registration;
- Terrain assignments;
- scene/map filters;
- height-range calculations;
- TerrainData mutation calls in related editor assemblies;
- evidence of use or non-use in campaign authoring.

The current evidence establishes only that the helper accesses Terrain position and TerrainData height size.

## 12. Mesh To Terrain

Search static editor/project metadata for:

```text
InfinityCode.MeshToTerrain
MeshToTerrain
MeshToTerrainBoundsHelper
MeshToTerrainDocumentation
```

Capture actual campaign-scene references, editor invocations, saved settings, output TerrainData identities, or
build-tool integration if present.

The package's documented capabilities do not prove FOA used it for campaign maps.

## 13. Addressables and dependency metadata

Use metadata-first evidence. For relevant scene, TerrainData, mesh, and material assets, capture where available:

```text
semantic key
asset GUID
file/object ID
asset type
bundle hash/name
dependency keys
scene/subscene owner
```

Do not perform indiscriminate bundle extraction. A hashed bundle filename without semantic asset mapping does not
resolve the terrain source.

## 14. Required per-map decision

For each map, choose one only when evidence supports it:

- `TERRAINDATA_BASE_CONFIRMED`
- `MESH_BASE_CONFIRMED`
- `MIXED_BASE_CONFIRMED`
- `NO_CONTINUOUS_HEIGHTFIELD_SOURCE`
- `INSUFFICIENT_EVIDENCE`

The overall report may conclude `REPRESENTATION_VARIES_BY_MAP`.

## 15. Required reconstruction fields

For every confirmed base-terrain source, resolve:

| FOA-SDK field | Required evidence |
| --- | --- |
| `mapId` | catalogue/source identity |
| `sourceKind` | TerrainData, mesh, or mixed provider |
| `sourceObjectIdentifier` | exact asset/object identity |
| `width`, `height` | native heightmap or approved raster policy |
| sample spacing | TerrainData scale or deterministic mesh sampling policy |
| min/max height | source size/transform or bounded mesh surface range |
| handedness/up/forward axes | source object basis |
| row-zero orientation | native raster/provider rule |
| sample position | grid vertex/cell centre/provider rule |
| source-to-canonical transform | explicit matrix/formula |
| tile topology | Terrain neighbours or scene/chunk topology |
| provenance | source hashes/IDs and evidence bindings |

Do not fill absent values with defaults.

## 16. Heightfield fitness

For each map, state whether converting the confirmed base source to `TerrainHeightmapDocumentV1` is:

- lossless for the base surface;
- deterministically lossy with specified exclusions;
- blocked by multi-valued geometry;
- not applicable because no continuous heightfield source is identified.

Additional cliffs, caves, overhangs, structures, and decorative meshes must remain explicitly outside Highmap
coverage.

## 17. Zero-configuration decision

The report must answer whether the SDK can derive every required source field automatically after the user selects
a public map name.

The user must never be asked for:

```text
scene path or GUID
TerrainData path/GUID
bundle name
heightmap resolution
Terrain size or transform
tile topology
world origin
height range
coordinate conversion
```

If the evidence cannot resolve these values, the per-map vanilla provider remains blocked.

## 18. Required report format

### Execution header

```text
execution status
evidence lane
assemblies/assets and hashes
runtime validation state
private installation inspection state
commercial extraction state
repository mutation state
```

### Per-map inventory

One table per `CampaignMap_*` containing:

```text
scene identity
Terrain count
TerrainData identities and metadata
TerrainCollider count/refs
candidate base-ground meshes
Medusa-marked objects
scene/additive/Addressables dependencies
GroundBounds source/result
unknowns
```

### Exact evidence excerpts

For every consequential binding, include exact serialized field/value, type/method pseudocode, or metadata locator.

### Decision matrix

Per-map representation decision and `TerrainHeightmapDocumentV1` field-resolution matrix.

### Claim discipline

Every conclusion must be marked:

- `FACT`
- `INFERENCE`
- `HYPOTHESIS`
- `UNKNOWN`
- `CONTRADICTED`

and must name its evidence lane.

## 19. Stop conditions

Stop and report `BLOCKED` for the affected map when:

- scene/source metadata is unavailable;
- a required protected-data operation lacks exact authority;
- source identities cannot be distinguished from derived runtime products;
- TerrainData/mesh metadata is contradictory;
- the report would need guessed dimensions, transforms, topology, or height mapping.

Do not substitute another broad research task.

## 20. Success criteria

DR-TH-003 is `PASSED` for a map only when it establishes enough static source metadata to answer:

```text
what object/asset owns continuous base-ground elevation?
where is it identified?
what are its dimensions/resolution or mesh sampling inputs?
what is its world transform and topology?
how is elevation represented?
how can it map into TerrainHeightmapDocumentV1?
```

Results:

- all four maps resolved: `PASSED`;
- at least one map resolved and others blocked: `PARTIAL`;
- components found but source identity/reconstruction metadata missing: `PARTIAL`;
- no authoritative source owner established: `INSUFFICIENT_EVIDENCE`.

## 21. Repository/source references

Use `../SOURCE_REGISTER.md` and `../CLAIM_REGISTER.md`. Repository constraints are bound to the exact intake
baseline recorded there. This brief creates no authority beyond a future separately authorized research
evidence operation.
