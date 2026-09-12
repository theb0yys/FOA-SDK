# Campaign scene import and native editing

Status: PARTIAL. Full maps are not ready for 1:1 testing.
Goal: faithfully import all four campaign maps into the existing Editor, preserve editable scene identities, and establish verified game export.
Classification: Significant for private material conversion contracts; external game export requires separate operational proof.
Primary owner: world-authoring. Game/runtime state remains owned by the exact source profile and runtime adapter.
Authority: the owner requested completion, exact source fidelity, local validation, source commits and a pull request.
Current branch: codex/heightmap-importer-completion; draft PR #266.

Implemented in this continuation: source-bound saved material upload requests, direct pinned Unity D3D11 readback, receipt validation and original-layout constant packing. Actual GPU Color uploads differ from both saved numbers and Color.linear, so the converter retains exact measured bytes. Source edits, color-space changes, malformed receipts, unknown flags and missing/overridden uniforms reject. Numeric shader defaults and unknown runtime values are not inferred.
Validation: both Linear and Gamma runs passed for 875 source materials and 91,842 saved numeric properties using 34 generated declaration layouts; 973,176 property bytes matched shader output. Linear mode changed 479 Color properties. Native nonzero buffer-range checks, ten calibration cases, 90 invalid reads and a wrong-color-space rejection passed. Source-layout packing checked 24,620 combinations, 361,626 fields and 13,944,144 bytes. Seventeen new unit cases passed. Six isolated Unity output-path rejection cases also passed; rejected folders and existing diagnostics remain unchanged even during error reporting. Full validation and source-policy results belong to the final private evidence receipt.
Limits: the measurement shaders reproduce property declarations and explicit saved inputs; they do not execute original game shader passes. Another 970 layout combinations still require qualified _HeightMap_TexelSize values. This is an offline helper, not a completed four-map UI or full-map visual acceptance.

Previously completed: entity binding v4 updates explicit 48-byte raw instance matrix destinations from editable placements. Its pinned native proof matched 13 original-shader/reference pairs and 69,705,792 pixels with synthetic geometry and explicit sparse GPU slots. That evidence and the earlier initial LightData receipts remain bound to their recorded source and artifacts; they do not establish actual campaign visibility or complete lighting.

Remaining: actual campaign lighting/visibility, remaining material shader passes and texture uniforms, complete editable campaign scenes and instance assignments, the four-map UI and game export. Full maps remain unready for 1:1 testing.
Integration: retain main's accepted M3 Framework orchestration and item/recipe workspace protection, all 34 pane registrations and CI dependency fixes. M2 remains default-denied; no M4/M5 or game/Unity launch through the Framework is introduced.
Protected boundary: original game files and saves stay unchanged; proprietary captures, assets and native evidence remain outside Git. No merge, release or deployment is authorized by the source handoff.
