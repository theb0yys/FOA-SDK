# WA-TH-001 Terrain Heightmap Contract And Editor Gate

Status: accepted by repository owner/maintainer for the bounded local-only implementation slice; direct FoA source conversion remains blocked

Gate ID: `WA-TH-001`

Gate name: `World Authoring - Terrain Heightmap Contract and Editor Gate`

Target branch: `map-editor`

Accepted repository base: `931ce59e335f93edb54dc71309aa4e9e39cc3545`

Accepted at UTC: `2026-07-31T02:59:15Z`

Accepted by: repository owner/maintainer for the current task, as authorized in the active Codex thread

Authorized document path:
`Research/world-authoring-terrain-heightmap/gates/WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md`

Current task:
`four-map editable heightmap import gate before Terrain Authoring implementation`

Branch update:
`codex/designed-installer-wizard` was deleted before PR handoff. The repository owner/maintainer instructed
the WA-TH-001 implementation slice to continue on `map-editor` in the active Codex thread.

## Authority Decision

```text
terrain_heightmap_gate_accepted: true
terrain_contract_v1_authorized: true
terrain_authoring_tool_gem_authorized: true
user_exported_local_heightmap_import_authorized: true
direct_foa_install_scan_authorized: false
foa_game_file_extraction_authorized: false
external_process_conversion_authorized: false
asset_bundle_or_unity_assets_input_authorized: false
runtime_deployment_authorized: false
runtime_signoff_performed: false
```

This gate authorizes a first Terrain Authoring implementation slice only within the exact local-only boundary
below. It does not authorize reading, scanning, extracting, converting, publishing, deploying, or validating the
commercial Fall of Avalon maps themselves.

## Controlling Requirements

This decision is constrained by:

- `AGENTS.md`: research or gate documents require exact owner authorization for the current task.
- `README.md`: FOA-SDK is pre-alpha and grants no runtime compatibility or deployment authority.
- `GOVERNANCE.md`: new editor tools, schemas, persistence formats, identity, evidence, risk, and permission
  changes require reviewed design before implementation.
- `CONTRIBUTING.md`: importers and durable data formats are welcome, but display-name identity, invented native
  facts, private game fixtures, unbounded game-install scanning, and unclear dependencies are not accepted.
- `docs/protected-files-policy.md`: proprietary FoA files, extracted commercial content, private installs,
  private paths, saves, credentials, generated external output treated as source, and the external pinned O3DE
  checkout remain protected.
- `Research/README.md`: research records preserve facts, proposals, contradictions, unknowns, and gate routing;
  implementation still needs exact scoped authority and evidence.
- `docs/systems/SYSTEM_INDEX.md`: `world-authoring` is the primary owner classification; `road-atlas`,
  `external-toolchain`, `runtime-adapter-contracts`, and `runtime-evidence` remain separate systems.
- `docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md`: current external-tool authority does
  not allow game-file discovery, asset extraction, process launch, FoA API calls, deployment, save mutation, or
  live runtime connectivity.
- `Gems/ExternalToolchain/README.md` and `Gems/ExternalToolchain/docs/ARCHITECTURE.md`: current
  ExternalToolchain behavior is discovery-only; heightmap providers and process execution are later separately
  reviewed slices.
- `Research/tainted-grail-system-ports/areas/04-road-atlas/README.md`: Road Atlas owns road inventory, road
  geometry, topology, anchors, connectors, evidence requirements, and planning, not general terrain rasters.

If any later source contradicts these requirements, implementation must stop and a new authority decision is
required.

## Owner Split

| Layer | Owner | Authorized responsibility |
| --- | --- | --- |
| Pure contract | `schemas-and-persistence` / Foundation Core | `TerrainHeightmapDocumentV1`, schema ID, semantic validation, fingerprints, deterministic serialization, and malformed-input rejection |
| Workspace orchestration | `workspace-and-packs` / Framework | Active profile checks, contained staging, path containment, atomic publish, revision lineage, rollback, and no authority promotion |
| Editor user experience | `world-authoring` / `TerrainAuthoring` optional Tool Gem | Import form, map inventory, validation display, 2D/3D preview, editing, undo/redo, save/reopen, and O3DE preview projection commands |
| O3DE host projection | `world-authoring` plus pinned O3DE host boundary | Synthetic and workspace-owned `_gsi` preview assets through the normal Asset Processor boundary |
| Later conversion providers | `external-toolchain` | Not opened by this gate; requires separate process/source-artifact/provider authority |
| Road integration | `road-atlas` | Future read-only terrain sampling or terrain-reference binding only after both contracts are accepted |
| Runtime and deployment | `runtime-adapter-contracts`, `deployment-review`, `runtime-evidence` | Not authorized by this gate |

## Authorized Implementation Unit

The first implementation slice may add only:

```text
foa.terrain-heightmap schema version 1
+ TerrainHeightmapDocumentV1 pure contract
+ deterministic canonical JSON serialization
+ canonical U16 little-endian tile payload rules
+ semantic validators and issue codes
+ synthetic schema, parser, tile, path, and persistence fixtures
+ Framework contained staging and atomic workspace persistence
+ optional Plugins/Authoring/TerrainAuthoring Tool Gem
+ editor pane for local import, validation, preview, editing, and revision save
+ synthetic O3DE preview projection through reviewed Asset Processor roots
+ package guards excluding local terrain payloads and protected source observations
```

The implementation must stay inside product-owned FOA-SDK source paths. It must not copy, fork, patch, or
generate source inside the external O3DE checkout.

## Schema Contract

The durable document identity is:

```text
Schema ID: foa.terrain-heightmap
Schema version: 1
C++ contract: TerrainHeightmapDocumentV1
Manifest extension: *.tgheightmap.json
Canonical payload: unsigned 16-bit integer tiles
Tile byte order: little-endian
Tile storage order: row-major
Authority state: local-only, no runtime, no deployment, no publication, no evidence promotion
```

Version 1 documents must include, at minimum:

- `schema`
- `schema_version`
- `document_id`
- `map_identity`
- `profile_binding`
- `source_binding`
- `grid`
- `sample_encoding`
- `vertical_mapping`
- `coordinate_space`
- `tiles`
- `provenance`
- `legal_state`
- `revision`
- `local_payload_state`
- `authority`

Required behavior:

- unknown schema versions fail closed unless a later accepted migration exists;
- unknown required enum values fail closed;
- display aliases are not native map identities;
- native map IDs remain unknown until proved by a later lawful source-binding gate;
- `runtime_use_allowed`, `deployment_allowed`, `publication_allowed`, `packaging_allowed`,
  `game_write_allowed`, and `evidence_promotion_allowed` remain false in version 1;
- source, reviewed evidence, validation, permission, and runtime authority remain separate;
- durable/public documents must not contain absolute private input paths.

Canonical height conversion is:

```text
normalized = uint16_sample / 65535
height_metres = min_height_metres + normalized * (max_height_metres - min_height_metres)
```

Validation must reject:

- `max_height_metres <= min_height_metres`;
- non-finite numeric values;
- zero dimensions;
- multiplication overflow;
- dimensions or tile counts above accepted bounds;
- missing, overlapping, duplicated, or gapped tiles;
- payload hash mismatch;
- payload paths that are absolute, escaped, URI, UNC, symlink, junction, reparse-point, alternate-data-stream,
  reserved-device, or case-colliding paths.

## Local-Only Input Route

The authorized first route is:

```text
explicit user file selection
-> read-only source open
-> path containment and local-path validation
-> source byte SHA-256
-> active profile binding
-> parse into contained workspace staging
-> normalize into canonical U16 tiles
-> validate complete document and payload inventory
-> compute canonical document fingerprint
-> atomically publish into workspace-derived terrain root
-> generate optional synthetic/workspace-owned O3DE preview projection
-> edit only workspace-owned revisions
```

Accepted first-route inputs:

| Input | Status |
| --- | --- |
| 16-bit greyscale PNG | Authorized |
| 16-bit greyscale TIFF | Authorized |
| Raw little-endian U16 grid | Authorized only with mandatory sidecar |
| Raw big-endian U16 grid | Authorized only with mandatory sidecar and canonical conversion |
| Float32 grid | Deferred |
| Mesh-to-raster conversion | Deferred |
| Unity bundle, scene, `.assets`, `.resS`, Addressables bundle, executable, assembly, save, or encrypted/protected container | Prohibited |

Raw sidecars must provide dimensions, byte order, min/max height, spacing, coordinate basis, row-zero orientation,
and sample-position semantics. The importer must never guess raw dimensions, byte order, world scale, or
vertical range from file size or appearance.

## Protected Data Boundary

The repository, release packages, plug-in packages, fixtures, diagnostics, and screenshots must not contain:

- any of the four game-derived heightmap payloads;
- game bundles, scenes, asset files, assemblies, executables, saves, or extracted metadata;
- map-derived PNG, TIFF, RAW, canonical tile files, or O3DE `_gsi` images;
- Asset Processor products generated from protected map data;
- private installation paths, usernames, local drive inventories, or extraction logs;
- source screenshots showing protected map content unless separately reviewed for publication rights.

Workspace-derived terrain roots, source observations, revisions, preview images, and local tiles must be excluded
from Git tracking and official packaging by default.

## Road Atlas Boundary

Road Atlas does not own this contract. It remains limited to road inventory, names, segments, junctions, anchors,
connectors, exact vector geometry, connectivity, evidence requirements, and inert planning snapshots.

Terrain Authoring may later expose read-only terrain references to Road Atlas only through a separately accepted
consumer contract. Road Atlas must not write heightmap samples, mutate terrain revisions, or infer map identity
from road display names.

## Four Public Map Aliases

The editor may present these as unbound public aliases only:

```text
Horns of the South
Cuanacht / Cuanacht Village
Forlorn Swords
Sanctuary of Sarras / Sarras
```

These aliases do not establish stable native map IDs, source object identities, scene GUIDs, Addressables keys,
bundle object IDs, or compatible runtime terrain sources. Native identity remains blocked until a later lawful
source-binding gate proves it.

## Required Tests For Implementation

The implementation PR must provide synthetic-only evidence for:

- schema version 1 round trip and canonical ordering;
- unsupported version and unknown enum rejection;
- missing fields and malformed JSON;
- stable IDs, display-name rejection, and exact profile binding;
- dimension limits, overflow, zero dimensions, non-square grids, and excessive total samples;
- little-endian and big-endian raw input conversion;
- truncated, odd-byte, malformed, or incorrectly sized payloads;
- vertical range, non-finite value, and quantization cases;
- tile gap, overlap, duplicate coordinate, edge tile, and deterministic order cases;
- coordinate basis, row-zero orientation, axis flips, grid-vertex versus cell-centre semantics, and transform
  round trip;
- source and tile fingerprint mismatch;
- stale profile, stale source, changed importer lock, and repeatability;
- Windows path traversal, URI, UNC, symlink, junction, reparse point, alternate data stream, reserved device,
  and case-collision rejection;
- malformed PNG/TIFF, unsupported channel count, unsupported bit depth, huge metadata, and decompression-bomb
  rejection;
- staging-before-publish, interrupted save, atomic replace, rollback, cancellation, and cleanup containment;
- source byte immutability after success and failure paths;
- no absolute private path in durable/public documents or logs;
- no protected payloads or generated game-derived assets entering Git tracking;
- O3DE preview projection from synthetic/workspace-owned data only;
- validation cannot set runtime, deployment, packaging, publication, game-write, or evidence-promotion authority.

No fixture may contain FoA map samples, reconstructed terrain, private assemblies, extracted metadata, private
paths, proprietary Unity projects, or generated products from protected game data.

## Required Performance Gates

Implementation is high performance risk because it streams, validates, hashes, persists, previews, and edits
large raster data.

Acceptance targets for a fixed Windows performance host are:

| Workload | Target |
| --- | --- |
| 4097 x 4097 U16 import, validation, hashing, and preview mip generation | <= 5 seconds |
| 16385 x 16385 U16 tiled import and validation | <= 45 seconds |
| Peak incremental committed memory for 16385 x 16385 | <= 768 MiB, excluding O3DE Asset Processor |
| Open-map tile cache default | <= 256 MiB |
| One changed 1024 x 1024 tile rehash and atomic revision update | <= 500 ms, excluding Asset Processor |
| Interactive brush update over a 256 x 256 affected region | preview response <= 100 ms |
| Determinism | byte-identical manifest and tile fingerprints across three identical runs |
| Cancellation | stops at next tile boundary and leaves no published partial revision |

Ordinary CI must include deterministic operation-count, memory-bound, output-equivalence, and fixture-cardinality
guards. Wall-clock claims require CPU, memory, storage, Windows version, build configuration, map dimensions,
tile count, command, and source revision.

## Required O3DE And Editor Gates

Terrain implementation must be validated against pinned O3DE:

```text
O3DE version: 2.7.0
O3DE commit: 68683f23fb747380d3efa2424bd5f30242e9c5a2
```

Required implementation evidence:

1. static repository validation for schema files, source manifests, path-policy checks, package exclusions, and
   no runtime linkage;
2. pure Core tests for contract construction, validation, canonicalization, fingerprints, and malformed data;
3. Framework tests for workspace staging, containment, atomic persistence, rollback, and no auto-promotion;
4. Tool Gem manifest validation, deterministic registration, capability declaration, provenance, compatibility,
   and no implicit authority;
5. `TerrainAuthoring` Editor target built against the exact pinned O3DE checkout;
6. synthetic 16-bit `_gsi` Asset Processor input accepted in an isolated project/cache;
7. Editor lifecycle evidence for pane registration, open, close, reopen, command routing, and no stale services;
8. import acceptance for synthetic PNG/TIFF and RAW-sidecar fixtures;
9. 2D and 3D visual acceptance using synthetic content only;
10. edit, undo, redo, save, close, reopen, and lineage acceptance;
11. no writes outside workspace, build root, O3DE cache, or reviewed scratch root;
12. restart acceptance for workspace revisions and generated preview projection;
13. performance gate evidence;
14. evidence pack with commit, commands, logs, screenshots of synthetic content, fingerprints, and exclusions.

O3DE configure, build, Asset Processor success, and Editor acceptance prove only SDK-owned authoring behavior.
They are not Fall of Avalon runtime proof.

## Compatibility And Migration

Version 1 is a new durable format. The first implementation has no backward migration obligation because no
accepted terrain heightmap document format exists yet. It must still implement:

- explicit rejection of unknown future versions;
- deterministic diagnostics for unsupported documents;
- no silent downgrade;
- no display-name identity migration;
- no opportunistic repair of protected or stale source bindings;
- documentation updates in the implementation PR for any new user-visible or public data format fields.

Any later version 2 change requires a separate migration or rejection authority decision.

## Capabilities That Remain Prohibited

This gate does not authorize:

- direct scanning of a FoA installation;
- Steam, registry, Unity Hub, arbitrary drive, or neighboring-folder discovery;
- reading `.assets`, `.resS`, Addressables bundles, AssetBundles, game scenes, game assemblies, game executables,
  saves, or encrypted/protected content as importer input;
- external process launch, shell execution, IPC, downloader, installer, provider execution, or source-artifact
  handoff;
- FoA, Unity, BepInEx, Harmony, or game API calls from Core, Framework, Editor, or the Tool Gem;
- deployment to a game installation;
- save mutation;
- runtime adapter handoff;
- signing, publication, upload, or official release packaging of user terrain payloads;
- compatibility certification with the Fall of Avalon runtime;
- exact native map ID claims for the four maps;
- runtime sign-off.

## Revocation And Drift

This authority is invalidated before implementation starts if:

- `main` or the working base moves and changes Foundation, Framework, ExtensionAPI, authoring plug-in,
  persistence, protected-data, O3DE host, or package policy in a way that affects this gate;
- implementation touches files outside the accepted owner split without renewed authority;
- direct game-source inspection, extraction, external process execution, runtime deployment, or protected payload
  handling is proposed;
- new third-party dependencies are introduced without licence, security, maintenance, and provenance review;
- test, performance, O3DE host, or evidence-pack lanes are weakened or omitted.

Non-material drift must be reconciled in the implementation PR with exact comparison evidence and maintainer
confirmation.

## Current Review Result

WA-TH-001 is accepted for the first local-only Terrain Heightmap Contract and Editor implementation slice.

Implementation may begin in a separate focused code task under this gate, starting with the pure contract,
validators, path-policy tests, synthetic fixtures, and package exclusions before UI or preview work.

Direct game-source conversion, four-map native identity binding, runtime deployment, and Fall of Avalon runtime
sign-off remain blocked.

## Source Register

| Source | Relevance |
| --- | --- |
| `AGENTS.md` | Exact owner authorization requirement for research and gate document changes |
| `README.md` | Pre-alpha status, O3DE/FoA separation, generated-output boundary, no runtime/deployment authority |
| `GOVERNANCE.md` | Significant-change design review, architecture invariants, missing proof fails closed |
| `CONTRIBUTING.md` | Importer/data-format requirements, prohibited game assets, no display-name identity, no unbounded install scanning |
| `docs/protected-files-policy.md` | Protected external data boundary |
| `docs/systems/SYSTEM_INDEX.md` | Owner classification for `world-authoring`, `road-atlas`, `external-toolchain`, and runtime systems |
| `Research/README.md` | Research record rules and implementation-gate routing |
| `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md` | Allowed metadata/schemas/synthetic fixtures; prohibited ripped maps, bundles, assets, private files |
| `docs/tainted-grail-sdk/DATA_FORMATS.md` | Durable JSON rules, schema-version rejection, no private paths, no runtime permission by document existence |
| `docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md` | Current external-tool authority and no game scanning/extraction/runtime boundary |
| `Gems/ExternalToolchain/README.md` | Current discovery-only ExternalToolchain behavior |
| `Gems/ExternalToolchain/docs/ARCHITECTURE.md` | Local path restrictions, no process execution, later heightmap provider as separate review |
| `Plugins/Authoring/README.md` | Optional authoring Tool Gem model and no runtime/deployment/save/signing/publication/evidence-promotion authority |
| `Gems/TaintedGrailModdingSDK/README.md` | Foundation, workspace, source/evidence intake, exact profile binding, and fail-closed validation |
| `Research/tainted-grail-system-ports/areas/04-road-atlas/README.md` | Road Atlas scope is roads and planning, not terrain rasters |
| `Research/tainted-grail-system-ports/PORT_GATES.md` | Road Atlas editor slice remains inert; runtime movement and scene mutation prohibited |
| `o3de.lock.json` | Exact pinned O3DE version and commit for future host proof |
| Owner-supplied pasted-text attachment in the active Codex thread | Deep research brief supporting the terrain owner, schema, local-only route, tests, performance, and O3DE gates; private local attachment path intentionally not committed |
