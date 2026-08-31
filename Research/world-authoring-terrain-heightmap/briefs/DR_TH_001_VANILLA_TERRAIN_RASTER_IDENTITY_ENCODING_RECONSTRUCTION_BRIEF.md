# DR-TH-001 Research Brief

## Fall of Avalon Vanilla Terrain Raster Identity, Encoding, and Reconstruction Contract

Research ID: `DR-TH-001`

Owner system: `world-authoring / TerrainAuthoring`

Target capability: `Highmap Importer -> Edit Vanilla Map`

Research authority: research only

## Primary question

Determine what Fall of Avalon's public package files under:

```text
StreamingAssets/DepthTextures/CampaignMap_*/depth_tex_X_Y.raw
```

actually represent, how their bytes and spatial coordinates are interpreted, and whether they can serve as the
authoritative source for deterministic editable vanilla terrain.

Do not infer terrain semantics from the directory name, `.raw` extension, approximate size, or visual
plausibility.

## Maps

- `CampaignMap_HOS`
- `CampaignMap_Cuanacht`
- `CampaignMap_Forlorn`
- `CampaignMap_Sarras`

Preserve display identity, source-scoped identifiers, exact native identity, and terrain-source identity as
separate concepts.

## Required research areas

### Source role

Identify the producer and consumer of the files. Test terrain, world-depth, wetness, VFX, navigation, water,
occlusion, vegetation, streaming, physics, map/minimap, and custom-system hypotheses against evidence.

### Binary representation

Establish, if evidence permits:

- sample type and signedness;
- bits and channels per sample;
- byte order;
- exact tile width/height;
- row/storage order;
- header, compression, padding, and sentinel rules;
- normalization.

File-size arithmetic alone is not acceptable proof.

### Vertical mapping

Establish the exact stored-value to Unity world-height formula, including min/max, scale, offset, datum,
quantization, clamping, and any per-map/per-tile variation.

### Horizontal mapping

Establish the meaning of filename X/Y, grid origin, world origin, axis mapping, sample spacing, tile world size,
border overlap, sparse-tile meaning, row-zero orientation, and the tile/sample to Unity world-coordinate formula.

### Identity and system relationships

Investigate relationships to:

- CampaignMap scenes;
- Addressables;
- Leshy;
- PathfindingCache;
- Unity Terrain/TerrainData;
- custom world streaming;
- scene initialization.

### SDK mapping

Map the result to `TerrainHeightmapDocumentV1`, including:

```text
map identity
source kind and object identity
grid dimensions and spacing
sample encoding
vertical range
coordinate basis and transform
tile inventory
provenance
```

Every field must be marked deterministic, source-derived, profile-derived, catalogue-derived, provider-required,
or unknown.

## Required route classification

Choose only with evidence:

- existing canonical RAW/image importer compatible;
- dedicated FOA vanilla source provider required;
- DepthTextures are not authoritative terrain;
- insufficient evidence.

## Zero-configuration constraint

The user must not be asked for byte order, dimensions, tile coordinates, height range, world scale, axis mapping,
row orientation, source transform, bundle IDs, or JSON sidecars. Unknown consequential metadata blocks the
provider.

## Evidence discipline

Label consequential statements as `FACT`, `INFERENCE`, `HYPOTHESIS`, `UNKNOWN`, or `CONTRADICTED`, and preserve
public documentation, package metadata, static/decompilation, host execution, and live-runtime evidence as
separate lanes.

## Protected boundary

This brief does not authorize private-install inspection, commercial asset extraction, game launch, runtime
mutation, redistribution of proprietary content, or repository implementation. See `../SOURCE_REGISTER.md` for
the governing repository sources.

## Required deliverable

- executive route classification;
- per-map inventory;
- exact decoder and reconstruction contract if established;
- vertical and coordinate formulas if established;
- SDK field-resolution matrix;
- evidence register;
- explicit unknowns and follow-up.

## Success condition

The research passes only if the source identity, encoding, tile topology, horizontal spacing/origin, vertical
mapping, orientation, sample semantics, source coordinate basis, canonical transform, and per-map applicability
are deterministic without technical user input. Otherwise the result is partial or blocked.
