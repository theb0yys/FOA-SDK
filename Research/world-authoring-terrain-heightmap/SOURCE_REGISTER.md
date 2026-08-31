# World Authoring Terrain / Highmap Source Register

Intake baseline: `ef86d0542e01c1a1104e0564fd52c0695bd9a50d`

Retrieval/intake date unless otherwise stated: 31 August 2026

This register replaces conversation-local citation tokens for claims relied on by the cleaned research intakes.
Preserved inputs may retain their original tokens, but those tokens are not durable repository evidence.

## Repository sources

| Source ID | Source | Exact locator | Use |
| --- | --- | --- | --- |
| `SRC-REPO-AGENTS` | FOA-SDK Agent Execution Policy | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/AGENTS.md | Authority order, research escalation, evidence truth, repository-write rules |
| `SRC-REPO-ENGINEERING` | FOA-SDK Engineering Process | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/docs/tainted-grail-sdk/ENGINEERING_PROCESS.md | Change classification and evidence-layer separation |
| `SRC-REPO-RESEARCH` | FOA-SDK Research Policy | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Research/README.md | Input preservation, source/claim registers, fact/inference/unknown separation |
| `SRC-REPO-PROTECTED` | Protected Files Policy | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/docs/protected-files-policy.md | Proprietary and private game-material boundary |
| `SRC-REPO-TERRAIN-H` | TerrainHeightmapDocument contract | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Gems/TaintedGrailModdingSDK/Code/Source/TerrainHeightmapDocument.h | Canonical terrain identity, source, grid, vertical, coordinate, tile, provenance, revision, and authority fields |
| `SRC-REPO-TERRAIN-CPP` | TerrainHeightmapDocument implementation | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Gems/TaintedGrailModdingSDK/Code/Source/TerrainHeightmapDocument.cpp | Accepted local source kinds, fail-closed metadata validation, staging and publication behavior |
| `SRC-REPO-FOUNDATION` | Foundation catalogue and identity models | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Gems/TaintedGrailModdingSDK/Code/Source/FoundationModels.h | Display name, aliases, source-scoped refs, exact native refs, and evidence separation |
| `SRC-REPO-TERRAIN-CONTRACTS` | TerrainAuthoring contracts | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Plugins/Authoring/TerrainAuthoring/Gem/Code/Source/TerrainAuthoringContracts.h | TerrainAuthoring commands, authority flags, and initial shell status |
| `SRC-REPO-TERRAIN-CONTRACTS-CPP` | TerrainAuthoring contract implementation | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Plugins/Authoring/TerrainAuthoring/Gem/Code/Source/TerrainAuthoringContracts.cpp | ReadActiveProfile-only declaration, shell-disabled commands, and no visible pane/preview authority |
| `SRC-REPO-SETUP-DETECTION` | LocalSetupDetectionService | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Gems/TaintedGrailModdingSDK/Code/Source/LocalSetupDetectionService.cpp | Workspace/profile defaults, candidate installation recognition, and bounded path derivation |
| `SRC-REPO-WA-TH-001` | Terrain Heightmap Contract and Editor Gate | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Research/world-authoring-terrain-heightmap/gates/WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md | No-guess terrain contract and current direct-game-source restrictions |
| `SRC-REPO-WA-TH-002` | Terrain Authoring UI/Preview Gate | https://github.com/theb0yys/FOA-SDK/blob/ef86d0542e01c1a1104e0564fd52c0695bd9a50d/Research/world-authoring-terrain-heightmap/gates/WA_TH_002_TERRAIN_AUTHORING_UI_PREVIEW_GATE.md | UI/source-evidence boundaries and no implicit runtime/source authority |

## Preserved design, assessment, and research inputs

| Source ID | Input | Repository locator | Evidence lane | Limitation |
| --- | --- | --- | --- | --- |
| `SRC-INPUT-HIGHMAP-DESIGN` | Zero-configuration Highmap design baseline | `inputs/HIGHMAP_IMPORTER_ZERO_CONFIGURATION_DESIGN_BASELINE_2026-08-31.md` | Product-design context | Defines intended UX; does not establish FOA source facts or implementation authority |
| `SRC-INPUT-HIGHMAP-SDK-ASSESSMENT` | SDK accommodation assessment | `inputs/HIGHMAP_IMPORTER_SDK_ACCOMMODATION_ASSESSMENT_2026-08-31.md` | Repository/static assessment | Bound to repository baseline; no build, Editor, or runtime execution |
| `SRC-INPUT-DR-TH-000` | Public terrain source reconnaissance | `inputs/DR_TH_000_PUBLIC_TERRAIN_RECONNAISSANCE_REPORT_2026-08-31.md` | Public package/runtime/documentation reconnaissance | DepthTextures semantic role was unresolved at this stage and later superseded by static evidence |
| `SRC-INPUT-DR-TH-001` | DR-TH-001 Deep Research report | `inputs/DR_TH_001_DEEP_RESEARCH_REPORT_2026-08-31.md` | Returned ChatGPT Deep Research report / E1 context | Underlying sources must be checked before consequential promotion |
| `SRC-INPUT-DR-TH-001-STATIC` | DR-TH-001 static/CIL decompilation report | `inputs/DR_TH_001_STATIC_DECOMPILATION_REPORT_2026-08-31.md` | User-supplied static assembly metadata and CIL analysis | Underlying DLLs were not committed and analysis was not independently reproduced in this change |
| `SRC-INPUT-DR-TH-002` | DR-TH-002 Deep Research report | `inputs/DR_TH_002_DEEP_RESEARCH_REPORT_2026-08-31.md` | Returned ChatGPT Deep Research report / E1 context | Some original conversation citations remain unreconciled; cleaned intake relies only on registered claims |
| `SRC-INPUT-DR-TH-003` | DR-TH-003 CampaignMap scene/source-asset inventory report | `inputs/DR_TH_003_DEEP_RESEARCH_REPORT_2026-08-31.md` | Returned Deep Research report / public documentation + package metadata + supplied static context | Negative search observations are bounded to the public surface; no private extraction occurred |
| `SRC-INPUT-DR-TH-004` | DR-TH-004 Campaign terrain source-binding report | `inputs/DR_TH_004_DEEP_RESEARCH_REPORT_2026-08-31.md` | Returned Deep Research report / targeted static-source-binding review using supplied static context and public metadata | No new binary decompilation or serialized commercial scene inspection occurred; exact source-object joins remain unavailable |

The supplied static report binds these external assembly observations by SHA-256:

| Assembly | SHA-256 |
| --- | --- |
| `TG.Main(3).dll` | `749aabbfbec121bb69bda0ae226223154406d2c990df3312ad12365d513fa982` |
| `HLOD.dll` | `32bfbf78eb0c8359d81ca1234a1802a2a9627448d5c794af46d3f3b21b7bdd42` |
| `MeshToTerrain(1).dll` | `eb4f02ba366dec1e78fe16b2ab4607bfb0e5c193d816ed6296d44a4fb21fd2b3` |

These hashes bind the supplied report to named binaries; they do not prove source authenticity, installed-machine
state, or live runtime behavior.

## Public package and runtime-observation sources

| Source ID | Source | Direct URL | Publisher/origin | Use and limitation |
| --- | --- | --- | --- | --- |
| `SRC-PUB-DEPOT` | Fall of Avalon depot file listing | https://steamdb.info/depot/1466062/apps/ | SteamDB presentation of Steam depot metadata | Public path and approximate-size inventory; secondary source, not developer format documentation |
| `SRC-PUB-MANIFESTS` | Fall of Avalon depot manifests | https://steamdb.info/depot/1466062/manifests/ | SteamDB | Manifest/build context and retrieval date; file hashes may require Steam sign-in |
| `SRC-PUB-MEDUSA-CUANACHT-2024` | Mega Patch 0.7 / Cuanacht package changes | https://steamdb.info/patchnotes/14508111/ | SteamDB package-change presentation | Historical map-scoped Medusa products; does not expose source object membership |
| `SRC-PUB-MEDUSA-CUANACHT-2024-HOTFIX` | 28 May 2024 package changes | https://steamdb.info/patchnotes/14518981/ | SteamDB package-change presentation | Follow-up changes to map-scoped Medusa products; still derived metadata |
| `SRC-PUB-SARRAS-2025` | Sanctuary of Sarras expansion package/update record | https://steamdb.info/patchnotes/21099119/ | SteamDB package/update presentation | Sarras chronology and map-scoped derived products; not terrain-source identity |
| `SRC-PUB-MAPSCENE-LOG` | Public FOA runtime log with `CampaignMap_*` scene events | https://steamcommunity.com/app/1466060/discussions/3/824857476142790518/ | Steam Community user-posted runtime log | Direct logged strings; not independently reproduced runtime evidence |
| `SRC-PUB-MERLIN` | Merlin's Workshop public project/wiki | https://github.com/AR-Questline/merlin-workshop/wiki | AR-Questline | Public modding/Addressables context; does not expose commercial CampaignMap scene inventories |

## First-party/developer technical sources

| Source ID | Source | Direct URL | Date | Scoped support |
| --- | --- | --- | --- | --- |
| `SRC-DEV-INTRO` | Deep dive Tainted Grail [0] — Introduction | https://dev.to/kamilvdono/deep-dive-tainted-grail-0-introduction-5gka | 31 May 2025 | Developer-described final system inventory: Addressables, Leshy, Medusa, Drake, scene baking, HLOD |
| `SRC-DEV-DRAKE` | Deep dive Tainted Grail [1] — Drake | https://dev.to/kamilvdono/deep-dive-tainted-grail-1-drake-runtime-entity-renderer-registration-system-1751 | June 2025 | Renderer registration and semantic Addressables mesh/material key usage |
| `SRC-DEV-LESHY` | Deep dive Tainted Grail [2] — Leshy | https://dev.to/kamilvdono/deep-dive-tainted-grail-2-leshy-vegetation-streaming-and-rendering-4ndk | 2025 | Vegetation placement/rendering and offline-prepared-world role |
| `SRC-DEV-MEDUSA` | Deep dive Tainted Grail [3] — Medusa | https://dev.to/kamilvdono/deep-dive-tainted-grail-3-medusa-blazing-fast-cliffs-rendering-4pi1 | 19 October 2025 | Medusa authoring inputs, scene processing, immutable runtime bake, StreamingAssets and collider boundary |
| `SRC-DEV-TECH-UPDATE` | FOA Tech Development Update mirror | https://www.eprison.de/spiele/tainted-grail-the-fall-of-avalon/steam-news/6350729003498121703/7324/77618.html | Public Steam-news mirror | Leshy, Medusa/static-object, Drake and HLOD descriptions; mirror rather than canonical page |

The author profile associated with the developer articles identifies the author as a Unity/Unreal developer at
Awaken Realms. The articles are first-party project observations, not implementation authority.

## Upstream engine and tool sources

| Source ID | Source | Direct URL | Scoped support |
| --- | --- | --- | --- |
| `SRC-UNITY-TERRAINDATA` | Unity `TerrainData` API | https://docs.unity3d.com/6000.5/Documentation/ScriptReference/TerrainData.html | Generic heightmap, size, resolution, scale and height semantics; applies to FOA only after campaign binding |
| `SRC-UNITY-ADDRESSABLES-CATALOG` | Unity Addressables content catalog documentation | https://docs.unity3d.com/Packages/com.unity.addressables%402.0/manual/build-content-catalogs.html | Catalog maps semantic keys to physical locations; generic until FOA terrain key binding is proved |
| `SRC-UNITY-COORDS` | Unity coordinate-system documentation | https://docs.unity3d.com/2021.3/Documentation/Manual/QuaternionAndEulerRotationsInUnity.html | Generic Unity left-handed, +X right, +Y up, +Z forward basis |
| `SRC-O3DE-TERRAIN-WORLD` | O3DE Terrain World component | https://docs.o3de.org/docs/user-guide/components/reference/terrain/world/ | Required terrain min/max height and sample/query resolution concepts |
| `SRC-O3DE-SCENE-FORMAT` | O3DE Scene Format Support | https://docs.o3de.org/docs/user-guide/assets/scene-settings/scene-format-support/ | Current page statement about O3DE axes/units |
| `SRC-O3DE-ACTORS` | O3DE Actors-tab documentation | https://docs.o3de.org/docs/user-guide/assets/scene-settings/actors-tab/ | Conflicting current handedness statement retained as a contradiction requiring explicit transform |
| `SRC-MESH-TO-TERRAIN` | Infinity Code Mesh to Terrain documentation | https://infinity-code.com/documentation/mesh-to-terrain.html | Third-party mesh-to-Unity-Terrain function; project use remains unproved |

## Source-use rules

1. A preserved input is research context, not an accepted decision.
2. `design-context` records user-approved product direction; it does not prove a source format or implementation.
3. `static-report-supported` claims remain static/decompilation claims until independently reproduced or checked against the relevant binary evidence.
4. Generic Unity or O3DE documentation may define identified upstream structures; it may not establish that a particular `CampaignMap_*` uses them.
5. SteamDB establishes public package paths/displayed metadata, not proprietary semantics or source-object membership.
6. Unsuccessful public searches are bounded limitations, not proof that objects do not exist.
7. DR-TH-004 did not execute new decompilation, private-installation inspection, or commercial scene/bundle extraction.
8. No source in this register authorizes commercial extraction, live installation inspection, runtime mutation, deployment, packaging, publication, or evidence promotion.
