# World Authoring Terrain / Highmap Research

Status: research intake and bounded follow-up brief

Repository intake baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

Observation and intake date: 31 August 2026

## Authority boundary

This topic preserves research reports, static/decompilation observations, cleaned derivatives, claim state, and
bounded follow-up briefs for FOA-SDK Terrain Authoring and the Highmap Importer.

Nothing in this directory is an implementation permit, native-identity promotion, source-provider qualification,
commercial-content extraction permit, runtime-compatibility declaration, deployment authority, publication
permission, or release decision. Normative authority remains in the owning repository architecture, accepted
gates, and reviewed code contracts.

The governing repository sources at the intake baseline are:

- [`AGENTS.md`](https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/AGENTS.md)
- [`Research/README.md`](https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Research/README.md)
- [`docs/protected-files-policy.md`](https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/docs/protected-files-policy.md)
- [`TerrainHeightmapDocument.h`](https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Gems/TaintedGrailModdingSDK/Code/Source/TerrainHeightmapDocument.h)
- [`WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md`](gates/WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md)
- [`WA_TH_002_TERRAIN_AUTHORING_UI_PREVIEW_GATE.md`](gates/WA_TH_002_TERRAIN_AUTHORING_UI_PREVIEW_GATE.md)

## Preserved inputs

The supplied research reports are preserved below `inputs/` as research context:

- [`DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md`](inputs/DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md)
- [`DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md)

The original reports contain conversation-local citation tokens. Those tokens are preserved as part of the input
but are not durable repository citations. Durable source locators and scoped claim states live in
[`SOURCE_REGISTER.md`](SOURCE_REGISTER.md) and [`CLAIM_REGISTER.md`](CLAIM_REGISTER.md).

## Cleaned derivatives

The repository-facing derivatives are:

- [`DR_TH_001_CLEAN_INTAKE.md`](intakes/DR_TH_001_CLEAN_INTAKE.md)
- [`DR_TH_002_CLEAN_INTAKE.md`](intakes/DR_TH_002_CLEAN_INTAKE.md)

These files are derivatives, not replacements for the preserved inputs. They retain evidence-lane distinctions
and remove conversation-local citations from claims relied on by this topic.

## Research briefs

Completed briefs:

- [`DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md`](briefs/DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md)
- [`DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md`](briefs/DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md)

Current next brief:

- [`DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md`](briefs/DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md)

DR-TH-003 is intentionally narrow. It targets only the component and source-asset inventory of the four
`CampaignMap_*` scenes. It does not authorize another broad survey of FOA world systems.

## Current research disposition

The current E1 research context is:

```text
DepthTextures authoritative-terrain hypothesis:
    FAILED

DepthTextures observed consumer:
    wetness / precipitation / VFX top-down depth

Leshy authoritative-terrain hypothesis:
    FAILED

Medusa runtime archive as editable terrain:
    FAILED

Campaign world representation:
    MIXED_REPRESENTATION

Unity Terrain / TerrainData presence somewhere in project tooling/code:
    source-supported by supplied static report

CampaignMap -> exact Terrain/TerrainData or base-ground mesh binding:
    UNKNOWN

Per-map authoritative heightfield source:
    UNKNOWN

Production zero-configuration vanilla Highmap provider:
    BLOCKED
```

The zero-configuration product objective remains unchanged. Unknown source metadata is a provider/research
blocker; it must not become a normal user-facing form.

## Claim states

This topic uses:

- `repository-observed` — exact repository content at the recorded baseline supports the claim;
- `source-supported` — a durable public source in `SOURCE_REGISTER.md` supports the scoped claim;
- `static-report-supported` — the preserved decompilation report supports the claim, but the underlying binaries
  and analysis were not independently reproduced in this repository change;
- `input-observed` — a preserved Deep Research report states the claim, but its underlying citation has not yet
  been fully reconciled to a durable source entry;
- `inference` — a bounded conclusion derived from identified facts;
- `unknown` — required proof is missing;
- `contradicted` — accepted evidence conflicts with the claim;
- `superseded` — a later record replaces the claim.

No evidence state grants implementation, runtime, packaging, deployment, publication, or promotion authority.
