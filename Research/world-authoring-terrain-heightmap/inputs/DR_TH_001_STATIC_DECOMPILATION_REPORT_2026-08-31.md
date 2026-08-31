Fulfilled. This is written so it can be handed directly to the other agent; evidence states are deliberately separated from inference and runtime proof.

# DR-TH-001 — Static / Decompilation Evidence Report

Execution status: PASSED  
Evidence lane: static assembly metadata + CIL decompilation/disassembly  
Runtime validation: NOT_RUN  
Live installation inspection: NOT_RUN  
Archive/raw-payload inspection: NOT_RUN  
Repository mutation: NOT_APPLICABLE

C#-style pseudocode below is reconstructed from exact CIL instructions. Numeric Unity enum literals are preserved where the defining Unity assembly was not supplied; symbolic enum names are not guessed.

## Result

`DEPTH_TEXTURES_NOT_AUTHORITATIVE_TERRAIN`

Confidence: FACT for the runtime consumer classification.

The supplied `TG.Main(3).dll` exposes a complete DepthTextures loading chain. The `.raw` data is streamed into a four-layer GPU `RenderTexture` through a compute shader explicitly stored as `wetnessTexturesArrayDataSetShader`. Its direct consumers are `ScreenSpaceWetness` and `VFXTopDownDepthBinder`, and loading is enabled by `PrecipitationController` according to precipitation intensity.

No `TerrainData`, `SetHeights`, heightmap construction, `Terrain.CreateTerrainGameObject`, or equivalent terrain-writing API occurs anywhere in that call chain.

This does not prove what source was used to bake the files. They could still have been generated from terrain, meshes, scene geometry, or another depth representation. The player assembly contains only a stubbed `TopDownDepthTextureBaker` type, so provenance and reversibility remain unresolved.

---

## Assemblies

| Assembly | SHA-256 |
| --- | --- |
| `TG.Main(3).dll` | `749aabbfbec121bb69bda0ae226223154406d2c990df3312ad12365d513fa982` |
| `HLOD.dll` | `32bfbf78eb0c8359d81ca1234a1802a2a9627448d5c794af46d3f3b21b7bdd42` |
| `MeshToTerrain(1).dll` | `eb4f02ba366dec1e78fe16b2ab4607bfb0e5c193d816ed6296d44a4fb21fd2b3` |

---

## 1. Decisive DepthTextures call chain

### ASSEMBLY

`TG.Main(3).dll`

### PRIMARY TYPE

`Awaken.TG.Graphics.VFX.TopDownDepthTexturesLoadingManager`

### CALL CHAIN

```text
Awaken.TG.Graphics.PrecipitationController.Update()
    ↓
TopDownDepthTexturesLoadingManager.SetDepthTexturesLoadingEnabled(
    rain/precipitation intensity > 0.05f
)
TopDownDepthTexturesLoadingManager.Start()
    ↓
ResetState()
    ↓
InitializeConstantData()
    ↓
GroundBounds.CalculateGameBounds()
    ↓
SetConstantParams(bounds)
    ↓
new RenderTexture(
    ChunkTextureSize,
    ChunkTextureSize,
    0,
    (UnityEngine.Experimental.Rendering.GraphicsFormat)49
)
    ↓
dimension = enum literal 5
volumeDepth = 4
enableRandomWrite = true
Start()
    ↓
_mainSceneName = gameObject.scene.name
    ↓
for every calculated chunk:
    File.Exists(GetTextureFullPath(_mainSceneName, GetChunkCoord(index)))
GetTextureFullPath(scene, coord)
    ↓
GetTexturesDirectory(scene)
    ↓
Application.streamingAssetsPath
    ↓
"DepthTextures"
    ↓
scene name
    ↓
"depth_tex_{0}_{1}.raw"
    ↓
Path.Combine(...)
UnityUpdate()
    ↓
UpdateChunksLoading()
    ↓
StartLoadingChunkFromDisk(...)
    ↓
Awaken.Utility.Files.FileRead.ToNewBufferAsync<byte>(
    fullPath,
    0,
    _chunkTextureSizeInBytes,
    allocator literal 4,
    out UnsafeArray<byte>,
    out UnsafeArray<ReadCommand>
)
ProcessLoadingChunkFromDisk(...)
    ↓
StartLoadingToGpu(...)
    ↓
PrepareComputeShaderCommandsAndBuffer(...)
    ↓
wetnessTexturesArrayDataSetShader.FindKernel(
    "CopyDataBufferToTexturesArray"
)
    ↓
new ComputeBuffer(byteLength / 4, 4)
    ↓
Compute Shader writes into DepthTexturesArray RenderTexture
    ↓
ScreenSpaceWetness.Execute(...)
VFXTopDownDepthBinder.UpdateBinding(...)
```

### WHAT THIS PROVES

FACT: the runtime purpose of these files is a streamed top-down depth representation consumed by wetness/VFX rendering.

FACT: this is not a `TerrainData` construction path.

---

## 2. Exact path construction

### TYPE

`Awaken.TG.Graphics.VFX.TopDownDepthTexturesLoadingManager`

### METHOD

`GetTexturesDirectory(string mapSceneName)`

Relevant exact CIL semantics:

```text
directoryPath = Path.Combine(
    Application.streamingAssetsPath,
    "DepthTextures",
    mapSceneName
);
```

Literal constant:

```text
TexturesDirectoryInStreamingAssets = "DepthTextures"
```

### METHOD

`GetTextureFullPath(string mapSceneName, Unity.Mathematics.int2 chunkCoord)`

Equivalent pseudocode:

```text
return Path.Combine(
    GetTexturesDirectory(mapSceneName),
    string.Format(
        "depth_tex_{0}_{1}.raw",
        chunkCoord.x,
        chunkCoord.y
    )
);
```

### Path contract

```text
<StreamingAssets>/
    DepthTextures/
        <active scene name>/
            depth_tex_<x>_<y>.raw
```

Confidence: FACT

---

## 3. Reader and raw-file contract

### METHOD

`StartLoadingChunkFromDisk(...)`

The depth loader does not use:

```text
File.ReadAllBytes
FileStream
BinaryReader
Texture2D.LoadRawTextureData
```

Instead it calls:

```text
Awaken.Utility.Files.FileRead.ToNewBufferAsync<byte>
```

with:

```text
path       = GetTextureFullPath(...)
offset     = 0
byteCount  = _chunkTextureSizeInBytes
allocator  = numeric literal 4
```

and receives:

```text
UnsafeArray<byte> textureData
Unity.IO.LowLevel.Unsafe.ReadHandle
UnsafeArray<Unity.IO.LowLevel.Unsafe.ReadCommand>
```

The implementation of `Awaken.Utility.Files.FileRead` is in another assembly and was therefore not decompiled here.

### FILE-SIZE VALIDATION

`<Start>g__AreTexturesFilesValid|102_0()` performs:

```text
FileRead.GetFileInfo(firstExistingDepthTexture).FileSize
    == _chunkTextureSizeInBytes
```

If the size differs, the loader logs:

```text
Wetness Depth textures size does not match texture size in game constants.
You need to bake wetness depth textures using TopDownDepthTextureBaker
```

and destroys the manager.

### WHAT THIS PROVES

FACT: the entire file is treated as a fixed-size raw payload beginning at byte offset zero.

There is no allowance in this consumer for a header, metadata block, trailer, or variable payload size.

`Texture2D.LoadRawTextureData` does exist elsewhere in `TG.Main(3).dll`, but its only located caller is:

```text
Awaken.TG.Main.SocialServices.SteamServices.SteamLeaderboardEntry::GetAvatar(...)
```

It is unrelated to DepthTextures.

---

## 4. Width, height and expected byte count

### TYPE

`Awaken.TG.Graphics.VFX.DepthTextureStreamingParams`

### METHOD

`get_TextureSize()`

Exact formula:

```text
TextureSize =
    chunkTextureSizeInUnits
    * pixelsPerUnit;
```

### METHOD

`GetTextureSizeInBytes(int textureSize)`

Exact formula:

```text
return square(textureSize) * 4;
```

Therefore:

```text
width  = TextureSize
height = TextureSize
expectedByteCount =
    width
    × height
    × 4
```

### Default constructor values

`DepthTextureStreamingParams.Default` initializes:

```text
pixelsPerUnit                       = 16
chunkTextureSizeInUnits             = 128
smoothingAreaRadiusInUnits          = 1.0
heightDiffThreshold                 = 0.2
maxHeightDiff                       = 5.0
textureDataMaxBytesToCopyPerFrame   = 262144
```

The default therefore evaluates to:

```text
width  = 128 × 16 = 2048
height = 2048
bytes = 2048 × 2048 × 4
      = 16,777,216 bytes
```

Default spatial sampling:

```text
16 pixels / world unit
= 0.0625 world units / pixel
```

Default tile span:

```text
128 world units
```

### IMPORTANT QUALIFICATION

`GameConstants.depthTextureStreamingParams` is a public serializable field. `GameConstants::.ctor()` initializes it with `DepthTextureStreamingParams.Default`, but a Unity `ScriptableObject` asset can override serialized public values.

Therefore:

- FACT: the formulas above.
- FACT: the C# constructor defaults are 2048 × 2048, 4 bytes/texel, 16 MiB/file.
- UNKNOWN: whether the shipped serialized `GameConstants` asset overrides those defaults.

The assembly alone is insufficient to promote 2048 × 2048 to the installed game’s guaranteed runtime value.

---

## 5. Texture / sample format

### METHOD

`TopDownDepthTexturesLoadingManager.InitializeConstantData()`

Creates:

```text
new RenderTexture(
    ChunkTextureSize,
    ChunkTextureSize,
    0,
    (UnityEngine.Experimental.Rendering.GraphicsFormat)49
);
```

It then configures:

```text
Texture.dimension      = enum literal 5
enableRandomWrite      = true
filterMode             = enum literal 0
wrapMode               = enum literal 0
volumeDepth            = 4
RenderTexture.Create()
```

The upload path creates:

```text
new ComputeBuffer(
    textureByteLength / 4,
    4
);
```

### Proven format properties

FACT:

```text
4 bytes per texel
32 bits per texel
square texture
GPU RenderTexture destination
4 texture-array layers
GraphicsFormat numeric value = 49
```

UNKNOWN:

The symbolic Unity enum name corresponding to `GraphicsFormat` value `49`.

The defining `UnityEngine.CoreModule.dll` for this exact build was not supplied, so this report deliberately does not convert literal `49` into `R32_*`, `RFloat`, or another symbolic name.

Do not promote `R32_SFloat` or any similar symbolic interpretation to FACT from this report alone.

---

## 6. Tile X/Y calculation

### METHOD

`GetChunkIndex(int x, int y)`

Exact formula:

```text
index =
    y * _chunksMaxCountXY.x
    + x;
```

Out-of-range indices become:

```text
-1
```

### METHOD

`GetChunkCoord(int index)`

Exact inverse:

```text
y = index / _chunksMaxCountXY.x;
x = index % _chunksMaxCountXY.x;
return new int2(x, y);
```

Therefore the filename:

```text
depth_tex_X_Y.raw
```

uses the same zero-based two-dimensional grid coordinates.

Confidence: FACT

---

## 7. Grid/world-space calculation

`SetConstantParams(Bounds bounds)` establishes:

```text
_gameBounds2d = new MinMaxAABR(
    bounds.min.xz,
    bounds.max.xz
);
_chunkTextureSizeInUnits =
    depthTextureStreamingParams.chunkTextureSizeInUnits;
_textureSizeInUnitsRcp =
    1.0f / _chunkTextureSizeInUnits;
_chunksMaxCountXY =
    (int2)ceil(
        _gameBounds2d.Extents
        * _textureSizeInUnitsRcp
    );
```

During `UnityUpdate()`:

```text
heroWorldXZ = Hero.Current.Coords.xz;
heroPosInGameBoundsSpace =
    heroWorldXZ
    - _gameBounds2d.min;
```

Chunk coordinates used for loading are then derived by:

```text
floor(
    relativeXZ
    * (1.0f / chunkTextureSizeInUnits)
);
```

### Filename coordinate semantics

The indices are therefore an X/Z grid anchored to the minimum X/Z corner of `GroundBounds`.

Equivalent spatial interpretation:

```text
tile (0,0):
    starts at gameBounds.min.xz
tile (x,y):
    offset from gameBounds.min.xz by approximately
    x * chunkTextureSizeInUnits
    y * chunkTextureSizeInUnits
```

The latter world-origin expression is an INFERENCE from the exact relative-coordinate/grid arithmetic; no method explicitly constructs that final world-origin vector.

---

## 8. Top-down camera / vertical contract

`SetConstantParams(Bounds bounds)` records:

```text
NearPlane = 0.01f
FarPlane =
    bounds.max.y
    - bounds.min.y
CameraWorldPosY =
    bounds.max.y
```

The top-down orientation is statically initialized as:

```text
CameraRotation =
    Quaternion.Euler(
        90.0f,
        0.0f,
        0.0f
    );
```

During `UnityUpdate()` the virtual camera transform is constructed at:

```text
new Vector3(
    Hero.Current.Coords.x,
    CameraWorldPosY,
    Hero.Current.Coords.z
);
```

with the 90° X rotation.

The system generates both:

```text
WorldToCameraMatrix
WorldToCameraMatrixFlippedZ
CameraProjectionMatrix
CameraViewToClipMatrix
```

and passes the following to the wetness shader:

```text
_DepthTexturesArray
_DepthTexturesLayers
_DepthTexBottomLeftUVOffset
_DepthTexBottomRightUVOffset
_DepthTexTopLeftUVOffset
_DepthTexTopRightUVOffset
_DepthTexturesUVInvScale
_TopDown_NearPlane
_TopDown_FarPlane
_TopDown_YCamera
_MaxHeightDiff
_TopDown_VP
_TopDown_V
```

### Vertical interpretation

There is no C# formula in the supplied chain equivalent to:

```text
height = sample * scale + offset
```

or:

```text
worldY = ...
```

The raw depth value is interpreted by GPU shader code.

That shader code is not present in the supplied assemblies.

Therefore:

```text
Vertical sample → world height: UNKNOWN
```

What is known:

```text
near               = 0.01
far                = bounds.max.y - bounds.min.y
top-down camera Y  = bounds.max.y
```

and those values are explicitly supplied to the wetness shader.

---

## 9. Actual consumer

### TYPE

`Awaken.TG.Graphics.VFX.ScreenSpaceWetness`

### METHOD

`Execute(UnityEngine.Rendering.HighDefinition.CustomPassContext)`

It obtains:

```text
TopDownDepthTexturesLoadingManager.DepthTexturesArray
TopDownDepthTexturesLoadingManager.DepthTexturesLayers
TopDownDepthTexturesLoadingManager.Tex*UVOffset
TopDownDepthTexturesLoadingManager.DepthTextureRcpUVScale
TopDownDepthTexturesLoadingManager.NearPlane
TopDownDepthTexturesLoadingManager.FarPlane
TopDownDepthTexturesLoadingManager.CameraWorldPosY
TopDownDepthTexturesLoadingManager.MaxHeightDiff
TopDownDepthTexturesLoadingManager.CameraViewProjectionMatrix
TopDownDepthTexturesLoadingManager.WorldToCameraMatrixFlippedZ
```

places them in a `MaterialPropertyBlock`, and ends by calling:

```text
UnityEngine.Rendering.CoreUtils.DrawFullScreen(
    commandBuffer,
    wetnessMaterial,
    properties,
    ...
);
```

Its static shader-property names explicitly include:

```text
_RainIntensity
_Moisture
_DepthTexturesArray
_DepthTexturesLayers
_DepthTexBottomLeftUVOffset
_DepthTexBottomRightUVOffset
_DepthTexTopLeftUVOffset
_DepthTexTopRightUVOffset
_DepthTexturesUVInvScale
_MaxHeightDiff
_TopDown_VP
_TopDown_V
_TopDown_NearPlane
_TopDown_FarPlane
_TopDown_YCamera
```

### Secondary consumer

`Awaken.TG.Graphics.VFX.Binders.VFXTopDownDepthBinder.UpdateBinding(VisualEffect)`

passes the same depth texture array, layer IDs, UV offsets, near/far values and matrices into a Unity `VisualEffect`.

### Activation

`Awaken.TG.Graphics.PrecipitationController.Update()` contains:

```text
topDownDepthTexturesLoadingManager
    .SetDepthTexturesLoadingEnabled(
        precipitationIntensity > 0.05f
    );
```

### WHAT THIS PROVES

The runtime consumer role is wetness / precipitation / VFX rendering.

Confidence: FACT

---

## 10. TopDownDepthTextureBaker

The type exists:

```text
Awaken.TG.Graphics.VFX.TopDownDepthTextureBaker
```

Source-path metadata references:

```text
Assets\Code\Graphics\VFX\TopDownDepthTextureBaker.cs
```

However, in the supplied player assembly its only surviving method is:

```text
.ctor()
```

No bake implementation survives in this DLL.

Consequently the following remain UNKNOWN:

```text
original geometry source
whether Unity Terrain was sampled
whether meshes were sampled
exact bake shader
exact encoded depth convention
sample-to-world-height inversion
whether the raw representation is losslessly reversible
```

---

## 11. Map-specific behavior

The loader itself contains no branch or switch for:

```text
CampaignMap_HOS
CampaignMap_Cuanacht
CampaignMap_Forlorn
CampaignMap_Sarras
```

It simply uses:

```text
_mainSceneName =
    gameObject.scene.name;
```

and inserts that value into:

```text
StreamingAssets/DepthTextures/<scene name>/
```

The `CampaignMap_*` literals do exist elsewhere in `TG.Main(3).dll`, including portal, preset-selection, patching and scene-classification code, but they are not referenced by `TopDownDepthTexturesLoadingManager`.

Therefore:

FACT: no map-specific raw format or loader algorithm exists in this class.

FACT: chunk-grid dimensions are calculated from the active scene’s `GroundBounds`.

INFERENCE: assuming those campaign scene names are the manager’s active scene names, their paths would be:

```text
StreamingAssets/DepthTextures/CampaignMap_HOS/
StreamingAssets/DepthTextures/CampaignMap_Cuanacht/
StreamingAssets/DepthTextures/CampaignMap_Forlorn/
StreamingAssets/DepthTextures/CampaignMap_Sarras/
```

No evidence in this assembly establishes differing channel formats, resolutions, byte layouts, or vertical encodings between those maps.

---

## 12. HLOD.dll

### ASSEMBLY

`HLOD.dll`

No hits were found for:

```text
DepthTextures
depth_tex_
TerrainData
heightmap
heightMap
SetHeights
SetHeightsDelayLOD
Texture2D
RenderTexture
LoadRawTextureData
PathfindingCache
```

Its StreamingAssets usage is independent.

### TYPE

`Unity.HLODSystem.Streaming.HLODLoadManager`

### METHOD

`InitEntitiesData()`

Relevant semantics:

```text
_basePath = BakingDirectoryPath;
ArchiveUtils.TryMountAndAdjustPath(
    "HLOD",
    "HLODs",
    "hlods.arch",
    ref _basePath
);
```

Failure logging constructs:

```text
Path.Combine(
    Application.streamingAssetsPath,
    "HLODs",
    "hlods.arch"
);
```

### WHAT THIS PROVES

`HLOD.dll` has a separate HLOD archive/streaming system.

There is no static call-chain connection in this assembly between HLOD streaming and DepthTextures.

Confidence: FACT

---

## 13. MeshToTerrain(1).dll

### ASSEMBLY

`MeshToTerrain(1).dll`

This assembly contains only:

```text
8 TypeDefs
4 MethodDefs
```

The meaningful package types are:

```text
InfinityCode.MeshToTerrain.MeshToTerrainBoundsHelper
InfinityCode.MeshToTerrain.MeshToTerrainDocumentation
```

### TYPE

`MeshToTerrainBoundsHelper`

Fields:

```text
OnBoundChanged
OnDestroyed
bounds
```

Its only method is an empty constructor:

```text
MeshToTerrainBoundsHelper()
{
    base(); // MonoBehaviour
}
```

### TYPE

`MeshToTerrainDocumentation`

Its only method is likewise an empty `ScriptableObject` constructor.

The assembly references:

```text
netstandard
UnityEngine.CoreModule
```

It does not reference `UnityEngine.TerrainModule`.

No hits exist for:

```text
TerrainData
SetHeights
SetHeightsDelayLOD
heightmapResolution
heightmapScale
TerrainCollider
Terrain.CreateTerrainGameObject
SampleHeight
Raycast
resolution
Texture2D
RenderTexture
```

### WHAT THIS PROVES

The actual Mesh To Terrain conversion implementation is not present in this supplied DLL.

This file cannot establish FOA’s terrain conversion pipeline.

Confidence: FACT

Why the implementation is absent is UNKNOWN; this report does not infer stripping, packaging decisions, editor-only separation, or another assembly without evidence.

---

## 14. Other Unity Terrain evidence in TG.Main

A targeted scan of `TG.Main(3).dll` found no references to:

```text
TerrainData.SetHeights
SetHeightsDelayLOD
heightmapResolution
heightmapScale
Terrain.CreateTerrainGameObject
Terrain.SampleHeight
PathfindingCache
```

A small amount of ordinary Unity Terrain interaction does exist.

Most relevant:

```text
Awaken.TG.EditorOnly.TerrainHeightRemapper
```

Its getters do:

```text
CurrentLow =
    Terrain.transform.position.y;
CurrentHigh =
    CurrentLow
    + Terrain.terrainData.size.y;
```

This proves that Unity Terrain / TerrainData objects are known to game/editor code.

It does not connect those objects to DepthTextures and does not construct or mutate their heightmap.

---

## 15. Evidence matrix

| Evidence | Result | Confidence |
| --- | --- | --- |
| Assembly | `TG.Main(3).dll` | FACT |
| Loader type | `Awaken.TG.Graphics.VFX.TopDownDepthTexturesLoadingManager` | FACT |
| Directory | `StreamingAssets/DepthTextures/<scene>/` | FACT |
| Filename | `depth_tex_{x}_{y}.raw` | FACT |
| Reader | `Awaken.Utility.Files.FileRead.ToNewBufferAsync<byte>` | FACT |
| File offset | `0` | FACT |
| Expected bytes | `TextureSize² × 4` | FACT |
| Raw header | No header allowance in consumer; whole file is payload | FACT |
| Width/height formula | `chunkTextureSizeInUnits × pixelsPerUnit` | FACT |
| Constructor defaults | `2048 × 2048` | FACT |
| Shipped serialized dimensions | Not established from DLL alone | UNKNOWN |
| Bytes/texel | `4 / 32 bits` | FACT |
| Destination format | `(GraphicsFormat)49` | FACT |
| Symbolic format name | Not recoverable from supplied DLL | UNKNOWN |
| GPU object | four-layer `RenderTexture` array | FACT |
| X/Y indexing | `index = y * width + x`; inverse `%` and `/` | FACT |
| Grid anchor | `GroundBounds.min.xz` relative-space math | FACT |
| Default tile spacing | `128 world units` | FACT for constructor default |
| Default sample spacing | `1/16 = 0.0625 world unit` | FACT for constructor default |
| Near plane | `0.01f` | FACT |
| Far plane | `bounds.max.y - bounds.min.y` | FACT |
| Top-down camera Y | `bounds.max.y` | FACT |
| Sample → world-height formula | Shader-side, unavailable | UNKNOWN |
| Main consumer | `ScreenSpaceWetness` | FACT |
| Secondary consumer | `VFXTopDownDepthBinder` | FACT |
| Weather coupling | enabled by precipitation controller | FACT |
| Terrain construction consumer | none in this chain | FACT |
| Map-specific loader differences | none | FACT |
| HLOD relationship | independent `HLODs/hlods.arch` system | FACT |
| MeshToTerrain conversion implementation | absent from supplied DLL | FACT |
| Original bake source | unresolved | UNKNOWN |
| Reversibility into authoritative terrain | unresolved | UNKNOWN |

---

## Final disposition

The current static evidence changes the central DR-TH-001 question substantially.

The `.raw` files are not an authoritative Unity terrain representation consumed to construct terrain.

They are streamed top-down depth textures used by the game’s wetness/precipitation/VFX rendering systems.

Therefore the appropriate state is:

```text
DEPTH_TEXTURES_NOT_AUTHORITATIVE_TERRAIN
```

This does not establish:

```text
DEPTH_TEXTURES_CONFIRMED_DERIVED_BUT_REVERSIBLE
```

because the bake implementation and shader-side depth decoding formula are still unavailable.

The remaining unresolved evidence is specifically the producer side, not the runtime consumer side:

```text
TopDownDepthTextureBaker implementation
baking shader / compute shader source
GraphicsFormat 49 enum definition for the exact Unity build
wetness/depth sampling shader
original geometry source used during baking
```

Those are required before any claim that the depth files can be inverted into authoritative world terrain should be promoted beyond UNKNOWN.

I can also condense this into a claim-by-claim RH0/RH1 intake format without changing the evidence state.
