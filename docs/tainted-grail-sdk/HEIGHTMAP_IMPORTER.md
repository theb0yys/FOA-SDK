# Heightmap importer and native map editing

Open **FOA-SDK Home > Map editor**. This opens the heightmap import pane together with
O3DE's Asset Browser, Entity Outliner and Entity Inspector, leaving the main 3D viewport
available for editing. Road Atlas planning records are under **Advanced tools > Road
Atlas data**.

## Campaign map support

The full campaign scene task, including editable objects, navigation, lighting and game
return, is tracked in [Campaign scene editing](CAMPAIGN_SCENE_EDITING.md).

Campaign import and return to the game are unavailable. The previous collider-based
heightmap reconstruction did not preserve the actual map and filled missing cells with
estimated heights. That route is disabled. Existing campaign revisions are retained for
read-only diagnosis, labeled unsupported, and rejected by **Open in Editor**. Their RAW
exports are also rejected by the local import action when the campaign receipt is present.
Already-created O3DE level files are retained; opening them directly does not make them
faithful game maps or establish a game return path.

## Open a local heightmap

1. Choose **Import New Map** for a local heightmap and its explicit metadata sidecar.
2. Select the imported map and choose **Open in Editor**.
3. On first use of a workspace, restart Asset Processor when the importer requests it,
   then choose **Open in Editor** again. The Editor keeps the imported copy for retry.
4. The map opens as an ordinary O3DE level. Normal unsaved-level handling applies when
   switching between maps.

## Edit in the existing Editor

Select **Paint Terrain Heights** in Entity Outliner. In its **Image Gradient** component,
choose **Paint** to enter O3DE's native terrain-height painting mode. Use the native brush
settings and undo controls. Finish painting and save its image when prompted, then save
the level with **Ctrl+S**.

Place processed models or prefabs from O3DE's Asset Browser in the main viewport. Select
entities to move, rotate, scale, duplicate or delete them with O3DE's normal tools. Mesh
and prefab processing, their supported formats and their editing controls belong to O3DE.
The SDK does not introduce a second viewport, object editor or brush implementation.

A saved map retains native entity placements and component settings. Reopening the
imported revision opens that saved level and preserves the editable height image. The
original imported revision remains available as the source of the editable copy.

## Local heightmaps

Supported input is RAW/U16/R16, 16-bit grayscale PNG, or bounded uncompressed unsigned
16-bit grayscale TIFF. Keep a metadata sidecar beside the image, with the same filename
plus `.json`. It supplies dimensions, metre spacing, height range, coordinate orientation
and sample semantics. RAW also requires explicit byte order. The importer does not infer
these from file size or appearance.

Imports validate their source and tile hashes before publication, run outside the UI
thread, and can be cancelled. Revision lists are bounded to 4096 inspected entries and
64 displayed revisions. The native projection accepts up to 32 million samples. A
256-square grayscale overview is an import preview; editing happens in the main viewport.

## Source preservation requirement

The campaign provider entry point rejects requests before reading game content or
writing exports. Retained collider research helpers reject any uncovered heightmap cell;
nearest-sample filling has been removed. Even full coverage would only describe a sampled
upper surface, not prove original geometry, holes, overhangs, scene identities, materials
or object placement are preserved. Coverage is not an accuracy measurement.

The intended workflow requires an evidenced original source representation and an export
contract the game actually consumes. Before claiming a round trip, verify unchanged
import/export preservation, one controlled edit with unrelated data preserved, and an
explicitly authorized load in the target game profile. This work has not established that
return path. O3DE editing success cannot stand in for those checks.

## Local ownership and persistence

- `SourceExports/Terrain`: extracted heightmap, coverage mask, receipt and overview.
- `Derived/Terrain`: validated immutable imported documents and tiles.
- `EditorAssets/foa_maps`: editable height image and ordinary O3DE level.
- `Staging/TerrainCampaign` and `Staging/TerrainNative`: private worker coordination.

Editable terrain workspaces must be outside source control. Only an external scan-root
setting is written to the O3DE project's ignored `user/Registry` directory. Native level
and image files stay in the SDK workspace and are excluded from release packages.

This workflow does not modify the game installation or saves, deploy a map to Fall of
Avalon, or establish game-runtime compatibility. The former duplicate editor prototype
is superseded and has no supported scene-format migration.

## Validation status

Native integration validation remains partial; game round-trip validation is NOT_RUN. Configure and compilation are distinct
from actual terrain sampling, painting, object manipulation and saved-level reopen proof.
Do not use earlier custom-viewport test results as evidence for the native workflow.
