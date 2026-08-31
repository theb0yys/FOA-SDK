# World Authoring Terrain / Highmap Research

Status: research intake and bounded static-evidence planning

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

- [`HIGHMAP_IMPORTER_ZERO_CONFIGURATION_DESIGN_BASELINE_2026-08-31.md`](inputs/HIGHMAP_IMPORTER_ZERO_CONFIGURATION_DESIGN_BASELINE_2026-08-31.md)
- [`HIGHMAP_IMPORTER_SDK_ACCOMMODATION_ASSESSMENT_2026-08-31.md`](inputs/HIGHMAP_IMPORTER_SDK_ACCOMMODATION_ASSESSMENT_2026-08-31.md)
- [`DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md`](inputs/DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md)

The design baseline is product context rather than evidence that FOA uses a particular source representation. The
SDK assessment is repository/static evidence bound to the intake baseline. DR-TH-000 identified package systems
and isolated the original terrain-source blocker before Deep Research.

## Preserved Deep Research and static inputs

- [`DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md`](inputs/DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md)
- [`DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_003_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_003_DEEP_RESEARCH_REPORT_2026-08-31.md)
- [`DR_TH_004_DEEP_RESEARCH_REPORT_2026-08-31.md`](inputs/DR_TH_004_DEEP_RESEARCH_REPORT_2026-08-31.md)

Original reports may retain conversation-local citation tokens. Those tokens are preserved as input context but
are not durable repository citations. Durable source locators and scoped claims live in
[`SOURCE_REGISTER.md`](SOURCE_REGISTER.md) and [`CLAIM_REGISTER.md`](CLAIM_REGISTER.md).

## Cleaned derivatives

- [`DR_TH_000_PUBLIC_RECONNAISSANCE_CLEAN_INTAKE.md`](intakes/DR_TH_000_PUBLIC_RECONNAISSANCE_CLEAN_INTAKE.md)
- [`DR_TH_001_CLEAN_INTAKE.md`](intakes/DR_TH_001_CLEAN_INTAKE.md)
- [`DR_TH_002_CLEAN_INTAKE.md`](intakes/DR_TH_002_CLEAN_INTAKE.md)
- [`DR_TH_003_CLEAN_INTAKE.md`](intakes/DR_TH_003_CLEAN_INTAKE.md)
- [`DR_TH_004_CLEAN_INTAKE.md`](intakes/DR_TH_004_CLEAN_INTAKE.md)

These are derivatives, not replacements for preserved reports. They retain evidence-lane distinctions and remove
conversation-local citations from the claims relied on by this topic.

## Research briefs

Completed briefs:

- [`DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md`](briefs/DR_TH_001_VANILLA_TERRAIN_RASTER_IDENTITY_ENCODING_RECONSTRUCTION_BRIEF.md)
- [`DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md`](briefs/DR_TH_002_CAMPAIGN_AUTHORITATIVE_WORLD_GEOMETRY_SOURCE_BRIEF.md)
- [`DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md`](briefs/DR_TH_003_CAMPAIGNMAP_SCENE_COMPONENT_AND_SOURCE_ASSET_INVENTORY_BRIEF.md)
- [`DR_TH_004_CAMPAIGN_TERRAIN_SOURCE_BINDING_BRIEF.md`](briefs/DR_TH_004_CAMPAIGN_TERRAIN_SOURCE_BINDING_BRIEF.md)

## Research chronology

```text
zero-configuration Highmap design
    -> repository SDK accommodation assessment
    -> DR-TH-000 public package/source reconnaissance
    -> DR-TH-001 format/DepthTextures research
    -> DR-TH-001 static/CIL evidence
    -> DR-TH-002 authoritative world-source research
    -> DR-TH-003 CampaignMap scene/source inventory research
    -> DR-TH-004 targeted source-binding research
    -> remaining lawfully obtained static source binding
```

## Current research disposition

```text
Highmap product objective:
    zero-configuration normal workflow

Existing canonical terrain backend:
    substantial and reusable

DepthTextures authoritative-terrain hypothesis:
    FAILED

Leshy authoritative-terrain hypothesis:
    FAILED

Medusa runtime archive as editable terrain:
    FAILED

Campaign world representation:
    MIXED_REPRESENTATION at system level

Unity Terrain / TerrainData use somewhere in project code/tooling:
    supported by supplied static report

DR-TH-004 per-map result:
    CampaignMap_HOS       -> INSUFFICIENT_EVIDENCE
    CampaignMap_Cuanacht  -> INSUFFICIENT_EVIDENCE
    CampaignMap_Forlorn   -> INSUFFICIENT_EVIDENCE
    CampaignMap_Sarras    -> INSUFFICIENT_EVIDENCE

GroundBounds definition/backing source:
    UNKNOWN

TerrainHeightRemapper callers/campaign use:
    UNKNOWN

CampaignMap -> exact Terrain/TerrainData binding:
    UNKNOWN

CampaignMap -> exact continuous-ground mesh binding:
    UNKNOWN

Addressables terrain semantic mapping:
    UNKNOWN

Production zero-configuration vanilla Highmap provider:
    BLOCKED
```

The zero-configuration objective remains unchanged. Unknown source metadata is a provider/research blocker; it
must not become a user-facing technical form.

## Remaining evidence boundary

DR-TH-004 confirms that broad public research has reached its useful boundary for source binding. The missing
evidence is a static source-object join, preferably one of:

```text
CampaignMap_X
    -> Terrain component file ID
    -> TerrainData GUID/file ID
```

or:

```text
CampaignMap_X
    -> exact continuous-ground GameObject set
    -> Mesh GUID/file IDs + transforms + colliders
```

Highest-value remaining evidence:

1. definition/body/backing source of `GroundBounds.CalculateGameBounds()`;
2. full `TerrainHeightRemapper` type and caller/editor-tool graph;
3. complete FOA `Terrain` / `TerrainData` / `TerrainCollider` reference sweep;
4. CampaignMap scene-loading and exact dependency identities;
5. Medusa source-selection predicates and source-object join identifiers;
6. Addressables semantic key/GUID/object mapping for terrain candidates;
7. separately authorised static CampaignMap component/asset metadata if lawfully available.

No absent field may be replaced by Unity defaults, DepthTextures grid values, Pathfinding extents, Leshy matrices,
Medusa archive size, or hash-like bundle names.

## Claim states

- `design-context` — an explicit product requirement/proposal, not a source-format fact;
- `repository-observed` — exact repository content at the recorded baseline supports the claim;
- `source-supported` — a durable public source supports the scoped claim;
- `static-report-supported` — a preserved decompilation report supports the claim, without independent reproduction;
- `input-observed` — a preserved report records an observation or bounded search limitation;
- `inference` — a bounded conclusion derived from identified facts;
- `unknown` — required proof is missing;
- `contradicted` — accepted evidence conflicts with the claim;
- `superseded` — a later record replaces the claim.

No evidence state grants implementation, runtime, packaging, deployment, publication, extraction, or promotion authority.
