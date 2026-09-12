# Spawn and Encounter Editor

Open **FOA Development Hub > Spawns and encounters**.

Choose or create a saved mod for the active game profile. **Actors and troops** opens the existing editor
to load supported game actors or create local definitions. Encounters use those saved definitions.

## Build a patrol

1. Select **New encounter**, name it, and choose its first actor or troop.
2. Filter the composition picker for a guard, select the exact result, and press **Add to encounter**.
3. In the composition table, set the captain to minimum/maximum **1** and guards to **3**.
4. The preview should report **4-4 actors per instance**. A troop quantity multiplies its declared
   actor-size range.
5. Assign a saved world location/scene/region when available, or enter a placement reference such as
   **North gate courtyard**. An unbound reference remains visibly unverified.
6. Choose **Manual** activation or **All listed conditions** and enter one condition description per line.
7. Set maximum active instances and a population limit large enough for their combined maximum.
   A unique encounter requires one maximum active instance.
8. Describe cleanup and rollback requirements, then select **Save encounter**.

Changing quantities preserves entry identities. **Remove selected** removes that entry from the draft;
saving replaces this encounter's complete composition. The final entry cannot be removed and saved.
Other encounters, actors and troops remain intact.

## Save and reopen

An encounter is owned by the mod that created it. To edit another mod's encounter, activate its owning
mod first. Save or revert a dirty draft before choosing a different encounter or creating one.
Closing a dirty pane asks whether to discard it. Failed saves preserve the draft and the previously
published catalog. A workspace/profile change disables saving that draft until the original workspace
is restored or the draft is reverted.

After reopening the workspace or pane, select the encounter to continue. The preview identifies
missing actor/troop references, invalid quantity ranges, unique-actor conflicts and population limits.
Warnings about unassigned placement or missing cleanup notes permit saving an unfinished plan.

## Catalog compatibility

Encounters were introduced in catalog schema 3. Current saves use schema 7, which also supports world, society and quest
authoring. Schema-1/2/3/4/5/6 catalogs load through bound validation. Before overwriting an older catalog, the
writer saves and verifies the exact original bytes in
a sibling migration backup. Failure to create that backup prevents replacement.

Older Editors that support only schemas 1–6 cannot open the schema-7 catalog. Preserve your workspace before
downgrading, then restore the pre-migration catalog backup. New encounter edits do not have a schema-2
representation. See [catalog recovery](CATALOG_GUIDE.md#encounter-plans-and-migration-recovery).

## Current capability

This is a composition preview and authoring workflow. It does not render or place a 3D scene, spawn
characters, evaluate game conditions, or execute cleanup/rollback notes. Deployment and native
encounter execution require a separately supported game integration.

Schema 7 also carries project images, translations and presentation assignments; see [Asset and Localisation Manager](ASSET_LOCALISATION_MANAGER_GUIDE.md).
