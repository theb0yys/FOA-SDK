# DR-TH-003 — Clean Research Intake

Research ID: `DR-TH-003`

Observation/intake date: 31 August 2026

Deep Research execution: `PASSED`

Research disposition: `PARTIAL`

Evidentiary outcome: `INSUFFICIENT_EVIDENCE`

Runtime validation: `NOT_RUN`

Private-installation inspection: `NOT_RUN`

Commercial scene/bundle extraction: `NOT_RUN`

Repository mutation during the research run: `NOT_RUN`

## Primary conclusion

DR-TH-003 did not establish an authoritative continuous base-ground source for any campaign map from the permitted public/static evidence lanes.

Per-map result:

```text
CampaignMap_HOS       -> INSUFFICIENT_EVIDENCE
CampaignMap_Cuanacht  -> INSUFFICIENT_EVIDENCE
CampaignMap_Forlorn   -> INSUFFICIENT_EVIDENCE
CampaignMap_Sarras    -> INSUFFICIENT_EVIDENCE
```

This is not evidence that a map has no Unity Terrain or no mesh-authored base ground. It means the required serialized source-object binding was not available in the researched evidence.

## Facts retained

### Campaign identity

Current public package metadata exposes all four `CampaignMap_*` identities through map-scoped Leshy and PathfindingCache products.

These are valid source-scoped map references. They are not terrain-object identities.

### Unity Terrain / TerrainData

The supplied static/CIL report remains the project-specific evidence that FOA code/tooling accesses Unity `Terrain` and `TerrainData` through:

```text
Awaken.TG.EditorOnly.TerrainHeightRemapper
```

including:

```text
Terrain.transform.position.y
Terrain.terrainData.size.y
```

This establishes Terrain/TerrainData presence somewhere in FOA code/tooling, but no `CampaignMap_* -> Terrain -> TerrainData` binding has been established.

### Medusa

First-party developer material establishes that Medusa's source objects are ordinary Unity `LODGroup` + `MeshRenderer` objects and that the Medusa runtime representation is a downstream build product created during scene processing.

Public package history additionally exposes historical map-scoped Medusa products for at least HOS and Cuanacht, including `CampaignMap_HOS_Static/matrices.medusa` and corresponding Cuanacht data.

This strengthens the map-to-static-landscape relationship but still does not identify which source objects form the continuous base ground.

The current global `StreamingAssets/Medusa/medusa.arch` remains rejected as an editable authoritative terrain source.

### Leshy, DepthTextures and Pathfinding

The existing dispositions remain unchanged:

```text
DepthTextures:
    derived wetness / precipitation / VFX
    not authoritative terrain

Leshy:
    vegetation placement/rendering
    not authoritative terrain

PathfindingCache:
    navigation derivative
    not authoritative terrain
```

### Addressables

FOA's use of Addressables for mesh/material streaming is source-supported, and the public package exposes hashed Addressables bundles.

The semantic mapping required by Highmap remains unknown:

```text
CampaignMap / terrain semantic key
    -> GUID / object ID
    -> bundle
    -> asset type and dependencies
```

### GroundBounds

The supplied static report establishes that `GroundBounds.CalculateGameBounds()` supplies bounds used by the DepthTextures consumer.

Its implementation and backing source remain unknown. No evidence establishes whether those bounds derive from Terrain objects, colliders, authored volumes, ScriptableObject configuration, or another world owner.

## DR-TH-003 field-resolution state

For all four maps, the following are resolved only at map/catalogue level:

```text
public display identity
CampaignMap_* source-scoped map identity
map-level evidence/provenance references
```

The following remain unresolved for the authoritative base terrain:

```text
sourceKind
sourceObjectIdentifier
scene asset path
scene GUID / object IDs
TerrainData GUID / object ID
Terrain count
Terrain transforms
TerrainCollider refs
Terrain neighbours
native width / height
sample spacing
minimum / maximum elevation
tile count and topology
row-zero orientation
sample semantics
source-to-canonical transform
base-heightfield provenance hash
```

No value in this group may be replaced by a guessed Unity default, DepthTextures grid value, Pathfinding extent, or Medusa archive assumption.

## Heightfield fitness

The complete visible/collision CampaignMap world is not safely modelled as one heightfield because FOA has proven mesh-authored static landscape, including cliff-oriented content.

The intended Highmap domain therefore remains the continuous 2.5D base surface only.

Per-map heightfield fitness remains `UNKNOWN` until the base-ground owner is identified.

## What DR-TH-003 closed

The research has closed the broad architectural question as the next useful research target.

Another general survey of Medusa, Leshy, Drake, HLOD, DepthTextures, Pathfinding or generic Unity terrain concepts is unlikely to resolve the blocker.

The blocker is now specifically:

```text
CampaignMap_*
    -> exact serialized scene/source object
    -> TerrainData OR base-ground mesh OR both
```

## Remaining evidence target

The next useful evidence lane is static scene/asset/decompilation evidence focused on source binding, especially:

1. `GroundBounds.CalculateGameBounds()` implementation and callers;
2. all callers/references of `Awaken.TG.EditorOnly.TerrainHeightRemapper` and related editor terrain tooling;
3. CampaignMap scene-loading/dependency code that exposes scene/addressable/source identities;
4. Medusa build/source-selection types, filters and source-object join identifiers;
5. Addressables semantic key/GUID/object mapping for candidate scenes, TerrainData or base-ground meshes;
6. if separately authorised and lawfully available, serialized `CampaignMap_*` scene/component metadata.

Until one of those lanes establishes the source binding:

```text
Per-map authoritative base terrain: UNKNOWN
Deterministic TerrainHeightmapDocumentV1 reconstruction: BLOCKED
Production Edit Vanilla Map provider: BLOCKED
Zero-configuration UX requirement: UNAFFECTED
```

## Authority boundary

This intake records research context only. It does not authorise private-installation inspection, commercial scene/bundle extraction, redistribution of game assets, runtime execution, implementation, source-provider qualification, or evidence promotion.
