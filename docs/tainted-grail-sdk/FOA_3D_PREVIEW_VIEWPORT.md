# FOA 3D Preview Viewport Model and Render

Status: Alpha evidence slice.

This slice consumes bounded Asset Browser UI render evidence plus the matching Asset Browser pane model and emits local-only 3D preview viewport model/render evidence.

Input contract:

```text
foa-asset-browser-pane-ui-render.json
foa-asset-browser-pane-model.json
```

It must not consume raw O3DE conversion files, raw O3DE preview source files, or import-proof documents directly.

Output:

```text
PreviewArtifacts/Viewport3D/<viewport-render-id>/foa-3d-preview-viewport-render.json
PreviewArtifacts/Viewport3D/<viewport-render-id>/viewport/viewport.html
PreviewArtifacts/Viewport3D/<viewport-render-id>/viewport/viewport-data.json
```

Pipeline position:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ local O3DE preview source staging evidence
→ bounded O3DE Asset Processor import proof
→ Asset Browser pane model
→ bounded Asset Browser pane UI render
→ bounded 3D preview viewport model/render path
```

Boundary:

- does not invoke Unity;
- does not run FoA;
- does not invoke O3DE Asset Processor;
- does not mutate an O3DE editor viewport;
- does not create a live O3DE viewport;
- does not create item or recipe bindings;
- does not create typed selectors;
- does not mutate catalogs;
- does not grant runtime permission;
- does not deploy, sign, or publish;
- keeps `FunctionCompleteAllowed=false`.

Viewport entries preserve product asset IDs, `$assetcache/...` product paths, source pane entry IDs, and product evidence references. Product existence is display evidence only; it does not authorize binding into item or recipe tools.

Commands:

```text
python foa_3d_preview_viewport.py render --workspace <workspace.tgworkspace.json> --ui-render <foa-asset-browser-pane-ui-render.json> --pane-model <foa-asset-browser-pane-model.json>
python foa_3d_preview_viewport.py verify --input <foa-3d-preview-viewport-render.json>
python foa_3d_preview_viewport.py fixture --output <fixture-dir>
```

The next documented unit after this is a live O3DE viewport proof or the item/recipe visual selector gate, depending on whether the editor integration is ready to consume this evidence without granting authoring authority.
