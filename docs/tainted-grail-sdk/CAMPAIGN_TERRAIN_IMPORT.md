# Original campaign terrain import

The Heightmap Importer can prepare the original campaign terrain for the existing
O3DE viewport. Choose **Open campaign terrain**, then Horns of the South, Cuanacht,
Forlorn Swords or Sarras. Local image heightmaps retain their separate import route.

Campaign terrain is mesh geometry. It is not resampled into a heightfield: doing
that would lose vertical surfaces and geometry that cannot be represented by one
height per horizontal sample. The disabled estimated campaign-heightfield route
remains disabled.

## Scope and representation

The qualified source is the Unity 6000.0.64f1 Mono profile and its exact Medusa
renderer manager, mesh references and archived renderer/instance arrays. The
worker verifies the three relevant assembly fingerprints before conversion. It
reads the selected static scene and exact referenced mesh bundles, then checks
all inventoried inputs again before reporting success.

The viewport retains every source LOD0 Medusa draw, including the ground pieces
and surrounding cliffs. It uses a neutral inspection shader. Original game
materials, lighting, distance-based LOD visibility, other scene entities,
individual Medusa instance editing and game export are not provided by this
terrain milestone. Do not promote its acceptance to complete rendered-scene or
round-trip acceptance.

A native authoring group represents one source renderer/submesh draw and a bounded
contiguous slice of that renderer's instance indices. It is not a fabricated
GameObject identity. Original per-instance matrices remain in a read-only GPU
buffer. Native group transforms start at identity; applying the original source
placement to the group again would place the terrain twice.

## Private packet and persistence

`foa.campaign-terrain` version 1 contains exact source mesh positions, triangle
indices, matrix bytes, explicit source mesh/material identities and group
coverage. The worker compares the packet against the original draw table and mesh
records, independently of the packet's own hashes. Missing, duplicated,
substituted or changed draws fail preparation. No gap filling or geometric
tolerance is used.

Source placement binding version 4 is additive to versions 1, 2 and 3. Its identity
contains the archive hash, scene name, manager record hash, renderer ordinal,
draw ordinal, first instance, instance count and instance-array hash. Parent is
null, baseline local/world matrices are identity, and finite source-space bounds
are required. The bounds serve selection and framing; they do not replace source
vertex positions. Existing saved placement formats retain their readers.

Rendering uses existing binding version 2 and instanced draw version 3. A dedicated
64-byte camera constant buffer is shared across these draws. Camera changes update
that buffer once per view change instead of rebuilding each terrain group. Mixed
constant layouts retain their existing update path.

The preparation and native handoff remain under the active private workspace's
`Staging/CampaignTerrain`. Original assets and output packets must stay outside
Git and outside the game installation. The neutral shader and new level are under
`EditorAssets`. Each import creates a separate operation and level; it preserves
existing levels and uses the Editor's normal unsaved-level handling. The native
controller checks the prepared packet fingerprint, shader fingerprint, native
source binding and original placement readback before saving. It waits for the owning prefab path to match the new level and
sets native authoring focus before creating entities; a changed level name alone
is insufficient while asynchronous loading is still in progress.

The source worker is cancellable and has a 900-second deadline, a bounded
process log and a 1536 MiB process memory limit in the Editor host. The packet
limit is 128 MiB, with at most 1024 groups, at most 4096 instances per group and
a bounded per-draw indexed workload. Creation and rollback advance in batches.
Source files and game saves are never written by this route.

## Required proof

The private source comparison must enumerate every source draw and compare:

- Original mesh identity, vertex bytes and triangle indices.
- Exact renderer instance selection/order and packed matrix bytes.
- Complete coverage, including zero missing or extra groups/instances.
- Source inventory fingerprints before and after extraction.

`FoaTerrainMeshProbe.cs` provides independent Unity validation. It reads the exact
mesh subobject from a privately copied original asset bundle, obtains Unity's
existing GPU vertex/index buffers, and writes bounded private readback artifacts.
The built assets may omit the CPU mesh copy; the probe must not assume CPU
MeshData is available or recreate the mesh from decoder output. Selecting the
first mesh in a multi-subobject bundle is not an acceptable comparison.

`source_campaign_terrain_editor.py` exercises all four prepared imports and a
separate process can reopen their saved levels. The connected test requires Asset
Processor to remain available for registering new level sources. The test also
cancels source preparation and partial native creation, rejects malformed
unbound placement identities, and checks camera updates against a synthetic
mixed constant-buffer layout. Native readback and rendered
captures are required in addition to decoding tests. Captures are local evidence;
neutral shading does not prove original material rendering.

The qualified source inventory contains 588 distinct LOD0 meshes. Source draws
are 4,141 for Horns of the South, 4,534 for Cuanacht, 5,975 for Forlorn Swords and
3,527 for Sarras. All 541 typed ground-owner mesh references are present in this
source family. These counts are profile evidence, not hard-coded substitutes for
fresh source discovery and comparison.

## Acceptance scope

The source-data checks qualified all four terrain assemblies against their original
LOD0 draw tables. An independent Unity GPU readback matched all 588 mesh position
and triangle buffers: 1,890,745 vertices and 3,417,103 triangles across the distinct
meshes. Those triangle counts exclude repetition from placed instances.

| Map | Original draw instances | Native authoring groups |
| --- | ---: | ---: |
| Horns of the South | 4,141 | 191 |
| Cuanacht | 4,534 | 277 |
| Forlorn Swords | 5,975 | 208 |
| Sarras | 3,527 | 91 |

The operation-specific private evidence records the source comparison, Editor
bindings, saved-level reopening, captures and connected UI results separately.
It must include unresolved failures instead of promoting source or build checks
to Editor acceptance. This is terrain geometry qualification; complete game
rendering, individual instance editing and game export remain separate work.
