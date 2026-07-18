# Developer Preview Duplicate Review Companion

These project-owned synthetic files prepare the **Tainted Grail Economy Cross-Pack Duplicates** pane for the real Windows manual UI pass without changing the canonical generated persistence fixture.

## Prepare the generated workspace

1. Generate and verify Developer Preview 0 normally.
2. Copy `preview.duplicate-companion.tgpack.json` into the generated fixture's `Packs/` directory.
3. Open the generated workspace so both `preview.developer-preview-0` and `preview.duplicate-companion` are loaded.
4. Import `preview-duplicate-source.json` through **Tainted Grail Source Intake** with the structured JSON importer.
5. Promote `preview.evidence.duplicate.primary` as:
   - record ID `preview.item.duplicate.primary`;
   - owner pack `preview.developer-preview-0`;
   - domain `economy`;
   - kind `item`;
   - subject `subject:preview:item:duplicate-review`;
   - identity `synthetic`.
6. Promote `preview.evidence.duplicate.companion` with the same domain, kind, subject, and identity, but use:
   - record ID `preview.item.duplicate.companion`;
   - owner pack `preview.duplicate-companion`.

The report should show one exact `subject_ref` group across two owner packs. Both candidates should be `partial` because the promoted records deliberately have no typed item profiles yet.

## Boundary

The companion creates only project-owned synthetic authoring data. It performs no automatic merge, pack rejection, winner selection, governance decision, permission grant, runtime action, deployment, launch, injection, telemetry, or save mutation.
