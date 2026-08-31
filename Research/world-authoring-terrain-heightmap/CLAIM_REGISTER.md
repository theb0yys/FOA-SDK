# World Authoring Terrain / Highmap Claim Register

Intake baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

This register records scoped research claim state only. It grants no implementation, extraction, runtime,
deployment, publication, packaging, or promotion authority.

## State vocabulary

- `design-context` — explicit product direction; not a source-format fact or implementation permit;
- `repository-observed` — exact repository content at the recorded baseline supports the claim;
- `source-supported` — one or more durable public sources in `SOURCE_REGISTER.md` support the scoped claim;
- `static-report-supported` — the supplied static/CIL report supports the claim; underlying binaries and analysis were not independently reproduced in the repository intake;
- `input-observed` — a preserved Deep Research or reconnaissance report contains the observation but durable source reconciliation or exhaustive-source proof may be incomplete;
- `inference` — bounded conclusion derived from identified evidence;
- `unknown` — consequential proof is missing;
- `contradicted` — accepted evidence conflicts with the claim;
- `superseded` — a later record replaces the claim.

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
| `TH-C011` | The DepthTextures bake source and shader-side stored-depth-to-world-height conversion are known. | `unknown` | `SRC-INPUT-DR-TH-001-STATIC` | `TopDownDepthTextureBaker` and shader implementation were unavailable. |
| `TH-C012` | FOA's final world stack includes Leshy, Medusa, Drake, scene baking, HLOD, Addressables, and related custom systems. | `source-supported` | `SRC-DEV-INTRO`, `SRC-DEV-TECH-UPDATE` | Establishes a mixed world pipeline, not per-map terrain ownership. |
| `TH-C013` | Leshy is a vegetation streaming/rendering system. | `source-supported` | `SRC-DEV-LESHY`, `SRC-DEV-TECH-UPDATE` | Leshy supports map-scoped identity/placement evidence but is not authoritative terrain. |
| `TH-C014` | Leshy is the authoritative vanilla terrain geometry source. | `contradicted` | `TH-C013` | Closed as the primary Highmap source route. |
| `TH-C015` | Medusa authoring inputs are ordinary Unity `LODGroup` and `MeshRenderer` objects baked via `IProcessSceneWithReport` into immutable StreamingAssets data. | `source-supported` | `SRC-DEV-MEDUSA` | Confirms mesh-authored static landscape involvement; runtime archive is a derivative. |
| `TH-C016` | Medusa was designed for cliffs and generalized to fully static meshes; terrain-like static environment assets appear in its project domain. | `source-supported` | `SRC-DEV-INTRO`, `SRC-DEV-MEDUSA` | Strong landscape lead, not proof that continuous base ground is entirely Medusa mesh. |
| `TH-C017` | `medusa.arch` is an editable authoritative terrain source. | `contradicted` | `SRC-DEV-MEDUSA`, `SRC-PUB-DEPOT` | Medusa runtime data is a build derivative. |
| `TH-C018` | Drake creates/manages runtime rendering entities and streams meshes/materials through Addressables. | `source-supported` | `SRC-DEV-DRAKE`, `SRC-DEV-INTRO` | Runtime renderer/streaming product; not an editable terrain authority by itself. |
| `TH-C019` | HLOD, PathfindingCache, DepthTextures, and Leshy products are interchangeable substitutes for authoritative base-terrain source evidence. | `contradicted` | `TH-C007`, `TH-C013`, `SRC-DEV-INTRO` | Subsystem roles and evidence lanes remain separate. |
| `TH-C020` | The supplied static report found project-specific code reading `Terrain.transform.position.y` and `Terrain.terrainData.size.y` through `Awaken.TG.EditorOnly.TerrainHeightRemapper`. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC` | Establishes Terrain/TerrainData awareness somewhere in code/tooling. |
| `TH-C021` | A specific `CampaignMap_*` scene is bound to a specific `Terrain`/`TerrainData` asset. | `unknown` | `SRC-INPUT-DR-TH-002`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Central blocker for a TerrainData-based Highmap provider. |
| `TH-C022` | All four campaign maps use one identical authoritative base-terrain representation. | `unknown` | `SRC-INPUT-DR-TH-002`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Must be established per map. |
| `TH-C023` | The campaign world is best described at system level as a mixed representation containing mesh-authored static landscape and multiple derived/runtime subsystems, with possible TerrainData participation. | `inference` | `TH-C012`, `TH-C015`, `TH-C016`, `TH-C020` | Exact per-map base-ground owner remains unresolved. |
| `TH-C024` | One heightfield can encode all visible/collision geometry in every campaign map without material loss. | `contradicted` | `TH-C015`, `TH-C016`, `SRC-REPO-TERRAIN-H` | Highmap scope is the 2.5D base surface; non-heightfield meshes remain separate. |
| `TH-C025` | If a campaign `TerrainData` asset is identified, Unity exposes resolution, size, scale, and height semantics needed to construct most native source fields deterministically. | `source-supported` | `SRC-UNITY-TERRAINDATA` | Conditional upstream fact; no FOA campaign binding. |
| `TH-C026` | The presence of Infinity Code Mesh to Terrain proves FOA campaign terrain was produced with it. | `unknown` | `SRC-MESH-TO-TERRAIN`, `SRC-INPUT-DR-TH-001-STATIC` | Tool purpose known; project use unproved. |
| `TH-C027` | A production zero-configuration vanilla Highmap provider can currently resolve source object, resolution, bounds, height range, topology, and transform for any campaign map. | `unknown` | `TH-C021`, `TH-C022`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Current product route remains blocked. |
| `TH-C028` | The next highest-value evidence unit is a per-map scene component/source-asset inventory. | `superseded` | `SRC-INPUT-DR-TH-003` | DR-TH-003 executed that public/static question and found the source binding unavailable. |
| `TH-C029` | The intended Highmap UX contains only `Edit Vanilla Map` and `Import New Map` as primary actions. | `design-context` | `SRC-INPUT-HIGHMAP-DESIGN` | Provider uncertainty must not become a technical-user wizard. |
| `TH-C030` | Vanilla sources remain read-only and edits are stored as workspace-owned revisions. | `design-context` | `SRC-INPUT-HIGHMAP-DESIGN`, `SRC-REPO-TERRAIN-H`, `SRC-REPO-WA-TH-001` | Prevents direct source mutation. |
| `TH-C031` | The repository already implemented raw/image terrain canonicalisation, contained staging, source immutability checks, full validation, and atomic workspace publication. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-TERRAIN-CPP` | Generic backend should be reused. |
| `TH-C032` | The assessed TerrainAuthoring shell did not register a visible pane, enable preview projection, or expose commands as available. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-TERRAIN-CONTRACTS`, `SRC-REPO-TERRAIN-CONTRACTS-CPP` | Intended UX was not fulfilled. |
| `TH-C033` | The repository contained no proven vanilla terrain source provider or per-map Edit Vanilla Map workflow at the assessed baseline. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT` | Vanilla source research remained prerequisite. |
| `TH-C034` | Local setup detection could validate candidate installations and derive paths, but full automatic machine-level discovery was not established. | `repository-observed` | `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT`, `SRC-REPO-SETUP-DETECTION` | Environment discovery was partial. |
| `TH-C035` | DR-TH-000 correctly treated DepthTextures as unresolved and rejected guessing; its semantic uncertainty was superseded by static evidence. | `superseded` | `SRC-INPUT-DR-TH-000`, `SRC-INPUT-DR-TH-001-STATIC` | Inventory remains useful; terrain-source hypothesis closed. |
| `TH-C036` | Current public package metadata exposes all four `CampaignMap_*` identities through map-scoped Leshy and PathfindingCache products. | `source-supported` | `SRC-PUB-DEPOT`, `SRC-INPUT-DR-TH-003` | Resolves map/source identity, not terrain object identity. |
| `TH-C037` | Historical May 2024 package metadata exposed map-scoped Medusa products for HOS and Cuanacht. | `source-supported` | `SRC-PUB-MEDUSA-CUANACHT-2024`, `SRC-PUB-MEDUSA-CUANACHT-2024-HOTFIX` | Strengthens static-landscape association, not source-object membership. |
| `TH-C038` | DR-TH-003 located a public serialized CampaignMap scene/component inventory sufficient to count Terrain objects or bind TerrainData. | `contradicted` as a statement about DR-TH-003 | `SRC-INPUT-DR-TH-003` | Inventory was not located; not proof Terrain does not exist. |
| `TH-C039` | Each campaign has a proven authoritative TerrainData base. | `unknown` | `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004`, `TH-C020` | No map-specific binding or metadata. |
| `TH-C040` | Each campaign has a proven continuous mesh base with exact source object identities. | `unknown` | `SRC-DEV-MEDUSA`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Mesh landscape proven system-wide; base-ground objects unbound. |
| `TH-C041` | Campaign terrain semantic Addressables key -> GUID/object ID -> bundle mapping is known. | `unknown` | `SRC-UNITY-ADDRESSABLES-CATALOG`, `SRC-DEV-DRAKE`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Hashed bundle names do not resolve source identity. |
| `TH-C042` | `GroundBounds.CalculateGameBounds()` is known to derive from Unity Terrain objects. | `unknown` | `SRC-INPUT-DR-TH-001-STATIC`, `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Caller relationship known; definition/backing source unknown. |
| `TH-C043` | DR-TH-003 can classify any campaign as TerrainData, mesh, or mixed base. | `contradicted` as a statement about DR-TH-003 | `SRC-INPUT-DR-TH-003` | All four results were `INSUFFICIENT_EVIDENCE`. |
| `TH-C044` | The remaining blocker is serialized scene/source-object binding rather than another broad conceptual survey. | `inference` | `TH-C021`, `TH-C037`-`TH-C042` | Next evidence should target exact static bindings. |
| `TH-C045` | Highest-value next evidence includes GroundBounds implementation, TerrainHeightRemapper callers, scene dependencies, Medusa source selection, Addressables semantic mapping, and serialized metadata. | `inference` | `SRC-INPUT-DR-TH-003`, `TH-C044` | Prioritisation only; no extraction authority. |
| `TH-C046` | DR-TH-004 established an exact continuous-base-ground source binding for at least one campaign map. | `contradicted` as a statement about DR-TH-004 | `SRC-INPUT-DR-TH-004` | All four per-map outcomes remain `INSUFFICIENT_EVIDENCE`. |
| `TH-C047` | The supplied static evidence proves `TopDownDepthTexturesLoadingManager` calls `GroundBounds.CalculateGameBounds()` and consumes the result as map/world bounds. | `static-report-supported` | `SRC-INPUT-DR-TH-001-STATIC`, `SRC-INPUT-DR-TH-004` | Does not identify the `GroundBounds` definition or backing source. |
| `TH-C048` | The defining assembly/type and implementation body of `GroundBounds.CalculateGameBounds()` are known. | `unknown` | `SRC-INPUT-DR-TH-004` | Definition-level evidence was unavailable. |
| `TH-C049` | `TerrainHeightRemapper` campaign use, caller graph, serialized Terrain assignment, and TerrainData mutation path are known. | `unknown` | `SRC-INPUT-DR-TH-001-STATIC`, `SRC-INPUT-DR-TH-004` | Only the Terrain position/size reads are supported. |
| `TH-C050` | Exact `.unity` scene paths, scene GUIDs/file IDs, build indices, Addressables scene keys, and dependency graphs are known for all CampaignMaps. | `unknown` | `SRC-INPUT-DR-TH-004` | Public authorised surface did not expose them. |
| `TH-C051` | Exact Medusa selection predicates, marker components, source GameObject IDs, and archive-entry join keys are known. | `unknown` | `SRC-DEV-MEDUSA`, `SRC-INPUT-DR-TH-004` | Only the system-level authoring/bake contract is established. |
| `TH-C052` | A public serialized CampaignMap component inventory was found during DR-TH-004. | `contradicted` as a statement about the report | `SRC-INPUT-DR-TH-004` | No public inventory was located; private/commercial extraction was not run. |
| `TH-C053` | Another broad public Deep Research pass is the highest-value next source-binding action. | `contradicted` as a research-priority claim | `SRC-INPUT-DR-TH-003`, `SRC-INPUT-DR-TH-004` | Public lane is exhausted for the present join; bounded static evidence is needed. |
| `TH-C054` | The highest-value positive evidence is a lawful `CampaignMap -> Terrain -> TerrainData ID` or `CampaignMap -> exact continuous-ground mesh set` join. | `inference` | `SRC-INPUT-DR-TH-004`, `TH-C044` | Would collapse most remaining native terrain fields. |
| `TH-C055` | Source dimensions, spacing, elevation range, topology, and transform may be inferred from derived products when the source object is unknown. | `contradicted` | `TH-C002`, `SRC-INPUT-DR-TH-004` | DepthTextures, Leshy, Pathfinding, Medusa archive size, and bundle names are invalid substitutes. |
| `TH-C056` | Deterministic vanilla `TerrainHeightmapDocumentV1` reconstruction remains blocked for all four campaigns. | `inference` | `TH-C021`, `TH-C039`-`TH-C042`, `SRC-INPUT-DR-TH-004` | Required native source fields remain unknown. |

## Current promotion boundary

No claim in this register is promoted into normative architecture or implementation authority by these research intakes.

```text
CampaignMap -> TerrainData binding: UNKNOWN
CampaignMap -> continuous base-ground mesh binding: UNKNOWN
GroundBounds definition/backing source: UNKNOWN
TerrainHeightRemapper campaign use: UNKNOWN
Campaign scene path/GUID/dependency graph: UNKNOWN
Medusa source-object join: UNKNOWN
Addressables terrain semantic mapping: UNKNOWN
Per-map dimensions, elevation, topology, and transform: UNKNOWN
Deterministic source-to-canonical transform: UNKNOWN
Production vanilla Highmap provider: BLOCKED
```
