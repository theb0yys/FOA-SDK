# DR-TH-000 — Clean Public Reconnaissance Intake

Intake date: 31 August 2026

Source report: `../inputs/DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md`

Disposition: `PARTIAL`

This derivative records the usable pre-Deep-Research findings and their later supersession state. It does not
replace the preserved report and creates no implementation or extraction authority.

## Accepted observations

Public package metadata established map-scoped world-data identifiers and paths for:

```text
CampaignMap_HOS
CampaignMap_Cuanacht
CampaignMap_Forlorn
CampaignMap_Sarras
```

Observed subsystem families included:

```text
DepthTextures
Leshy
PathfindingCache
Addressables
```

DepthTextures contained sparse tiled `.raw` inventories for Cuanacht, Forlorn, and HOS. No corresponding Sarras
DepthTextures directory was observed in the same current listing.

Public runtime logging supported `CampaignMap_Cuanacht` and `CampaignMap_HOS` as map-scene/source-scoped strings.
That evidence did not establish exact terrain asset identity.

## Accepted SDK/O3DE conclusion

The existing FOA-SDK terrain contract requires explicit source, grid, vertical, coordinate, tile, provenance,
and revision metadata.

O3DE projection likewise needs explicit world bounds, sample spacing/query resolution, and vertical min/max.

Therefore a vanilla provider must resolve source interpretation deterministically. A package path and `.raw`
extension are not enough.

## Superseded candidate conclusion

The initial report treated DepthTextures as an unresolved raster candidate and concluded `INSUFFICIENT_EVIDENCE`.

The later static/CIL report superseded the candidate role:

```text
DepthTextures authoritative-terrain hypothesis:
    CONTRADICTED / CLOSED

Observed role:
    wetness / precipitation / VFX top-down depth
```

The package inventory remains valid research context; the terrain-source hypothesis does not.

## Deterministic SDK-owned fields

The reconnaissance established that the following should never be normal user inputs:

```text
workspace/profile binding
operation identity and timestamp
importer identity/version
staging and revision paths
source and tile hashes
map display identity/catalogue aliases
image width/height for inspectable image sources
```

## Provider-blocked fields

For a vanilla source, the following remain provider/evidence responsibilities:

```text
exact source object identity
native dimensions or mesh sampling policy
native encoding
metric sample spacing
minimum/maximum elevation
world origin and tile topology
row orientation
sample semantics
source-to-canonical transform
```

Unknown values remain blockers and must not be moved into the ordinary UI.

## Current status after later research

```text
Public package inventory:
    PASSED

Source-scoped map identity:
    PARTIAL

DepthTextures as terrain:
    FAILED / superseded by static evidence

Campaign world representation:
    MIXED_REPRESENTATION research context

Exact per-map base-ground owner:
    UNKNOWN

Production vanilla Highmap provider:
    BLOCKED

Zero-configuration UX:
    UNAFFECTED
```
