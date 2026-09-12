# Campaign scene editing and game return

Status: PARTIAL. Current owner scope includes the full campaign scene and editable game
assets, navigation, lighting and a verified return to Fall of Avalon. The implementation
must use the existing O3DE Editor. The old estimated campaign heightmap route is disabled.

## Required editing scope

| Scene surface | Required behavior |
| --- | --- |
| Terrain and world geometry | Preserve actual meshes/topology, holes, overhangs, source heightfields when present, and transforms. No inferred ground or gap filling. |
| Objects and instances | Individually selectable entities with original asset references, parent relationships and transforms; move, rotate, scale, supported property edits, undo and saved reopen. |
| Materials and textures | Preserve source shader/material/texture identity and parameters; qualify each editable mapping and report conversion limits. |
| Collision and physics | Bind colliders and physics settings to the right entity; object and geometry edits must maintain their intended relationship. |
| Navigation | Identify the actual game graph/source, links, areas and build settings. Preserve unchanged data and prove updates/rebuilds in the target game format. |
| Lighting and environment | Preserve lights, units, shadows, probes, environment settings and source bake references. Track when edits invalidate derived lighting. |
| Vegetation, water and effects | Account for instance placement, volumes, animation/effect dependencies and specialized runtime products. |
| Audio, triggers and game components | Preserve source references, identifiers and serialized state; editable fields require verified type and semantic mappings. |

This is the acceptance scope. Listing a surface here does not mean its importer, Editor
mapping or exporter exists.

## Source and identity contract

Source records remain authoritative. Bind an observed object to its source bundle
fingerprint, serialized-file identity and object path ID. These identify one inspected
source build; do not claim stability across game updates. Keep prefab/instance and game
identities separately when verified. Display labels, nearby positions and similar meshes
must never substitute for identity.

Preserve local transforms and hierarchy before any coordinate conversion. Qualify axis,
handedness, units, parent scale, rotation and shear behavior using numerical source and
host tests. Flattening a hierarchy or using an approximate mesh match is not accepted.

Each record needs distinct states for source preservation, asset resolution, O3DE
projection, editable operations and game export. Unknown components remain explicitly
unmapped with original source references retained. Never silently discard them or replace
them with guessed defaults. A preserved opaque record is not an editable supported feature.

Rendering/collision/navigation/bake products may have different owners and representations.
A collider entity is not evidence that the visible object was reconstructed. A built
runtime representation must be joined to source/instance records through verified
references before edits can be exported. The source scene cohort and its load relationships
must also be proven; simply loading every similarly named bundle can duplicate content.

## Existing owner boundaries

World Authoring owns scene authoring; Entity Placement owns placement operations.
Foundation owns durable identity, workspace records and validation. Existing canonical
interchange owns neutral cross-host handoff. ExternalToolchain and qualified providers
own bounded source/conversion work. Runtime adapters own game interpretation and mutation;
O3DE never becomes the game runtime.

Do not silently extend the closed canonical-interchange v1 schema or bypass its inventory
limits for a campaign-sized scene. Any new scene contract, partitioning or migration must
be defined and tested with its owning producer and consumer.

Original source and unknown serialized data stay immutable in the private workspace or
remain referenced from the read-only installation. Edited candidates are separate. No
source game assets or reconstructed proprietary code belong in repository fixtures or
published packages without redistribution rights.

## Return-path proof

The required sequence is:

1. Inventory the source scene cohort and resolve every relevant external dependency.
2. Decode/re-encode supported source records without changing bytes or semantics, recording
   unsupported types rather than assigning assumed schemas.
3. Import a source-bound entity with its real asset and hierarchy into O3DE. Prove its
   transform, properties, selection, undo and saved reopen against source measurements.
4. Perform an unchanged authoring/export round trip and verify preservation of all source
   references, untouched component data and required runtime products.
5. Make one controlled edit and verify the intended change and every dependency it
   invalidates. Block export for stale navigation, collision, lighting or other required
   products until their update route is verified.
6. Validate a staged candidate against the exact game profile and, under explicit
   destination/rollback authorization, load it in the game. Test behavior and persistence.

Source-record byte equality is only step 2. It does not prove steps 3-6. Export strategy
and native game payload format remain unestablished; no replacement-bundle or runtime-patch
approach is selected by this document.

Unity's generic [navigation build API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AI.NavMeshBuilder.UpdateNavMeshDataAsync.html)
uses explicit build settings and sources. Its [renderer lightmap mapping](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Renderer-lightmapScaleOffset.html)
is separate renderer state. These are background engine contracts, not evidence that FoA
uses either as its authoritative navigation or lighting source.

## Current inspection and reproducible audit

`Tools/foa_scene_source_audit.py` under the SDK Gem audits explicit local `.bundle` paths.
It does not produce an O3DE scene or write a game-native output. It verifies source
fingerprints before/after, rejects output inside the source or source control, and
uses embedded type trees only. Missing schemas and unsupported records remain explicit.
Reports contain private source-derived diagnostics and must remain outside source control.

Use the qualified UnityPy 1.24.2 environment and explicit `--source-root`, repeated
`--bundle` arguments, and a new `--output` path outside source control. Optional flags:

- `--all-records`: decode/re-encode every embedded-schema record, including otherwise
  unmapped types. Byte equality does not make those types editable.
- `--component-scripts`: resolve each MonoBehaviour's actual MonoScript PPtr through
  bounded local CAB dependencies. Signed 64-bit path IDs retain their full precision.
  Resolution uses file identities and reference tables, with no ambient file search or
  classification by component labels or field names. Failed shared dependencies are not
  retried for every instance. Unresolved references stay separate from byte preservation.
- `--scene-file`: select an exact serialized primary scene file for reciprocal GameObject,
  component and Transform ownership checks. Repeat for each intended primary file.
  Duplicate/absent selections, missing targets, mismatched owners, incomplete child lists,
  cycles, excessive depth and unlisted owned components fail. Shared prefab files are not
  automatically included as placed scene instances. This is ownership validation, not a
  component-semantic mapping or native scene import.
- `--navigation-cache`: inspect one explicit `.bytes` container inside the source root.
  This inventories entry names, offsets, sizes and hashes. It does not decode graph
  topology, rebuild navigation or interpret names as filesystem paths.

The audit bounds input files/bytes, records, dependency loading, script types, cache
entries, names, elapsed work and output size. It never replaces an earlier report.
These are diagnostic limits; an OS-enforced memory limit and preallocation bounds for
all decompressed container payloads still need qualification before product integration.

## Inspected source findings (2026-09-09)

The source `SubdividedScene` component references three scene GUIDs. The inspected
Addressables catalog binds these to the Horns static, merged and merged-static scenes.
The inspected loader uses additive subscene loading. This establishes the source cohort
relationship for this installation, not live runtime acceptance or update compatibility.

The four scene headers report Unity version `0.0.0`; the parser used the recorded
6000.0.64f1 container fallback and embedded record schemas. That fallback is not proof of
the installed game's Unity version.

All 299,641 serialized records in the four selected scene bundles re-encoded identically
in the full audit, with zero mismatches, decode/write failures or skipped records. This
includes 31,797 MonoBehaviour records, all resolved to their source script identities.
Six inspected source files (scene bundles, the script dependency and navigation cache)
retained their before/after hashes. External geometry, texture and other referenced
payloads are not included in the 299,641-record claim.

`foa_scene_record_writer.py` supplies a scoped correction for UnityPy 1.24.2's writer:
the reader omits data for explicit null managed references, but the original writer
indexes that absent field. The adapter handles only the inspected null sentinel with
empty class/namespace/assembly and an embedded resolver confirming no data schema.
It rejects unexpected payloads and preserves errors for unresolved non-null types.
The in-memory hook is restored even on failure; installed parser and game files remain
unchanged. The correction enabled exact re-encoding of the 1,109 affected records.
Synthetic tests reproduce the upstream failure and prove null/non-null preservation,
unknown-type rejection and hook restoration using the qualified parser itself.

The standard NavMeshData references are null and standard lightmap arrays empty; these
are not evidence that the game lacks navigation or lighting. Actual source components
include AstarPath, navigation cuts/modifiers, HDRP additional light data and game light
controllers. The inspected AstarData loader derives its cache name from the scene name
and removes the static-scene suffix. Its entry container uses counted names and byte
payloads despite the internal `ZipFile` name; it is not a standard ZIP file. The Horns
cache has five entries. Its graph settings and metadata entries match the scene's
serialized settings byte-for-byte. Graph topology and rebuild behavior remain unverified.

Two `DrakeMergedRenderers.dataGuid` values select exact members of the renderer archive.
Private inspection found 10,890 renderer instances in those members; their section bounds,
indices and finite transforms validated against inspected record layouts. The members
reference 1,167 distinct mesh keys and 287 material GUIDs whose base asset identities
exist in the inspected catalog. Native layout execution,
material conversion and joins to scene GameObjects/colliders are still unverified.
The inspected instance record has a transform and LOD/definition indices, but no
GameObject identity. Never infer that missing association from proximity or appearance.
Unity's [archive interface](https://docs.unity.cn/6000.0/Documentation/ScriptReference/Unity.IO.Archive.ArchiveFileInterface.html)
is background documentation for the loader's archive API, not source-to-object proof.

### Exact asset binding and native entity proof

`foa_scene_asset_catalog.py` reads the inspected compact Addressables catalog tables
with bounded sizes, indices, strings and dependency edges. It selects the exact resource
class, assembly and provider; a GUID can identify both a GameObject and Mesh. Bundle
selection preserves the dependency order used by the inspected provider. Only the
recognized local runtime-root expression is mapped; expressions are never executed.

`foa_scene_asset_binding.py` selects the actual AssetBundle container PPtr for the
requested type and subobject. Duplicate entries for the same pointer are one asset;
distinct matching pointers are an error. Preload dependencies and unlisted objects with
similar names are not candidates. Missing schemas, external targets outside the loaded
bundle, unsupported keys and ambiguity fail explicitly. Signed path IDs retain all
64 bits and their serialized-file scope.

All 1,167 mesh subobjects and 287 material assets referenced by the two inspected archive
members resolved through 281 bundles, with zero unresolved entries. Catalog and bundle
hashes stayed unchanged. This inventory covers those archive references, not every
campaign dependency. Material identity resolution is not shader/texture conversion.

A private proof used one placed campaign renderer from the main serialized scene file,
not a shared prefab template. Its explicit mesh contains 205 vertices and 250 triangles.
The pinned O3DE Asset Processor retained the float32 positions bit-for-bit and all
oriented triangle geometry. The native model, LOD and buffer references were followed
through GUID/sub-ID entries in its asset database; path hints were not authoritative.
Source-to-host axes were explicitly converted to (x,z,y), reversing triangle winding.
O3DE's inspected importer normalizes normals and changes UV V to 1-V. UV rows matched
that convention exactly; observed normal direction error was below 0.000003 degrees.
Original normals and other source channels remain private and unchanged. Source shader
appearance, additional UV channels, tangents and material-slot rendering are unqualified.

The native test loaded that model, preserved its two-level parent chain, selected it,
moved it two metres, performed undo/redo, renamed it and saved it. A fresh Editor process
resolved the persisted source tokens, verified the parent and exact model binding, and
recovered the saved position without difference. Imported world-bound error against the
source calculation was below 0.1 mm. The source parent scale differs from unity by less
than one float32 epsilon; this proof used a recorded 1 mm transform tolerance and retained
the exact original transform. General nonuniform scale, shear and renderer offsets still
require their own host mappings and tests.

Reusable staged checks live in `Tools/editor_tests/source_entity_native_import.py` and
`source_entity_native_reopen.py`. Run them in separate disposable Editor processes with
`FOA_SCENE_PROOF_ROOT` and `FOA_SCENE_PROOF_CACHE` set to the authorized private fixture
and its processed pc cache. The fixture includes explicit source metadata, the checked
native cache/position evidence and a fresh level inside a registered scan folder.
These are qualification scripts, not user-facing import commands. They capture no
screenshots. Native tags are test-only source tokens into private evidence; they do not
establish a product scene schema, duplicate/import semantics or export authority.

The prototype exposed an Editor Python callback crash when nested Qt event loops ran
under the engine Python lock. Staged ticks passed the model-ready and edit/reopen tests;
the failed attempts and crash evidence remain private. No external-engine patch was made.

### Source ownership and controlled source edit

`foa_scene_ownership.py` checks explicit primary files through their serialized references.
The four inspected primary scene files contain 95,969 GameObjects and 202,748 owned
components. All component-to-GameObject and parent/child Transform links were reciprocal;
no unowned component records were found. The audit keeps the full signed path ID and file
scope, excludes unselected shared prefab templates, and bounds records, bytes, references,
components per object and hierarchy depth. Transform edit impact includes the exact owner
and descendants. It does not infer navigation/bake impact or archive-instance associations.

The inspected placed cliff has one collider and five Drake mesh renderers under the same
source GameObject, plus its source LOD group and surface component. The native geometry
proof above covers one of those renderers only. A separate reference check across all four
scenes verified 7,162 Drake renderers and 3,782 LOD groups through source script identities
and reciprocal renderer/group references. Combining their asset references with the two
archive inventories resolved 1,668 mesh subobjects and 430 materials across 308 bundles,
without unresolved bindings or changed source fingerprints. This still excludes non-Drake
renderers and texture/shader dependencies. Direct collider PPtrs resolved 435 distinct
CAB-scoped mesh records in 152 bundles; four component references to Unity's built-in
resource file remain unqualified. Mesh identity resolution does not qualify collider cooking,
physics settings, material rendering or archive-to-collider associations.

`foa_scene_transform_candidate.py` prepares a local-translation candidate in memory. It
requires the original serialized-file/path-ID/hash binding, the exact original translation
baseline and an embedded float32 vector schema. The entire source record must re-encode
identically before editing. Stale identities, unsupported records, nonfinite/overflowing
values, unrepresentable small edits and unrelated field changes are rejected. The original
reader remains unchanged. No source file or game-native bundle is written by this module.

The saved native move produced a separate 68-byte source Transform candidate. The no-op
candidate was byte-identical. Only `m_LocalPosition` changed in the edited candidate; its
world position agreed with the freshly reopened native edit within 0.051 mm. Exact source
rotation, scale, references and all other fields stayed intact. Collider/navigation/lighting
updates and packaging/runtime acceptance remain NOT_RUN. This is one record-level edit
proof, not a full unchanged authoring round trip or an exportable campaign.

The repository source-audit command was rerun with all-record, script, navigation-cache
and explicit scene-file options: all 299,641 records still re-encoded identically and all
selected source ownership checks passed. The focused scene suite now has 72 passing tests,
including actual serializer candidate tests and malformed ownership/identity cases.

The source serialization and ownership checks are complete for the selected files and one
renderer has native edit/reopen evidence. Campaign assembly, full component semantics,
material conversion, navigation/lighting rebuilding and the game return path remain
unimplemented or NOT_RUN. The existing Mono adapter logs startup only. Campaign import
stays unavailable until the complete workflow is qualified.


### Complete bound mesh batch and native preservation (2026-09-10)

`foa_scene_mesh.py` reads explicit streamed vertex layouts and packed compressed meshes
without resampling or filling geometry. It retains source file/path-ID/record and stream
hashes, channels and material-slot triangles. Half precision, normalized integer channels,
base-vertex indices, compressed UV offsets, allocation bounds and cancellation have
synthetic tests. Source records and streamed bytes are unchanged.

The complete binding batch has 1,668 references to 1,668 distinct mesh records. Every
reference decoded and generated a GLB projection successfully; the
vertex count is 4,290,227. Six compressed meshes exposed a third-UV offset error in the
comparison decoder. A separate packed-bit implementation confirmed the cumulative offsets;
no source UV values were replaced to agree with that erroneous reference calculation.
The full source check took 160.593 seconds. These counts do not represent scene completion.

`foa_scene_mesh_glb.py` declares an explicit Y-up root rotation and keeps all local vectors
in the same canonical basis. Native cache tests caught the pinned importer converting
normals while leaving tangent vectors in another basis; the explicit root corrected that
mismatch. The projection preserves individual material slots, source UV ordinals and source
bindings. Neutral materials remain unqualified previews; source shaders/textures are not
mapped. Original fourth normal components remain in the projection metadata/attribute;
that additional lane has no qualified native rendering mapping.

`foa_scene_native_mesh_audit.py` follows native asset IDs through the cache database and
checks oriented triangle corners, mapped channels and material slots. It rejects dropped
faces, altered winding, changed UVs/colours/tangents, nonfinite outputs and changed cache
products. Its direction comparison models native normalization and float32 cross products;
position, colour and mapped UV comparisons have no error allowance. Native vertex indices
are not treated as source identities. The focused scene suite has 102 passing tests.

The unmodified host still fails some batch entries: it welds nearby vertices, generates
invalid tangent frames, and can overwrite source tangent signs. These failures are retained
in private per-file evidence. `SourceMeshPreservationComponent` exports the verified original
imported mesh and source tangent presence/signs for explicitly marked `_foamesh.glb`
projections. Assimp may expand indexed triangles into corner vertices; correspondence is
established through each ordered source face and its indices, with exact position checks.
The adapter rejects incompatible root transforms and schema markers, removes the optimized
mesh remapping, and restores source tangent data through the verified corner mapping.
It does not invent bitangent channels for missing or parallel tangent frames. Original
source records remain required; GPU vertex indices are not source edit identities.

The SDK component compiled against the pinned host. Its seven native Asset Processor cases
passed: four valid cases (multiple material slots, close vertices, missing tangents and
parallel frames), and three malformed projections rejected without published products.
The earlier `.foamesh.glb` suffix was replaced because its extra period caused invalid
native generated node names. The failed attempts remain in private logs.

All 1,668 source mesh payloads were then processed through this component. Native comparison
passed all 5,038,438 oriented triangles and mapped corner channels in 95.078 seconds, with
zero failed meshes. Position, colour and mapped UV comparisons are exact; direction
comparisons retain the documented normalization allowance. All original fourth normal
components in this batch were zero. These results qualify this mesh projection path, not
source material rendering, editable vertex identity or the game return path.

The 430 bound source materials were separately inspected without changing their records.
Their non-null texture/shader references identify 1,197 texture records and 19 shader
records in 326 serialized files. The exact files were located in declared catalog
bundle dependencies through bounded directory metadata reads. All 1,216 referenced records
then resolved with the expected types (1,197 Texture2D and 19 Shader), and all referenced
texture stream payloads were bounded and hashed in their source bundles. Source bundle
fingerprints remained unchanged. Payload verification took 79.468 seconds. The first
verification attempt incorrectly required identical archive directory/member names;
checking the actual same-bundle `.resS` member path corrected that diagnostic error.
Texture pixel decoding, colour-space/sampler mapping and native shader conversion remain
unqualified. The shader set includes custom game shaders as well as HDRP Lit and Unlit;
substituting a neutral PBR material is not a conversion of that source behaviour.

Full native scene assembly, source material rendering, collision cooking, navigation and
lighting updates, sculpted-vertex identity/persistence and game export are still incomplete.
No campaign import or game-return capability is enabled by these geometry checks.

### Source texture preservation (2026-09-10)

`foa_scene_texture.py` validates the exact embedded PC Texture2D record and explicit
same-bundle stream. All 1,197 referenced textures passed preparation: 13,204 mip levels
and 2,499,265,354 original payload bytes. Source hashes remained unchanged. The batch
contains RGB24 (6), RGBA32 (8), BC1/DXT1 (317), BC3/DXT5 (861) and BC4 (5).
Format 26 is BC4; BC5 is 27 and is not accepted by this conversion path.

Texture data colour space is Linear=0 or sRGB=1, as verified against the installed
`TextureColorSpace` enum and Unity's [reference declarations](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Graphics/GraphicsEnums.cs).
This is distinct from the project's active colour space and does not establish the
source shader's sampling behaviour. The checked batch contains 512 sRGB and 685 linear
records. All sampler fields, including wrap axes, anisotropy and mip bias, remain in
the private source descriptors; native sampler/shader bindings remain unqualified.

The new `.foatexture` version-1 projection is internal generated input for the SDK
builder, not a public canonical scene or game format. It contains an eight-byte
`FOATEX01` marker, six little-endian uint32 fields (version, width, height, mip count,
source format, data colour space), a SHA-256 of the first 32 bytes plus payload,
and the complete original mip payload. Dimensions are bounded to 8192, mip counts to
the source dimensions, and source/native payload allocation to 128 MiB. Unknown
versions, unsupported formats, invalid surface layouts, conflicting/escaping streams,
truncation, extra bytes and hash mismatches are rejected. Hashes detect inconsistent
content; they do not establish authorization or authenticity. Original records and
exact source bindings remain required for game return. Existing scene/canonical
contracts and the disabled estimated-heightmap route are unchanged.

The pinned stock image converter retains only the top source mip before regeneration
for ordinary 2D textures. `SourceTextureBuilderComponent` therefore uses native
StreamingImage/ImageMipChain creators directly for this explicitly marked input.
It preserves BC blocks, mip order, row order and the data-colour-space format without
pixel decoding, gamma conversion or recompression. RGB24 maps to RGBA8 with an explicit
opaque alpha lane; every original RGB byte is retained. Higher mips are streamable,
with the small tail grouped into at most 64 KiB when the supplied mip chain allows it.
The component is restricted to Asset Builder hosts and the `pc` platform; it does not
open source game bundles or register an end-user campaign import operation.

Native texture qualification is recorded separately from pixel/shader rendering.
`foa_scene_native_texture_audit.py` resolves product references through the native asset
database, checks image format and dimensions, mip/view lookup tables, per-mip layouts
and every supplied block/texel byte. Source-oriented row order is deliberate: mapping
it to the qualified mesh UV projection, shader texture transforms, packed normal
semantics, sampler overrides and custom game shader behaviour remains outstanding.
A successful image-product audit does not establish visual equivalence to the game.

Native texture acceptance PASSED: 18 valid format/colour-space/mip cases, nine invalid
inputs rejected without products, and five deliberate native-data corruptions detected.
The SDK component configured and compiled against the pinned host. An initial dependency
target-name error and a build invocation holding stale generated include paths were
corrected; the successful build used the regenerated project and actual native targets.
All 1,197 bound source textures and 13,204 mip levels then passed native comparison,
with zero failures across 4,482 products. The full native audit took 36.969 seconds.
The focused scene suite now has 118 passing tests, including the source-record allocation
guard. These results qualify source-oriented GPU image data, not shader rendering,
material/UV/sampler binding, full campaign editing or return to the game.


## Material and shader binding checkpoint (2026-09-10)

`foa_scene_material.py` joins material Shader pointers to exact serialized file and
signed object IDs, and joins saved values to their shader declarations. It retains
the complete source material, flags, keywords, pass settings, queue, texture scale
and offset, and differently typed or undeclared saved values. Shader-owned texture
pointers resolve in the shader's external file table, independently of the material's.
Null texture bindings retain the declared default and an unresolved state; no white,
normal, array or other default texture is manufactured. A declared property is not
proof that a selected compiled variant uses it; an undeclared saved value is not
proof that no compiled uniform uses it.

The output is a private inspection record with schema `foa-source-material-binding`
version 1, not canonical interchange, a persisted editable scene, an executable
material, or a game export format. Existing scene/public contracts and engine pins
are unchanged. World Authoring/Entity Placement consume qualified mappings;
Foundation retains identity and persistence ownership. Source decoding stays in the
SDK Tools surface, outside the generic provider-discovery Gem. These modules do not
write source data or launch conversion automatically from the Editor.

`foa_scene_shader.py` reads every explicitly declared PC shader segment with bounded
LZ4 expansion, exact entry partitions and explicit paired code/parameter indices.
It decodes constant groups, structured fields and resources without inventing their
runtime values. Common and per-variant bindings are combined by register namespace
and index; conflicting declarations or sampler owners fail. A texture-owned sampler
is joined through the serialized sampler index. Inline samplers retain their original
encoded state. All variants, keyword indices and source pass forms remain available;
no runtime variant selection or HDRP/custom-shader replacement is inferred.

The observed program envelope is `202012090`; embedded source version headers are
`0.0.0`. The configured UnityPy fallback version is not evidence of the installed
Unity version. The segmented entry and parameter layout was compared with the
[archived reader implementation](https://github.com/AssetRipper/AssetRipper/blob/7b1c20c1f316612d5b009565b722a191acab9183/Source/AssetRipper.Export.Modules.Shader/ShaderBlob/ShaderSubProgram.cs)
and then checked against actual record boundaries, embedded common declarations and
native DXBC disassembly. That reference is format context, not game-runtime authority.
No third-party or proprietary shader source was copied into repository fixtures.

[Unity's property types and flags](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Shaders/ShaderProperties.cs)
control property interpretation. [Unity's sampler documentation](https://docs.unity3d.com/6000.0/Documentation/Manual/SL-SamplerStates.html)
explains why textures may share a sampler or use an inline state. The bounded sampler
helper preserves filter, wrap axes, mip bias and anisotropy. Constructing an RHI sampler
requires an explicitly supplied effective anisotropy, because texture settings alone
do not establish quality overrides. Inline filter/wrap/comparison bits are separate:
the observed `0x801` state requests linear filtering, repeat on all axes and 16x
anisotropy. The synthetic compiler checkpoint below establishes this encoding.
Unknown higher bits remain visible and reserved anisotropy codes are rejected.
Comparison functions, full LOD policy and global/renderer overrides remain unqualified.

The source UV reconstruction helper applies scale and offset after undoing the native
mesh V reflection: `(u * sx + ox, (1 - v) * sy + oy)`. Tests use asymmetric corners,
negative tiling and coordinates outside 0-1. This is coordinate algebra subject to
native float precision, not GPU image orientation or tangent-space normal proof.
The source texture route still retains original row and mip bytes. Native shader
consumers must qualify their sampling coordinates, channel selection, packed normals,
colour conversion and shader-specific procedural UV operations before claiming parity.
Original UV/texture/material records remain return authority.

Current private results:

- 430 freshly re-read materials; 57,166 declared properties and 1,901 undeclared or
  differently typed saved entries retained; zero material-binding failures.
- 8,510 texture declarations: 1,910 material-owned bindings, 96 shader-owned bindings,
  and 6,504 unresolved shader-default bindings. Sixteen saved sampler combinations
  remain distinct. The shader-owned references use already identified textures.
- 19 direct shaders plus 21 distinct fallback shader records; identical fallback
  display names do not collapse their identities.
- 5,037 shader entries: 4,110 code and 927 parameter records; 4,254 variants;
  zero unreferenced entries or unclaimed segment bytes.
- 40,867 native resource declarations matched to exact serialized register owners.
  All code records were accepted by Microsoft's `D3DDisassemble`. This verifies
  bytecode structure and binding coverage, not GPU execution or rendering equivalence.
- 156 focused scene tests and eight synthetic native shader cases passed. The native
  cases include deliberately missing texture and incorrect comparison bindings.
- 96 source bundle fingerprints checked before/after the material/shader pass.
  Extracted records and shader tooling diagnostics remain private and local.

Limits are enforced before expansion or copying: 4,096 material properties, 100,000
material tree nodes/depth 24, 32 MiB compressed shader data, 128 MiB total expansion,
65,536 entries/variant references, 256 passes and 131,072 aggregate parameter names.
Individual parameter records are at most 4 MiB and 16,384 parameter fields. Cancellation
is checked between records, segments, passes, variants and parameter groups.

Remaining work is native game shader/pass and uniform-value implementation,
shader defaults, effective sampler/global state and complete appearance comparisons.
The following synthetic GPU checkpoint qualifies only its explicitly tested route.
These results do not enable the complete campaign import workflow. Full scene editing,
navigation/lighting rebuilding and game export remain PARTIAL.


## Sampler compiler and native GPU checkpoint (2026-09-10)

The independent `FoaSamplerProbe.cs` fixture runs in an empty private Unity
6000.0.64f1 project with no proprietary inputs and no external packages. It declares
27 shaders covering point/linear/trilinear filters; all per-axis address modes;
comparison; mixed axes; and requested anisotropy none/2/4/8/16. Unity generates the
shader records, GUIDs and bundle. `source_sampler_compiler_cases.py` checks each
compiled parameter binding against the fixture declaration and disassembles both
programs per shader. The program envelope and embedded player/parameter layout
match the observed source layout. This qualifies the encoding; it does not establish
the game's Unity version or its runtime quality settings.

The measured anisotropy code occupies bits 9-11: codes 0, 1, 2, 3 and 4 request
none, 2, 4, 8 and 16 respectively. Code 0 preserves the absence of an explicit inline
anisotropy request; it is not a texture quality policy. Codes 5-7 fail qualification.
Bits above 11 remain unresolved. The previous `0x801` uncertainty is therefore
resolved as an explicit 16x request. The full 430-material / 40-shader source audit
was repeated: all 40,867 register checks pass, with zero unresolved inline encodings
over 4,254 variants, and all 96 source bundle fingerprints unchanged.

The fixture also renders four exact UV coordinates against an asymmetric, linear
RGBA32 texture loaded from explicit raw row bytes. Hardware Unity GPU readback
returns the expected red, green, blue and yellow samples. This avoids interpreting
a rendered image's top-left/bottom-left display convention as texture storage proof.

The corresponding O3DE fixture uses the actual source mesh projection, compiled
mesh-preservation adapter, compiled texture builder, material compiler and native
viewport. Its small synthetic shader consumes UV0, reverses the qualified native
V reflection and then applies source scale/offset. It is a qualification shader,
not a replacement for a game shader. Native object/draw SRGs are bound normally;
clip-space swatches isolate sampling from lighting, camera projection and tone mapping.
Two correct cases and two deliberately wrong cases compare 3x3 regions at four
predetermined positions each. The wrong-V case must produce the predicted row swap;
the wrong-transform case must produce its predicted repeated columns. A missing
image, blank viewport or unchanged material cannot satisfy these checks.

That consumer test initially failed and exposed an integration defect which the
byte-preservation audit alone could not detect. The pinned
`StreamingImageAsset::GetImageAssetSubId()` returns 1000 and `MaterialUtils`
uses it when binding an image source path. The source image builder used sub-ID 0.
It now uses the engine-owned 1000 image ID and 1001+n mip-chain IDs, with builder
version 2 to regenerate products. The `.foatexture` packet stays version 1 and
source IDs, payloads, public scene contracts, dependencies and the O3DE pin stay
unchanged. This is regeneration of experimental native products, not a promised
migration of saved scenes or game assets. The inspected AP database retained legacy
product aliases; the auditor and native materials resolve the qualified 1000/1001+n
identities explicitly.

After rebuilding, all 1,197 textures, 13,204 mips and 4,482 products again pass exact
native comparison. All 27 texture acceptance cases and five auditor corruption
checks pass. The native viewport comparison passes all four cases and 144 sampled
pixels. There are 157 passing scene tests. Generated Unity/O3DE projects, assets,
logs and pixel captures remain outside the repository; captures contain synthetic
swatches only. No game installation or save was modified.

This checkpoint does not qualify other source UV channels, packed normals,
anisotropic GPU appearance, shader-default textures, renderer/global uniforms,
pass selection, native game shader execution, lighting, full scene reconstruction,
navigation rebuilding or a game-ready export.


## Native source shader execution checkpoint (2026-09-10)

The next native proof executes preserved DXBC, without reconstructing shader source
or substituting a generic material. `foa_scene_shader_constants.py` combines embedded
common and per-variant constant-buffer fields by exact names and source offsets.
Buffer sizes and duplicate declarations must agree. Overlap, register-crossing
vectors, missing/extra values, unsupported arrays/structures/types and invalid
matrix layouts fail qualification. Supported values are explicit finite float or
signed-int vectors and 16 column-major floats for a float4x4 matrix. Only bytes
outside declared fields are zero padding. No colour-space conversion, transpose,
shader default, material override, global or renderer value is inferred.

`source_shader_gpu_probe.cpp` is an independent, bounded Windows SDK test consumer.
It reads private `FOAGPU01` packets and returns JSON execution metadata followed by
`FOAPIX01` float readback through stdout. The supervising Python tool owns evidence
writes. The executable creates no window, writes no files, opens no game process
and changes no Editor state. Its explicit test pipeline uses POSITION/UV0 triangle
lists, float4 textures and render targets, no blending/depth/culling, and selected
point/linear single-mip samplers. These are controlled fixture conditions, not game
render-state defaults. The packet is a private test format, not canonical scene
interchange, a game payload or a published asset contract.

The consumer requires hardware D3D11 feature level 11.0 and the debug layer. Shader
creation and resource/draw warnings fail execution. Packet sizes, shader counts,
resource slots and dimensions are bounded; duplicate bindings, truncation, trailing
bytes, future versions and invalid bytecode are rejected. The GPU completion wait
is limited to 15 seconds and the supervisor enforces a process timeout. Pixel
comparison rejects nonfinite output, duplicate sample positions and deviations
above 0.00002 in linear float colour. A negative sentinel clear prevents an undrawn
surface from passing. Direct3D permits unbound texture reads returning zero, so
absence of debug warnings alone is deliberately insufficient.

Current private hardware results:

- All 4,110 original programs across 19 direct and 21 distinct fallback shader
  records pass device creation, with no graphics warnings/errors. Their exact
  source fingerprints remain unchanged. This does not claim all programs were drawn.
- The explicitly selected non-instanced HDRP Unlit ForwardOnly vertex/pixel pair
  passes eight rendered source cases using unchanged bytecode: identity UVs,
  negative scale/offset, a deliberately wrong transform, tint/emission/exposure,
  integer-controlled alpha, zero exposure, object translation and view transforms.
- The source fixture matrix passes 19 cases and 368 sampled pixels. It includes
  synthetic calibration, malformed packets, missing source texture/constant/sampler,
  extra constants, wrong keywords, absent program selectors and a wrong black-output
  control. A missing texture must fail the normal colour oracle even when its native
  execution has no warning.
- The focused scene suite passes 167 tests, including ten buffer-layout/value tests.
  This is separate from the earlier native O3DE UV and texture-preservation evidence.

Build the probe as a standalone C++17 console executable with the installed Windows
SDK (`d3d11.lib`, `dxgi.lib`) and WRL headers. Use an external build/output directory;
no O3DE engine source, build graph or dependency pin changes are needed. Its portable
synthetic calibration can then be reproduced with:

```powershell
python -I -B Gems/TaintedGrailModdingSDK/Tools/editor_tests/source_shader_gpu_cases.py `
  --executable "$probeExecutable" --output-root "$newPrivateOutputDirectory"
```

Source draws use `source_draw` with exact subshader/pass/stage/tier/variant selectors,
checked keyword indices and a value for every resource and declared constant field.
Source program bytes, hashes, selectors, controlled inputs, sampled outputs and
execution diagnostics remain in the private evidence workspace. No proprietary
shader code or game asset is a repository fixture.

O3DE's material pipeline has not yet been wired to these source programs. The tests
do not establish the game's effective exposure, colour conversion, global constants,
variant/quality policy, default textures, render states, lighting or scene appearance.
A complete layout census qualifies 1,947 stage variants for the explicit value
packer. Another 1,781 require structured constants and 526 require arrays; all 2,307
remain rejected. Source variant vectors use one row, and common vector declarations
normalize to that shape before duplicate comparison. These counts describe layout
support, not supplied runtime values or rendered variants. The remaining native
shader mappings and O3DE integration require their own rendered comparisons. Full map editing, navigation/lighting rebuilds and game export remain
unfinished; runtime sign-off is not performed.

## Four campaign map source pipeline (2026-09-10)

The owner requires all four campaign maps for one-to-one comparison in the existing
Editor. The importer's source-selection and asset-binding stages now support this
set. This checkpoint does not expose a working four-map native scene importer.
The local heightmap dialog and previous controlled shader fixtures are not that
workflow, and the disabled estimated campaign export remains disabled.

`foa_scene_cohort.py` binds the root scene from its exact typed catalog location.
The streamed-scene container has a null asset pointer; the owning primary serialized
file instead comes from its exact `AssetBundle.m_SceneHashes` entry. Shared asset
files and display-name matches are not candidate primary scenes. Exactly one enabled
source `SubdividedScene` loader is selected through its serialized MonoScript identity.
Its GUID references retain root/static/dynamic roles, order and stable source fields.
Duplicate scene loads, invalid/future reference wrappers, missing scene hashes and
unresolved script identities reject selection. Runtime node/load policy is not inferred.

| Campaign | Primary scene files | Source GameObjects | Source components | Byte-identical records | Merged instances |
| --- | ---: | ---: | ---: | ---: | ---: |
| Horns of the South | 4 | 95,969 | 202,748 | 299,641 | 10,890 |
| Cuanacht | 4 | 90,489 | 189,436 | 281,605 | 8,536 |
| Forlorn Swords | 4 | 97,709 | 210,558 | 310,146 | 16,027 |
| Sarras | 4 | 31,296 | 69,733 | 101,945 | 51,776 |

Ownership and record preservation passed for this complete selected set, without
skipped records or unresolved script references. Counts include preserved source
components whose editing/runtime meaning remains unmapped. Direct Drake renderer
LOD-parent reciprocity was separately checked. References to Unity built-in collider
resources remain unqualified (four, four, three and eighteen component references
respectively); they are not replaced with primitive guesses. Other renderer families,
runtime-spawned objects and dependent products need their own coverage checks.

`SourceAssetReference` retains type, exact GUID and optional exact subobject name.
An empty serialized subobject selects the main typed container asset; it does not
fall back to the first named mesh. This handles the two main Mesh references observed
in Forlorn. `foa_scene_asset_inventory.bind_references` groups explicit typed selectors
by their catalog-selected bundle, verifies actual container pointers and source
record hashes, and rejects the whole binding operation if any requested asset fails.
Exact duplicate selectors share a load; type and named/main distinctions remain.
A second fingerprint pass covers all released source bundles before publication.
Empty selections, changed sources, installation/source-checkout destinations and
existing output files fail rather than yielding a successful manifest.

The combined direct Drake and merged-instance set binds 4,060 asset references:
3,185 meshes and 875 materials in 331 bundles (1,592,632,864 compressed bytes).
This inventory does not claim complete renderer-family or runtime asset coverage.
All 3,185 mesh references decode and project successfully, with 11,496,039 vertices;
source bytes and reference decoder comparisons are checked independently. The
expanded native cache comparison passes for all 3,185 projected meshes: 13,207,074
oriented triangles and their projected vertex channels, UVs and ordered material
slots, across 23,862 native products. It took 390.265 seconds within the 1,200-second
private audit budget. This is geometry proof for the collected renderer set; it does
not establish complete scene coverage, material binding or visual parity.

`foa_scene_merged_renderers.py` retains the five exact framed POD sections of an
explicitly source-bound archive member. It decodes mesh keys, material GUIDs,
definition references and finite instance matrix columns, and retains every source
record byte, including padding and uninterpreted filter/LOD fields. Instance identity
is member GUID, member fingerprint, section and ordinal. Same-looking matrices or
meshes never collapse instances or manufacture GameObject/collider associations.
All eight actual members reassemble byte-identically; every one of their 87,229
instances follows an existing definition and ordered bound material slots. Native
POD layout execution, runtime LOD/filter policy and editable GameObject joins remain
NOT_RUN. Raw records remain required for any eventual return path.

Source-selection bounds are 32 scenes, 500,000 objects per bundle, a 32 MiB catalog,
16 MiB manifest and a default 180-second timeout. The asset-binding stage accepts
at most 16,384 input selectors, 512 bundles, 64 MiB per bundle, 3 GiB in total and a
32 MiB manifest, with a default 420-second timeout. The archive decoder caps a member
at 64 MiB and each section at one million records. Cancellation is checked throughout
record and reference traversal. The measured all-map asset bind took 90.391 seconds;
independent full mesh decode/projection took 782.203 seconds within its 1,200-second
private test budget. These are local source-processing measurements, not Editor
responsiveness or frame-rate evidence.

The private manifests use `foa.campaign-source-cohort` and
`foa.campaign-source-assets`, version 1. Both have overall PARTIAL status and explicit
NOT_RUN native scene/visual/export fields. They are private diagnostic stage results;
no public canonical-interchange schema, persisted editable scene or runtime adapter
contract was changed. All outputs remain outside source control and the installation.
The source pipeline has 211 passing focused tests, including framing, exact identity,
main/subobject selection, cancellation, budgets, changed-source and protected-output
negative cases. Twelve pinned source-policy validators cover the files in the current evidence pack.

The expanded material inventory contains 34 direct Shader identities and 2,240 saved
texture identities. Fifteen shaders and 1,043 textures are outside the previous Horns
qualification. There are also 11,268 null saved texture entries whose effective
shader defaults must not be guessed. Complete shader/material/render-pipeline
integration, full scene assembly, lighting/navigation and one-to-one game comparison
remain unfinished. A source-record hash pass is not visual parity or game-export proof.

The broader static run exposed misplaced native mesh/texture builder implementations
in the Editor composition target. They now belong to the existing Framework target,
with explicit native processing dependencies; Editor retains module registration.
Component identities and processing bodies are unchanged. The seven build-graph
contract tests, full static validator set and pinned native SDK rebuild pass after
that correction. The compiled consumers pass seven native mesh cases, 27 native
texture cases and five auditor corruption checks. The disposable project needed
its own standard ShaderLib includes; targeted reprocessing of the 35 earlier failed
source files clears all unintended asset failures. Only the twelve deliberately
invalid fixture jobs remain rejected. Other editor processes were left alone.

The 34 direct shaders and 2,240 Texture2D records also have a completed source
identity/type inspection over 445 unchanged dependency bundles. Their compressed
inputs total 15,932,470,464 bytes; the bounded private inspection allows 20 GiB in
total, 256 MiB per bundle and 600 seconds, retaining one bundle at a time. It took
104.281 seconds. Texture format 1 (Alpha8) occurs once; 1,566 records use format
12, 651 use 10, seven use 3, ten use 4, and five use 26. Resolving these records does
not qualify their shaders, defaults or complete appearance.

The initial projection preserves 5,475,737,390 payload bytes and 24,947 mip levels
for 2,239 records, with an independent source stream comparison in 243.125 seconds.
The additional Alpha8 record preserves its 5,592,405 bytes: 2048 by 2048 texels with
12 mip levels. Both source colour-space flags use native A8_UNORM; alpha receives
no RGB transfer conversion. Image builder version 3 invalidates generated caches
without changing the component identity or version-1 private packet format.

A repeatable synthetic Unity 6000.0.64f1 D3D11 fixture checks all 21 texels across
three mips for both colour-space flags: zero RGB and alpha equal to the supplied
byte divided by 255. The pinned native O3DE D3D12 viewport independently renders
both Alpha8 cases, an RGB-channel case and a flat RGBA reference. All 756 sampled
pixels agree; intentional wrong-mip, alpha-conversion and nonzero-RGB captures are
rejected, and original captures are restored. Native image fixtures now pass 31
cases (22 accepted, nine rejected), plus five auditor corruption checks.

After reprocessing with builder version 3, all 2,240 actual texture records and
24,959 original mips pass native image/format/byte comparison across 8,612 products
in 72.578 seconds within the 900-second private audit budget. There are no remaining
unqualified formats in this collected texture set. Native image data and synthetic
sampling remain separate from game shader/material binding and complete scene rendering.

The source material-slot census covers 15,675 direct Drake renderers and 87,229
merged definitions. Seventy-two bindings have different material and mesh-submesh
counts: 46 have two submeshes/one material, 18 have three/one, and eight have one/three.
Exact installed Drake code selects the requested submesh using each material's
ordinal. A synthetic Unity 6000.0.64f1 D3D11 BatchRendererGroup fixture independently
qualifies final-submesh clamping for overflow ordinals and retained repeated draws.
Nine cases pass, including additive multiplicity; 243 PNG pixels independently
match their captured assertions. The private source audit produces geometry plans
for all 127,883 serialized draws. Source baking/modification hooks still require
coverage before those serialized declarations become effective renderer state.

`foa_scene_draws.py` retains distinct material ordinals and exact typed identities;
unassigned source submeshes receive no projected draw. The qualified nonnegative
signed-byte ordinal range is 0 through 127; larger lists and unknown profiles reject.
Combined geometry retains a primitive per draw. A separate single-submesh projection
retains the original mesh binding and returns its selected source submesh index;
that report and the original source record remain required for reconstruction.
Several draw entities may reuse one geometry product without collapsing material
draws or losing their ordinals.

The native repeated-opaque test exposed reordering inside a combined model: green
appeared where the explicitly ordered source fixture produced blue. O3DE applies
one sort key to a model's primitives. The corrected fixture uses distinct native
draw entities under one parent, exact material assignment and explicit per-draw
sort keys. Four cases pass 108 sampled pixels, with independent processed-geometry
checks. Three deliberately wrong-order, missing-draw and extra-draw captures reject;
original bytes are restored. The failed combined-model capture is retained.

This demonstrates the native ability to reproduce an explicit draw order. It does
not establish the game's final order: inspected Entities Graphics code sorts draw
bins using filter/flags, material and mesh identities, submesh and batch fields.
Runtime resource IDs, queue/depth rules, LOD/filter state and source modification
hooks remain separate obligations. Do not derive final game rendering order from
the serialized material list or promote the synthetic fixture to game parity.

Final fingerprint inspection confirms the two inspected source assemblies are
unchanged. A separate final recheck covers all 32,474 audited native mesh/image
products (6,332,290,609 bytes) in 129.203 seconds, with zero unexpected unsuccessful
asset jobs. The complete native map scenes, four-map importer workflow and
one-to-one game comparison remain NOT_RUN; no full campaign map was launched.

## Exact transforms and expanded shader qualification

`foa_scene_transforms.py` joins the existing reciprocal source ownership graph to
original Transform records. It preserves source identity, parent/child order,
local TRS, active flags and raw-record fingerprints. Matrix construction does not
normalize saved quaternions or discard negative, nonuniform or zero scale. Bounds
are 500,000 nodes, depth 256, 256 MiB decoded input and 1 MiB per transform record;
cancellation and before/after record checks remain required. The mathematical
float64 matrices are diagnostics, not the game's float32 evaluator.

`FoaTransformProbe.cs` evaluates private source-bound TRS in Unity 6000.0.64f1,
matching the inspected installed UnityPlayer version. It creates plain temporary
objects and assigns serialized fields without running game scripts or scene loading.
The ordinary quaternion setter numerically changed saved rotations; serialized
assignment fixes that discrepancy. Editor signed-zero canonicalization is counted
separately. Original records remain required for source preservation and any future
return path. RectTransform layout and game baking are outside this fixture.

Capture schema version 3 carries raw input, assigned TRS and local/world matrix
float32 bits. `foa_scene_transform_capture.py` rejects older captures, duplicate
input fields, unknown profiles, changed numerical TRS, stale input hashes, missing
rows and inconsistent parent/matrix data. Signed-zero matrix bits survive JSON
through explicit integer bit fields. Arithmetic bounds are sanity checks; they do
not qualify a bit-identical host evaluator. Captures are private diagnostics, not
a public scene format or native persistence contract.

All 315,463 transforms across the 16 source scene files pass the capture audit in
65.718 seconds. The 221,972 signed-zero TRS canonicalizations are recorded; zero
numerical TRS changes are accepted. Another 128 synthetic transforms pass. Four
modified real-capture controls fail as expected. All 15,675 direct Drake renderer
records bind to exact owning source transforms; 4,659 retain nonidentity renderer
offsets. Serialized float4x4 columns are read without decomposition. Original
matrices and diagnostic double-precision products remain separate; the inspected
game multiplication and baking path has not been executed.

The pinned O3DE Transform representation cannot retain all reflected, nonuniform
and sheared source hierarchies. `source_affine_gpu_cases.py` and
`source_affine_gpu_editor.py` qualify a full affine shader transform plus its inverse
transpose normal matrix in six native viewport cases. All 72 sampled pixels match
independently calculated source-basis references. Two wrong-pose/normal controls
reject. Seventeen native asset jobs and 65 products complete successfully. The
fixture waits for model material slots before binding and has bounded process and
asset waits. Its per-material matrix constants do not yet provide a complete native
scene component, instancing, bounds, picking, undo or saved hierarchy behavior.

Expanded material inspection retains all 875 materials and 70 Shader identities,
including 36 additional fallback records. Equal payload hashes do not merge distinct
CAB/path-ID bindings. All 7,309 segmented entries have an explicit role: 5,838 code
records and 1,471 parameter records. Native disassembly agrees with all 54,131
resource bindings across 6,012 variants, with no unresolved inline sampler flags.
All 5,838 programs pass hardware D3D11 device creation with the debug layer and zero
warnings/errors. No complete game shader pipeline or every-variant draw is claimed.

There are 107,238 declared material properties and 14,487 retained undeclared saved
fields. Of 15,396 texture properties, 3,917 bind a material texture and 222 bind a
shader-owned texture; 11,257 still require effective shader-default qualification.
Twenty saved sampler combinations are preserved. All 113 source bundles rechecked
by the expanded binding stage are unchanged. Source binding took 40.719 seconds;
all-program device creation took 18.171 seconds. Full scene/UI performance remains
unmeasured. There are 226 passing focused scene tests.

Complete native campaign scenes, a usable four-map importer launch, lighting,
navigation, remaining component families, effective material/pipeline behavior,
manual one-to-one game comparison and game export remain unfinished. Private
fixtures and source-record preservation do not satisfy those acceptance lanes.

## Native source shader path (2026-09-11)

The SDK now builds private `.foashader` packets into native Atom ShaderAsset and
ShaderVariantAsset products on the pinned Windows DX12 host. The original SM5.0
vertex and fragment bytecode is retained. Stage-local SRGs preserve distinct VS
and PS constant buffers even when they share a register number. No generic PBR
material, recompiled substitute, pass selection or inferred render state is used.

The private `FOASHD01` version-1 packet has a 56-byte header, an explicit profile
identifier, bounded body length and a SHA-256 over the header prefix plus body.
Its body contains a draw-list name, twelve explicit raster/depth/blend values,
and exactly two bounded DXBC programs with their constant-buffer extents, image
types and sampler registers. Whole packets are bounded to 64 MiB; each program to
32 MiB. The reader rejects unknown versions/profiles, extra/truncated bytes,
wrong shader stages, overlapping chunks, duplicate registers, invalid states and
unsupported MRT blending. Structured/UAV resources and additional shader stages
are not implemented. This is a private native asset projection, not a canonical
scene format, source authenticity proof or game export format.

`SourceShaderRenderComponent` exposes a private automation boundary for one
explicit draw. Its version-1 JSON descriptor carries shader/image asset paths,
indexed vertex streams, separate constant buffers and complete sampler states.
Input is bounded to 16 MiB, sixteen streams, one million vertices and sixty
seconds of asynchronous asset loading. Resource counts, extents and vertex
semantics must agree with the native shader asset. Invalid initial descriptors
retain the existing draw; a later asset/binding failure reports FAILED and does
not submit a replacement. Draw submission uses the native viewport notification
and DynamicDrawInterface. Geometry, SRGs and buffers remain alive until the
renderer releases its retained draw packet. Retired draws are bounded.

The exact-pin build passes. Two shader asset jobs pass; five malformed packets
reject without products. Native variant products contain all four original
program byte sequences unchanged. The O3DE DX12 viewport renders both a synthetic
reference and the original source HDRP Unlit VS/PS pair with explicit fixture
constants and synthetic textures. All three rendered color buffers, including
recreation after rejected inputs, match byte-for-byte at the observed 4077 by 399
viewport. Forty-eight sampled pixels verify four quadrant colors and removal.
Twelve invalid descriptor/binding cases and two wrong-image comparison controls
reject. Two engine DynamicDrawContext shutdown warnings occur after capture;
this is not a zero-warning host claim. A first run could not write its selected
output location; the successful run uses private system temporary storage.

This qualifies one source shader pair under stated inputs. The single-draw
boundary is not the complete scene renderer: source-linked entities, shared mesh
buffers, instancing, camera/material updates, game pass scheduling, effective
shader defaults, lighting/probes and additional shader resource families remain
required. The complete native scenes and four-map importer workflow remain
unfinished, as do game comparison and export. No complete campaign map was
presented as ready for user testing.

## Native source placement component

`SourceScenePlacementComponent` is an editor-only component owned by World
Authoring/Entity Placement and implemented in Framework. Its private version-1
binding retains the source bundle/record fingerprints, serialized file, signed
64-bit Transform and GameObject identities, original source-parent identity and
captured local/world float32 matrix bits. Bindings are bounded to 8 KiB, immutable
once accepted and stored in the native prefab. They are not a replacement for the
original source records, a public interchange format or an authenticity claim.
Unknown versions, profiles, duplicate fields, malformed identities, nonfinite or
perspective matrices and invalid bounds reject. Initial binding requires the
correct already-bound source parent and an exact translation-only native anchor.

Source Y and Z axes are exchanged by coefficient permutation. The unchanged world
matrix returns those captured bits directly, including signed zero; there is no
TRS decomposition, quaternion normalization or source-parent re-evaluation.
The native Transform supplies a separate placement delta around the object's
original world origin. The effective matrix is the current native anchor times
the full original affine matrix with its translation removed. Native parent edits
therefore propagate through the editor hierarchy while source scale/reflection/
shear remain in the component. The original local matrix and source-parent record
remain unchanged. Source-local TRS export of these edits is not implemented.

The component provides transformed bounds to the existing editor selection bus.
These are bounding-box selection bounds, not triangle-level ray intersection or
collision. A null source bound remains null. The source binding is hidden from
the normal inspector; native Transform controls edit placement. Binding does not
attach a renderer, resolve a mesh/material, interpret arbitrary game components or
provide game-mode execution. Consumers must check the component's READY state.
Hierarchy assembly, copied-object identity policy, large-scene partitioning and
renderer integration remain separate obligations of the complete scene workflow.

`Tools/editor_tests/source_scene_placement_editor.py` runs in a private level and
uses a second Editor process for saved-reopen checks. Its fixture includes exact
captured hierarchies from all four maps and synthetic reflection, nonuniform
scale, shear, singular-matrix and parent-edit cases. Native acceptance results are
recorded separately from source preservation and from complete map rendering.

Native placement acceptance passes 16 objects (nine source objects from selected
hierarchies across the four campaign roots, plus seven synthetic controls). The
unchanged matrices and a fresh process's saved edited matrices match bit-for-bit.
Editor API selection, numerical bounds and move/undo/redo pass. Parent rotation
and scale retain full source affine data. Nineteen malformed bindings reject;
three independently altered evidence/prefab controls also reject. The independent
edited-matrix reference has maximum coefficient error below 0.000000799 for these
cases. This does not qualify every scene object, triangle picking or rendering.
The private fixture runs with its Asset Processor connection disabled and records missing icon/scan-folder
diagnostics plus post-test pipeline shutdown warnings; those are retained, not
reported as a clean production Editor launch.

## Shader default textures

`FoaTextureDefaultProbe.cs` measures six synthetic shader declarations matching
the observed default-name/dimension pairs. It tests material/global/null precedence
in Unity 6000.0.64f1 on Direct3D11, separately in Gamma and Linear color space. The
fixture loads no game content. Independent acceptance covers sixty draws, 240
sampled pixels and three corrupted-result controls. These are sampling and binding
precedence measurements, not a complete native resource reconstruction.

The five named 2D defaults account for 8,638 unresolved source properties. Their
material defaults resist global texture overrides in this fixture. The empty
2DArray declaration accounts for 2,619 properties and permits global overrides
when its material texture is unset or null. Its observed fallback and named grey
change RGB behavior with color space. Black has zero alpha; bump and linearGrey
retain 127/255 values in their measured channels. Do not replace these resources
with assumed white/black images or infer game runtime globals from the fallback.

Material.GetTexture returns null for unset defaults, so the probe does not establish
the underlying default resource dimensions, mip data or sampler descriptors.
Those resource facts, native projection and actual game runtime overrides remain
unqualified. The original game settings file is unchanged; its color-space field
was not decoded because the inspected record lacks an embedded schema. Neither
sampling both modes nor the game shader names establish that missing field.

## Persistent native source draws

`SourceSceneRenderComponent` attaches one explicit source draw to an editor entity
with `SourceScenePlacementComponent`. Its private version-1 binding contains an
unchanged version-1 draw descriptor and one to eight explicit matrix destinations.
Each destination identifies the shader stage, constant-buffer register and aligned
64-byte offset. The supported value is `source_object_to_world`, packed as four
column-major float32 columns. Source shader metadata must establish the destination;
the component does not discover register meanings or renderer baking offsets.
Original mesh channels and source shader programs retain source axes. The placement
matrix is converted back from host axes by coefficient permutation before upload.

The binding is immutable after acceptance and persists in the native prefab. A
rejected initial binding leaves the entity unbound. Native transform events mark
that entity's matrix data dirty; visibility events suppress its draw. Deactivation
removes the drawing registration. Activation reconstructs resources from the saved
binding. The existing single-draw automation API remains available independently.
No camera-relative origin, inverse matrix, previous-frame matrix, lighting input,
material animation, source bake or game pass schedule is inferred.

The drawing service currently limits admission to 256 entity draws and 64 MiB of
resident raw stream/index/constant payload, including retired draws. JSON is bounded
to 16 MiB per binding. At most four native resource builds or matrix updates run per
editor tick. Dirty draws are not submitted until their matrix update completes.
Asset loading remains asynchronous with a 60-second deadline. This is a bounded
integration step; it does not provide the buffer sharing, instancing, large-scene
partitioning, culling or all-submesh renderer assembly needed for complete maps.
Private binding strings, shader metadata and explicit fixture inputs are not a
public campaign interchange format or a game export format.

The entity-render fixture compares source-shader draws with separately transformed
reference geometry and checks edit, visibility, deletion and reopening behavior.
Its current executed result is recorded in the private evidence pack. Passing this
fixture does not qualify complete native scenes or the four-map import workflow.
Native entity-render acceptance passes ten edit/reopen states using the original
HDRP Unlit shader pair, synthetic affine geometry and explicit fixed-camera inputs.
All full color buffers match separately transformed reference draws byte-for-byte;
432 sampled pixels, 19 invalid bindings and three altered-image controls pass.
An eight-draw batch respects four resource builds/updates per tick and releases
removed payloads. A fresh Editor restores the saved draws. The unattended fixture
requires --autotest_mode so the pinned editor continues ticking without focus.
Disabling helper icons removes unrelated editor overlays from the image comparison.
Earlier failures and shutdown diagnostics remain recorded. The 64 MiB payload
limit counts raw streams, indices and constants, not total GPU texture/shader memory.


### Explicit viewport bindings

Private render binding version 2 adds camera matrix and vector destinations to
version 1. The version-1 reader shape remains supported without implicit migration.
Version 2 requires `vectors`; each destination identifies a stage, constant-buffer
slot, aligned byte offset and layout. Matrix destinations support
`source_object_to_world`, `viewport_source_world_to_clip` and
`viewport_source_relative_world_to_clip` with `column_major` layout. The vector
`viewport_source_camera_position` uses `float4` layout. Overlapping destinations,
unknown fields, layouts or versions reject. These are editor projection bindings,
not a public scene schema or runtime camera contract.

The viewport matrix exchanges source/host Y and Z input axes. The relative form
also composes the host camera translation before that exchange; original source
programs with explicit camera subtraction receive source-axis camera position
with a zero fourth lane. Camera changes dirty only camera-dependent draws and use
the existing four-updates-per-tick budget. A missing viewport suppresses stale
draws; initial loading remains subject to its deadline. Previous-frame matrices,
TAA jitter, stereo views and game render schedules remain unqualified.

Acceptance covers absolute and relative projection, camera translation/rotation,
native placement/visibility edits and fresh-process restoration: twelve full color
buffers equal separately transformed references, with 528 pixel checks and 29
invalid bindings. A saved version-1 binding also reopens through the new reader.
These tests do not establish complete map performance or game visual parity.

### Measured shader-default resources

`shader_default_2d` accepts an explicit Unity version and rendering color space.
For the qualified version it produces the five measured built-in 2D defaults using
the existing v1 texture packet. Each resource is 4x4 RGBA8 with one mip. Black
retains zero alpha; bump and both grey defaults retain their measured 127-byte
lanes. In linear rendering, white, black and grey use sRGB views; bump and
linearGrey use UNORM views. Gamma rendering uses UNORM for all five.

The caller must first resolve a shader-declared default. This helper does not
replace a missing required texture or resolve a runtime global. Array fallback
and global-override behavior remain separate from this 2D projection. The isolated
native probe records texture/view/sampler descriptors and raw subresources using
bounded readback. It requires verified direct rendering for immediate native
inspection; the event-callback route did not retain the draw's bindings.

Sampler capture includes the quality setting: the tested array fallback changes
under ForceEnable anisotropy. The measured 2D default sampler uses linear min/mag,
point mip filtering and wrapping. The original D3D11 zero anisotropy member is
inactive for this filter; the D3D12 projection uses a valid value of one and keeps
the original descriptor in evidence. The native reader accepts exact finite
float32 extrema and continues rejecting nonfinite or overflowing LOD values.
Shader defaults and these tests do not supply missing game runtime inputs.

Native acceptance passes all ten 2D default images (five declarations in both color
spaces): native cache formats and bytes match the captured resources, and original
Unlit shader renders equal the independent reference color buffers. The fixture
checks 24 captures, 116 sampled pixels, 15 rejected bindings and three deliberately
wrong comparisons. Direct and persisted render descriptor readers use full-precision
JSON parsing to avoid rounding the exact float32 limit above the allowed range.
Native arrays, complete game-material/pipeline integration and map parity remain
separate unfinished work.


### Explicit texture-array projection

World Authoring owns source resource identity; Framework owns the native image
builder and Tools owns the private GPU packet producer and its verification.
This Significant change extends the private `.foatexture` contract. Existing v1
2D packets and their hashes remain byte-identical; no saved source migration or
canonical/game-format change is involved. Old consumers reject v2. The new
consumer accepts both versions and its Asset Processor builder version is 4.

Version 2 uses the same `FOATEX01` magic and a 68-byte little-endian header:
magic, version, width, height, mip count, source format, colour space, array layer
count, then SHA-256. Integer fields are uint32. The digest covers the 36-byte
prefix followed by the payload. Its explicit kind is Texture2DArray even when
there is one layer. Version 1 keeps its 64-byte header and single 2D surface.
Array payload order is mip-major, then layer, with original tightly packed rows
or compressed blocks inside each layer. A mip layout's size is one layer and its
offset addresses that mip's first layer. Neither version flips or resamples data.

The new producer takes an explicit TextureArray value. It does not extend the
proprietary Texture2D record reader to unqualified Texture2DArray layouts and
never supplies a missing runtime global. Arrays have 1–256 layers, existing
1–8192 dimensions and valid mip counts. Both total source bytes and native bytes
(including explicit RGB24 opaque-alpha expansion) remain bounded at 128 MiB.
Unknown versions, invalid counts/formats, mismatched payloads and failed hashes
reject before product publication. Cancellation remains checked between mips,
layers and bounded RGB expansion blocks. The existing streaming-tail policy
counts every layer, and an explicit array view preserves single-layer SRVs.

Verification covers legacy v1 bytes, all supported formats, rectangular block
mips, aggregate limits, malformed inputs, native cache subresource ordering and
GPU Load/SampleLevel comparisons against independent per-surface references.
These checks qualify resource transport and sampling; game array contents,
material passes, lighting and complete scene rendering require their own inputs
and evidence.


### Stored rendering color space

A narrow read-only settings reader now resolves the stored build color-space
scalar. The installed general class database fails to decode PlayerSettings for
the exact profile; its fallback is not used. The synthetic Unity settings probe
uses the exact installed editor's
[game-manager writer](https://raw.githubusercontent.com/Unity-Technologies/UnityCsReference/6000.0/Modules/BuildPipeline/Editor/Managed/ContentBuildInterface.bindings.cs)
to emit Gamma and Linear records with embedded type trees. Both generated trees
have identical structure hashes matching the inspected game's PlayerSettings
record. A separately pinned schema-content hash prevents an altered tree from
being accepted merely because its declared structure hash matches.

Unity's native binary inspector reports the expected Gamma/Linear scalar values
in the generated records. The qualified reader locates that scalar through the
schema, requires the exact source version/platform/type, bounds the record at
64 KiB and checks an exact in-memory roundtrip of the schema-described prefix.
All three records have four zero bytes after that prefix. Those bytes remain an
explicit opaque suffix; they are neither interpreted as padding nor discarded.
Unknown suffixes, schemas, versions, scalar values and changing records reject.
Whole-record interpretation therefore remains PARTIAL. Original bytes remain
return authority, and this reader performs no writes or game launch.

The inspected game record stores Linear. Fourteen malformed-input controls pass,
and the synthetic writer rejects an existing output without replacing its report.
This proves a stored build setting, not the live renderer's active color space,
quality level, shader globals, light buffers or camera state. Those inputs still
need separate evidence before complete material rendering can be claimed.


### Shared native geometry and bounded scene scheduling

World Authoring retains source identity and placements. Framework owns this
Significant extension of the existing drawing service. Identical immutable
input-assembly bytes share storage and a native buffer; a SHA-256 lookup is
followed by full byte equality before reuse. Each draw retains its own semantic
layout, views, shader/material binding, source identity and mutable constants.
No saved binding version, game format or public interchange changes.

Admission is transactional and limited to 1,024 entity draws, 32,768 unique
geometry buffers, and 64 MiB of unique geometry plus per-draw constant payload.
Retired draws keep their ownership until native frame references release them;
removing the final owner removes the cached bytes and buffer. These counters
exclude descriptor strings, container overhead, driver allocations and textures.
No total GPU or process-memory budget is claimed. Oversized/conflicting data and
missing assets reject without removing an existing direct draw.

Dense round-robin scheduling visits at most 64 entities and performs at most four
resource builds or placement updates per tick. Removal repairs the dense index;
geometry hashing/parsing occurs during binding, never during camera updates or
render ticks. Native acceptance must check sharing, independent edits, removals,
rejected admissions, saved reopen, full-image comparisons and both tick budgets.
This raises bounded scene capacity; full-map culling, partitioning, camera-global
sharing, all material families and the importer remain separate requirements.

The first 1,024-object native run exhausted DX12 sampler tables because each
object duplicated an immutable fragment-stage SRG. The continuation shares a
whole stage only when it has no per-entity matrix/vector destinations and its
complete explicit constants, images, samplers, stage and resolved shader/image
instances agree. Hash matches require full signature equality. Dynamic stages
remain private. Shared-stage ownership follows the same retired-draw lifetime.
The service reserves at most 512 sampler entries before native stage creation;
native frame rings multiply that count. This limit does not guarantee capacity
for unrelated engine users or every material family. Resource failures remain
explicit. Native performance and pixel proof must use the corrected build.


Executed shared-resource acceptance passes 1,024 repeated synthetic entities in
6.735 seconds for the capacity phase. Three geometry buffers and one immutable
material stage serve those entities; each object's constants remain independent.
Fifty large distinct-geometry draws fill the raw-payload budget before the next
admission rejects without publishing partial cache entries. The sampler test
creates 511 additional distinct material stages and rejects three more before
native pool exhaustion; removal restores the original stage and reservations.

The native fixture includes a different-UV object sharing only its identical
position/index resources. All twelve complete RGB comparisons and 528 independent
samples pass, including a fresh-process saved reopen. Twenty-two malformed or
capacity controls and five corrupted-evidence controls pass. The independent
`source_scene_sharing_cases.py` auditor requires both native runs and verifies
resource accounting, capture hashes, full-image equality and sampled RGB values.
The final-owner cleanup returns all service counters to zero. These are bounded
native resource/lifecycle results, not total memory, full-map frame rate, shader
hot-reload acceptance, complete materials or game-runtime parity.

### Bounded source-hierarchy assembly continuation

World Authoring owns a private, versioned hierarchy handoff produced from the
source ownership snapshot and the qualified Unity float32 capture. This is a
Significant producer/consumer contract. Framework's existing placement component
consumes each immutable source binding; the Editor adapter owns entity creation,
undo and rollback. Public canonical interchange and the component serialization
version stay unchanged. Source GameObject names and active flags remain explicit.
Editor visibility uses effective `active_in_hierarchy` for authoring; it does not implement
runtime activation or any other source component behavior.

A plan retains the complete ordered hierarchy of one explicitly selected primary
scene, its bundle/record identities, input fingerprints and captured matrix bits.
The producer checks reciprocal children, parent order, ownership uniqueness and
source-to-capture correspondence. An assembly batch selects explicit Transform
identities and includes every ancestor. It never infers an owner by position or
name. Batches are bounded at 4,096 objects and 16 MiB. A batch is complete only for
that scene's hierarchy when all source objects are selected; it is never evidence
of a complete rendered campaign scene. Source records and unsupported components
remain required for return. No render/material/navigation/light implementation is
implied by creating placement entities.

Preflight rejects malformed, duplicate, incomplete or conflicting identities
before native mutation. Native creation runs in bounded steps, preserves hierarchy
and source bindings, and rolls back every created entity after cancellation or
failure. Cleanup failures remain explicit and report outstanding native entities.
The Editor adapter must preserve unrelated entities and verify complete imported
bindings, names, visibility, parentage and exact unchanged matrix bits, including
save/reopen and undo/redo. Four-map source preparation and native execution are
separate gates. Campaign import and game export remain unavailable until their
complete routes are qualified.

The private placement binding gains version 2 for exact source matrices whose
editor anchor is rounded by float32 parent-relative storage. Version 1 retains
its existing closed fields and behavior. Version 2 adds `native_anchor_bits`
(three unsigned float32 bit patterns in host coordinates). The producer derives
this anchor using the ordered native translation subtraction/addition. The native
consumer checks that derivation against the actual unedited parent and anchor.
This is an editor placement baseline, not a modified source transform. Unchanged
objects return their original matrix bits; native edits compose a delta from the
recorded editor baseline with the original full affine transform.

Existing version-1 descriptors and saved component serialization remain readable;
the component's serialized `Source` string and type ID do not change. Unknown
versions and unexpected fields reject. Older native readers reject version 2, so
downgrading new assembly batches requires retaining/reopening the old private
level with its old matching build; no downgrade converter is claimed. Acceptance
must include both new rounded-anchor cases and existing version-1 saved placements,
malformed/mismatched baseline rejection, controlled edits, undo/redo and fresh
saved reopening. The Unity and O3DE pins, game profile, canonical interchange,
installer and runtime adapters are unchanged.


The native consumer publishes each fully configured entity to the prefab cache
inside the undo batch before yielding to Qt. The acceptance test must restore
complete source bindings after redo, not merely the same number of entity shells.
Cancellation keeps the progress dialog modal until rollback finishes. Native
modal tests run without `--autotest_mode`: the pinned host's modal-window dismisser
closes dialogs under that flag and would cancel the import before creation.

`GetWorldMatrixBits` reads the actual native matrix through unsigned integer
coefficients. Float-to-double script conversion can flush subnormal coefficients
to zero under the host's floating-point mode. Exactness checks therefore use the
integer readback and must include subnormal source values. This does not change
the matrix delivered to the native renderer, source records, or saved bindings;
shader arithmetic and full visual parity require their separate GPU proof.


Executed acceptance prepares 315,463 objects across 16 source scenes and imports
5,204 of them natively: the four complete root hierarchies plus explicit selections
with all ancestors from the remaining scenes. Exact native matrix bits, identities,
parentage, names and effective visibility survive import/save and fresh-process
reopening. Seven native controls pass, including cancellation, rollback after seven
created objects, undo/redo, duplicate preflight, wrong-bundle parent rejection,
three malformed anchors, and editing the rounded Forlorn anchor. Ten corrupted
acceptance reports reject. The fixture verifies a real subnormal coefficient and
reads all three old version-1 source bindings plus two previously captured edited
placements without rewriting the older level. Full native scenes, material and
component families, the operator importer and game export remain unfinished.


### Native multi-draw entities

The Framework-owned renderer accepts an explicit private version-3 group binding:
`{"version":3,"draws":[...single-draw version-1 or version-2 bindings...]}`.
World Authoring remains responsible for qualified source submesh/material/pass
selection. The native service never infers those selections. Array order is
preserved, including repeated geometry and materials. Explicit sort keys must
increase strictly within the group; this establishes only the requested native
ordering, not the game's full renderer-bin ordering. Groups cannot nest.

One group belongs to one editable placement entity. All draws follow its matrix,
visibility, deletion and persisted binding. Admission validates the complete group
before publication and releases earlier reservations if a later admission fails.
Rendering waits until every member is ready, visible, current and in the correct
scene; one missing/failed member cannot leave a partially rendered object.

This Significant private contract adds no component field, type ID, canonical
interchange, engine pin, game profile, installer or runtime-adapter change.
Existing v1/v2 single-draw bindings retain their closed schemas. Older readers
reject v3; retain the older level/build when downgrading, as there is no converter.
The complete group is bounded to 128 draws and 16 MiB. The service's 1,024-draw,
64 MiB raw-resident and 512-sampler limits apply across all groups. Scheduling
continues to limit work to 64 draw visits and four builds/updates per tick.
Required acceptance covers distinct submeshes/material data, multiplicity/order,
all-or-nothing failures, resource limits, transforms/visibility, undo/redo,
delete/restore, saved reopening and old single-draw compatibility. Implementation
and executed acceptance are recorded separately; this contract does not establish
complete map or material rendering.

Executed grouped acceptance uses three native entities and four draws, with two
independent material UV transforms bound through the previously qualified original
Unlit shader layout. The reference bakes those UV transforms into separate geometry
and uses the unchanged identity material. All 13 complete RGB frame pairs and
1,152 independent color samples match, including move/rotate/scale, undo/redo,
visibility, delete/restore, failed-member suppression and fresh saved reopening.
Versions 1, 2 and 3 retain exact source/binding state in the reopened level.

Fifteen native rejection cases cover malformed groups/order, missing assets,
late native failure, aggregate draw capacity and failure after earlier members
reserve memory. Eight corrupted-evidence controls reject, including merged
material state and wrong-pose/missing-object images. The native capacity test
reaches 1,024 draws across 11 entities, then removes alternating groups to exercise
dense scheduler repair. It preserves two distinct immutable material states while
sharing identical geometry. Maximum work remains four builds/updates and 64 visits
per tick. A later byte-budget rejection leaves all live counters unchanged; final
removal releases every draw, stage, sampler and accounted byte.

The fixture explicitly disables native diagnostic text before full-frame capture;
no pixel region is cropped or masked. Initial overlay/sample-control failures
remain private evidence. The tested source pair does not qualify other shaders,
source pass selection, game draw-bin ordering, full-map performance or export.

### Rendered hierarchy transaction

World Authoring connects a qualified hierarchy batch to explicit native render
bindings through a separate private `foa.source-render-assembly` version-1 packet.
It binds the exact hierarchy bytes and each complete placement descriptor by
SHA-256, including bundle/file/path identity and original transform state. Hashes
bind already qualified inputs; they neither establish source ownership nor select
a material, shader pass, instance owner or missing component mapping.

The packet carries one native v1/v2 draw or v3 group per selected rendered placement.
Unknown, duplicate or stale placement bindings reject before mutation. The packet
is bounded to 64 MiB and 1,024 aggregate draws, with the existing 128-draw/16 MiB
per-object bounds. Hierarchy-only v1 packets and native component serialization
remain unchanged. Earlier readers reject the separate new packet; no conversion
or complete campaign/export claim is implied.

One modal native import transaction creates placements, attaches their explicit
render groups, verifies exact binding readback and waits for every renderer to
become ready. Readiness polling yields to Qt, uses the existing 32-operation step
bound and has a 120-second timeout. Cancellation, bind failure, native resource
failure or timeout roll back every entity created by the transaction. Unrelated
entities are retained, and removal failures report their exact recovery IDs.
Prefab state includes both placement and rendering before yielding so undo/redo
and saved reopening restore the complete imported objects. The front-end receives
success only after native readiness, not merely after creating entity shells.

### Direct source renderer assembly

World Authoring now prepares direct Drake renderers from the reciprocal source
GameObject/component graph and a complete qualified hierarchy. Component types
resolve through exact MonoScript references. Every hierarchy Transform record,
owner, parent and primary serialized-file identity must agree with the source.
Renderer and LOD-group records retain their own fingerprints and explicit links.
Names and positions do not participate in the join.

Preparation preserves exact typed mesh/material selectors, repeated material
ordinals, LOD masks and reciprocal group membership. Unknown scripts, changed
records, unsupported selectors, missing groups and inconsistent membership fail.
Merged archive instances are outside this direct-component route; no owner is
inferred for them. Serialized LOD membership does not implement runtime LOD
selection, material modification hooks or game draw-bin order.

The immutable private preparation result can join explicitly qualified native
bindings to selected renderers and emit the existing hierarchy/render packets.
Selected renderer records must match exactly. Multiple renderers on one object
keep every draw in their supplied order, and the native importer receives one
combined group for that placement with the complete ancestor hierarchy. Missing
material ordinals, duplicate/stale renderer selections, invalid group order and
capacity overflow reject without truncation. No persisted schema, existing reader
or native component version changes in this addition.

This preparation runs outside the Editor event loop. Source limits are 100,000
renderer/group records, 256 MiB of additional decoding, 4 MiB per source record
and two million LOD edges. Cancellation is checked while visiting source objects
and references. Existing native packet and ancestor-closure bounds still apply.

### Merged archive placements

World Authoring prepares exact selected archive-instance ordinals through the
private `foa.merged-placements` version-1 packet. Each source contains a map key,
archive fingerprint, member GUID/fingerprint and complete instance count. Entries
retain the original 56-byte instance record and its fingerprint. The producer and
consumer compare every matrix bit against those bytes before native mutation.

Native placement binding version 3 uses archive/member/ordinal identity. It has no
GameObject ID or source parent. Four stored float3 columns expand to an affine
world matrix by bit permutation; no loader transform, normalization, TRS
decomposition or LOD filtering is applied. Selected instances are visible for
authoring; this does not establish their runtime activation or LOD policy.

The existing bounded import transaction and explicit render packet accept these
placements. Duplicate source instances reject before mutation, and failure,
cancellation, undo/redo and saved reopening retain the same obligations. Batches
remain limited to 4,096 placements/16 MiB and rendering to 1,024 draws. The caller
must qualify loader/member association and material bindings before import.

This Significant private persistence addition leaves component serialization and
old placement bindings v1/v2 unchanged. Earlier builds reject the new v3 binding;
keep the prior build and level for downgrade. No lossy downgrade or GameObject
conversion exists. Native and source-layout execution are recorded separately;
this contract alone does not qualify full scenes or game return.

Executed merged-placement acceptance covers all 87,229 original instance records
across eight members/four maps. The original managed matrix-expansion method and
struct layout agree with every prepared bit under a standalone .NET 10 probe;
this is neither Unity-player nor game-runtime evidence. The pinned native Editor
imports 40 selected real placements and undoes them without retaining partial
objects. A synthetic three-placement/four-draw fixture retains exact rendering,
editing, transaction failure behavior and fresh saved reopening using the one
previously qualified source Unlit shader pair. Old saved v2 scenes (5,204 objects)
and three v1 bindings also reopen with unchanged matrices and source descriptors.
Viewport triangle picking, full-map streaming and the remaining scene/material
mappings are not established by these checks.

### Constant arrays and structure values

The private material conversion helper accepts source float4/int4 vector arrays,
column-major float4x4 arrays and flat variant-format structure arrays. A structure
retains its explicit member-relative offsets, element stride and array count.
Common-format structures, nested structures, narrower vector arrays and other
matrix/scalar shapes remain unsupported. The supported source declarations agree
with [Microsoft's HLSL constant packing rules](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules);
compiler reflection and GPU execution qualify the tested byte layouts separately.

Array inputs are exact-length lists of explicit elements; structure arrays are
lists of member dictionaries. Missing/extra fields or elements reject. Matrix
inputs remain 16 column-major floats. Integer inputs remain signed 32-bit values;
the helper does not infer unsigned semantic conversions. Only undeclared/padding
bytes are zeroed. Buffers are limited to 64 KiB and 4,096 expanded field values;
checks reject excessive work before expansion. All fields, extents, overlap,
alignment and identities are revalidated before packing a caller-supplied layout.

This Significant World Authoring/ExternalToolchain change extends a private Python
layout/value contract. Existing scalar/vector/matrix layout dictionaries retain
their exact shape and bytes; new structured layouts add a `structures` dictionary.
Native shader/draw packets, component serialization, source identities, public
canonical formats and runtime adapters are unchanged. Old converters reject these
new shapes; no persisted-level migration or game-format conversion is introduced.

All 6,012 captured stage layouts from 70 source shaders pass declaration checks;
631 contain arrays and 2,999 contain structures. Synthetic compiler reflection and
hardware tests exercise explicit offsets, integer/vector/matrix arrays and a
nonzero structure-array base. Wrong strides and matrix transposes fail independent
pixel checks. The original Unlit instancing pair also executes with explicit test
values: selecting its second structure element matches the prior source shader,
while selecting an out-of-view first element fails the normal pixel oracle.

The unchanged original pair is retained in the native shader product and draws
through the existing pinned O3DE service. Four full RGB captures agree, with 180
sampled pixels, 15 malformed/native resource checks and two comparison controls.
This proof uses synthetic geometry/textures and explicit fixture globals. It does
not supply game lighting, instance/visibility/LOD policy, structured GPU resources,
other shader families or full campaign material settings. Full-map visual parity
and return-to-game acceptance remain unqualified.

### Read-only raw and structured GPU buffers

The private native shader packet supports version 2 alongside the unchanged
version 1 reader. Each v2 stage appends a bounded buffer count and explicit
`slot`, `type`, `stride` triples after its sampler descriptors. Type 2 is structured;
type 4 is raw with stride 4. Strides must be multiples of four from 4 through
2,048 bytes. Buffers and images share the same stage-local `t` register namespace;
constant buffers and samplers retain their separate namespaces. Every declared
raw/structured resource must have exactly one matching supplied descriptor.

Types and strides come from the original compiled declarations, following the
published [raw resource declaration](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-resource-raw--sm5---asm-)
and [structured resource declaration](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-resource-structured--sm5---asm-)
formats. The bounded reader rejects unsupported declaration operands, extended
forms, duplicate slots and malformed instruction extents. It does not derive
buffer contents or lighting/instance semantics from names. SM4 remains outside
the native SM5 profile. UAV resources remain unsupported.

Raw draw JSON version 2 requires a `buffers` list in both stages, with strict
`slot`, `type`, `stride`, `hex` rows. Contents are explicit opaque bytes; each
payload is nonempty, element-aligned and limited to 8 MiB within the existing
16 MiB draw document and 64 MiB resident-service limits. The renderer validates
access/type/stride against the processed shader before binding native read-only
buffer views. Constant-buffer accounting retains its meaning; the added
`read_only_buffer_payload_bytes` statistic reports data-buffer payloads separately.
These are accounted payload sizes, not total process memory or GPU allocation size.

This Significant World Authoring/ExternalToolchain and Framework change adds a
private packet version. Existing packets without buffer descriptors retain their
version 1 bytes; native readers accept both versions. Entity binding envelopes and
component serialization are unchanged, and new buffer packets survive native
save/reopen. The asset builder version is 2 to reprocess affected cached products.
An earlier build cannot read the new packet; keep its original levels and binary
for downgrade. No lossy downgrade, public canonical schema change, installer or
runtime-adapter migration is supplied.

The 70-shader source census independently compares all 5,944 SM5 stages with
Microsoft disassembly. Its 1,519 buffer-consuming stages contain 2,422 raw and
3,246 structured declarations with eleven observed strides; 68 SM4 stages are
explicitly excluded. Thirteen valid native assets retain 26 exact stage programs,
including original Unlit DOTS and architecture programs, while three inconsistent
resource tables reject. Two prior shader products remain byte-identical after
reprocessing. Native asset success does not establish source rendering parity.

Pinned O3DE DX12 GPU tests cover both shader stages and all eleven observed
structured strides. Sixteen complete RGB frames match the reference; four
wrong-content/removal frames fail it. All 720 pixel samples and 31 rejection
controls pass. The controls exposed and fixed a false-ready condition: the native
pipeline cache can return a non-null uninitialized pipeline after compilation
fails. Such a pipeline now rejects before draw submission.

A synthetic buffer-backed entity passes move/undo/redo and exact saved binding
reopening, with five CPU-transformed reference pairs and 360 sampled pixels. The
existing grouped scene, transaction and capacity regression also passes with
thirteen frame pairs, 1,152 samples and fresh reopening. The new tests qualify
resource binding and persistence under explicit fixture values. Original DOTS,
light-list and visibility data, complete material/pass selection and HDRP render
policy still need mapping and execution proof. Full campaign rendering, navigation,
lighting authoring, four-map UI and game export remain unfinished.

## Exact source Light ownership and migrated properties

`foa_scene_lighting.py` is a private, in-memory source adapter. It consumes the
validated `SceneOwnership` graph and a `ComponentScriptAudit` with its explicit
dependency loader. It captures native Light, GameObject, Transform and same-owner
MonoBehaviour records, their raw bytes and hashes, and the exact resolved script
identities. Display names do not establish a Light/HDRP/controller association.
One native Light and at most one exact HDRP companion may bind to an entity.

The qualified source profile is Unity 6000.0.64f1 with the fingerprinted HDRP
runtime assembly recorded in the adapter. Shipped scene files store the observed
`0.0.0` version marker. Accepting that marker requires the separate exact profile
and embedded record schemas; it does not authorize an ambient class-database
fallback. The profile fingerprint identifies input bytes, not execution authority.

Inspection of the exact HDRP assembly establishes that its v13 intensity, type,
unit, lux-distance and reflector APIs delegate to native `Light`. Older serialized
additional-light fields remain migration storage. `stored_light_values` therefore
reads native fields without substituting those obsolete values. Unknown migration
versions retain the complete original records but expose no qualified value
projection. No enum name, photometric conversion, color-space conversion, active
controller state, GPU light list or shadow/bake result is inferred.

The snapshot schema `foa.private.source-lighting`, version 1, is private evidence,
not public canonical interchange or a game-export format. `raw_hex` is the byte
preservation authority. `tree` is an inspection view: nonfinite decoded floats
use an explicit `$foa_float64_bits` tag and byte arrays use `$foa_bytes_hex`.
The view must not be used to reserialize original source records. `stored_values`
is null with a `BLOCKED` state when an HDRP migration remains unqualified.
Bounds are 32,768 lights, 4 MiB per record, 64 MiB of captured source records and
256 levels of view depth; cancellation is checked through entity/record traversal.

The all-map source check captured 3,488 Lights and exact HDRP companions plus
1,935 controller records in 16 primary scenes, preserving all source fingerprints.
3,486 v13 Lights have qualified stored-value selection; two Horns dynamic-scene
v12 records remain blocked for migration. All 3,488 native intensity fields differ
from the obsolete HDRP intensity storage. The capture took 38.750 seconds.
Eighteen synthetic tests cover exact ownership, script identities, the obsolete
field trap, explicit migration blockers, nonfinite views, malformed values,
preservation, resource bounds and cancellation. This proves source binding and
stored-field selection only. Native GPU lighting and game runtime are NOT_RUN.

Rollback removes this additive reader and its private snapshots. It does not
change existing scene/heightmap/native shader packets or source game records.
Actual lighting/instance state, full materials/scenes, four-map UI and game export
still require implementation and their own native/game acceptance.

## Qualified Point-light migration projection

`foa_scene_lighting.project_light_values` produces a separate value projection
for the exact source profile. HDRP v13 records read current native Light values.
For v12 Point lights, the qualified migration transfers the stored reflector,
light-unit and lux-distance fields, preserving native intensity and all unrelated
values. Its result records source/projected versions and changed fields. It never
mutates or rewrites the source record or consumes obsolete additional-light intensity.
Other v12 shapes and unqualified versions are rejected; Pyramid's area-size behavior
has not been qualified. Nonfinite, lossy, incomplete or invalid migration inputs fail.

`capture_scene_lights(..., include_projection=True)` explicitly selects private
snapshot version 2. Each light gains `value_projection` and `projected_values_status`;
the packet adds aggregate `projected_values_status`. The default is still version 1,
with the same keys and behavior. A successfully projected v12 record keeps its
original `stored_values_status=BLOCKED` and `migration_required=12`: these describe
the immutable source, while the separate projection describes derived values.
Unqualified migrations retain their original records with a blocked projection.
Neither snapshot version promotes stored values into GPU or runtime acceptance.

The reproducible `Tools/editor_tests/FoaLightMigrationProbe.cs` runs in an isolated
Unity 6000.0.64f1 project with no packages. It requires explicit `FOA_LIGHT_MANAGED`,
`FOA_LIGHT_INPUT` and unused `FOA_LIGHT_OUTPUT` paths. The installed HDRP assembly is
fingerprinted before loading; observed assembly dependencies and fixture input are
rechecked afterward. Inputs are bounded Point-light fixtures: `cases` contains
`name`, `nativeJson` (a `Light` object containing the twelve native value fields),
`unit`, `reflector`, `distance` and deliberately separate `obsoleteIntensity`.
Use `-executeMethod FoaLightMigrationProbe.Run` in the qualified Unity host. Keep
source-derived input, output and loaded-assembly diagnostics outside source control.
Native properties are assigned explicitly; deserializing a native Light JSON object
without its serialization version can apply an unintended native migration.

The accepted original-assembly test covers 600 synthetic combinations and both
captured v12 lights. Every native value matches the independent projection at
float32 precision; unrelated native and companion fields remain unchanged. The
four-map v2 capture contains 3,488 projected lights and exactly two migrations.
This is Unity host migration evidence, not Fall of Avalon player execution, GPU
lighting acceptance, a completed map importer interface or game-return proof.

## Original directional-light GPU records

`foa_scene_directional_lights.py` encodes complete, explicitly supplied directional
records for the independently fingerprinted source profile. All 26 fields and
176 bytes are required. Integer signedness, vector components, float32 range,
record ordering and the structured-buffer declaration are checked; missing or
unknown fields fail. The codec does not calculate light properties, choose visible
lights or fill defaults. Its 1,024-record bound is an adapter bound, not a claim
about the game's renderer capacity. Existing draw packet version 2 is unchanged.

The isolated `Tools/editor_tests/FoaDirectionalGpuProbe.cs` executes the original
HDRP light builder with actual Unity culling results. Use the qualified Unity host
with `-batchmode -executeMethod FoaDirectionalGpuProbe.Run`; graphics are required.
Copy it and `FoaLightMigrationProbe.cs` into an isolated project's Editor folder.
Set `FOA_LIGHT_MANAGED`, `FOA_DIRECTIONAL_INPUT` and an unused
`FOA_DIRECTIONAL_OUTPUT` outside source control and the game installation. The
input contains at most 256 named cases, each with native Light JSON, the exact
thirty HD fields, up to 256 ancestor TRS nodes and sixteen independently qualified
source-world float bits. The input is bounded to 16 MiB. Original assemblies and
input fingerprints are rechecked; outputs stay private.

The fixture tests each light independently at an explicitly synthetic camera and
render configuration. Source hierarchy geometry and stored properties remain
separate from controller/scene activation. A culled-light matrix must not be
substituted with its Transform matrix: the captured Unity run produced different
float bits for six lights while all nineteen Transform matrices matched their
independently qualified hierarchies. The actual `VisibleLight` values go directly
to the original HDRP builder. The private comparison additionally verifies every
assigned local TRS value and native Light property against the source fixture.

All nineteen captured directional lights across the four campaign roots pass
original-assembly preparation and independent byte comparison: 3,344 bytes and
all 26 field offsets. `source_directional_light_gpu.py` builds a native acceptance
fixture against these independent original bytes, with one cell per word and
light, plus deliberate all-word and single-word corruptions.
`source_directional_light_editor.py` captures those cases in the existing O3DE
viewport; pixel comparison is a separate required acceptance step. The recorded
DX12 run passes all 2,508 cells and 7,027,236 interior pixel comparisons across
the original and two corrupted cases, plus draw-removal checks. This proves the
initial record bytes reach the native GPU buffer unchanged.

These are the builder's initial directional records. Later cookie, atmosphere,
contact-shadow and shadow-index processing is not qualified by this fixture.
Live controller state, camera policy, other light shapes, final scene lighting,
instance state, remaining material passes, complete scenes, four-map UI and game
export remain unfinished. Neither isolated light preparation nor buffer transfer
establishes full-map appearance or Fall of Avalon runtime acceptance.

## Exact merged-instance matrix candidates

`foa_scene_merged_candidate.matrix_candidate` prepares a separate complete-member
candidate in memory from the original `MergedRenderers` bytes, a validated original
placement packet and explicitly selected native edits. Each edit carries the full
immutable placement identity and sixteen `host_world_bits` from
`SourceScenePlacementBus.GetWorldMatrixBits`. Displayed or decomposed native TRS
values cannot replace that matrix. The caller must independently establish the
archive/map/loader association; matching hashes do not establish authorization.

The conversion reverses the qualified source/host axis mapping at uint32 precision
and restores the twelve original column-major matrix words. Only those 48 bytes
per selected record may change. Both following reference indices, unselected
instances, section framing and uninterpreted member bytes are preserved. Original
bytes are reparsed before use so mutable cached offsets are not write authority.
The result is reparsed and every selected matrix is expanded and compared with the
native input bits. Invalid/stale identities, unsupported bottom-row bits,
nonfinite values, duplicate selections and oversized packets fail before return.
The existing 64 MiB member, 16 MiB packet and 4,096-placement limits still apply.
Cancellation is checked throughout preparation and verification; source objects,
packets and original payloads remain unchanged. Existing packet schemas are unchanged.

Ten synthetic tests cover complete-member preservation, exact no-op conversion,
movement/rotation/scale/shear, signed zero and subnormals, independent column
expectations, selection ordering, cancellation, stale identities and invalid input.
The independent private record check covers all 87,229 previously captured
instances across eight members and all four maps: every no-op restores all 56
original bytes, and a controlled bit edit per instance changes only its selected
matrix word. All twelve stored word positions are covered. This took 8.011 seconds.
That check qualifies record conversion; it does not qualify full campaign-member
output, rendered appearance or game execution.

The reproducible `Tools/editor_tests/source_merged_candidate_editor.py` exercises
native selection, movement, undo/redo, rotation/scale, candidate conversion and
saved reopening using a private synthetic fixture. Set `FOA_MERGED_CANDIDATE_ROOT`
to an isolated directory containing `member.bin`, `placement.json` and
`fixture.json` with `project`, `level`, `member_guid`, `member_sha256` and
`placement_sha256`. Run through the pinned Editor's `--runpython` entry point.
A fresh second process with `FOA_MERGED_CANDIDATE_REOPEN=1` checks the saved level.
Outputs must be unused private paths; keep fixtures and diagnostics outside Git.

Every result explicitly reports `game_export=BLOCKED` and
`dependent_products=NOT_RUN`. Moving an instance can invalidate navigation,
lighting, LOD bounds, occlusion and other derived products. Byte-preserving matrix
conversion supplies a candidate for subsequent qualification, not permission or
proof that a changed member is safe to return to the game. No archive publisher,
game write, deployment action or user-facing export command is introduced.

Native synthetic acceptance passed movement, undo/redo, rotation/scale and a fresh
saved-level reopen. An independent framing/column audit matched all six complete
member candidates byte-for-byte; unchanged and undo candidates equal the original.
The saved edit reproduces the identical candidate in the fresh Editor process.
The maximum 4,096-edit batch against 87,229 synthetic member records completed in
0.474 seconds and preserved the other 83,133 records. These checks do not establish
full-map rendering, derived-product validity or game return.

## Explicit DOTS instance streams

The qualified Unity 6000.0.64f1 renderer stores per-instance properties in separate
GPU streams. `foa_scene_dots_buffers.py` accepts exact ordered property bytes,
strides and instance counts for the fingerprinted Entities Graphics/Awaken ECS
profile. The 64-byte shared zero allocation, 16-byte stream alignment and metadata
upper-bit/address split follow the inspected original allocator and metadata writer.
Property order is supplied explicitly; runtime property IDs and batch order are
not generated from names. Existing native shader/draw packet schemas are unchanged.

`pack_matrix_columns` converts explicit source-space row-major matrix words into
four float3 columns with integer permutation. It preserves signed zero/subnormals
and rejects nonfinite or unrepresentable affine matrices. It does not calculate
world-to-object matrices: the original GPU uploader computes inverses separately,
and that execution has not been qualified. Host placement matrices must pass
through their owning source-coordinate conversion first.

`visibility_buffers` encodes explicitly supplied visible-index words for the
inspected shader's direct or indirect branch. The direct constant array uses
16-byte entries; the indirect branch reads a raw uint stream. Optional upper-byte
stripping is explicit and preserves the original words. This adapter does not
choose visible objects, interpret LOD ownership or execute culling. Unsupported
counts, absent indices, incorrect profiles, duplicate/incomplete streams and
non-boolean modes fail. Bounds are 4,096 instances, 256 property streams and
16 MiB of instance data; direct visibility is bounded to 256 entries.

Nine focused tests check independent bytes, offsets, alignment, ordering,
visibility modes, rejection and size bounds. All 87,229 previously captured
instance transforms across the four maps reproduce their original 48 matrix bytes
in the packed streams, using independently captured source-world matrices as
input. This complete transform check took 4.054 seconds.

`Tools/editor_tests/source_dots_shader_cases.py` supplies a strictly synthetic
render configuration to the exact inspected original DOTS Unlit vertex/fragment
programs, requiring their fingerprints. It explicitly sets camera/exposure values,
white textures, object transforms, visible indices and per-instance color/emission.
The original programs remain unchanged. `source_dots_shader_editor.py` runs the
nine-case fixture through the existing native viewport, with a cleared-frame
control. Inputs and captured frames stay private. Pixel and placement comparison
is a separate required check; shader asset creation or READY status is insufficient.

This is a single-instance draw fixture. Multi-instance draw dispatch, live culling,
GPU inverse generation, light-probe values, previous-frame state, actual controller
values and all other material variants remain separate obligations. Final lighting,
complete rendered scenes, four-map UI and game export are not qualified by this
adapter. The inspected DLLs and existing private source captures remain read-only.

The slot count is explicit GPU capacity. The original renderer reserves each ECS
chunk's Capacity while uploading only its active Count; those gaps and the visible
slot mapping must be supplied by the caller. Source ordinals are not GPU indices.
An additional negative test checks sparse active slots and rejects a compacted count.

Native DX12 pixel/placement acceptance passed all nine original-program cases:
1,352,325 interior pixels match their exact expected colors, and 7,974 outside
samples agree with the cleared frame. Direct/indirect visibility, upper-byte
stripping, shared-transform addressing, per-instance color/emission and material
fallback are exercised. Both original programs remain byte-identical in the
processed native shader asset. This validates their explicit synthetic inputs,
not actual scene visibility or full-map appearance. The inspected DLL hashes
remain unchanged; no game files or saves were modified.


## Native multi-instance draw submission

The private native draw packet v3 retains every v2 field and requires an explicit
`instance_count` integer from 1 through 4,096. The native instance offset is zero.
The caller owns the matching visibility and property streams; the renderer does
not infer their source ordering or treat source ordinals as GPU slots. The pinned
RHI draw packet receives this exact count through `SetDrawInstanceArguments`.

Existing draw v1/v2 packets retain one instance and reject the new field. Entity
binding wrappers v1/v2 and grouped wrapper v3 are unchanged; their nested draw
may use v3. Persisted binding JSON retains the count. Earlier native binaries
reject v3 draws; retain the earlier level and binary when rolling back. There is
no implicit upgrade or lossy downgrade.

Each draw is bounded to 2,097,152 index-instance invocations, preserving the prior
8 MiB index-stream ceiling as a work bound when instancing multiplies geometry.
Invalid counts, unsupported versions and excessive work fail before resource
admission. Existing per-draw, total resident-memory and scheduling bounds remain.
Direct/indirect visibility, sparse GPU slots, saved bindings and original-shader
pixels require native acceptance. Submission does not qualify game culling,
controller/probe state, complete scene appearance or game return.


`Tools/editor_tests/source_instance_dispatch_cases.py` and
`source_instance_dispatch_editor.py` qualify the new draw field using the exact
original DOTS Unlit programs and synthetic inputs. The native run covers nineteen
descriptors: nine prior single-instance cases, eight multi-instance cases including
capacity gaps, direct 256 and indirect 4,096 instances, and conventional v1/v3
count-one equivalence. Sixteen malformed/version/count/workload inputs reject
without changing the admitted draw. At 4,096 instances, 510 indices are admitted
and 513 indices exceed the work bound and reject.

The test clears entity selection before capture so editor transform gizmos do not
contaminate scene comparisons. The original attempt exposed that fixture error;
corrected fresh captures pass. Every expected instance interior and all pixels
outside the bounded rasterization margins are checked. A reopened editor may use
a different viewport resolution: its expected clip geometry/colors and cleared
frame are tested at that native resolution without resampling. Initial and fresh
runs pass 4,676,922 interior pixels and 37,073,875 outside pixels; eleven
same-resolution full-frame pairs pass 25,915,032 pixels. The saved binding hash
is identical, and hide/show, deletion undo/redo, restoration and full release pass.

The synthetic persistent entity uses an unused global-constant matrix destination
to satisfy the existing binding envelope. Its per-instance transforms remain
caller-owned buffer data. This persistence test does not claim that moving the
entity updates those instance matrices; individual instance editing and actual
runtime visibility remain separate obligations. Full maps, final lighting and
materials, the four-map interface and game export are still unfinished.


### Non-directional light selection and GPU records

The private LightData codec (`foa_scene_light_data.py`) accepts complete records
for the qualified Unity/HDRP profile. Its 224-byte ABI contains 34 fields; all
vectors, scalar types, integer ranges, padding and unused fields are explicit.
Missing fields, unknown fields, unqualified profiles and mismatched structured
buffer declarations are rejected. The 4,096-record limit bounds one packed
payload to 917,504 bytes. Finite values use floats; non-finite values require a
`$foa_float32_bits` tag containing four little-endian bytes as lowercase hex.
This preserves original GPU NaN payloads without silently clamping them.

`editor_tests/FoaVisibleGpuProbe.cs`, alongside `FoaLightMigrationProbe.cs`, runs
in the isolated, package-free pinned Unity fixture. `FOA_LIGHT_MANAGED` selects
the already authorized original assembly directory. `FOA_VISIBLE_INPUT` and
`FOA_VISIBLE_OUTPUT` must identify private files outside Git and the game install.
The fixture verifies the HDRP fingerprint and calls the original visible-light
processing and initial GPU-record methods by reflection. Native culling receives
the stored hierarchy, active state, masks and baking inputs. Bounding-sphere
values use explicit float bits, avoiding Unity JSON's loss of tagged NaNs.
Getter readback verifies the values; source raw Light records remain separately
preserved. Decoded source NaNs do not establish their original payload bits.

The qualification uses an explicit camera per light and explicit global settings;
it includes no scene occluders or live controller execution. Across 3,469 source
non-directional lights, 3,446 produced initial records, 22 were absent from the
native cull and one was filtered. Two separate controls verify inactive hierarchy
and zero-dimmer rejection. All 771,904 produced bytes matched the codec, including
one non-finite original GPU field retained with its exact bits. A failed initial
run that converted source bounding-sphere NaNs to zero remains in private evidence;
only the corrected run supports the source-state result.

`source_light_data_gpu.py` constructs independent original and codec structured
buffers. `source_light_data_editor.py` submits them through the native DX12 route
using `FOA_LIGHT_DATA_DRAW_ROOT` for private fixture/capture storage. Three cases
check unchanged data, corruption of every word, and corruption of one word per
light. Native comparisons passed for 578,928 words and 579,264 cleared-draw cell
checks. The fixture tests transfer of the records, not light shading equivalence.

Full game visibility, final cookies and shadow atlases, light-volume construction,
remaining source shader passes, complete scenes, four-map UI and game export
remain unfinished. These receipts do not qualify full maps for 1:1 testing.


### Editable raw instance matrices

Private entity binding v4 adds `buffer_matrices` alongside `draw`, `matrices` and
`vectors`. Each destination explicitly names `stage`, `slot`, `offset`,
`layout: "float3_columns"` and `value: "source_object_to_world"`. The native
renderer converts the source placement to four float3 columns and updates only
that 48-byte range in the raw buffer. The initial serialized binding remains
immutable; the saved placement supplies the current transform when reopened.

The destination must resolve to a raw read-only buffer (type 4, stride 4), fit in
its payload, start at a 16-byte aligned offset and overlap no other destination.
There are at most eight destinations, and v4 requires exactly one rendered
instance per draw. A sparse GPU index is explicitly caller-qualified; it is not
inferred from a source renderer ordinal. Shared batches with several placement
owners and inverse-matrix synthesis remain unsupported. Affine source matrices
are required. Invalid descriptors reject before resident resources are admitted.

Stages with mutable instance matrices are never shared across draws. Identical
bindings on two entities therefore retain independent GPU storage. Geometry can
still share the existing cache. Matrix uploads remain inside the existing four
updates/64 visits per tick and 64 MiB resident-payload limits; each dirty instance
destination uploads only 48 bytes. No JSON parsing or buffer allocation is added
to the transform-update path.

Existing single-binding v1/v2 formats and v3 groups retain their meaning. A v3
group can contain v4 members; older native readers reject the new leaf version.
The outer render-assembly format remains v1. Python assembly compatibility and
malformed-shape tests cover direct and grouped v4 bindings. Native validation
covers stage/slot/type/range/alignment, empty/excess/overlapping destinations,
unknown or duplicate fields, versions, multi-owner counts and immutable rebinding.

`editor_tests/source_instance_edit_editor.py` uses `FOA_INSTANCE_EDIT_ROOT` for a
private fixture outside Git and `FOA_INSTANCE_EDIT_REOPEN=1` for a fresh-process
saved-level check. The qualified original DOTS vertex/fragment programs remain
unchanged. Two entities start with identical bindings to sparse slot 4 and have
separate scale/reflection/shear placements. Independent reference geometry is
transformed on the CPU, with the selected reference GPU matrix reset to identity.
Move, scale, parent rotation, transform undo/redo, hide/show, delete/restore and
fresh reopen produced 13 full-frame reference pairs with zero differing pixels
across 69,705,792 pixels. Saved source identities, bindings and transforms matched.
Twenty-one invalid/immutable attempts left residency unchanged; eight draws hit
the four-update bound, and final resource counts returned to zero.

This is original-shader editing proof using synthetic geometry and explicit
instance input. It does not qualify actual campaign slot ownership, shared-batch
editing, complete scenes, remaining materials, lighting, four-map UI or export.


### Exact saved material uploads

`foa_scene_material_upload.py` joins measured material-property bytes to the
original constant-buffer layouts. Saved Color values, the public `Color.linear`
result and actual GPU uploads differed by several float bits in the pinned host.
The importer therefore consumes direct upload bytes instead of applying an
approximate gamma formula. Numeric shader defaults remain a separate unsupported
origin; they are not silently treated as saved material values.

The private request/receipt formats are version 1, profile
`unity-6000.0.64f1-d3d11`, with explicit `Linear` or `Gamma` color space. Requests
contain source material keys, shader/property fingerprints, declared types/flags
and exact little-endian float inputs. The receipt must match the canonical request
bytes and preserve every material/property identity and count. Color-space or
source edits invalidate reuse. These hashes bind local measurement inputs; they
are not authenticity certificates or proof of game rendering.

The workflow is an offline private conversion helper:

1. Call `upload_request([(source_key, binding), ...], color_space="Linear")` on
   qualified source-material bindings, then write `canonical(request)` as bytes
   to a private JSON file outside Git.
2. Build `editor_tests/source_material_binding_probe.cpp` as the isolated Unity
   D3D11 plugin `FoaMaterialBindingProbe`; copy it and
   `editor_tests/FoaMaterialUploadProbe.cs` into a private Unity fixture. Configure
   the exact pinned host and requested color space. Run with `-force-d3d11`,
   `-force-gfx-direct`, and `-executeMethod FoaMaterialUploadProbe.Run`.
   `FOA_MATERIAL_UPLOAD_INPUT` names the request and `FOA_MATERIAL_UPLOAD_OUTPUT`
   names an existing private output directory without `uploads.json`.
3. Validate `uploads.json` with `read_uploads(request, receipt)`. Pass the selected
   immutable upload to `pack_material_constants(layout, source_key, binding,
   upload, color_space="Linear", explicit_values=...)`.

The fixture copies declared property types/flags into generated measurement
shaders and sets the exact saved values. It compares shader-visible constant bytes
with float render-target output for every property. It does not load original
material objects or execute original game shader passes. The downstream packer
uses the original uniform names, offsets and widths, zeroing only padding. Source
texture ST uses explicit scale/offset. Unresolved texture uniforms, renderer
properties and global values must be supplied by their owning producer; explicit
values cannot silently replace measured material properties.

Each request is bounded to 1,024 materials, 4,096 properties per material,
262,144 aggregate properties and 64 MiB serialized input. Each native buffer read
is at most 64 KiB. The reader honors D3D11.1 shader-visible ranges, including
nonzero offsets, and leaves bindings and rejected output buffers unchanged. Native
standalone tests compile with `FOA_MATERIAL_BINDING_SELF_TEST`; the WARP device
checks both stages, nonzero subranges, exact copied bytes, unchanged tails and
rejections. The conversion helper is offline and adds no work to Editor ticks.

Both Linear and Gamma runs passed for all 875 captured materials and their 91,842
saved numeric properties using 34 generated declaration layouts. A total of
973,176 property bytes matched shader output. Linear mode changed 479 saved Color
properties; Gamma mode retained every input byte. Separate controls compare
shader defaults, setters, cross-setters and saved reload, including Gamma/HDR
flags, with 90 invalid-read rejections. A mismatched host color space rejects
before drawing and produces no upload receipt. Error reporting only writes to an
accepted private output folder and never overwrites an existing diagnostic.
Six isolated Unity rejection cases passed for the two probes: Git-parent output,
existing receipt and existing diagnostic. Rejected outputs remained unchanged;
diagnostic-write failure still exited with an error.

Source layout validation packed 24,620 material/layout combinations and checked
361,626 uniform fields (13,944,144 bytes) against measured values and original
byte offsets. The packing run took 19.25 seconds in the recorded local profile.
Another 970 combinations still require explicitly qualified `_HeightMap_TexelSize`
values; no substitute is supplied. Original shader-pass rendering, full campaign
lighting/visibility, complete scene integration, four-map UI and game export
remain unfinished. These material-upload measurements do not qualify full maps
for 1:1 testing. Game files and saves remain unchanged.
