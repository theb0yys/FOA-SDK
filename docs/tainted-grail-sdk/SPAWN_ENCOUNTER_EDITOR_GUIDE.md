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
Closing a dirty docked or floating pane offers **Save / Discard / Cancel**. **Save** validates and
saves the encounter name, fields and complete composition before closing. If validation or writing
fails, the pane stays open with its raw fields and staged additions, quantity edits and removals intact;
correct the problem and try again. **Discard** closes without saving, so reopening shows the last saved
encounter. **Cancel**, Escape and dismissing the prompt keep the pane open. Cancel is the default.
A clean pane closes without a prompt or another save.

With unsaved encounter changes, opening a workspace through **SDK Status** or **Catalog Browser**,
including reloading the current workspace, offers the same **Save / Discard / Cancel** choices. Save writes to the original
workspace before switching; a failed Save or Cancel keeps the current workspace and raw draft intact.
Discard takes effect only when the switch succeeds. If another pane cancels or the destination reload
fails, the encounter draft remains available. An already completed Save remains saved if another pane
later cancels the switch.

After a successful switch or reload, the old encounter form clears. Select an encounter in the loaded
workspace to continue. Matching workspace or record IDs in a different folder do not carry drafts
across; saving is bound to the original canonical workspace document and root.

The preview identifies missing actor/troop references, invalid quantity ranges, unique-actor conflicts
and population limits.
Warnings about unassigned placement or missing cleanup notes permit saving an unfinished plan.

## Workspace-switch acceptance

The native runner uses disposable synthetic workspaces on a private inactive Windows desktop, the
pinned built Editor and an already prepared asset cache. Use fresh output directories outside product
and engine sources for each run:

```powershell
& Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_encounter_close_smoke.ps1 `
  -Suite workspace-status -EditorExecutable '<external-build>/bin/profile/Editor.exe' `
  -EngineRoot '<pinned-engine>' -CacheRoot '<prepared-cache>' -OutputRoot '<fresh-status-output>'
& Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_encounter_close_smoke.ps1 `
  -Suite workspace-catalog -EditorExecutable '<external-build>/bin/profile/Editor.exe' `
  -EngineRoot '<pinned-engine>' -CacheRoot '<prepared-cache>' -OutputRoot '<fresh-catalog-output>'
```

Each route checks docked and floating panes: Save/Discard/Cancel and dismissal; raw fields and staged
actor/troop composition; malformed destinations; validation and real write-lock failures with retry;
same-root and alias reload freshness; matching IDs in another root; and a later Pack Manager veto.
Discard also retains drafts through a failed post-admission candidate reload. The fixture compares
saved catalog bytes and untouched records, requires the pane to remain open, checks form reset after
commit, enforces a five-second automated transition budget, and requires the expected loaded SDK DLL
hash and normal Editor exit. `-Suite close` runs the independent pane-close regression.

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
