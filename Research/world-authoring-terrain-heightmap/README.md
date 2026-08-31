# World Authoring Terrain / Highmap Research

Status: research intake and bounded follow-up evidence planning

Repository intake baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

Observation and intake date: 31 August 2026

## Authority boundary

This topic preserves product-design context, repository assessments, public reconnaissance, Deep Research reports,
static/decompilation observations, cleaned derivatives, claim state, and bounded follow-up briefs for FOA-SDK
Terrain Authoring and the Highmap Importer.

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

## Preserved pre-Deep-Research records

The complete conversation research sequence began before DR-TH-001. These records preserve that earlier work:

- [`HIGHMAP_IMPORTER_ZERO_CONFIGURATION_DESIGN_BASELINE_2026-08-31.md`](inputs/HIGHMAP_IMPORTER_ZERO_CONFIGURATION_DESIGN_BASELINE_2026-08-31.md)
- [`HIGHMAP_IMPORTER_SDK_ACCOMMODATION_ASSESSMENT_2026-08-31.md`](inputs/HIGHMAP_IMPORTER_SDK_ACCOMMODATION_ASSESSMENT_2026-08-31.md)
- [`DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md`](inputs/DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md)

The design baseline is product context rather than evidence that FOA uses a particular source representation. The
SDK assessment is repository/static evidence bound to the intake baseline. DR-TH-000 is the public reconnaissance
that identified package systems and isolated the original terrain-source blocker before Deep Research.

## Preserved Deep Research and static inputs

The supplied/returned reports are preserved below `inputs/` as research context:

- [`DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md`](inputs/DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md)
- [`DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_003_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_003_DEEP_RESEARCH_REPORT_2026-08-31.md)

The original reports may contain conversation-local citation tokens. Those tokens are preserved as part of the
input but are not durable repository citations. Durable source locators and scoped claim states live in
[`SOURCE_REGISTER.md`](SOURCE_REGISTER.md) and [`CLAIM_REGISTER.md`](CLAIM_REGISTER.md).

## Cleaned derivatives

The repository-facing derivatives are:

- [`DR_TH_000_PUBLIC_RECONNAISSANCE_CLEAN_INTAKE.md`](intakes/DR_TH_000_PUBLIC_RECONNAISSANCE_CLEAN_INTAKE.md)
- [`DR_TH_001_CLEAN_INTAKE.md`](intakes/DR_TH_001_CLEAN_INTAKE.md)
- [`DR_TH_002_CLEAN_INTAKE.md`](intakes/DR_TH_002_CLEAN_INTAKE.md)
- [`DR_TH_003_CLEAN_INTAKE.md`](intakes/DR_TH_003_CLEAN_INTAKE.md)

These files are derivatives, not replacements for the preserved inputs. They retain evidence-lane distinctions
and remove conversation-local citations from claims relied on by this topic.

## Research briefs

Completed briefs:

- [`DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md`](briefs/DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md)
- [`DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md`](briefs/DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md)
- [`DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md`](briefs/DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md)

No DR-TH-004 brief is committed by the DR-TH-003 intake step. The next research question should be derived only
from the remaining source-binding unknowns recorded below.

## Research chronology

```text
zero-configuration Highmap design
    -> repository SDK accommodation assessment
    -> DR-TH-000 public package/source reconnaissance
    -> DR-TH-001 Deep Research brief and report
    -> DR-TH-001 static/CIL report
    -> DR-TH-001 clean intake
    -> DR-TH-002 Deep Research brief and report
    -> DR-TH-002 clean intake
    -> DR-TH-003 CampaignMap scene/source-asset inventory brief and report
    -> DR-TH-003 clean intake
    -> remaining static source-binding evidence
```

## Current research disposition

The current research context is:

```text
Highmap product objective:
    zero-configuration normal workflow

Existing canonical terrain backend:
    substantial and reusable

DepthTextures authoritative-terrain hypothesis:
    FAILED

DepthTextures observed consumer:
    wetness / precipitation / VFX top-down depth

Leshy authoritative-terrain hypothesis:
    FAILED

Medusa runtime archive as editable terrain:
    FAILED

Campaign world representation:
    MIXED_REPRESENTATION at system level

Unity Terrain / TerrainData presence somewhere in project tooling/code:
    source-supported by supplied static report

DR-TH-003 per-map result:
    CampaignMap_HOS       -> INSUFFICIENT_EVIDENCE
    CampaignMap_Cuanacht  -> INSUFFICIENT_EVIDENCE
    CampaignMap_Forlorn   -> INSUFFICIENT_EVIDENCE
    CampaignMap_Sarras    -> INSUFFICIENT_EVIDENCE

CampaignMap -> exact Terrain/TerrainData or base-ground mesh binding:
    UNKNOWN

Per-map authoritative heightfield source:
    UNKNOWN

Production zero-configuration vanilla Highmap provider:
    BLOCKED
```

The zero-configuration product objective remains unchanged. Unknown source metadata is a provider/research
blocker; it must not become a normal user-facing form.

## Remaining evidence boundary

DR-TH-003 substantially closes broad public-system research as the next useful lane. The remaining blocker is a
serialized source-object binding rather than another conceptual survey of FOA rendering systems.

Highest-value remaining evidence includes:

1. `GroundBounds.CalculateGameBounds()` implementation and backing source;
2. callers/references of `Awaken.TG.EditorOnly.TerrainHeightRemapper` and related terrain editor tooling;
3. CampaignMap scene-loading/dependency code exposing exact scene and source identities;
4. Medusa build/source-selection types and source-object join identifiers;
5. Addressables semantic key/GUID/object mapping for candidate terrain or base-ground assets;
6. separately authorised static `CampaignMap_*` scene/component metadata if lawfully available.

No absent field may be replaced by Unity defaults, DepthTextures grid values, Pathfinding extents, or assumptions
about all Medusa meshes being continuous base ground.

## Claim states

This topic uses:

- `design-context` — an explicit product requirement/proposal, not a source-format fact;
- `repository-observed` — exact repository content at the recorded baseline supports the claim;
- `source-supported` — a durable public source in `SOURCE_REGISTER.md` supports the scoped claim;
- `static-report-supported` — the preserved decompilation report supports the claim, but the underlying binaries
  and analysis were not independently reproduced in the repository intake;
- `input-observed` — a preserved Deep Research or reconnaissance report states the observation, but an absence or
  exhaustive-search result is not promoted beyond the evidence actually available;
- `inference` — a bounded conclusion derived from identified facts;
- `unknown` — required proof is missing;
- `contradicted` — accepted evidence conflicts with the claim;
- `superseded` — a later record replaces the claim.

No evidence state grants implementation, runtime, packaging, deployment, publication, or promotion authority.
