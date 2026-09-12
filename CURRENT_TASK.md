# Campaign scene import and native editing

Status: PARTIAL. Full maps are not ready for 1:1 testing.
Goal: faithfully import all four campaign maps into the existing Editor, preserve editable scene identities, and establish verified game export.
Classification: Significant for private draw contracts and Editor authoring; external game export requires separate operational proof.
Primary owner: world-authoring. Game/runtime state remains owned by the exact source profile and runtime adapter.
Authority: the owner requested completion, exact source fidelity, local validation, source commits and a pull request.
Current branch: codex/heightmap-importer-completion; draft PR #266.

Implemented in the latest source change: private draw v3 submits explicit instance counts up to 4,096 through the pinned RHI with a bounded index-instance workload; v1/v2 remain single-instance. Synthetic fixtures cover sparse slots, direct/indirect visibility, original DOTS Unlit programs, saved bindings, hide/show and deletion undo/redo.
Before integration, nineteen native descriptors and sixteen rejection controls passed; direct 256 and indirect 4,096 instances rendered. Exact pixel checks passed for 4,676,922 interior and 37,073,875 outside pixels plus eleven same-resolution full-frame pairs. The prior build, 595 compiled cases and 1,249 static cases passed, with two compiled and nine static Windows symlink skips. Those receipts bind the pre-integration artifacts; combined-source validation must be recorded separately.

Remaining: actual scene lighting/visibility, remaining material rendering, complete editable campaign scenes, the four-map UI and game export. Synthetic input proof does not establish campaign completeness or source visual equivalence. Per-entity edits are not yet propagated into shared instance batch matrix slots.
Integration: retain the incoming main changes, all 34 pane registrations and the CI dependency fixes. ExternalToolchain M2 remains default-disabled; this task does not authorize an M3 transition or game launch through that service.
Protected boundary: original game files and saves stay unchanged; proprietary captures, assets and native evidence remain outside Git. No merge, release or deployment is authorized by the source handoff.
