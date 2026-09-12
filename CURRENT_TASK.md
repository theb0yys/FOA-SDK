# Campaign scene import and native editing

Status: PARTIAL. Full maps are not ready for 1:1 testing.
Goal: faithfully import all four campaign maps into the existing Editor, preserve editable scene identities, and establish verified game export.
Classification: Significant for private draw contracts and Editor authoring; external game export requires separate operational proof.
Primary owner: world-authoring. Game/runtime state remains owned by the exact source profile and runtime adapter.
Authority: the owner requested completion, exact source fidelity, local validation, source commits and a pull request.
Current branch: codex/heightmap-importer-completion; draft PR #266.

Implemented in the latest source change: private native entity binding v4 updates explicit 48-byte object-matrix destinations in raw DOTS instance buffers. Individual source placements now drive the original shader after move, scale, parent rotation, undo/redo and saved reopen. Mutable resources are isolated per draw, including when two owners have identical binding JSON. Existing v1/v2 packets and v3 grouping remain readable; grouped members can opt into v4.
Validation: rebuilt the pinned Editor and all three compiled test targets; 617 compiled cases passed and two Windows symlink cases skipped. Thirteen original-shader render/reference pairs matched all 69,705,792 pixels exactly, including fresh reopen. Twenty-one malformed/immutable bindings rejected without admission/residency changes. Eight resident draws respected four updates per tick and all resources released. The Python assembly suite passed 17 cases; static validation passed 1,325 SDK cases with 33 skips and 10 ExternalToolchain cases. Source-policy and exact source hashes are recorded in the final private receipt.
Limits: the native fixture uses synthetic geometry and placements with an explicitly qualified sparse GPU slot and original unmodified DOTS shader programs. A v4 draw owns one rendered instance; multiple-owner shared batches, inverse-matrix synthesis and source-to-GPU slot inference are unsupported. This does not establish complete campaign instance binding or game visual equivalence. Prior initial LightData preparation/transfer receipts remain valid for their recorded scope only.

Remaining: actual campaign lighting/visibility, remaining material rendering, complete editable campaign scenes and instance assignments, the four-map UI and game export. Full maps remain unready for 1:1 testing.
Integration: retain the incoming main changes, all 34 pane registrations and the CI dependency fixes. ExternalToolchain M2 remains default-disabled; this task does not authorize an M3 transition or game launch through that service.
Protected boundary: original game files and saves stay unchanged; proprietary captures, assets and native evidence remain outside Git. No merge, release or deployment is authorized by the source handoff.
