# DR-TH-004 Research Brief

## Campaign Terrain Source Binding — Static Code, Scene Metadata, and Asset Identity

Research ID: `DR-TH-004`

Owner system: `world-authoring / TerrainAuthoring`

Target capability: `Highmap Importer -> Edit Vanilla Map`

Research state: `COMPLETED`

Primary evidence lane: static/decompilation plus static scene/asset metadata

Implementation authority: none

## 1. Exact research question

For each `CampaignMap_*`, identify the exact object or asset that owns continuous base-ground elevation and establish the durable binding from campaign scene identity to that source.

The required result is one of:

```text
CampaignMap_*
    -> UnityEngine.Terrain
    -> TerrainData
    -> exact source identity + dimensions + transform + topology
```

or:

```text
CampaignMap_*
    -> base-ground scene objects
    -> MeshFilter / MeshRenderer / MeshCollider
    -> exact mesh identity + transforms + bounds
```

or:

```text
CampaignMap_*
    -> TerrainData base
    + mesh landscape extensions
    -> mixed source contract
```

This is a source-binding investigation, not another broad survey of FOA world systems.

## 2. Starting evidence

The durable terrain research context establishes:

```text
DepthTextures:
    wetness / precipitation / VFX derivative
    authoritative-terrain route closed

Leshy:
    vegetation
    authoritative-terrain route closed

PathfindingCache:
    derived navigation
    authoritative-terrain route closed

HLOD:
    derived rendering
    authoritative-terrain route closed

Medusa:
    static mesh landscape authoring confirmed
    runtime archive is a derivative
    exact source-object membership unknown

Unity Terrain / TerrainData:
    project-specific code use confirmed
    CampaignMap binding unknown

Campaign world:
    mixed representation at system level

Production vanilla Highmap provider:
    blocked
```

Use `../SOURCE_REGISTER.md` and `../CLAIM_REGISTER.md` as the controlling research context.

## 3. Maps in scope

Investigate independently:

- `CampaignMap_HOS`
- `CampaignMap_Cuanacht`
- `CampaignMap_Forlorn`
- `CampaignMap_Sarras`

Do not infer one map's representation from another.

## 4. Evidence priority

```text
GroundBounds
    -> TerrainHeightRemapper
    -> CampaignMap scene/dependency loading
    -> direct Terrain / TerrainData references
    -> Medusa source selection
    -> Addressables semantic mapping
    -> serialized CampaignMap scene metadata
```

The objective is a durable join, not an additional type name.

## 5. Evidence boundary

Acceptable evidence includes:

- user-supplied decompilation/static-analysis reports;
- static CIL/type/member analysis;
- lawfully available serialized scene or asset metadata;
- GUID/file-ID/object-ID references;
- Addressables catalogue/index metadata;
- public developer and package metadata.

This brief does not authorize private-installation inspection, arbitrary commercial scene/bundle extraction, DLL or asset redistribution, game launch, runtime instrumentation, game-file mutation, or SDK implementation.

Unavailable protected operations are blockers. Missing values must not be guessed.

## 6. Target A — `GroundBounds.CalculateGameBounds()`

Capture:

```text
assembly and SHA-256
namespace/type
fields and configuration references
method body or exact pseudocode
callers and callees
```

Determine whether bounds come from Terrain, colliders, renderers, authored configuration, GameConstants, scene markers, streaming cells, or another custom owner.

Classify as one of:

```text
GROUNDBOUNDS_FROM_TERRAIN
GROUNDBOUNDS_FROM_COLLIDERS
GROUNDBOUNDS_FROM_RENDERERS
GROUNDBOUNDS_FROM_AUTHORED_CONFIG
GROUNDBOUNDS_FROM_CUSTOM_WORLD_SYSTEM
GROUNDBOUNDS_SOURCE_MIXED
GROUNDBOUNDS_SOURCE_UNKNOWN
```

## 7. Target B — `TerrainHeightRemapper`

Investigate `Awaken.TG.EditorOnly.TerrainHeightRemapper` completely:

```text
defining assembly/hash
base type/interfaces
fields/properties/methods
serialized Terrain/TerrainData references
height ranges and remap values
callers, inspectors, windows, menu tools, build processors
scene/map filters
TerrainData read/write calls
```

Classify use as:

```text
CAMPAIGN_TERRAIN_REMAP_CONFIRMED
GENERIC_TERRAIN_TOOL_ONLY
NON_CAMPAIGN_USE_CONFIRMED
USAGE_UNKNOWN
```

## 8. Target C — CampaignMap scene loading and identity

Search every direct reference to the four `CampaignMap_*` values and prioritize scene loading, world initialization, Addressable scenes, additive scenes, scene baking, SubScenes, entity scenes, and streaming.

Resolve where possible:

```text
exact Unity scene path
scene GUID/file ID
Addressables scene key
build index
additive/SubScene dependencies
world initialization owner
```

## 9. Target D — direct Terrain references

Search TypeRef, MemberRef, MethodSpec, field/property signatures, serialized fields, generics, and editor types for:

```text
UnityEngine.Terrain
UnityEngine.TerrainData
UnityEngine.TerrainCollider
```

For every FOA-specific reference capture exact type/member, read/write usage, map filtering, asset-reference behavior, and editor/runtime status.

## 10. Target E — Medusa source selection

Locate the Medusa process-scene implementation and identify:

```text
selection method
inclusion/exclusion predicates
marker components
tags/layers/static flags
scene filters
source object IDs
mesh/material refs
transforms/bounds
archive IDs and source-object join keys
```

Determine whether durable metadata distinguishes continuous ground from cliffs, rocks, overhangs, decorative landscape, and structures.

## 11. Target F — Addressables semantic identity

Find any semantic mapping from CampaignMap, TerrainData, ground/landscape mesh, Medusa mesh, or scene identity to:

```text
PrimaryKey / RuntimeKey
GUID / InternalId
ResourceType
dependencies
bundle
```

A hash-named bundle without semantic mapping is insufficient.

## 12. Target G — serialized scene metadata

If lawfully available, enumerate for every map:

```text
GameObjects and components
file IDs/object IDs
Transform
Terrain/TerrainCollider
TerrainData references
MeshFilter/MeshRenderer/MeshCollider
LODGroup
Medusa/custom markers
```

For TerrainData capture GUID/file ID, heightmap resolution, heightmap scale, size, and holes resolution. For candidate mesh ground capture mesh IDs, transforms, bounds, colliders, tags/layers/static flags, and scene ownership.

Metadata only; no proprietary geometry or texture payloads need to be reproduced.

## 13. Per-map decision

Choose only when evidence supports it:

- `TERRAINDATA_BASE_CONFIRMED`
- `MESH_BASE_CONFIRMED`
- `MIXED_BASE_CONFIRMED`
- `NO_CONTINUOUS_HEIGHTFIELD_SOURCE`
- `INSUFFICIENT_EVIDENCE`

Do not choose TerrainData merely because project code references it. Do not choose mesh merely because Medusa exists.

## 14. Reconstruction contract

If TerrainData is confirmed, capture the exact Terrain transform, TerrainData size, heightmap resolution, heightmap scale, and neighbours. Do not assume identity rotation, unit scale, shared size, shared origin, or shared resolution.

If mesh base is confirmed, first establish the exact mesh set, transforms, bounds, colliders, LOD relationships, overlap and chunk topology. Classify fitness as:

```text
SINGLE_VALUED_GROUND_SURFACE
MULTI_VALUED_GEOMETRY
PARTIAL_HEIGHTFIELD
MESH_RASTER_POLICY_REQUIRED
```

Any future mesh-raster policy must separately define resolution, ray direction, surface selection, overlaps, holes, caves, bridges, overhangs, and cliff treatment.

## 15. `TerrainHeightmapDocumentV1` matrix

For each map resolve or mark `UNKNOWN`:

```text
map identity
source kind
source object ID
width/height
sample spacing
min/max world height
coordinate basis
row orientation
sample position
source-to-canonical transform
tile topology
provenance
```

Classify values as source-derived, static-code-derived, static-asset-derived, catalogue-derived, SDK-derived, requires-provider-policy, unknown, or not applicable.

## 16. Report format

Include:

- execution/evidence header;
- assemblies and hashes;
- static call/reference graph;
- per-map source-binding table;
- exact code/metadata excerpts for consequential joins;
- per-map decision and `TerrainHeightmapDocumentV1` matrix;
- claim labels: `FACT`, `INFERENCE`, `HYPOTHESIS`, `UNKNOWN`, or `CONTRADICTED`.

## 17. Stop conditions

Stop and mark the exact question `BLOCKED` when the assembly/type/metadata is unavailable, the operation lacks authority, source identity cannot be separated from a runtime derivative, a field would require guessing, or evidence conflicts.

Do not substitute another broad public search.

## 18. Success criteria

A map passes only when this chain is established:

```text
CampaignMap
    -> exact continuous base-ground source
    -> exact identity
    -> dimensions/geometry
    -> transform
    -> topology
    -> elevation representation
```

Overall:

```text
all four resolved: PASSED
one or more resolved: PARTIAL
source family known but object metadata incomplete: PARTIAL
no exact source binding: INSUFFICIENT_EVIDENCE
```

## 19. Terminal sentence

For each map the report must answer:

> `CampaignMap_X` gets its editable continuous base-ground elevation from __________, identified by __________, positioned by __________, with topology __________.

Unsupported blanks remain `UNKNOWN`.

## 20. Authority boundary

This brief creates no implementation, source-extraction, runtime, deployment, publication, packaging, or evidence-promotion authority.
