# Faction and Authority Editor

Open **FOA Development Hub > Factions and authority**. Choose or create a saved mod for the active game profile.

## Create the Town Guard

1. Select **New culture**, enter **Northlanders**, then use **Edit culture** to add its description and language.
2. Select **New faction** and enter **Town Guard**. Assign Northlanders in the Culture field and describe the faction and its authority.
3. In **Members and leadership**, select the saved captain, choose **Leader**, and press **Add**.
4. Select the guard actor or a saved patrol troop, choose **Member** or **Officer**, and press **Add**.
5. Select **Save faction**.

One individual actor can be the faction leader. Troops can be members or officers. An actor or troop appears once per faction; assigning it does not rewrite its actor/troop profile.

## Relationships

Create **Bandits**, then return to Town Guard. In **Faction relationships**, choose Bandits, choose **Hostile**, press **Add**, and save.

Relationships are directed: this describes Town Guard's view of Bandits. Bandits' view of Town Guard is a separate declaration. A faction cannot target itself and has one relationship per target. Friendly, neutral and hostile are local planning values.

Select an existing row, change its target, value or notes, and use **Update selected**. **Remove selected** removes the row from the draft. Saving commits the complete edited faction; links belonging to other factions remain intact.

## Authority and jurisdiction

Use **Authority and leadership** to describe responsibilities and command structure. In **Jurisdiction**, choose a saved world location/scene/region if one exists, or leave **Unverified reference only** selected and enter a territory such as **North gate courtyard**.

Choose **Controls**, **Claims** or **Protects**, add notes, press **Add**, and save. Unbound territory remains visibly unverified; a description is not a discovered world identity. One jurisdiction plan is allowed per exact territory reference in each faction.

## Save, reopen and errors

Culture and faction editing require the owning mod to be active. Referencing another mod's definition does not grant permission to edit it. Empty factions may be saved while authoring, with missing-member/leader/jurisdiction notes.

Save or revert a faction draft before changing the selected faction, creating another definition or editing a culture. Failed saves preserve the draft and previous catalog. Closing a dirty pane asks before discarding it. A changed workspace/profile disables saving the original draft until restored or reverted.

Reopen the workspace and select the faction to recover its culture, members, relationships and jurisdiction plans. Before editing again, use **Choose or create mod** and **Open selected** to activate its saved owning mod. Use the tabs and scrollable form at smaller window sizes; Save and Revert remain outside the scrolling body.

## Compatibility

Catalog schema 4 introduced CultureProfiles, FactionProfiles and independently identified FactionLinks. Current saves use schema 5, which also supports world plans. Schema-1/2/3/4 catalogs load through bound validation. Before overwriting an older catalog, the writer preserves and verifies its exact original bytes in a sibling migration backup. A backup failure blocks replacement.

Editors supporting only schemas 1–4 reject the current schema 5. Downgrade requires restoring the pre-migration backup; newer society edits cannot be represented by the older format. Preserve the catalog with its pack manifests and source/evidence files. See [catalog recovery](CATALOG_GUIDE.md#encounter-plans-and-migration-recovery).

## Current capability

This feature creates local faction and culture definitions. It does not discover native game factions, change in-game allegiance or hostility, control territory, deploy content or modify saves. Runtime application requires a separate supported integration.
