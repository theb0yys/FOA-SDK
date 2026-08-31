# DR-TH-000 — Public Terrain Source Reconnaissance

Observation date: 31 August 2026

Evidence lane: public package metadata, public runtime logs, public engine documentation, and repository contract
comparison

Deep Research: not used for this report

Private installation inspection: not performed

Commercial asset extraction: not performed

Repository mutation during original research: none

This report preserves the public reconnaissance completed before DR-TH-001. It identified candidate FOA world
sources, mapped known metadata to O3DE/FOA-SDK needs, and isolated the initial terrain-format blocker.

## Executive result

The public package layout established several map-scoped world-data systems, but did not establish an exact
terrain decoding contract.

The strongest initial candidate was:

```text
Fall of Avalon_Data/
    StreamingAssets/
        DepthTextures/
            CampaignMap_Cuanacht/
            CampaignMap_Forlorn/
            CampaignMap_HOS/
```

with files named:

```text
depth_tex_<x>_<y>.raw
```

At this stage the files were treated only as a candidate tiled raster source. The report explicitly rejected
inferring dimensions, endian order, height scale, coordinate orientation, or terrain semantics from the `.raw`
extension or approximate file size.

Later static evidence superseded the candidate role and classified DepthTextures as wetness/VFX data. This
preserved report remains useful for its package inventory, SDK field analysis, and research chronology.

## Public package layout

Public depot metadata exposed a Unity StreamingAssets structure containing multiple independent world systems.

Observed candidate paths included:

```text
StreamingAssets/
├── DepthTextures/
│   ├── CampaignMap_Cuanacht/
│   ├── CampaignMap_Forlorn/
│   └── CampaignMap_HOS/
├── Leshy/
│   ├── CampaignMap_Cuanacht_Static/
│   ├── CampaignMap_Forlorn_Static/
│   ├── CampaignMap_HOS_Static/
│   └── CampaignMap_Sarras_Static/
├── PathfindingCache/
│   ├── CampaignMap_Cuanacht.bytes
│   ├── CampaignMap_Forlorn.bytes
│   ├── CampaignMap_HOS.bytes
│   └── CampaignMap_Sarras.bytes
└── aa/
    ├── AddressablesLink/link.xml
    └── StandaloneWindows64/<hash>.bundle
```

This established that the playable world is distributed across several map-related systems rather than one
obvious heightmap directory.

## Initial DepthTextures inventory

The public listing showed map-scoped tiled raw collections for:

- `CampaignMap_Cuanacht`;
- `CampaignMap_Forlorn`;
- `CampaignMap_HOS`.

No equivalent `CampaignMap_Sarras` directory was observed in the same current DepthTextures block.

The initial report derived the following manifest inventory:

| Map key | Observed files | Observed filename-coordinate bounds | Topology observation |
| --- | ---: | --- | --- |
| `CampaignMap_Cuanacht` | 316 | X `0..25`, Y `0..16` | sparse |
| `CampaignMap_Forlorn` | 399 | X `0..23`, Y `0..23` | sparse |
| `CampaignMap_HOS` | 148 | X `0..13`, Y `0..13` | sparse |
| `CampaignMap_Sarras` | none observed | N/A | source route unknown |

The counts and bounds were manifest-derived observations/inferences, not developer-published format metadata.

The sparse coordinate sets could not establish missing-tile meaning. Missing coordinates could represent ocean,
non-covered world space, subsystem-specific omission, or another condition.

## Map identity evidence

Public runtime logging included map-scene events for:

```text
CampaignMap_Cuanacht
CampaignMap_HOS
```

This supported treating those strings as source-scoped map-scene identities.

Forlorn and Sarras were publicly visible as map-scoped keys in package systems such as PathfindingCache/Leshy,
but equivalent direct runtime MapScene evidence was not established during this reconnaissance.

The report therefore preserved the distinction between:

```text
public display name
source-scoped CampaignMap key
exact native scene identity
exact terrain source-object identity
```

## Initial representation model

The package evidence supported this broad model:

```text
CampaignMap region
│
├── Unity/Addressables scene and asset products
├── Leshy static data
├── Pathfinding cache
├── candidate DepthTextures tiled raster
└── other streamed/baked world data
```

The semantic relationship between those systems remained unknown at this stage.

## Binary-format findings at reconnaissance stage

Only the following properties were public observations:

```text
extension: .raw
map-scoped directory
integer X/Y filename pair
approximately uniform displayed file size
```

The following remained unknown and were explicitly not guessed:

```text
exact byte count
header presence
compression
sample type
bits per sample
signedness
endianness
channels
width
height
row order
padding
sentinel values
normalisation
```

The report rejected file-size arithmetic as proof of format.

## World and vertical mapping findings

The package listing did not establish:

```text
filename X axis meaning
filename Y axis meaning
tile world origin
tile physical size
sample spacing
tile overlap or duplicated borders
row-zero orientation
grid-vertex versus cell-centre semantics
stored value meaning
minimum elevation
maximum elevation
vertical scale or offset
```

Consequently no valid transform from:

```text
(tile X, tile Y, sample i, sample j, stored value)
```

to:

```text
Unity world X/Y/Z
```

could be produced.

## Unity and O3DE context

Unity documentation supplied the generic source-engine basis:

```text
left-handed
+X right
+Y up
+Z forward
metre-scale world units by convention
```

Unity TerrainData documentation established what could become available if a campaign TerrainData binding were
proved:

```text
heightmapResolution
heightmapScale
TerrainData.size
normalised height samples
Terrain transform position
```

The reconnaissance did not prove that DepthTextures or any CampaignMap terrain used TerrainData.

O3DE terrain documentation established that reconstruction needs explicit:

```text
terrain horizontal extent
terrain vertical min/max
height sample spacing / query resolution
source placement and orientation
height-data source
```

Therefore an unknown FOA source could not be projected correctly by loading arbitrary grayscale values.

## FOA-SDK field analysis

The report compared the unresolved candidate with `TerrainHeightmapDocumentV1`.

### Fields already deterministic from SDK state

```text
workspace root
active profile ID
game version
branch
runtime target
operation ID
created-at timestamp
importer ID/version
staging paths
revision paths
source hash at import time
tile hashes after canonicalisation
```

### Fields deterministic for normal image imports

```text
source image kind
image width
image height
source byte size
```

The existing image importer already derived dimensions directly from the source image.

### Fields expected from a vanilla catalogue/provider

```text
map ID
display name
public aliases
source-scoped map reference
source locator
source object identifier
configuration fingerprint
```

### Fields blocked pending exact FOA source contract

```text
native width/height
sample type and bit depth
byte order
metric sample spacing
minimum/maximum elevation
handedness as applied to raster
up/forward axes as applied to raster
row-zero orientation
sample position semantics
source-to-canonical transform
tile topology
```

The design conclusion was that blocked provider fields must not become a user form.

## Initial provider classification

The reconnaissance defined four possible outcomes:

```text
A. Existing canonical importer directly compatible
B. New FOA source provider required
C. Candidate source is not authoritative terrain
D. Insufficient evidence
```

At this stage the correct result was:

```text
D — insufficient evidence
```

Later DR-TH-001 static evidence changed the specific DepthTextures branch to:

```text
C — not authoritative terrain
```

## Initial critical blocker

The report narrowed the first Deep Research question to:

> Establish what `StreamingAssets/DepthTextures/CampaignMap_*/depth_tex_X_Y.raw` contains, which subsystem owns it,
> and whether it is genuine terrain data or a derived representation.

Required proof included:

```text
consumer/producer system
encoding
tile dimensions
tile coordinate convention
overlap
world spacing
world origin
vertical mapping
row orientation
sample semantics
relationship to CampaignMap scene terrain
```

## Zero-configuration conclusion

The public reconnaissance supported the product design but not a vanilla provider.

The correct user-visible behavior remained:

```text
Edit Vanilla Map
    -> select map
    -> SDK resolves source contract
```

not:

```text
select map
    -> enter dimensions/endian/scale/range/axes/transform
```

## Original disposition

```text
Public package inventory: PASSED
Map-scoped source references: PARTIAL
O3DE reconstruction requirements: PASSED
SDK deterministic-field analysis: PASSED
DepthTextures semantic role: UNKNOWN at this stage
Exact binary decoder: BLOCKED
World-space reconstruction: BLOCKED
Sarras equivalent source: BLOCKED
Production vanilla provider: BLOCKED
```

## Supersession note

This report is preserved as prior research. Its DepthTextures semantic uncertainty was later superseded by the
user-supplied static/CIL report, which identified the observed consumer as wetness/precipitation/VFX and
contradicted the authoritative-terrain hypothesis.
