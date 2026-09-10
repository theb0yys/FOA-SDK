# World and Route Editor

Open **World and routes** from the SDK Home, then choose or create an authoring mod.

1. Choose **New world definition**, select **region**, and give it a name.
2. Create a **scene** under that region, then **locations** under the scene.
3. Select a location to edit its description or enable **Use a location plan position**. Enter X/Z in scene-local plan units and save.
4. Create a **road** or **route** for the scene. A route can reference a saved road in the same scene. Describe travel constraints in the Definition tab.
5. In **Nodes and edges**, add saved locations as nodes. Choose two different nodes, a travel mode, direction and planning cost, then add a connection.
6. Open **Visual preview** to see the graph. Save the definition and reopen it from the list to continue editing.

The preview uses saved plan positions when every node has one. Otherwise it shows a labelled schematic topology. Planning cost is an authored weight, not elapsed travel time. Names are labels; lists retain exact record identities and show a short ID suffix to distinguish equal names.

Select a node or edge row to update it or remove it. Remove a node's incident edges before removing that node. Duplicate locations, duplicate connection directions, self-edges and cross-scene locations are rejected. Disconnected paths can be saved as drafts and show a review note. Saving replaces only the selected path's nodes and edges. Empty roads and routes are valid drafts.

Save or revert a draft before choosing another definition. Closing an unsaved draft asks whether to discard it. Failed saves keep the draft and the published catalog. To edit a definition after reopening the workspace, choose its owning mod again in the Pack Manager. Other mods' definitions can be referenced; their contents cannot be changed from the active mod.

Saved regions, scenes and locations appear in faction jurisdiction and encounter placement lists. Those associations describe local authoring intent. **Road Atlas** opens the existing independent map evidence pane; its snapshot schema and evidence review process are unchanged. This editor does not read a native map, execute travel, spawn actors, deploy a mod or edit a game/save.

Catalog schema 5 stores WorldPlaces, WorldPaths, WorldPathNodes and WorldPathEdges. Schemas 1–4 remain supported as validated load inputs. Before the first successful replacement of an older catalog, the writer preserves and verifies a byte-identical sibling backup. A backup or write failure blocks publication. Earlier Editors cannot read schema 5; preserve the workspace and restore its earlier catalog backup to return to an earlier version. Keep pack manifests and source/evidence documents alongside the catalog.

The supported limits are 5000 places, 1000 paths, 10000 nodes and 20000 edges per catalog, with at most 128 nodes and 256 edges per path. Coordinates and positive planning costs are bounded to one million. See [design and acceptance](WORLD_ROUTE_EDITOR_DESIGN.md) and [catalog recovery](CATALOG_GUIDE.md#encounter-plans-and-migration-recovery).
