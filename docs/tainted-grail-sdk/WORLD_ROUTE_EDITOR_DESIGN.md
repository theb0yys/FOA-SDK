# World and Route Editor design

## Scope and ownership
The owner requested the next expansion slice after factions and authority. This Significant change belongs to world-authoring. Core owns typed definitions, graph rules and deterministic analysis; Foundation owns evidence, pack ownership, candidate validation and persistence; the Editor owns controls and rendering. The independently owned Road Atlas snapshot extension remains unchanged and can be opened from this pane.

## Contracts
Pack-owned synthetic canonical records use domain `world` and kinds `region`, `scene`, `location`, `road`, or `route`. Names are presentation; every reference uses an exact stable ID.

WorldPlaceProfile stores record ID, parent ID, description, optional X/Z position, and evidence IDs. Regions have no parent; scenes reference a region; locations reference a scene. Parent references resolve exact saved canonical world records. Optional location positions are **scene-local plan units**, not measured game coordinates. Non-location positions are forbidden. Coordinates must be finite and between -1000000 and 1000000.

WorldPathProfile stores record ID, scene ID, optional road ID, description, travel-constraint notes and evidence IDs. A route may refer to a saved road in the same scene. A road cannot refer to another road. The reference is a planning association, not geometry inheritance.

WorldPathNode has its own stable node ID, owning path ID, exact location ID, notes and evidence. A location appears once per path and must have a saved WorldPlaceProfile in that scene. WorldPathEdge has its own edge ID, owning path ID, exact endpoint node IDs, travel mode (walk/ride/boat), bidirectionality, positive finite planning cost, notes and evidence. Cost is a planning weight, not travel time. Existing node and edge IDs cannot move between paths. Duplicate connections (including overlapping reverse/bidirectional edges) and self-edges are rejected.

Collections are bounded at 5000 places, 1000 paths, 10000 nodes and 20000 edges. Each path allows 128 nodes and 256 edges. Descriptions and constraints allow 2048 bytes; notes 1024; names 512; each is single-line. Exact evidence ID lists allow 1–64 unique IDs. Every save records current user intent for the profile and each node/edge with exact profile binding; reference evidence is validated separately. No authoring evidence grants runtime permission.

## Graph and editing
The preview draws node labels, directional edges and travel modes. It uses supplied plan positions only when all nodes have positions; otherwise it clearly labels a deterministic schematic layout. Empty/disconnected paths are valid drafts with review warnings. Invalid graphs cannot save. Removing a node with edges requires removing its incident edges first. References and graph edits validate on an unpublished candidate, preserving the saved catalog on failure.

The pane provides create/select/edit/save/revert, node and edge add/update/remove, exact-ID choices, visible errors, dirty selection/close/workspace guards, and scrollable forms with fixed save controls. Saved locations flow into existing faction jurisdiction and encounter placement choices through the canonical catalog.

## Persistence and compatibility
Catalog schema 5 adds WorldPlaces, WorldPaths, WorldPathNodes and WorldPathEdges. The reader accepts 1–5; the writer emits 5. Old-version files cannot carry new collections; future versions are rejected. Existing migration machinery preserves a content-addressed, byte-identical old-schema backup before replacement. Workspace, pack, interchange, Road Atlas and runtime schemas remain unchanged. Existing economy, population, encounters and society collections must survive migration.

## Validation
Run full SDK static validation/source policy; compiled Core, Framework, Editor, catalog and interchange tests at the pinned engine; synthetic authoring roundtrip and negative contracts; schema 4 backup/preservation and old/future rejection; failed writes and ownership/profile rejection; real Editor save/reopen, graph display, graph edits, cross-pane references, small-window controls and bounded UI pauses. Private catalogs/screenshots/build artifacts stay outside tracked source. Report each lane independently and retain explicit skips. No live-game travel/deployment claims are in scope.

## Acceptance result

PASSED on the pinned O3DE host: compiled world contracts and persistence tests, all catalog/interchange suites, and the actual Editor workflow with a private schema-4 catalog. The live run preserved existing collections, verified the exact backup, created and edited a route graph, exercised both preview modes, linked locations into factions/encounters, reopened and edited saved definitions, rejected invalid graphs, retained a draft after a forced failed write, and checked 860x640 controls. Captured graph labels and arrows were visually reviewed. Maximum measured UI pause was 1.578 seconds against a 3-second budget. Engine binaries from the same exact pin were reused; all changed SDK targets were compiled. Full engine rebuild and runtime sign-off are not claimed.
