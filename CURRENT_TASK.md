# Campaign scene import and native editing

Status: PARTIAL. Full maps are not ready for 1:1 testing; 0/4 complete comparisons have passed.
Goal: faithfully import all four campaign maps into the existing Editor, preserve editable scene identities, and establish verified game export.
Classification: Significant for private source material/texture conversion contracts; external game export requires separate operational proof.
Primary owner: world-authoring. Game/runtime interpretation remains owned by the exact source profile and runtime adapter.
Authority: the owner requested completion, exact source fidelity, local validation, source commits and a pull request.
Current branch: codex/heightmap-importer-completion; draft PR #266.

Implemented: the numeric material upload route now has a companion measured Texture2D uniform route. All 25,590 captured material-layout combinations pack, including the 970 previously unresolved _HeightMap_TexelSize fields. The qualified null-black GPU resource is 4x4 but its uniform is (1,1,1,1); no reciprocal-size substitute is inferred. Three exact source Texture2D assets and null/default controls passed direct Unity D3D11 readback in both Linear and Gamma. Current source descriptors, complete receipts and qualified full-resolution resource selection are required; stale/unknown/overridden inputs reject.

Validation: 17 new unit cases, repeated Linear/Gamma GPU receipts, seven isolated Unity rejection cases and the 25,590-layout byte audit passed. Packing checked 378,601 fields and 14,533,904 bytes in 27.75 seconds against a 120-second bound. Original HDRP/Lit normal-buffer fragment execution also passed D3D11 tests for three captured material constant groups covering 485 materials, with synthetic normals and explicitly supplied rendering-layer bits. Six original-fragment/reference pairs also matched every RGB pixel in the pinned DX12 Editor, with two normal-flip controls differing as expected. Unity direct-render-texture facing agreed with O3DE; an incorrect default in the standalone D3D11 test harness was corrected and tested for both windings. Camera/pass inversion remains explicit and unqualified. A shader fixture is not complete game rendering.

Next action: advance the remaining original material pass inputs, actual campaign lighting/visibility and full scene assembly. Complete editable scene/instance assignments, the four-map UI and verified game export remain outstanding. There is no 1:1 full-map acceptance yet.

Integration: retain main's accepted M3 Framework orchestration and item/recipe workspace protection, all 34 pane registrations and CI dependency fixes. M2 remains default-denied; no M4/M5 or game/Unity launch through the Framework is introduced.
Protected boundary: game files and saves stay unchanged; proprietary captures, assets and native evidence remain outside Git. No merge, release or deployment is authorized by the source handoff.
