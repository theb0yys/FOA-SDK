# Quest inspection and local authoring

Open **Quests and state** from the Home pane. Choose a saved quest or **Load quest document** to inspect a local `*.tgquest.json` file. Search filters the saved quest list. Imported documents show parsing errors and retain their original file; **Save editable copy to mod** creates a separate pack-owned quest.

## Create and edit
Choose or create a compatible mod, then select **New quest** and enter a name. The initial quest has Start and Complete phases, a Finish transition, one objective and an optional participant role with a binding requirement. Edit the name, summary, content version and declared lifecycle in **Definition**.

Use **Quest elements** to choose Phases, Objectives, Transitions, Conditions, Actions, Outcomes, Roles, Binding requirements, State keys or Catalog links. Select **Add**, **Edit selected** or **Remove selected**. Phase/objective/transition relationships use exact identities behind readable choices. Condition/action lists use checkboxes. Removing a referenced element leaves a visible validation error until the remaining references are repaired.

State keys describe authored boolean, integer (±1,000,000,000) or text defaults. They are not live game variables. Use fact conditions/actions with boolean keys, counters with integer keys and decisions with text keys. **Catalog links** associate logical subjects with existing actors, items, locations or quests. A location condition needs a world location; an actor-role condition needs an actor. Unavailable native quest intake is not inferred from a local JSON document.

## Inspect and preview
**Progression graph** shows phases and directed transitions, including explicitly allowed repeats. Entry and terminal nodes include text labels as well as different colours. Use **Fit graph** to restore the overview. This diagram does not run the quest. **Validation** lists invalid definitions, broken references and unresolved external requirements.

## Save and recover
**Save quest** commits the complete validated draft to the workspace catalog. A failed save keeps the draft and the previously published quest. Revert requires confirmation when changes would be lost. Saving after another editor updates the same quest reports a conflict; revert and reopen the newer definition.

Switching workspace, profile or active mod preserves a dirty draft but prevents saving it into the wrong context. Return to its original workspace/mod to save, or revert. Imported drafts are saved only as new definitions in the selected active mod.

QuestDefinition V1 files and QuestBindingManifest V1 retain their original contract. Catalog schema 6 stores authoring profiles, labels, state declarations and local bindings. Schemas 1–5 load with validation; the first schema-6 save preserves and verifies an exact pre-migration backup. An older Editor requires that backup for downgrade. Keep pack manifests and source/evidence documents with the catalog.

Local authoring does not execute quests, resolve runtime bindings, modify game state, edit saves or deploy content. Conditions and actions retain their existing inert V1 vocabulary.
