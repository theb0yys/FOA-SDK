# Asset and Localisation Manager

Open **Assets and text** from the Development Hub. Choose a saved authoring mod first.

## Images

Select **New image**, name it, choose a PNG or JPEG, and describe its source or creator. Select source rights and enter licence details for licensed work. Review the preview, then **Save entry**.

The Manager copies the image into the workspace's managed media folder. It accepts a single decoded PNG/JPEG up to 8 MiB, 4096 pixels per side and 4,194,304 pixels total. Files from configured game, engine, extraction, source-evidence and save folders are rejected. Your original image is unchanged. Redistribution declarations record your review; they do not establish permission.

To replace an image, select its saved entry, choose the replacement, and save. Existing assignments follow the same stable image identity. Missing or changed saved bytes produce a visible preview error. Choose a valid source to repair a missing file; a conflicting file at a managed content path is preserved and reported rather than overwritten.

## Translations

Select **New translation** and enter a key such as `Watchkeeper.Name`. Keys are case-sensitive and unique within the mod. Add a language and its plain text with **Add language**. Edit or remove variants with the adjacent buttons, then save.

Use lowercase language codes such as `en`, `fr` or `en-gb`. A saved entry needs a translation for its default language. The **Preview language** field displays an exact match when available, otherwise the declared default with a visible fallback notice. Entries support 1–32 languages and 8192 UTF-8 bytes per translation.

## Content assignments

In **Content assignments**, choose an item, actor or quest, select the field, and choose a saved image or translation. Preview it and press **Save assignment**. **Clear assignment** removes the saved value for that slot. Icons apply to items, portraits to actors, and name/description translations to all three types.

Assignments belong to the active mod. They can target native definitions or definitions owned by that mod, and can only reference images/text owned by the same mod. Custom item icons and actor portraits also appear in their existing editors. Translation rendering is available in the Manager; it does not change the Editor's interface language or the running game's text.

## Saving and recovery

Save or revert an entry before switching entries or saving an assignment. Closing an unsaved draft offers Save, Discard or Cancel. A workspace/mod change preserves the draft and requires its original context for saving. Concurrent changes require reloading the latest entry.

Catalog saves use schema 7. Schemas 1–6 load with validation; the first successful upgrade preserves and verifies an exact older-catalog backup. Older Editors need that backup for downgrade; later changes are not backported. Keep the entire workspace, including `Media/Owned`, packs and source/evidence documents.

A failed catalog save preserves the published entry/assignment. An image successfully copied before a later catalog failure may remain unreferenced; it is not automatically deleted. Metadata can reopen when an image is unavailable so it can be repaired.

This slice covers local images, translated text and presentation assignments. Mesh/audio authoring, translation import/export, packaging, deployment and in-game runtime changes are separate capabilities.


## Developer acceptance

Generate a fresh disposable workspace with:

    python Gems/TaintedGrailModdingSDK/Tools/editor_tests/assets_localisation_fixture.py --output <new-private-directory>

Set FOA_SDK_ASSET_WORKSPACE to its preview.tgworkspace.json and FOA_SDK_ASSET_RESULT to a private result JSON path. Launch the built Editor against an isolated project and the pinned engine, passing --runpython with Gems/TaintedGrailModdingSDK/Tools/editor_tests/assets_localisation_live_smoke.py. The script creates original synthetic artwork and uses only the disposable fixture; preserve the result JSON and screenshots locally. It checks live previews, assignments, fallback, missing files, failed catalog save/recovery, reopening and dirty-close cancellation, with a three-second maximum measured UI timer gap. The fixture helper refuses to replace existing output.
