# Campaign scene import and native editing

Status: PARTIAL. Full maps are not ready for 1:1 testing.
Goal: faithfully import all four campaign maps into the existing Editor, preserve editable scene identities, and establish verified game export.
Classification: Significant for private draw contracts and Editor authoring; external game export requires separate operational proof.
Primary owner: world-authoring. Game/runtime state remains owned by the exact source profile and runtime adapter.
Authority: the owner requested completion, exact source fidelity, local validation, source commits and a pull request.
Current branch: codex/heightmap-importer-completion; draft PR #266.

Implemented in the latest source change: a strict 224-byte HDRP LightData codec preserves all 34 fields, signedness, padding and explicitly tagged non-finite bit patterns. The isolated Unity fixture runs original visible-light processing and initial GPU record preparation using captured hierarchy, enabled state, masks and baking inputs.
Validation: 3,469 non-directional campaign lights plus two controls were audited. Under explicit per-light qualification camera/settings, 3,446 source lights produced records, 22 source lights were absent from the native cull and one source light was filtered. All 771,904 record bytes matched. Native O3DE DX12 comparisons passed for 578,928 words across original, all-word corruption and per-light corruption cases, with 579,264 cleared-draw cell checks. Nine codec tests passed. The final static/source-policy receipt is recorded separately with the exact source hashes.
Limits: this is initial light selection/data preparation and native buffer transfer. It does not reproduce scene occluders, live controllers, final cookies/shadows, light volumes or full game lighting. Raw source records remain preserved; NaN payload identity is not inferred from decoded source value-tree floats. Original generated GPU NaN bits are preserved exactly.

Remaining: actual scene lighting/visibility, remaining material rendering, complete editable campaign scenes, the four-map UI and game export. Synthetic input proof does not establish campaign completeness or source visual equivalence. Per-entity edits are not yet propagated into shared instance batch matrix slots.
Integration: retain the incoming main changes, all 34 pane registrations and the CI dependency fixes. ExternalToolchain M2 remains default-disabled; this task does not authorize an M3 transition or game launch through that service.
Protected boundary: original game files and saves stay unchanged; proprietary captures, assets and native evidence remain outside Git. No merge, release or deployment is authorized by the source handoff.
