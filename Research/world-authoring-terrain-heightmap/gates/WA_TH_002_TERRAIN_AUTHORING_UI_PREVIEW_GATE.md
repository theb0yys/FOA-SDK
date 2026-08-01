# WA-TH-002 Terrain Authoring UI Preview Gate

Status: accepted by repository owner/maintainer for bounded future Terrain Authoring UI/preview implementation slices; this document slice adds no UI, preview, Asset Processor projection, editor visual validation, or implementation code

Gate ID: `WA-TH-002`

Gate name: `World Authoring - Terrain Authoring UI/Preview Gate`

Target branch: `codex/terrain-authoring-ui-preview-gate`

Accepted repository base: `aa93e0b74ebc00b1dfd753ec2173d238c7ca9b2a`

Accepted at UTC: `2026-07-31T23:36:57Z`

Accepted by: repository owner/maintainer for the current task, as authorized in the active Codex thread

Authorized document path:
`Research/world-authoring-terrain-heightmap/gates/WA_TH_002_TERRAIN_AUTHORING_UI_PREVIEW_GATE.md`

Current task:
`TerrainAuthoring UI/preview gate before any pane, preview, Asset Processor projection, editor visual validation, or UI implementation`

Authority note:
`CURRENT_TASK.md` currently records an older process-pack task. It is not edited by this exception. The current
owner/maintainer authorization above is the exact document-change exception required for this gate path, and the
accepted WA-TH-001 terrain contract remains the controlling terrain authority.

## Authority Decision

```text
terrain_authoring_ui_preview_gate_accepted: true
terrain_authoring_code_in_this_pr_authorized: false
terrain_authoring_ui_implementation_authorized_after_gate: true
terrain_authoring_preview_implementation_authorized_after_gate: true
editor_pane_authorized_after_gate: true
static_ui_render_evidence_authorized_after_gate: true
synthetic_2d_preview_authorized_after_gate: true
synthetic_3d_preview_authorized_after_gate: true
synthetic_o3de_asset_processor_projection_authorized_after_gate: true
workspace_owned_preview_projection_authorized_after_gate: true
user_exported_local_heightmap_preview_authorized_after_gate: true
direct_foa_install_scan_authorized: false
foa_game_file_extraction_authorized: false
foa_game_derived_preview_publication_authorized: false
unity_asset_bundle_or_scene_input_authorized: false
runtime_deployment_authorized: false
runtime_signoff_performed: false
```

This gate authorizes future implementation slices only after this gate lands through the repository pull-request
process. Future slices must remain local-only, synthetic-first, contract-first, and evidence-separated. This
gate does not authorize any direct conversion of the four commercial Fall of Avalon maps, runtime deployment,
save mutation, catalog promotion, or Fall of Avalon runtime compatibility claim.

## Controlling Requirements

This decision is constrained by:

- `AGENTS.md`: research, gate, governance, validation, and folder-governing document changes require exact
  owner authorization for the current task and exact path.
- `README.md`: FOA-SDK is pre-alpha, O3DE is the authoring host, FoA remains a separate Unity runtime, and
  repository work must not claim runtime compatibility or deployment authority.
- `GOVERNANCE.md`: new editor tools, durable data formats, identity, evidence, risk, and permission changes
  require reviewed design before implementation; missing proof fails closed.
- `CONTRIBUTING.md`: imports and schemas must use lawful, bounded, explicit inputs; display labels are not
  stable identity; proprietary game assets and unbounded install scans are not accepted.
- `docs/protected-files-policy.md`: proprietary FoA files, extracted commercial content, private installs,
  private paths, saves, credentials, generated external output treated as source, and the pinned external O3DE
  checkout remain protected.
- `Research/README.md`: research routes later implementation through normative design and delivery gates, and
  must distinguish facts, proposals, unknowns, contradictions, and accepted decisions.
- `docs/systems/SYSTEM_INDEX.md`: `world-authoring` is the primary owner for terrain authoring; `road-atlas`,
  `external-toolchain`, `runtime-adapter-contracts`, `deployment-review`, and `runtime-evidence` remain
  separate owners.
- `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md`: synthetic fixtures, schemas, metadata, and lawfully
  shareable observations are allowed; ripped maps, bundles, assets, private files, and unclear third-party
  content are prohibited.
- `docs/tainted-grail-sdk/DATA_FORMATS.md`: `foa.terrain-heightmap` schema version 1 is local-only and cannot
  grant runtime, deployment, publication, packaging, game-write, or evidence-promotion authority.
- `docs/tainted-grail-sdk/PATH_POLICY.md`: UI paths are suggestions only; canonical persistence boundaries must
  validate containment before creating directories or writing files.
- `docs/tainted-grail-sdk/VISUAL_GAME_CONTENT_BROWSER_AND_PREVIEW_PIPELINE.md`: preview identity layers are
  evidence layers only; preview success does not create runtime, catalog, deployment, signing, publication, or
  function-complete authority.
- `docs/tainted-grail-sdk/FOA_ASSET_BROWSER_PANE_MODEL.md`: pane models consume proof evidence and must not
  mutate the O3DE Asset Browser or create typed bindings.
- `docs/tainted-grail-sdk/FOA_ASSET_BROWSER_PANE_UI_RENDER.md`: UI render evidence is local-only, consumes the
  pane model, and must not mutate a live O3DE editor pane or mark workflows function-complete.
- `docs/tainted-grail-sdk/FOA_3D_PREVIEW_VIEWPORT.md`: static 3D preview evidence does not invoke Unity, FoA,
  O3DE Asset Processor, or mutate a live O3DE viewport.
- `docs/tainted-grail-sdk/FOA_O3DE_ASSET_PROCESSOR_IMPORT_PROOF.md`: Asset Processor observations and imported
  products are local preview evidence only and do not grant authoring binding, catalog, deployment, or runtime
  authority.
- `Plugins/Authoring/README.md`: authoring plug-ins may emit candidate evidence, validation results, typed
  documents, plans, and reviewed handoffs, but cannot grant runtime, deployment, save, signing, publication, or
  evidence-promotion authority.
- `o3de.lock.json`: future editor, Asset Processor, and visual validation proof must bind to O3DE `2.7.0` at
  commit `68683f23fb747380d3efa2424bd5f30242e9c5a2`.
- `Research/world-authoring-terrain-heightmap/gates/WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md`:
  the accepted terrain route is explicit user-selected local heightmap input, contained workspace staging,
  canonical U16 tiles, workspace-owned revisions, synthetic/workspace-owned preview projection, and no direct
  FoA source conversion.

If a future implementation branch, new research source, or updated repository policy contradicts any of these
requirements, Terrain Authoring UI/preview implementation must stop for a new authority decision.

## Owner Split

| Layer | Owner | WA-TH-002 responsibility |
| --- | --- | --- |
| Terrain contract truth | `schemas-and-persistence` / Foundation Core | Owns `TerrainHeightmapDocumentV1`, canonical fields, issue codes, fingerprints, schema-version rejection, and authority flags |
| Workspace persistence | `workspace-and-packs` / Framework | Owns active profile binding, contained staging, atomic revision publish, rollback, package exclusion, and path containment |
| Terrain authoring UI | `world-authoring` / `TerrainAuthoring` optional Tool Gem | May present import state, validation issues, local map inventory, revision history, preview state, and editing commands after this gate |
| Preview model/render | `world-authoring` | May emit synthetic/workspace-owned preview evidence and visual-validation artefacts without making runtime claims |
| O3DE host projection | `world-authoring` plus pinned O3DE host boundary | May create synthetic/workspace-owned `_gsi` preview inputs through reviewed source roots and isolated Asset Processor cache after this gate |
| Road consumption | `road-atlas` | Not owner; may only consume read-only terrain references after a separate consumer contract |
| Runtime, deployment, release | `runtime-adapter-contracts`, `deployment-review`, `runtime-evidence` | Not authorized by this gate |

Terrain Authoring UI must be a presentation and command layer over validated contract/workspace services. It
must not become the durable document owner, schema owner, path-policy owner, package-policy owner, runtime owner,
or Road Atlas owner.

## Authorized Future Scope

After this gate is merged, future focused implementation slices may add the following, provided each slice
includes the tests and evidence required below:

- a `TerrainAuthoring` editor package or Tool Gem registration that declares no runtime/deployment authority;
- editor commands for explicit user-selected local heightmap import, validation, open, close, save revision,
  revert, undo, redo, and preview refresh;
- a UI pane that presents the active workspace/profile, local-only source observation, validation issues,
  terrain document identity, tile inventory, revision lineage, stale state, blocker state, and authority state;
- static UI render evidence for the pane before or alongside live editor-pane integration;
- synthetic and workspace-owned 2D height preview, including mip/thumbnail generation and issue overlays;
- synthetic and workspace-owned 3D terrain preview, with explicit fidelity state and no game-derived content in
  committed screenshots or fixtures;
- optional O3DE `_gsi` preview source projection under a reviewed generated-output root, followed by bounded
  Asset Processor observation/proof through an isolated project/cache;
- edit previews for workspace-owned revisions, including brush previews, changed-tile markers, and revision
  diff summaries;
- package guards proving `.tgheightmap.json`, tile payloads, source observations, preview images, `_gsi` files,
  Asset Processor products, and private local paths are excluded from official packages by default.

The UI may display the user's current local input path inside the live local editor session when the user has
selected it. Durable manifests, public logs, screenshots committed to the repository, PR artefacts, validation
receipts, and packages must use redacted or tokenized paths instead.

## Prohibited Scope

This gate does not authorize:

- direct scan of a Fall of Avalon installation;
- Steam, registry, Unity Hub, arbitrary-drive, or neighboring-folder discovery;
- reading `.assets`, `.resS`, Addressables bundles, AssetBundles, Unity scenes, game assemblies, game
  executables, saves, encrypted containers, or protected content as terrain input;
- launching Unity, FoA, BepInEx, Harmony, game APIs, extractor processes, shell commands, or external converters
  from the Terrain Authoring UI;
- creating previews from protected game-derived map data for committed fixtures, screenshots, PR artefacts, or
  public documentation;
- mutating O3DE source outside repository-owned code, the reviewed generated-output root, or isolated cache;
- mutating the O3DE Asset Browser as a source of truth;
- creating typed Road Atlas bindings, item/recipe bindings, placement bindings, runtime adapter handoffs, or
  deployment work orders;
- copying terrain payloads, source observations, generated preview products, or Asset Processor products into
  official packages by default;
- save mutation, runtime deployment, signing, release publication, or runtime compatibility certification;
- exact native map ID claims for the four public map aliases.

## UI Contract

The Terrain Authoring UI must present contract state without weakening the underlying validators:

- every visible terrain document is backed by a validated `foa.terrain-heightmap` version 1 document or a
  clearly blocked candidate;
- commands must be disabled when the active workspace/profile is missing, the source is unsafe, the document is
  stale, validation has errors, the importer lock changed, the preview projection is stale, or authority flags
  remain false for the requested operation;
- validation issues must expose stable issue codes, severity, actionable message, and bounded locator context;
- UI models must consume service snapshots and issue collections, not reimplement schema validation or
  canonical path decisions in widgets;
- import, hashing, tile generation, mip generation, and preview projection must run off the UI thread with
  cancellation and progress;
- cancellation must leave no published partial revision and must return the pane to a deterministic state;
- undo/redo must apply only to workspace-owned revisions and changed-tile deltas, not the original input file;
- save/reopen must validate the complete document and payload inventory before replacing the active revision;
- stale-state calculation must include profile fingerprint, source fingerprint, importer version, contract
  version, preview schema, tile hash, and O3DE import settings where relevant;
- public display aliases for the four maps must remain aliases only and must never become exact native IDs;
- every pane must make the local-only, no-runtime-authority state visible without implying the workflow is
  function-complete.

The UI may be ergonomic, but it must not hide blockers, auto-promote evidence, bypass validators, or turn a
preview success into authoring, catalog, runtime, deployment, publication, or packaging permission.

## Preview Contract

Preview outputs are evidence and review aids. They are not game assets, runtime products, or release assets.

Required preview layers are:

| Layer | Required boundary |
| --- | --- |
| Preview source | Synthetic or workspace-owned terrain tiles only |
| 2D preview | Generated local-only image/mip/overlay artefact outside official packages |
| 3D preview | Generated local-only mesh/texture/view state from validated terrain data only |
| O3DE `_gsi` input | Optional generated source under reviewed generated-output roots, never protected map data |
| Asset Processor observation | Separate bounded evidence that records execution externally or through a separately approved host lane |
| Pane presentation | Consumes terrain document plus preview evidence and keeps runtime/deployment authority false |

Every preview manifest or model must include:

- schema or document kind;
- tool ID and tool version;
- active workspace/profile binding;
- source terrain document fingerprint;
- source tile fingerprint set or aggregate fingerprint;
- preview settings fingerprint;
- O3DE version and commit when O3DE projection is involved;
- generated artefact list with SHA-256 and byte size;
- explicit fidelity state: `exact`, `approximate`, `partial`, `placeholder`, `unsupported`, or `blocked`;
- stale/invalidation inputs;
- authority flags that keep runtime, deployment, catalog, package, publication, and evidence promotion false.

Committed preview fixtures and screenshots must be synthetic. User-exported local heightmap previews may be
viewed locally and used for private acceptance, but they must not be committed, packaged, uploaded, or used as
public evidence unless a separate rights review authorizes that exact content.

## Asset Processor And Source Policy

Any future Asset Processor projection must obey all of the following:

- use the pinned O3DE host from `o3de.lock.json`;
- stage generated `_gsi` or related O3DE preview source under a reviewed generated-output root outside the
  external O3DE source checkout;
- use an isolated project and cache path for proof runs;
- record exact source file fingerprints, generated source fingerprints, product identifiers, cache-tokenized
  product paths, log fingerprints, start/completion time, exit code, timeout, and failures;
- prove products are generated from synthetic or workspace-owned terrain data, not protected game content;
- keep generated source, cache products, logs, and local terrain payloads out of Git and official packages by
  default;
- fail closed when source roots, cache roots, project roots, symlinks, junctions, case collisions, URI paths,
  UNC paths, alternate data streams, or private-path leakage violate policy;
- distinguish an observed Asset Processor run from a tool-initiated run and from editor/runtime authority.

Asset Processor success may prove only that a synthetic or workspace-owned preview projection was accepted by
the pinned authoring host. It does not prove Fall of Avalon compatibility.

## Required Tests For Future Implementation

Every future UI/preview implementation PR must include synthetic-only evidence for the surfaces it touches.
Required lanes are:

- static repository validation for source policy, package exclusions, protected-file avoidance, document links,
  and no runtime/deployment linkage;
- Foundation and Framework regression lanes for `TerrainHeightmapDocumentV1`, path containment, workspace
  staging, atomic publish, rollback, and package guards;
- Tool Gem or package manifest validation for deterministic registration, capability declaration, dependency
  boundaries, provenance, and no implicit authority;
- UI model tests for missing workspace/profile, unsafe source, validation issue mapping, stale source, changed
  importer, failed preview, authority flags, disabled commands, cancellation state, and redacted public output;
- pane lifecycle tests for registration, open, close, reopen, service detachment, command routing, restart, and
  no stale service pointers;
- import UI acceptance using synthetic PNG, TIFF, raw little-endian U16 sidecar, raw big-endian U16 sidecar, and
  malformed/unsupported input blockers;
- 2D preview tests for mip generation, deterministic pixels, issue overlays, stale-state invalidation, and
  nonblank render output;
- 3D preview tests for deterministic mesh/texture generation, camera framing, nonblank render output, blocked
  state, fidelity state, and no overlap or clipped critical UI text in captured editor evidence;
- edit/undo/redo tests for changed-tile deltas, brush bounds, quantization, revision fingerprints, close/reopen,
  interrupted save, rollback, and source-byte immutability;
- Asset Processor projection tests, if touched, for isolated project/cache roots, source-policy rejection,
  generated source fingerprints, observation/proof manifests, failure records, and product cache tokenization;
- visual validation tests or scripts proving screenshots are synthetic, redacted, bound to commit/O3DE pin, and
  free of private paths or protected content;
- package tests proving terrain manifests, local tile payloads, source observations, preview artefacts, `_gsi`
  source, Asset Processor products, and local logs are excluded from official package outputs by default;
- negative tests for path traversal, URI, UNC, symlink, junction, reparse point, alternate data stream, reserved
  device, case collision, oversized dimensions, decompression bombs, stale profile, stale source, and false
  runtime authority.

No test fixture may include FoA map samples, reconstructed terrain, extracted metadata, proprietary Unity
projects, game-derived preview products, private paths, saves, or generated products from protected game data.

## Required Performance Gates

Future UI/preview work remains bound by the WA-TH-001 import budgets and adds UI-specific budgets:

| Workload | Target |
| --- | --- |
| UI command dispatch after file selection | queues validated background work within 100 ms |
| UI thread blocking during import/hash/tile/mip/preview work | no blocking span above 50 ms in measured paths |
| 4097 x 4097 U16 import, validation, hashing, and preview mip generation | <= 5 seconds |
| 16385 x 16385 U16 tiled import and validation | <= 45 seconds |
| Peak incremental committed memory for 16385 x 16385 import | <= 768 MiB, excluding O3DE Asset Processor |
| Open-map tile cache default | <= 256 MiB |
| One changed 1024 x 1024 tile rehash and atomic revision update | <= 500 ms, excluding O3DE Asset Processor |
| Interactive brush preview over a 256 x 256 affected region | visible preview response <= 100 ms |
| 2D preview pan/zoom over cached tiles | no full-document copy and no visible layout hitch above 100 ms |
| Cancellation | stops at next tile/preview boundary and leaves no published partial revision |
| Determinism | byte-identical manifests and fingerprints across three identical synthetic runs |

Performance evidence must record CPU, memory, storage, Windows version, O3DE version/commit where applicable,
build configuration, map dimensions, tile count, command, source revision, cache state, and whether Asset
Processor time was included or excluded. Ordinary CI may use operation-count, allocation-count, fixture-size,
and deterministic-output guards where fixed wall-clock proof is not available.

## Visual Validation Gate

Before any Terrain Authoring pane, preview, or editor visual evidence is accepted as implemented, the PR must
provide a visual validation pack using synthetic content only:

- screenshot or rendered artefact for empty/missing-workspace state;
- screenshot or rendered artefact for import validation success with synthetic terrain;
- screenshot or rendered artefact for validation failure and blocked commands;
- screenshot or rendered artefact for 2D preview with deterministic issue overlays;
- screenshot or rendered artefact for 3D preview, if 3D preview is touched;
- before/after visual evidence for edit preview, undo, redo, save, and reopen, if editing is touched;
- screenshot or render metadata with commit, branch, O3DE commit, build configuration, workspace/profile token,
  synthetic source fingerprint, preview settings fingerprint, and capture command;
- automated checks for nonblank output, expected synthetic colour/height distribution, redacted private paths,
  no protected content, no critical text overlap, no clipped command labels, and stable dimensions at supported
  Windows DPI settings.

Visual validation proves only the editor presentation of synthetic or workspace-owned terrain. It is not Fall of
Avalon runtime proof and must be reported with `runtime sign-off not performed`.

## Evidence Pack Requirements

Every future implementation PR under this gate must include an evidence pack containing:

- authority source and gate ID (`WA-TH-002`) plus referenced `WA-TH-001` authority;
- exact branch and commit;
- protected-file audit;
- owner/blast-radius classification;
- changed file inventory;
- schema/contract compatibility statement;
- source-policy and package-policy results;
- test commands, logs, and pass/fail results;
- performance command, host facts, and budget results when performance-relevant code is touched;
- O3DE configure/build/Asset Processor/Editor evidence when those host surfaces are touched;
- synthetic fixture provenance and fingerprints;
- visual validation artefact manifest when UI/preview is touched;
- explicit statement that no FoA runtime, deployment, save mutation, signing, publication, or runtime sign-off
  was performed unless a later independent runtime authority exists.

Skipped gates must be listed with the reason and the remaining risk. A green static validator cannot substitute
for compiled tests, Asset Processor proof, Editor acceptance, visual validation, or runtime proof.

## Implementation Ordering

Future implementation should proceed in small reviewed slices. The recommended order is:

1. `TerrainAuthoring` package or Tool Gem shell, command contracts, service interfaces, registration tests, and
   package/source guards, without a visible pane.
2. UI model/view-model and static pane-render evidence using synthetic fixtures, without live O3DE preview
   projection.
3. Live editor pane lifecycle and import/validation presentation using synthetic and user-exported local
   heightmaps only.
4. Synthetic 2D preview generation and visual-validation evidence.
5. Synthetic/workspace-owned O3DE `_gsi` projection and Asset Processor proof in an isolated project/cache.
6. Synthetic 3D preview and edit-preview evidence.
7. Editing, undo/redo, save/reopen, revision lineage, and performance expansion.
8. Separate Road Atlas read-only consumer contract, if terrain sampling is needed by roads.

Any attempt to skip directly to game-derived four-map previews, direct installation scans, runtime deployment,
Road Atlas mutation, Asset Processor publication from protected content, or visual function-complete claims
invalidates this gate for that slice.

## Review Result

WA-TH-002 is accepted for bounded Terrain Authoring UI/preview implementation after this gate lands.

The accepted path is:

```text
WA-TH-001 contract and local import
-> WA-TH-002 UI/preview gate
-> future TerrainAuthoring implementation slices
-> synthetic-first UI render and editor pane proof
-> synthetic/workspace-owned preview proof
-> optional isolated O3DE Asset Processor projection proof
-> later separately gated consumer/runtime work, if ever authorized
```

This gate does not itself implement or validate a pane, preview, Asset Processor projection, editor visual pass,
or UI code. The next code slice must start from this accepted boundary and must keep Terrain Authoring local-only
and contract-first.

## Source Register

| Source | Relevance |
| --- | --- |
| `AGENTS.md` | Exact owner authorization requirement for gate document changes, repository-reading gate, PR-only delivery, and protected-file rules |
| `README.md` | FOA-SDK pre-alpha status, O3DE/FoA separation, branch process, generated-output boundary, and no runtime/deployment authority |
| `GOVERNANCE.md` | Significant-change design review, architecture invariants, evidence requirements, and fail-closed rule |
| `CONTRIBUTING.md` | Importer/data-format requirements, prohibited game assets, display-name identity boundary, and no unbounded install scanning |
| `CURRENT_TASK.md` | Records an older task; not edited by this exception and not used as current terrain authority |
| `DECISIONS.md` | Repository authority, research authority, and stop-on-missing-proof decisions |
| `docs/protected-files-policy.md` | Protected external data boundary, private path restrictions, generated output boundary, and pinned O3DE protection |
| `docs/systems/SYSTEM_INDEX.md` | Owner classification for `world-authoring`, `road-atlas`, `external-toolchain`, and runtime systems |
| `Research/README.md` | Research record rules and implementation-gate routing |
| `Research/world-authoring-terrain-heightmap/gates/WA_TH_001_TERRAIN_HEIGHTMAP_CONTRACT_AND_EDITOR_GATE.md` | Accepted local-only terrain heightmap contract, owner split, schema, tests, performance, O3DE/editor gates, and prohibitions |
| `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md` | Allowed synthetic fixtures and prohibited ripped/protected content |
| `docs/tainted-grail-sdk/DATA_FORMATS.md` | `foa.terrain-heightmap` V1 schema rules and local-only authority flags |
| `docs/tainted-grail-sdk/PATH_POLICY.md` | Canonical path containment, UI path suggestions, symlink/junction handling, and fail-closed persistence |
| `docs/tainted-grail-sdk/VISUAL_GAME_CONTENT_BROWSER_AND_PREVIEW_PIPELINE.md` | Preview identity separation and non-authority boundary |
| `docs/tainted-grail-sdk/FOA_ASSET_BROWSER_PANE_MODEL.md` | Pane model precedent for consuming proof evidence without editor mutation or typed binding |
| `docs/tainted-grail-sdk/FOA_ASSET_BROWSER_PANE_UI_RENDER.md` | Static UI render precedent and function-complete prohibition |
| `docs/tainted-grail-sdk/FOA_3D_PREVIEW_VIEWPORT.md` | Static 3D preview precedent and no live editor/runtime mutation boundary |
| `docs/tainted-grail-sdk/FOA_O3DE_ASSET_PROCESSOR_IMPORT_PROOF.md` | Asset Processor observation/proof precedent and product non-authority |
| `Plugins/Authoring/README.md` | Authoring plug-in authority limits |
| `o3de.lock.json` | Exact pinned O3DE version and commit for future host proof |
| Owner/maintainer authorization in the active Codex thread | Exact current-task exception for this gate path, branch, adjustment, and review/accepted-or-blocked decision |
