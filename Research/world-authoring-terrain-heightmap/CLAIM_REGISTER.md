# World Authoring Terrain / Highmap Claim Register

Intake baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

This register records scoped research claim state only. It grants no implementation, extraction, runtime,
deployment, publication, packaging, or promotion authority.

## State vocabulary

- `design-context` — explicit product direction; not a source-format fact or implementation permit;
- `repository-observed` — exact repository content at the recorded baseline supports the claim;
- `source-supported` — one or more durable public sources in `SOURCE_REGISTER.md` support the scoped claim;
- `static-report-supported` — the supplied static/CIL report supports the claim; underlying binaries and analysis
  were not independently reproduced in the repository intake;
- `input-observed` — a preserved Deep Research or reconnaissance report contains the observation but durable
  source reconciliation or exhaustive-source proof may be incomplete;
- `inference` — bounded conclusion derived from identified evidence;
- `unknown` — consequential proof is missing;
- `contradicted` — accepted evidence conflicts with the claim;
- `superseded` — a later accepted record replaces the claim.

## Claims

| Claim ID | Claim | State | Sources | Consequence / limitation |
| --- | --- | --- | --- | --- |
| `TH-C001` | FOA-SDK already defines an explicit canonical terrain document carrying source, grid, vertical, coordinate, tile, provenance, revision, and authority data. | `repository-observed` | `SRC-REPO-TERRAIN-H`, `SRC-REPO-TERRAIN-CPP` | A vanilla provider must resolve these fields rather than hide guesses in the document. |
| `TH-C002` | WA-TH-001 prohibits guessing raw dimensions, byte order, world scale, vertical range, coordinate basis, row orientation, or sample semantics. | `repository-observed` | `SRC-REPO-WA-TH-001` | Unknown source metadata is a blocker, not normal user configuration. |
| `TH-C003` | Public package metadata exposes `StreamingAssets/DepthTextures/CampaignMap_Cuanacht`, `CampaignMap_Forlorn`, and `CampaignMap_HOS` tiled `.raw` collections. | `source-supported` | `SRC-PUB-DEPOT`, `SRC-INPUT-DR-TH-000`, `SRC-INPUT-DR-TH-001` | Establishes source-scoped inventory only, not terrain semantics. |
| `TH-C004` | Current public package metadata does not expose a corresponding `DepthTextures/CampaignMap_Sarras` collection. | `source-supported` | `SRC-PUB-DEPOT`, `SRC-INPUT-DR-TH-000`, `SRC-INPUT-DR-TH-001` | Absence from this subsystem does not imply that Sarras lacks terrain. |
| `TH-C005` | `CampaignMap_Cuanacht` and `CampaignMap_HOS` appear as runtime map-scene strings in a public FOA log. | `source-supported` | `SRC-PUB-MAPSCENE-LOG` | Strong scene/source-scoped identity; not an exact terrain-asset identity. |
| `TH-C006` | The supplied static report identifies `TopDownDepthTexturesLoadingManager` as the loader for `DepthTextures/<scene>/depth_tex_X_Y.raw`. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Static evidence only; not independently reproduced live runtime proof. |
| `TH-C007` | The supplied static report identifies `ScreenSpaceWetness`, `VFXTopDownDepthBinder`, and precipitation control as direct consumers/activation of the DepthTextures system. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Classifies observed consumer role as wetness/precipitation/VFX. |
| `TH-C008` | The observed DepthTextures call chain contains no `TerrainData` construction, `SetHeights`, heightmap population, or equivalent terrain-writing operation. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Rejects the DepthTextures runtime consumer as authoritative terrain construction. |
| `TH-C009` | `DepthTextures` are the authoritative vanilla terrain source. | `contradicted` | `TH-C006`, `TH-C007`, `TH-C008` | Closed as the primary Highmap route unless new contradictory evidence appears. |
| `TH-C010` | The supplied static report establishes a fixed payload-size formula of `TextureSize² × 4`, zero-based chunk X/Y indexing, and GroundBounds-relative X/Z chunking for DepthTextures. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Useful for derived wetness-data understanding, not sufficient to recover authoritative terrain. |
| `TH-C011` | The DepthTextures bake source and shader-side stored-depth-to-world-height conversion are known. | `unknown` | `SRC-INPUT-DR-TH-001-STATIC` | `TopDownDepthTextureBaker` and shader implementation were unavailable in the supplied evidence. |
| `TH-C012` | FOA's final world stack includes Leshy, Medusa, Drake, scene baking, HLOD, Addressables, and related custom systems. | `source-supported` | `SRC-DEV-INTRO`, `SRC-DEV-TECH-UPDATE` | Establishes a mixed world pipeline, not per-map terrain ownership. |
| `TH-C013` | Leshy is a vegetation streaming/rendering system. | `source-supported` | `SRC-DEV-LESHY`, `SRC-DEV-TECH-UPDATE` | Leshy files may support map-scoped identity/placement evidence but are not authoritative terrain. |
| `TH-C014` | Leshy is the authoritative vanilla terrain geometry source. | `contradicted` | `TH-C013` | Closed as the primary Highmap source route. |
| `TH-C015` | Medusa authoring inputs are ordinary Unity `LODGroup` and `MeshRenderer` objects that are baked via `IProcessSceneWithReport` into immutable StreamingAssets data. | `source-supported` | `SRC-DEV-MEDUSA` | Confirms mesh-authored static landscape involvement; runtime archive is a derivative. |
| `TH-C016` | Medusa was designed for cliffs and later generalized to fully static meshes; the project overview includes terrain-like static environment assets in Medusa's domain. | `source-supported` | `SRC-DEV-INTRO`, `SRC-DEV-MEDUSA` | Strong landscape-source lead, but not proof that continuous base ground is entirely Medusa mesh. |
| `TH-C017` | `medusa.arch` is an editable authoritative terrain source. | `contradicted` | `SRC-DEV-MEDUSA`, `SRC-PUB-DEPOT` | Medusa runtime data is a build derivative and immutable for the scene. |
| `TH-C018` | Drake creates/manages runtime rendering entities and streams meshes/materials through Addressables. | `source-supported` | `SRC-DEV-DRAKE`, `SRC-DEV-INTRO` | Runtime renderer/streaming product; not by itself an editable terrain authority. |
| `TH-C019` | HLOD, PathfindingCache, DepthTextures, and Leshy products are interchangeable substitutes for authoritative base-terrain source evidence. | `contradicted` | `TH-C007`, `TH-C013`, `SRC-DEV-INTRO` | Evidence lanes and subsystem roles remain separate. |
| `TH-C020` | The supplied static report found project-specific code that reads `Terrain.transform.position.y` and `Terrain.terrainData.size.y` through `Awaken.TG.EditorOnly.TerrainHeightRemapper`. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Establishes Terrain/TerrainData awareness somewhere in code/tooling. |
| `TH-C021` | A specific `CampaignMap_*` scene is bound to a specific `Terrain`/`TerrainData` asset. | `unknown` | `SRC-INPUT-DR-TH-002`, `SRC-INPUT-DR-TH-003` | Central blocker for a TerrainData-based Highmap provider. |
| `TH-C022` | All four campaign maps use one identical authoritative base-terrain representation. | `unknown` | `SRC-INPUT-DR-TH-002`, `SRC-INPUT-DR-TH-003` | Must be established per map; Sarras may differ. |
| `TH-C023` | The campaign world is best described, at system level, as a mixed representation containing mesh-authored static landscape and multiple derived/runtime subsystems, with possible TerrainData participation. | `inference` | `TH-C012`, `TH-C015`, `TH-C016`, `TH-C020` | Research/design context only; exact per-map base-ground owner remains unresolved. |
| `TH-C024` | One heightfield can encode all visible/collision geometry in every campaign map without material loss. | `contradicted` | `TH-C015`, `TH-C016`, `SRC-REPO-TERRAIN-H` | Cliffs/overhangs/caves/static meshes require separate world-geometry layers; Highmap scope is the 2.5D base surface. |
| `TH-C025` | If a campaign `TerrainData` asset is identified, Unity exposes the resolution, size, scale, and height semantics required to construct most native source fields deterministically. | `source-supported` | `SRC-UNITY-TERRAINDATA` | Conditional upstream fact; does not establish a FOA campaign binding. |
| `TH-C026` | The presence of the Infinity Code Mesh to Terrain package proves that FOA campaign terrain was produced with it. | `unknown` | `SRC-MESH-TO-TERRAIN`, `SRC-INPUT-DR-TH-001-STATIC` | Tool purpose is known; project use and output identities are unproved. |
| `TH-C027` | A production zero-configuration vanilla Highmap provider can currently resolve source object, resolution, bounds, height range, topology, and transform for any campaign map. | `unknown` | `TH-C021`, `TH-C022`, `SRC-INPUT-DR-TH-003` | Current product route remains blocked. |
| `TH-C028` | The next highest-value evidence unit is a per-map scene component and source-asset inventory covering Terrain, TerrainData, TerrainCollider, landscape meshes, Medusa markers, scene dependencies, and exact transforms/identifiers. | `superseded` | `SRC-INPUT-DR-TH-003` | DR-TH-003 executed this public/static research question and found the serialized source-object binding still unavailable. |
| `TH-C029` | The intended normal Highmap user experience contains only `Edit Vanilla Map` and `Import New Map` as primary actions. | `design-context` | `SRC-INPUT-HIGHMAP-DESIGN` | Provider uncertainty must not become a technical-user wizard. |
| `TH-C030` | Vanilla sources must remain read-only and edits must be stored as workspace-owned revisions. | `design-context` | `SRC-INPUT-HIGHMAP-DESIGN`, `SRC-REPO-TERRAIN-H`, `SRC-REPO-WA-TH-001` | Supports reset/reimport and prevents direct source mutation. |
| `TH-C031` | The repository already implemented raw/image terrain canonicalisation, contained staging, source immutability checks, complete-document validation, and atomic workspace publication. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-TERRAIN-CPP` | Generic terrain-import backend should be reused rather than replaced. |
| `TH-C032` | The observed TerrainAuthoring shell did not register a visible pane, enable preview projection, or expose its command inventory as available shell commands. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-TERRAIN-CONTRACTS`, `SRC-REPO-TERRAIN-CONTRACTS-CPP` | Explains why the intended two-action UX was not fulfilled. |
| `TH-C033` | The repository contained no proven vanilla terrain source provider or per-map Edit Vanilla Map workflow at the assessed baseline. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT` | Vanilla source research remained a prerequisite. |
| `TH-C034` | Local setup detection at the assessed baseline could validate candidate installations and derive workspace/profile paths, but full automatic machine-level discovery was not established. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-SETUP-DETECTION` | Environment discovery was partial, not complete. |
| `TH-C035` | DR-TH-000 correctly treated DepthTextures as an unresolved candidate and rejected guessing its technical interpretation; its semantic uncertainty was later superseded by the static report. | `superseded` | `SRC-INPUT-DR-TH-000`, `SRC-INPUT-DR-TH-001-STATIC` | Package inventory remains useful; terrain-source hypothesis is closed. |
| `TH-C036` | Current public package metadata exposes all four `CampaignMap_*` identities through map-scoped Leshy and PathfindingCache products. | `source-supported` | `SRC-PUB-DEPOT`, `SRC-INPUT-DR-TH-003` | Resolves map/source-scoped identity, not terrain object identity. |
| `TH-C037` | Historical May 2024 package metadata exposed map-scoped Medusa products for HOS and Cuanacht, including `CampaignMap_HOS_Static/matrices.medusa` and corresponding Cuanacht data. | `source-supported` | `SRC-PUB-MEDUSA-CUANACHT-2024`, `SRC-PUB-MEDUSA-CUANACHT-2024-HOTFIX` | Strengthens per-map static-landscape association but does not identify source GameObjects or continuous base ground. |
| `TH-C038` | DR-TH-003 located a public serialized `CampaignMap_*` scene/component inventory sufficient to count Terrain objects or bind TerrainData assets. | `contradicted` as a statement about the DR-TH-003 result | `SRC-INPUT-DR-TH-003` | The report explicitly records that this public serialized inventory was not located; this is a research-surface limitation, not proof that Terrain objects do not exist. |
| `TH-C039` | HOS, Cuanacht, Forlorn, and Sarras each have a proven authoritative TerrainData base. | `unknown` | `SRC-INPUT-DR-TH-003`, `TH-C020` | No map-specific Terrain/TerrainData binding, asset ID, resolution, size, transform, or topology is established. |
| `TH-C040` | HOS, Cuanacht, Forlorn, and Sarras each have a proven continuous mesh base with exact source object identities. | `unknown` | `SRC-DEV-MEDUSA`, `SRC-INPUT-DR-TH-003` | Mesh-authored landscape is proven at system level; continuous base-ground ownership and source-object IDs are not. |
| `TH-C041` | Campaign terrain semantic Addressables key -> GUID/object ID -> bundle mapping is known for any of the four maps. | `unknown` | `SRC-UNITY-ADDRESSABLES-CATALOG`, `SRC-DEV-DRAKE`, `SRC-INPUT-DR-TH-003` | Hashed bundle filenames and generic Addressables usage do not resolve terrain source identity. |
| `TH-C042` | `GroundBounds.CalculateGameBounds()` is known to derive its bounds from Unity Terrain objects. | `unknown` | `SRC-INPUT-DR-TH-001-STATIC`, `SRC-INPUT-DR-TH-003` | Consumer relationship is known; implementation/backing source is not. |
| `TH-C043` | DR-TH-003 can classify any campaign map as `TERRAINDATA_BASE_CONFIRMED`, `MESH_BASE_CONFIRMED`, or `MIXED_BASE_CONFIRMED`. | `contradicted` as a statement about the DR-TH-003 result | `SRC-INPUT-DR-TH-003` | All four per-map decisions are `INSUFFICIENT_EVIDENCE`. |
| `TH-C044` | The remaining blocker is primarily the serialized scene/source-object binding rather than another broad conceptual survey of FOA world systems. | `inference` | `TH-C021`, `TH-C037`, `TH-C038`, `TH-C039`, `TH-C040`, `TH-C041`, `TH-C042` | Next evidence should target exact static bindings, not repeat broad public-system research. |
| `TH-C045` | The highest-value next evidence lanes are GroundBounds implementation, TerrainHeightRemapper callers/editor tooling, CampaignMap scene-loading/dependency identities, Medusa source-selection/join metadata, Addressables semantic mapping, and separately authorised serialized scene/component metadata. | `inference` | `SRC-INPUT-DR-TH-003`, `TH-C044` | This prioritises evidence collection; it does not authorise protected-data inspection or implementation. |

## Current promotion boundary

No claim in this register has been promoted into a normative architecture or implementation decision by these
research intakes. In particular:

```text
CampaignMap -> TerrainData binding: UNKNOWN
CampaignMap -> continuous base-ground mesh binding: UNKNOWN
Per-map terrain dimensions and topology: UNKNOWN
Addressables terrain source mapping: UNKNOWN
GroundBounds backing source: UNKNOWN
Deterministic source-to-canonical transform: UNKNOWN
Production vanilla Highmap provider: BLOCKED
```
