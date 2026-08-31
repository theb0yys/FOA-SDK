# DR-TH-002 Research Brief

## Campaign Vanilla Terrain / Authoritative World Geometry Source

Research ID: `DR-TH-002`

Owner system: `world-authoring / TerrainAuthoring`

Target capability: `Highmap Importer -> Edit Vanilla Map`

Research authority: research only

## Starting evidence

DR-TH-001 and the supplied static/CIL evidence reject `StreamingAssets/DepthTextures/<scene>/depth_tex_X_Y.raw`
as authoritative terrain. The identified consumer is wetness/precipitation/VFX. This brief therefore targets the
actual campaign terrain or base-world geometry owner.

## Primary question

For each `CampaignMap_*` scene, what source representation owns continuous base-ground elevation, where is that
source identified/stored, and what deterministic metadata is required to construct an editable
`TerrainHeightmapDocumentV1`?

## Maps

- `CampaignMap_HOS`
- `CampaignMap_Cuanacht`
- `CampaignMap_Forlorn`
- `CampaignMap_Sarras`

Do not assume a common representation across all four.

## Candidate source families to test

### Unity Terrain / TerrainData

Determine whether campaign scenes contain or reference `UnityEngine.Terrain`, `TerrainData`, and
`TerrainCollider`. If established, capture asset identity, resolution, size, scale, transform, neighbours, storage
route, and recoverability.

### Mesh-based terrain/base world

Identify `MeshFilter`, `MeshRenderer`, `MeshCollider`, `LODGroup`, world chunks, terrain/ground meshes, transforms,
bounds, LOD/collision relationships, and heightfield fitness. Separate base surface from cliffs, caves, overhangs,
rocks, structures, and decoration.

### ECS/custom streamed geometry

Test Drake, entity scenes, SubScenes, blob assets, custom world streaming, and related systems only where evidence
connects them to source geometry. Runtime rendering derivatives are not automatically authoring sources.

### Medusa

Determine the relationship between scene-authored static mesh objects and the Medusa bake. Establish source
object selection, map/scene binding, transforms, mesh identities, archive index/entries, and whether any selected
objects form continuous base ground.

### Leshy, HLOD, Pathfinding, Addressables, scenes

Establish subsystem roles and dependency/identity metadata without treating derived products as substitute
terrain proof. Addressables research should be metadata-first: semantic key/GUID/object ID to bundle/dependency.

## Targeted leads

- `GroundBounds.CalculateGameBounds()` — identify owner type, source of bounds, map overrides, and relationship
  to terrain/scene geometry.
- `Awaken.TG.EditorOnly.TerrainHeightRemapper` — identify callers, targeted Terrain objects, editor workflow, and
  any campaign bindings.
- Infinity Code Mesh to Terrain — establish actual FOA editor/project use, not merely package presence.

## Required representation classification

Choose only with evidence, including per-map exceptions:

- `UNITY_TERRAINDATA_AUTHORITATIVE`
- `MESH_WORLD_AUTHORITATIVE`
- `CUSTOM_STREAMED_TERRAIN_AUTHORITATIVE`
- `MIXED_REPRESENTATION`
- `REPRESENTATION_VARIES_BY_MAP`
- `INSUFFICIENT_EVIDENCE`

## Required reconstruction contract

Whatever source is identified, establish:

```text
source sample/vertex
    -> local coordinates
    -> object transform
    -> CampaignMap world coordinates
    -> FOA-SDK canonical coordinates
```

Required metadata includes source identity, units/basis, world origin, per-object/tile transform, extent,
resolution where raster, topology/neighbours, vertical semantics, and deterministic canonical transform.

## Heightfield-fitness assessment

State whether the authoritative base surface is:

- directly a heightfield;
- deterministically rasterizable with defined loss;
- not representable as one heightfield;
- mixed with additional non-heightfield world geometry.

The Highmap Importer may own only the editable 2.5D base layer; other world geometry remains a separate authoring
surface.

## SDK mapping

For every map, resolve or mark unknown:

```text
map/source identity
sourceKind and sourceObjectIdentifier
width/height or provider raster policy
sample spacing
vertical range
coordinate basis/orientation/sample semantics
sourceToCanonicalTransform
tile/chunk topology
provenance
```

## Zero-configuration constraint

The user must not provide TerrainData paths, scene GUIDs, bundle names, asset GUIDs, dimensions, height range,
terrain transform, topology, world origin, or coordinate conversion. A provider remains blocked when those
values are not deterministic.

## Evidence discipline and protected boundary

Distinguish public documentation, package metadata, static/decompilation, static asset metadata, runtime log, and
live runtime. Research does not authorize scene/bundle extraction, private-install inspection, game launch,
commercial-content redistribution, provider implementation, or runtime claims.

## Required deliverable

- primary and per-map representation classification;
- per-map authoritative-source table;
- evidence-backed source dependency graph;
- reconstruction contract if established;
- SDK field matrix;
- heightfield-fitness assessment;
- provider recommendation;
- evidence register;
- remaining blockers.

## Success condition

The research passes for a map only when it deterministically establishes what owns the base terrain, source
identity, spatial placement, elevation representation, topology, and conversion to `TerrainHeightmapDocumentV1`
without technical user input.
