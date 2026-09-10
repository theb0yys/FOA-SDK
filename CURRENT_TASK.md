# World and Route Editor

Owner instruction: continue with the next system after faction and authority authoring.
Branch: `codex/world-route-editor`; base: `688c6f7cbecfab9c8002e91b353922a253442859`.
Dependency: faction PR #253 (it depends on encounter PR #252).
Classification: Significant. Primary owner: world-authoring.
Supporting owners: catalog-and-identity, schemas-and-persistence, workspace-and-packs, ui-framework.

Implemented [World and Route Editor](docs/tainted-grail-sdk/WORLD_ROUTE_EDITOR_GUIDE.md): pack-owned regions, scenes and locations; roads/routes; independently identified nodes/edges; plan-position and schematic graph previews; fixed save controls, dirty-draft guards and existing faction/encounter references. [Design](docs/tainted-grail-sdk/WORLD_ROUTE_EDITOR_DESIGN.md) records bounds, identity, evidence and migration.

Catalog readers accept schemas 1–5; saves emit 5. Older catalogs receive an exact verified backup before replacement. Workspace, pack, interchange and Road Atlas formats are unchanged. No engine, game, save, native-map extraction, deployment or runtime changes.

Validation:
- PASSED: full static validation, 827 Python tests (818 passed; nine explicit platform skips), plus all ten enabled source-policy checks per configured Gem.
- PASSED: pinned-engine configure and SDK Core, Framework, Editor and both compiled test targets. Unchanged exact-pin engine artifacts were reused; this was not a clean engine rebuild.
- PASSED: 452 catalog tests, including seven world-authoring groups; two Windows symlink tests explicitly skipped because required link privileges were unavailable. All 39 interchange tests passed.
- PASSED: private schema-4 Editor workflow, exact migration backup and prior economy/population/encounter/society preservation; graph rendering, malformed/dangling/cross-scene rejection, failed write, save/reopen/edit/remove and faction/encounter world references.
- PASSED: visual review of position/schematic/final graph captures and 860×640 controls. The Qt binding exposes C++ text items as base wrappers, so labels were verified from the actual captured Editor images. Maximum measured UI pause: 1.578 seconds (budget: 3 seconds).
- NOT_APPLICABLE: runtime, deployment, installer, Unity conversion and release proof. Runtime sign-off not performed.

Private evidence and QA data remain outside tracked source. Initial startup/native-picker/graphics-wrapper test harness failures are retained with the successful final run.
Implementation and local validation are complete. Next transition: focused DCO commit and PR handoff, with maintainer audit/merge left to the owner. No subsequent feature is authorized by this task.
