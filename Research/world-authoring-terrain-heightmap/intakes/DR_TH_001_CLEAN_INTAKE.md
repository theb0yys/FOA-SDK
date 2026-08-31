# DR-TH-001 Clean Research Intake

Research subject: FOA vanilla terrain candidate under `StreamingAssets/DepthTextures`

Preserved inputs:

- `../inputs/DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md`
- `../inputs/DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md`

Status: `PARTIAL`

Final DepthTextures disposition: `DEPTH_TEXTURES_NOT_AUTHORITATIVE_TERRAIN`

Implementation authority created: none

Runtime validation: `NOT_RUN`

Underlying static analysis independently reproduced in this repository change: `NOT_RUN`

## Accepted intake conclusion

The public package inventory first established map-scoped collections of files using the locator pattern:

```text
StreamingAssets/
  DepthTextures/
    <CampaignMap scene>/
      depth_tex_<x>_<y>.raw
```

The supplied static/CIL report then identifies the consumer chain as:

```text
PrecipitationController
    -> TopDownDepthTexturesLoadingManager
    -> fixed-size raw-byte read
    -> ComputeBuffer
    -> wetnessTexturesArrayDataSetShader
    -> four-layer RenderTexture
    -> ScreenSpaceWetness / VFXTopDownDepthBinder
```

Source state:

- path/inventory: `source-supported` by `SRC-PUB-DEPOT`;
- loader and consumer classification: `static-report-supported` by `SRC-INPUT-DR-TH-001-STATIC`;
- live runtime reproduction: `NOT_RUN`.

No `TerrainData` construction, heightmap population, `SetHeights`, or equivalent terrain-writing operation is
reported in the observed call chain. The DepthTextures system is therefore rejected as the authoritative vanilla
terrain/highmap source.

This conclusion does not establish the original geometry sampled by the bake process and does not establish that
the derived data is or is not mathematically reversible. Those producer-side questions are no longer prerequisites
for identifying the authoritative base-terrain source.

## Resolved claims

| Claim | State |
| --- | --- |
| Map/scene-scoped DepthTextures collections exist | `source-supported` |
| Runtime path construction uses active scene name and `depth_tex_X_Y.raw` | `static-report-supported` |
| Observed consumer is wetness/precipitation/VFX | `static-report-supported` |
| Observed chain constructs authoritative terrain | `contradicted` |
| DepthTextures are the production vanilla Highmap source | `contradicted` |
| Filename coordinates form a zero-based chunk grid | `static-report-supported` |
| Chunking is relative to `GroundBounds.min.xz` | `static-report-supported` |
| Payload size uses `TextureSize² × 4` | `static-report-supported` |
| Bytes per texel are four in the observed consumer | `static-report-supported` |
| Shipped serialized texture dimensions | `unknown` |
| Exact symbolic name of `GraphicsFormat` literal `49` | `unknown` |
| Shader-side stored-depth-to-world-height conversion | `unknown` |
| Original geometry sampled by `TopDownDepthTextureBaker` | `unknown` |
| Lossless reversibility to authoritative terrain | `unknown` |

## Static report observations retained for derived-data work

The supplied report states that `DepthTextureStreamingParams` uses:

```text
TextureSize = chunkTextureSizeInUnits * pixelsPerUnit
TextureSizeInBytes = TextureSize² * 4
```

and that constructor defaults imply:

```text
pixelsPerUnit = 16
chunkTextureSizeInUnits = 128
TextureSize = 2048
expected bytes = 16,777,216
sample spacing = 0.0625 Unity world unit
```

These defaults are not promoted to shipped runtime values because serialized `GameConstants` data may override
public constructor defaults. Public depot size presentation also does not provide sufficient exact-byte evidence
to resolve the shipped values.

The report further states that the top-down camera contract uses:

```text
near = 0.01
far = bounds.max.y - bounds.min.y
cameraY = bounds.max.y
rotation = Quaternion.Euler(90, 0, 0)
```

The actual sample interpretation remains shader-side and unavailable in the supplied evidence.

## Other assembly findings

- `HLOD.dll` is reported to use an independent `StreamingAssets/HLODs/hlods.arch` system and provides no observed
  static connection to DepthTextures or TerrainData.
- The supplied `MeshToTerrain(1).dll` is reported to contain helper/documentation stubs rather than conversion
  implementation.
- `TG.Main(3).dll` is reported to contain `Awaken.TG.EditorOnly.TerrainHeightRemapper`, which reads
  `Terrain.transform.position.y` and `Terrain.terrainData.size.y`.

The final observation establishes project-specific Terrain/TerrainData awareness somewhere in FOA code/tooling.
It does not bind a campaign scene to a TerrainData asset.

## Highmap impact

Rejected route:

```text
DepthTextures -> decode as vanilla terrain -> TerrainHeightmapDocumentV1
```

Required route:

```text
CampaignMap scene
    -> authoritative base-terrain/world-geometry owner
    -> deterministic native source contract
    -> TerrainHeightmapDocumentV1
    -> workspace-owned editable revision
```

The intended zero-configuration UX is unaffected. Unknown technical interpretation fields remain provider-side
blockers and must not be presented as normal user questions.

## Residual DepthTextures research

The following is deferred from the primary Highmap path:

- `TopDownDepthTextureBaker` implementation;
- bake/compute/wetness shader source;
- exact symbolic `GraphicsFormat 49` mapping for the shipped Unity build;
- shipped serialized `DepthTextureStreamingParams`;
- exact encoded depth convention;
- reversibility.

These questions become relevant only if FOA-SDK later needs to regenerate wetness/VFX support data from edited
world geometry.

## Source bindings

Claims in this intake use:

- `SRC-INPUT-DR-TH-001`
- `SRC-INPUT-DR-TH-001-STATIC`
- `SRC-PUB-DEPOT`
- `SRC-PUB-MAPSCENE-LOG`
- `SRC-REPO-TERRAIN-H`
- `SRC-REPO-WA-TH-001`

See `../SOURCE_REGISTER.md` and `../CLAIM_REGISTER.md`.
