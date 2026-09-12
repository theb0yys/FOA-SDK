# Current Task

## Status

PARTIAL - full campaign scene reconstruction, native editing and return to the game. The owner expanded the task on 2026-09-08 to movable/editable game assets as entities, navigation, lighting and the rest of the scene. No estimated terrain. The former heightmap reconstruction route remains disabled.

## Goal

Import the actual Tainted Grail campaign into the existing O3DE Editor as editable entities with their real geometry, source identities, hierarchy, components and dependencies. Support terrain/object edits, navigation and lighting authoring, and a verified return to the game. A terrain preview, collider shell, empty entity hierarchy or generic lighting does not satisfy this goal.

## Classification and owners

Significant world-authoring and entity-placement work spanning Foundation identity/persistence, canonical interchange and ExternalToolchain source conversion. Game return, deployment and runtime verification are Critical/Runtime and separately owned. Follow [Campaign scene requirements](docs/tainted-grail-sdk/CAMPAIGN_SCENE_EDITING.md).

## In scope

- Read-only source inventory and private local reconstruction of campaign scene/assets.
- Exact entity identity, hierarchy, original transforms, geometry, materials and dependencies.
- Collision, navigation data and rebuild requirements; lights, probes, environment and bake requirements.
- Vegetation/instances, water, effects, audio, triggers and game component references, with explicit unsupported states until mapped.
- Native O3DE selection, move/rotate/scale, supported property editing, undo and persistence.
- Unchanged round-trip and controlled-edit proof before game export is offered.
- Existing native Editor layout and default-level improvements remain part of the authoring task.

## Boundaries

Keep source installation and saves read-only. Store all extracted data and diagnostics in the private local workspace, outside source control; do not commit/upload game content. No replacement custom editor. The owner requested committing all accumulated source changes and opening a PR on 2026-09-12, then continuing actual game lighting/instance data, materials, complete scenes, four-map UI and export. No merge, deployment or release is authorized. The requested return-to-game feature does not establish a supported format or grant permission for unreviewed game-install writes.

## Acceptance criteria

Every source component/dependency is accounted for. Source IDs are not inferred from display names, nearest positions or matching shapes. Unchanged data survives without estimated geometry or silently removed components. An edit affects only its intended source data and explicitly identified dependent data. Navigation, lighting and other derived data must be verified or block export when stale. Target-game acceptance is separate from source-record serialization and O3DE rendering.

## Current branch

`codex/heightmap-importer-completion`, isolated from concurrent original-checkout work.

## Current evidence and next action

Campaign rejection checks previously passed 45 Python tests, 59 compiled tests and an isolated Editor smoke. The expanded scene audit now passes 72 focused tests, including actual UnityPy synthetic serialization contracts. All 299,641 records in the four inspected Horns scene bundles decode/re-encode byte-for-byte with no skipped records or failures. A scoped null-reference writer correction resolves the previously observed 1,109 writer failures. All 31,797 custom components resolve to source script identities. Original source fingerprints remain unchanged. This is source-record preservation, not a complete imported map or game round trip.

The scene's three subscene GUIDs resolve in its catalog. Actual A* cache settings match the scene's serialized settings. Two renderer archive members bind through source data GUIDs and contain 10,890 instances; all 1,167 mesh subobjects and 287 material assets resolve through exact catalog types/providers and bundle container pointers across 281 unchanged bundles. Native renderer layout execution, original-object/collider joins, material conversion, lighting-product mapping and navigation rebuilding remain unverified. The renderer instance records do not contain GameObject IDs; do not guess those associations.

One actual placed renderer now has native O3DE evidence: its 205 positions and 250 oriented triangles survived processing, its two-level hierarchy imported within the recorded transform tolerance, and selection, move, undo/redo, rename, save and fresh-process reopen passed. Source tokens and the exact model binding persisted independently of its display name. This private geometry projection does not qualify full materials, other LODs/components or export; its test-only identity tags are not a product scene contract. The saved move also produces a private source Transform candidate: the unchanged case is byte-identical, only local translation changes in the edited case, and its world position agrees within 0.051 mm. This is not a game-ready payload. Next work is complete scene/entity assembly and supported component/dependency mappings, including material conversion and navigation/lighting updates. The complete import/edit/export workflow remains PARTIAL. Runtime adapter source currently only logs startup; no scene exporter is implemented.

The source ownership audit now verifies all 95,969 GameObjects and 202,748 components in the four explicit primary scene files, with reciprocal component and Transform links and no unowned component records. All 7,162 scene Drake renderers have consistent LOD-group references. Combined scene/archive references resolve to 1,668 mesh subobjects and 430 materials across 308 unchanged bundles. All 435 distinct CAB-scoped collider mesh references resolve; four additional component references to Unity's built-in resource file remain unqualified. Direct GameObject ownership joins are established; archive-instance-to-GameObject joins, derived products and the complete editable scene remain unresolved. `foa_scene_source_audit.py --scene-file` exposes reproducible ownership checks without treating shared prefab records as placed instances.


Mesh completion checkpoint (2026-09-10): all 1,668 bound mesh references (1,668 distinct
records) decode and project without source modifications. The focused scene suite has
102 passing tests. Full native cache auditing exposed host welding, generated invalid
tangent frames and overwritten tangent signs. The SDK source-preservation adapter is
compiled and passed seven native acceptance cases. All 1,668 meshes and 5,038,438
oriented triangles then passed native geometry/channel comparison, with zero failed meshes.
The 430 materials' 19 shader and 1,197 texture references have exact declared bundle
locations; native material conversion remains unfinished. See the campaign scene
requirements for exact current limits. Full campaign import/edit/game-return remains PARTIAL.

Texture completion checkpoint (2026-09-10): all 1,197 bound textures and 13,204 original
mip levels preserve their supplied bytes through the compiled native image builder.
The full cache audit passed 4,482 products with zero texture failures; 27 native
acceptance cases and five corruption-detection checks passed. The scene suite now has
118 passing tests. Source-oriented image data is qualified. Next is exact shader,
material, sampler and UV binding for the 430 materials and 19 shaders; generic PBR
substitution is not source fidelity. Complete scene assembly, dependent systems and
game return remain PARTIAL.


Material/shader binding checkpoint (2026-09-10): all 430 materials now pass exact
source Shader/property/texture binding, including shader-owned textures and all
saved fields. The 19 direct shaders reference 21 additional fallback Shader
records; all 40 identities remain distinct. All 5,037 segmented shader entries are
accounted for: 4,110 code records and 927 parameter records. Microsoft disassembly
and source binding checks cover all 4,254 variants and 40,867 resource registers.
No missing registers or unreferenced entries were accepted. The scene suite has
156 passing tests; eight independent native shader cases passed.

Sampler and GPU UV checkpoint (2026-09-10): 27 synthetic Unity 6000.0.64f1 compiler
cases establish the inline anisotropy codes. Source sampler 0x801 requests linear
filtering, repeat on all axes and 16x anisotropy. All 4,254 source variants now have
zero unresolved inline sampler encodings; effective runtime quality policy remains
separate. All 96 material/shader source bundles retain their checked fingerprints.

The actual O3DE viewport test exposed a native texture binding defect: material
source paths resolve streaming images with sub-ID 1000, while our builder used 0.
The builder now uses the pinned engine's image sub-ID and 1001+n mip-chain IDs.
Builder version 2 invalidates generated products. The rebuilt target, 27 native
texture cases, five corruption checks and all 1,197 textures / 13,204 mips / 4,482
native products pass. The focused scene suite has 157 passing tests.

The synthetic Unity GPU and native O3DE viewport agree on asymmetric raw texture
rows and UV0 recovery. Native scale/offset tests include negative scale; two deliberate
V/transform corruptions produce their independently predicted wrong colours. All
144 sampled native pixels pass. This qualifies that synthetic UV/image route, not
the game's 19 shader implementations, packed normals, lighting or complete appearance.
Native game shaders, shader defaults/global state, full map editing, derived systems
and game export remain unfinished.


Native shader execution checkpoint (2026-09-10): all 4,110 preserved programs from
40 exact shader records are accepted by a hardware D3D11 device with its debug
layer enabled and no warnings/errors. This is device acceptance, not a draw of
every variant. The selected non-instanced HDRP Unlit ForwardOnly vertex/pixel pair
now executes unchanged source DXBC in a private headless fixture. Eight source
render cases cover UV scale/offset, negative scale, tint, emission/exposure,
signed integer alpha control, object translation and view-matrix packing.
The complete source fixture matrix passes 19 cases and 368 pixel samples,
including explicit wrong-output and rejected-input controls. Missing source
bindings fail before GPU submission; a legal null texture returning black fails
the normal colour oracle. The scene suite has 167 passing tests.

This adds strict common/variant constant-buffer joining and explicit value packing
for supported float/int vectors and float4x4 columns. It does not infer material
colour conversion, defaults, runtime globals, pass selection, arrays or structures.
The headless fixture is validation infrastructure, not a replacement editor or an
O3DE game-material implementation. Next is the remaining shader input/pass mappings
and native O3DE material/render-pipeline integration, with rendered parity evidence.
Full visual matching, scene editing, derived systems and game export remain PARTIAL.

## Four-map import checkpoint (2026-09-10)

The current owner request is to import and manually compare all four real campaign
maps: Horns of the South, Cuanacht, Forlorn Swords and Sarras. No new empty/local-file
importer handoff qualifies that request. Complete native campaign assembly and
one-to-one appearance testing remain unfinished.

The source-selection layer now follows each root's exact SubdividedScene script
identity, serialized GUID references and AssetBundle scene-hash table. All four
cohorts resolve to four primary scene files each. Their 315,463 GameObjects and
672,475 components have reciprocal source ownership; all 993,337 source records
round-trip byte-identically with no skipped records or unresolved script identities.
This establishes source preservation, not runtime loading policy or native editing.

Combined direct Drake and merged-archive references resolve to 3,185 meshes and
875 materials across 331 unchanged bundles. The typed selector supports both named
mesh subobjects and main Mesh assets, including two main-asset references found in
Forlorn. Every one of the eight source-bound merged members rebuilds byte-identically;
87,229 instances retain their matrix columns, definition, LOD-group reference and
ordered material slots. Archive ordinals are not guessed GameObject identities.
All 3,185 meshes decode and project successfully, with 11,496,039 vertices. The
expanded native comparison also passes: 13,207,074 oriented triangles, every
projected vertex channel, UV and ordered mesh material slot across 23,862 products.
This covers the collected direct Drake and merged-renderer mesh set; other renderer
families and complete scene assembly still require coverage.

There are 211 passing focused scene tests and twelve passing pinned source-policy
validators for this slice. Material source inventory now finds 34 direct Shader
records and 2,240 saved texture references: 15 shaders and 1,043 textures extend
beyond the previous Horns qualification. Native materials, full scene assembly,
lighting/navigation and game export remain PARTIAL. Do not launch or label the maps
as ready for a one-to-one user comparison until the actual native scene is assembled
and its supported rendering/component coverage is explicit.

The broad static validator set now passes after moving the native source mesh and
texture builder implementations into Framework (the existing module still registers
them). Seven build-graph contract tests pass. Actual source inspection binds all
34 Shader and 2,240 Texture2D identities across 445 unchanged dependency bundles.
All 2,240 textures now preserve their 24,959 mip levels through 8,612 native products.
The additional Alpha8 format has independent Unity and O3DE GPU evidence: RGB is
zero, alpha retains its data value for both source colour-space flags, and all three
synthetic mip levels match the flat RGBA reference. Four native capture cases pass
756 pixel samples; three deliberately corrupted captures are rejected. Seven native
mesh cases, 31 texture cases and five native-image auditor corruption checks pass.
The rebuilt image builder uses version 3 to invalidate older generated products;
component and packet identities remain unchanged. Native prerequisite reprocessing
clears all unintended asset failures. These checks qualify image data and synthetic
sampling, not the 875 materials, game shaders or complete map appearance.

The source-slot census covers 15,675 direct Drake renderers and 87,229 merged
renderer definitions. Exact source inspection establishes material ordinal to
requested submesh selection; nine synthetic Unity BRG cases independently qualify
out-of-range selection of the final submesh and retained repeated draws. All
127,883 serialized draws have a geometry plan, including the 72 unequal-count
bindings. Source baking hooks and the game's final draw-bin order remain NOT_RUN.
The inspected Entities renderer sorts bins using material/mesh/filter identities;
material-list order alone must not be treated as runtime submission order.

A native GPU test caught reordered repeated opaque draws in a combined model.
Separately ordered native draw entities now pass four cases and 108 sampled pixels;
three wrong-order, missing-draw and extra-draw captures are correctly rejected.
The nine Unity captures also pass an independent 243-pixel check. This qualifies
explicit synthetic ordering and geometry selection, not the game's final bins,
material pipeline or map appearance. Original inspected game assemblies remain
unchanged. All 32,474 collected native mesh/image products pass a final hash recheck.

Full four-map user testing remains NOT_RUN: no complete native campaign scenes or
usable four-map importer workflow have been assembled or launched. Remaining work
includes full renderer/affine hierarchy/LOD/streaming mapping, 875 materials and 34
direct shaders with their defaults and runtime inputs, navigation, lighting and
other scene components. Game export and unchanged/controlled-edit runtime checks
remain unimplemented/unexecuted. Do not substitute an empty level, geometry-only
projection or these synthetic GPU fixtures for the requested maps.

## Source transform and four-map shader checkpoint (2026-09-10)

All 315,463 source transforms now retain their original hierarchy, local TRS and
record identity. The installed UnityPlayer file reports 6000.0.64f1; its before/after
fingerprints match. A private Unity Editor with that exact version evaluates the
serialized transforms, retaining every evaluated float32 matrix bit, including
signed zero. Normal quaternion property assignment changed source values; direct
serialized assignment preserves every numerical TRS value. The 221,972 signed-zero
canonicalizations are recorded separately and original source bits remain intact.
This is serialized Transform evaluation, not execution of game scripts or baking.

The four-map capture audit passes all 16 scene files plus 128 synthetic transforms.
Four altered real-capture controls reject changed rotation, world translation,
incomplete rows and stale input identity. All 15,675 direct renderer offsets also
bind to their exact owning Transform; 4,659 are nonidentity. Their original offset
matrices remain distinct from diagnostic mathematical composition. Native source
baking and the final game renderer matrix execution remain NOT_RUN.

Native full-affine arithmetic passes six synthetic viewport cases and 72 pixel
samples, covering nonuniform scale, reflection, shear, parent scaling and renderer
offsets. Two wrong-pose/normal capture controls reject. A model-loading race in the
fixture was fixed by waiting for its named material slot before assignment. This
qualifies shader arithmetic; a complete source-linked native transform component,
hierarchy editing, bounds, picking, instancing and persistence are not implemented.
Pinned native Transform decomposition cannot preserve the full source affine data.

Material/shader source coverage now spans all 875 materials and 70 distinct Shader
records (34 direct and 36 additional fallback records). All 7,309 entries partition
into 5,838 programs and 1,471 parameter records. Disassembly and source declarations
agree for 6,012 variants and 54,131 resource bindings; no inline sampler encodings
remain unresolved. A hardware D3D11 device accepts every one of the 5,838 programs
without debug warnings or errors. Device creation is not rendered visual parity.
All 113 source bundles checked by this stage retain their fingerprints.

The 107,238 declared material properties retain 14,487 undeclared saved fields;
11,257 texture properties still require effective shader-default qualification.
Native game material/pipeline integration, effective render state and runtime
inputs remain unfinished. The focused scene suite has 226 passing tests. Full
four-map scene assembly, usable importer launch, user one-to-one comparison and
game export remain NOT_RUN. The owner request is still PARTIAL.

## Native source shader checkpoint (2026-09-11)

Implemented the SDK's private native DX12 shader builder and explicit drawing
boundary. The pinned host build passes, two native asset cases pass and five
malformed packets reject. Native variants retain four original program byte
sequences unchanged. The original source HDRP Unlit shader pair now renders in
O3DE with explicit fixture inputs; its complete captured color buffer matches the
synthetic reference and recreated draw. Forty-eight sampled pixels, twelve
invalid binding cases and two wrong-image controls pass. This is one source
shader pair, not all game materials or full-map fidelity. The first output-path
failure and two post-capture engine shutdown warnings are retained in evidence.

Complete native scene entities/instances, remaining shader resource and pass
families, effective defaults and runtime inputs, lighting/navigation and the
usable four-map importer are still unfinished. The owner explicitly requested
these three surfaces on 2026-09-11; the native shader checkpoint does not complete
that request. Details and bounds are in CAMPAIGN_SCENE_EDITING.md.

## Native source placement checkpoint (2026-09-11)

The source placement component now retains captured affine matrix bits and exact
source identity in native editor entities. The native build and static suite pass:
1,064 tests discovered, 1,056 passed and eight host-privilege cases skipped. Twelve
pinned source-policy validators cover the cumulative 46-file request scope.

Native component acceptance passes 16 objects: nine source objects in selected
hierarchies from all four campaign roots and seven synthetic affine controls.
All unchanged matrix bits are retained. Selection through the editor API,
numerical bounds, moving, undo/redo, parent rotation/scale and saved reopening pass.
A fresh Editor process preserves all 16 edited matrices bit-for-bit and resolves
the same source identities and native parents. Nineteen invalid bindings and three
independent corrupted-evidence controls reject. The independent edited-matrix
reference differs by at most 0.000000799 in its tested coefficients. This is not
triangle picking, native game rendering or full campaign assembly.

Failed preliminary runs are retained: exact-comparison API misuse, integer
arguments in the native Python fixture, and a malformed reopen launch path. The
private fixture with its Asset Processor connection disabled also reports missing icon/scan-folder diagnostics
and post-test pipeline shutdown warnings; no zero-warning Editor claim is made.

Native renderer/entity integration, all material/pass/resource families and runtime
inputs, full scene/component coverage, lighting/navigation and the four-map importer
workflow remain unfinished. No complete campaign map is ready for a one-to-one
user comparison, and game export remains unimplemented. The owner request is PARTIAL.

## Shader-default sampling checkpoint (2026-09-11)

The remaining 11,257 unresolved material texture properties reduce to six source
default-name/dimension pairs. A synthetic Unity 6000.0.64f1 Direct3D11 GPU fixture
now measures all six in both Gamma and Linear color space, with unset, global
override, material override and explicit-null operations. Sixty draws and 240
pixel samples pass an independent audit; three corrupted-result controls reject.

The observed black default has zero alpha. Bump and linearGrey use 127/255 in the
tested channels. Named 2D defaults resist global overrides; the empty 2DArray
binding accepts them. Grey RGB and the array fallback differ between Gamma and
Linear. The array cases cover 2,619 material properties whose runtime global inputs
must be resolved; the named 2D defaults cover 8,638 properties. No one-pixel or
zero-array replacement has been substituted for native game resources.

Native default resource dimensions, mip contents and sampler state still need
qualification and projection. The installed game settings record did not expose an
embedded schema, so its color-space value was not inferred. The input file remains
unchanged. These sampling measurements do not finish native game materials, full
scenes, the four-map importer, manual comparison or game export. Scope now includes
47 source/document/test files, with prior unrelated working-tree changes retained.

## Native entity rendering integration (2026-09-11)

The persistent source render component connects native placement and visibility to
original-program drawing. Ten GPU edit/reopen states pass: each full color buffer
is byte-identical to separately transformed reference geometry, with 432 sampled
pixels and three corrupted-image controls. Nineteen invalid bindings reject.
The eight-draw batch stays within four resource builds/updates per tick and
releases removed payloads. A fresh process restores the two source-shader draws.

This qualifies one HDRP Unlit pair with synthetic geometry and fixed camera inputs.
Complete scene assembly, all game materials and the four-map workflow remain
unfinished. Preliminary build, source-policy, comparison-control and inactive
Editor timeout failures are retained. The successful unattended launch uses the
pinned host's --autotest_mode with helper icons disabled for full-image comparison.
Native build and static validation pass; no complete campaign map is ready for a
one-to-one handoff. The cumulative request scope is 50 files and remains PARTIAL.


## Native camera and shader-default continuation (2026-09-11)

Explicit version-2 entity render bindings now update original-program camera
matrices from the native viewport. Absolute and camera-relative projection forms,
source-axis camera position, translation/rotation and fresh-process restoration
pass twelve GPU states. Complete color buffers match the independent reference
for every state; 528 sampled pixels, 29 invalid bindings and three corrupted-image
controls pass. Version-1 saved bindings remain readable without rewriting them.
The current proof covers one source HDRP Unlit pair and synthetic placements.

Synthetic default-resource tests measured six declaration/dimension pairs in both
color spaces: 618 GPU Load texels across 36 resources and their mip/array controls.
A separate native D3D11 probe captures 180 bindings, 1,998 raw texels and 720 GPU
samples across both color spaces and three anisotropy modes. Six deliberately
corrupted capture reports reject. Event callbacks returned unbound state; the
successful capture directly follows DrawProceduralNow, guarded by verified Direct
threading. Failed attempts are retained. These measurements do not identify the
game's live color-space setting, global arrays, lights or other runtime inputs.

The five measured 2D defaults can now produce existing v1 native texture packets
with exact dimensions, mip count, RGBA8 bytes and explicit color-space handling.
The native sampler reader now accepts the exact finite float32 limit and uses
full-precision JSON parsing: the default parser was measured rounding that value
upward. Ten native default images preserve exact cache bytes/formats and match
independent reference color buffers. Twenty-four captures, 116 sampled pixels,
15 rejected bindings and three corrupted-image controls pass. Earlier failed
native attempts remain recorded.
Complete campaign assembly, all game materials/passes, four-map user testing and
game export remain unfinished. The cumulative 52-file request remains PARTIAL.


## Native texture-array continuation (2026-09-11)

The native texture builder now accepts explicit version-2 2D arrays while
preserving version-1 2D packets byte-for-byte. Single-layer arrays keep their
array view. All supported formats retain mip-major/layer-major payloads, including
rectangular block mips and streamable mip chains. Layer count and aggregate
source/native byte bounds reject invalid inputs before product publication.

Native verification passes 34 valid image cases, 12 malformed-packet rejections
and eight corrupted-cache controls. Twenty-four GPU comparisons across single-
and three-layer arrays, three mips, Load and point SampleLevel match complete
reference color buffers. Forty captures and 780 exact RGB samples pass, with
four wrong-layer/mip/UV/missing-draw controls. The initial capture audit assumed
800x450 logical viewport dimensions; the actual 2376x1503 window attachments are
now recorded and validated without changing any captured pixels.

The pinned native build passes. Static validation discovers 1,072 tests, passes
1,064 and retains eight governed skips. The cumulative scope is 53 files. These
results qualify array resource transport and sampling, not the contents of the
game's runtime globals or full material rendering. Complete native scenes, all
materials/passes, the four-map importer and game export remain unfinished. No
complete campaign map is ready for a one-to-one user handoff.


## Stored color-space continuation (2026-09-11)

The exact Unity editor now emits synthetic Gamma/Linear PlayerSettings records
with embedded schemas. Both schema hashes match the inspected game's stored
record, and native binary inspection confirms the control values. A narrow
read-only reader identifies the game's stored value as Linear. Fourteen malformed
input controls pass; an existing-output retry rejects without changing the prior
probe report. No game file changed. The four-byte suffix remains explicitly opaque,
so this is scalar proof and whole-record interpretation remains PARTIAL.

Native texture-array transport and sampling are verified. The cumulative source
scope is now 56 files. Complete native scenes, remaining game materials/runtime
inputs, the four-map importer, one-to-one user testing and game export remain
unfinished. The stored setting does not establish live game rendering behavior.


## Native scene resource sharing (2026-09-11)

The renderer now shares immutable vertex/index bytes and immutable shader stages
while keeping source identities, layouts and per-object matrix constants separate.
Admission supports 1,024 entity draws. A dense scheduler limits work to 64 visits
and four resource builds/placement updates per tick. Resident raw payload is
bounded at 64 MiB, and the service reserves at most 512 sampler entries.

The native test passes 1,024 repeated synthetic objects using three geometry
buffers and one immutable material stage. The capacity phase takes 6.735 seconds
in this fixture; this is not full-map frame-rate proof. Different UV bytes remain
separate. Capacity, byte-budget and sampler-budget rejection leave existing
objects intact. Final-owner cleanup returns all service resource counters to zero.
Twelve complete RGB comparisons pass, including edits, undo/redo, visibility,
deletion, distinct UVs and fresh-process saved reopening: 528 independent sampled
pixels, 22 rejected bindings/capacity controls and five corrupted-evidence controls.

The first capacity run exhausted native sampler tables; shared immutable stages
and a pre-allocation reservation bound correct that failure. A malformed reopen
launcher path was corrected without creating the unintended path. The optional
Get-AgentTestPlan helper has a pre-existing parse error; the applicable lanes were
recorded directly from CI_AND_LOCAL_VALIDATION.md. All failures remain in private
evidence. Native build and static validation pass (1,072 discovered, 1,064 passed,
eight governed skips). The cumulative request scope is now 57 files.

Complete native campaign assembly, other material/pass families and runtime inputs,
large-map partitioning/culling, the four-map importer and game export remain
unfinished. No complete map is ready for one-to-one user testing.


## Native source-hierarchy assembly (2026-09-12)

The private hierarchy producer now preserves all 315,463 source objects across
16 source scenes. The native consumer creates explicitly selected objects with
all ancestors, exact source identity/matrix bindings, names and effective
visibility. A single undo transaction commits complete prefab state before each
Qt yield. Cancellation and later failures roll back created objects while
preserving unrelated entities; cleanup failures retain explicit recovery IDs.

Native acceptance passes 5,204 entities across all four campaigns: four complete
root hierarchies and explicit ancestor-closed batches from the other 12 scenes.
Every matrix coefficient, parent, name, binding and visibility value matches after
import, save and reopening in a fresh Editor process. Seven native controls cover
cancellation, injected later failure, undo/redo, duplicates, wrong-bundle parents,
invalid anchors and a rounded-anchor edit. Ten corrupted-evidence controls reject.
These are bounded hierarchy results, not complete rendered campaign scenes.

Two precision boundaries are handled explicitly. Version-2 private placement
bindings retain the native parent-relative anchor separately from immutable source
matrix bits, preserving unchanged source values while supporting native edits.
Integer matrix readback avoids subnormal values being flushed by float-to-double
script marshalling. The real-scene test includes one subnormal coefficient and a
rounded Forlorn placement. All three older version-1 bindings and both previously
recorded edited matrices reopen unchanged; their saved level is not rewritten.

Initial preparation, modal-cancellation, undo-cache, precision, build-lock and
legacy-fixture failures remain recorded. The old legacy fixture initially omitted
the parent binding and compared source-coordinate values with host-coordinate
values; its corrected check uses the existing saved file and prior captured edits.
Native build and acceptance pass. Original game source hashes remain unchanged.
The cumulative source scope is now 62 files. Full scene/render assembly, remaining
material/pass families and runtime inputs, large-map loading/culling, four-map
user testing and game export remain unfinished. No full map is ready for 1:1 testing.

## Native grouped rendering (2026-09-12)

A source placement can now own an explicit group of up to 128 native draws. Each
submesh/material draw keeps its geometry, constants and requested order while
following the same native object edits and saved binding. A malformed or failed
member prevents partial object rendering; later admission failures release all
earlier reservations. Existing single-draw binding versions 1 and 2 remain readable.
The new private version 3 is not a game-export or canonical interchange format.

Pinned native build and acceptance pass. Three native entities with four draws,
including two distinct material UV settings from the qualified original Unlit
shader layout, match independent references in 13 complete RGB frames and 1,152
color samples. Move/rotate/scale, undo/redo, visibility, delete/restore, failure
suppression and fresh-process reopening pass. Fifteen native rejection cases and
eight corrupted-evidence controls pass. Aggregate capacity reaches 1,024 draws;
work remains bounded to four builds/updates and 64 visits per tick. Late byte-budget
failure leaves live resources unchanged, and final release returns their counts
to zero. This is bounded mechanism acceptance with synthetic geometry and one
original shader pair, not complete scene/material-family or game draw-bin parity.

Initial capture comparisons included changing native diagnostic text; the fixture
now explicitly disables it and compares whole frames without masking. A wrong-pose
control now compares complete images because interior color samples alone cannot
detect small movements. The distinct-material fixture requires two immutable
material states; merging them is an explicit rejected evidence control.

The cumulative source scope is 64 files. Complete rendered scenes, remaining
material/pass families and runtime inputs, large-map loading/culling, the four-map
importer interface and game export remain unfinished. Full maps are not ready for
1:1 testing. Original game files and saves have not been modified.

## Rendered hierarchy import (2026-09-12)

The private World Authoring handoff now joins exact placement descriptors to
explicit qualified native render groups. It rejects stale hierarchy bytes and
unknown or duplicate placement identities before mutation. Hierarchy-only packets
remain compatible. Native importing creates, binds and verifies the complete
objects in one modal undo transaction, waits for rendering readiness and rolls
back all created entities on cancellation, binding/resource failure or timeout.

Focused unit tests pass 29 cases. The native transaction passes source preflight,
late binding/resource failures, cancellation after two creations, readiness
timeout, duplicate detection and whole-import undo/redo. Undo/redo of each failed
transaction also preserves the original level without resurrecting partial
objects. Readiness polling is bounded to the existing 32-operation step budget.

The native fixture uses synthetic geometry and one previously qualified original
Unlit shader pair. The background test host must explicitly set
`ed_keepEditorActive=1`; without it, losing focus pauses rendering and the first
whole-import redo test timed out. That failed run remains evidence. No native
renderer defect was established by that timeout.

The cumulative source scope is 66 files. This connects prepared hierarchy and
rendering batches; it does not supply the missing source renderer ownership,
material/pass selection, runtime inputs or unsupported component mappings.
Complete campaign scenes, remaining materials, four-map importer UI, large-map
loading/culling and game export remain unfinished. Full maps are not ready for
1:1 testing. Original game files and saves remain unchanged.

## Direct renderer ownership and import preparation (2026-09-12)

Production preparation now joins source Drake renderer components to the exact
hierarchy placements, mesh selectors, ordered material references and reciprocal
LOD groups. It can emit the existing native import packets for explicit qualified
render selections, preserving multiple renderers/material draws on one owner and
all selected ancestors. Stale, duplicate, missing or ambiguous inputs reject.

All 16 original scenes across the four maps pass the source join: 315,463 hierarchy
objects, 15,675 direct renderers, 8,313 LOD groups and 17,645 serialized material
draws. Every renderer matches the earlier independent source inventory. The 1,514
referenced mesh/material assets resolve through exact prior asset records, and
consumed scene/script/asset bundle fingerprints remain unchanged. This measures
source association and material ordinals; it is not rendered map parity.

Forty-five focused unit tests pass. Preparation finishes in 64.109 seconds for
all scenes and source verification on this machine; each scene's join stays below
the enforced 45-second preparation limit. The overall operation has a 600-second
timeout and the code retains explicit record/decode/edge bounds.

The cumulative source scope is 68 files. Complete rendered campaign scenes,
merged-instance owner mappings, remaining material/pass families and runtime
inputs, four-map importer UI, large-map loading/culling and game export remain
unfinished. Full maps are not ready for 1:1 testing. Game files and saves are
unchanged.

Native acceptance also passes packets generated through this new source join:
three synthetic source owners/four draws, exact edit/undo/redo and failed-import
history, saved reopening, 13 full RGB frame comparisons and 1,152 material samples.
The original qualified Unlit pair is used; no additional shader family is promoted.
The final static suite discovers 1,116 tests, with 1,108 passing and eight governed
Windows symlink skips. All 12 pinned source-policy validators pass the 68-file
scope. No C++/build changes or new build are claimed for this continuation.

## Merged archive instance import (2026-09-12)

Merged scenery now imports through its actual archive/member/instance-ordinal
identity. It does not invent a source GameObject or apply a loader transform.
The production packet retains each original instance record and verifies its
world-matrix bits before native mutation. The existing rendered import transaction
accepts these placements and preserves failure rollback and undo/redo.

All 87,229 instances in eight members across the four maps pass exhaustive bounded
preparation. Every expanded matrix bit matches the original managed game method;
the original instance struct also confirms 56-byte size and offsets 0/48/52.
This method/layout probe runs under .NET 10, not the game player. Source preparation
finishes in 17.859 seconds under its 600-second budget. The original archive and
consumed scene/managed inputs are verified read-only.

The rebuilt pinned O3DE host imports 40 selected real instances across all eight
members with exact matrix readback and complete undo removal. A synthetic grouped
render fixture using the previously qualified original Unlit pair passes all 13
complete RGB frame comparisons, 1,152 material samples, native edit/undo/redo,
failed-import history, cancellation, readiness and saved reopening. Seven new
native malformed placement controls reject. This does not qualify other materials
or full-map rendering. Older saved v2 hierarchies (5,204 objects over 16 scenes)
and three v1 bindings reopen correctly, with exact prior state and unchanged levels.

Forty-four focused tests pass. The full static suite discovers 1,122 tests: 1,114
pass and eight governed Windows symlink cases skip. All 12 pinned source-policy
validators pass the 70-file cumulative scope. Native binding version 3 is an
explicit archive-identity addition; component serialization and v1/v2 readers
remain unchanged. Earlier builds reject v3 and require retaining their old levels.

Remaining: complete rendered scenes and component mappings (including navigation,
lighting and runtime LOD behavior), remaining material/pass families and runtime
inputs, viewport picking, full-map streaming/culling, four-map importer UI and game
export. Full maps are not ready for 1:1 testing. No game/save mutation or upload
occurred. The obsolete estimated campaign-heightmap route remains disabled.

## Source constant arrays and structures (2026-09-12)

The material converter now packs declared float4/int4 arrays, column-major
float4x4 arrays and flat variant structure arrays using their original byte
locations, member-relative offsets and explicit structure strides. Every value
and every array element is required. Missing values, incompatible shapes,
unaligned/overlapping fields, bad strides and expanded-work overflow reject;
no game globals, renderer state or material defaults are inferred. Buffers remain
bounded to 64 KiB and 4,096 expanded fields. Existing non-array layouts and native
shader/draw packet versions remain unchanged.

All 6,012 stage layouts from the 70 captured shader records now validate, including
631 with arrays and 2,999 with structures. The final source census takes 6.672
seconds on this machine; all 140 cached shader inputs and their 34 original bundles
retain their audited hashes. This is declaration coverage, not full shader rendering.

Thirty focused tests pass. Synthetic compiler reflection and hardware GPU tests
verify vector/integer/matrix arrays, structure strides and relative offsets;
deliberate wrong-stride and transposed-matrix payloads fail the pixel oracle.
The game's original Unlit instancing pair selects array element 1 under explicit
fixture values and matches the existing noninstanced reference. Its original
program bytes survive native processing. Four pinned O3DE captures have identical
full RGB buffers, with 180 sampled pixels, 15 native binding rejections and two
comparison controls passing. The measured viewport is 2,919 by 1,503; the requested
800 by 450 test size was not applied. No full-map viewport-size claim is made.

The static suite discovers 1,134 tests: 1,126 pass and eight governed Windows
symlink cases skip. An additional focused rerun verifies the final work-bound
checks. This continuation changes Python conversion/tests and documentation;
it reuses the exact previously built native DLL without claiming a new C++ build.

Remaining material work includes complete source value/global/instance policy,
structured GPU resources, lighting/pass dependencies and all remaining shader
families. Complete rendered scenes, four-map UI and game export remain unfinished.
Full maps are not ready for 1:1 testing. The estimated campaign-heightmap route
remains disabled; original game files and saves were not written or uploaded.

## Native read-only shader buffers (2026-09-12)

The native source renderer now supports explicit raw and structured GPU buffers
in vertex and fragment stages. The converter reads types/registers/strides from
compiled SM5 declarations and checks them against serialized source bindings;
it does not infer contents from resource names. All 5,944 SM5 stages pass the
independent declaration comparison, including 1,519 buffer-consuming stages with
2,422 raw and 3,246 structured declarations. The other 68 SM4 stages remain outside
the qualified native profile. Eleven structured-buffer strides occur in this set.

Private shader and draw packets add explicit version 2 buffer descriptors. Old
version 1 packet bytes and readers remain supported. Buffer data shares the
existing 64 MiB resident limit and has separate accounting; malformed, missing or
incompatible bindings reject. The native shader builder is version 2. Component
serialization is unchanged. Earlier builds cannot consume the new packet format;
retain the prior build and saved levels for downgrade. No game-format migration
or export is introduced.

The pinned native module builds. Thirteen valid shader assets retain all 26 program
payloads, including unchanged original Unlit DOTS and architecture shader pairs;
three deliberately inconsistent buffer layouts fail processing. Original shader
asset construction does not qualify their runtime buffer contents or rendering.
Native GPU tests exercise raw/structured inputs in both stages and all eleven
observed strides. Sixteen complete RGB frames match the qualified reference;
four wrong-input/removal frames fail that reference, with 720 sampled pixels and
31 native rejection controls passing.

That test exposed a false-ready defect: a failed native pipeline could be returned
as a non-null object. The renderer now requires an initialized draw pipeline before
submission. The failed-linkage control reports failure without submitting invalid
draws. An additional saved entity test verifies move, undo, redo and fresh-process
reopening with exact version 2 buffer bytes, five reference-frame pairs and 360
pixel samples. The previous scene assembly regression also passes: thirteen frame
pairs, 1,152 samples, 1,024-draw capacity, failure/undo controls and fresh reopening.
Forty selected original merged placements across all four maps still import and undo.

The static suite discovers 1,144 tests: 1,136 pass and eight governed Windows
symlink cases skip. Twelve pinned source-policy validators pass the 77-file
cumulative scope. Original shader bundles and cached inputs retain their hashes.
Full source lighting/instance/visibility/LOD buffer values, remaining materials
and render passes, complete scenes, four-map UI and game export remain unfinished.
Full maps are not ready for 1:1 testing; the estimated heightmap route stays disabled.

## Source lighting ownership checkpoint (2026-09-12)

The owner requested committing all accumulated changes and opening a PR, then
continuing actual game lighting/instance data and the remaining campaign workflow.
PR #266 contains the source checkpoint and a merge of current main. The merged
native Editor and all three compiled test registrations pass: 595 cases passed,
two Windows symlink tests skipped. The merged static suite discovered 1,204 tests:
1,195 passed and nine Windows symlink tests skipped. These counts precede the
additional lighting-reader tests below; they are not full-scene rendering proof.

The new source-light reader captures all 3,488 native Light records in the 16
primary scenes across all four maps, their 3,488 exact HDRP companions and 1,935
same-entity controller records. Original bytes, source identities, owners and
transforms are retained. The exact installed HDRP assembly establishes that the
legacy additional-light intensity/unit fields are obsolete; migrated values come
from the native Light record. All 3,488 captured native intensities differ from
that obsolete field, so using the old field would produce incorrect inputs.

3,486 lights use the qualified v13 field selection. Two Horns dynamic-scene lights
still store HDRP v12 and remain explicitly blocked for migration. Nonfinite values
in unmapped controller data have explicit inspection tags; their original bytes
remain authoritative. The complete private capture took 38.750 seconds and source
fingerprints were unchanged. Eighteen synthetic tests cover ownership, incorrect
script identity, missing fields, migration versions, preservation, bounds and
cancellation. No original game asset or diagnostic is included in the PR.

Next: qualify those two light migrations and supply the actual light, instance,
visibility and controller state to the native shaders. GPU lighting, remaining
materials/passes, complete rendered scenes, four-map UI and game export remain
unfinished. Full maps are not ready for 1:1 testing.

## Source light value projection (2026-09-12)

The two Horns v12 Point lights now have a separately qualified v13 value
projection. The original installed HDRP migration ran in the exact Unity host
against 600 synthetic cases and the two captured lights. All twelve native value
fields agree at float32 precision; the native before/after comparison changes only
unit, lux distance or reflector fields, and the companion changes only its version.
Native intensity is preserved. An initial JSON-based fixture was rejected because
missing native serialization-version metadata applied an unintended older migration;
the accepted fixture assigns native properties explicitly and verifies their readback.

The optional source-lighting snapshot v2 carries this projection separately from
immutable original records. Default v1 capture behavior remains unchanged. Older or
future unqualified migrations remain blocked, and v12 shapes other than Point are
unsupported. This projects stored data; controller execution, final GPU light lists,
shadows and bakes are separate obligations.

All 3,488 lights in all sixteen primary campaign scenes now have projected values;
two require the qualified migration. Original Light, companion, owner, transform
and controller records remain unchanged. The fresh private capture took 60.906
seconds. Full rendered lighting/instance/visibility data, remaining material passes,
complete scenes, four-map UI and game export remain unfinished. Full maps are not
ready for 1:1 testing; no game files or saves were written.

Directional-light checkpoint (2026-09-12): the original HDRP builder now runs in an
isolated Unity graphics fixture for all 19 captured campaign directional lights.
All source hierarchy matrices match their independent reference; actual culling
results are retained separately because six lights differ at float-bit precision.
The explicit native codec matches all 26 original field offsets and all 3,344
record bytes. Native DX12 transfer passes all 836 words, two corruption patterns
and removal checks: 2,508 cells and 7,027,236 interior pixel comparisons.
Final cookies/shadows/atmosphere, controller state, other lighting/instance data,
remaining materials, complete scenes, four-map UI and export remain unfinished.
Full maps are not ready for 1:1 testing; game files and saves remain read-only.
