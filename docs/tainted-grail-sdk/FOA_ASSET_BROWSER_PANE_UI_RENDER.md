# FOA Asset Browser Pane UI Rendering

Status: Alpha evidence slice. This stage consumes only `foa-asset-browser-pane-model.json` and emits local static UI render evidence for the Asset Browser pane.

## Purpose

The renderer turns a validated pane model into a bounded local HTML/data render so reviewers can inspect the intended pane presentation before any live O3DE editor integration exists.

Implemented chain:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ local O3DE preview source staging evidence
→ bounded O3DE Asset Processor import proof
→ Asset Browser pane model
→ bounded Asset Browser pane UI render
```

## Commands

```text
render  --workspace <workspace.tgworkspace.json> --model <foa-asset-browser-pane-model.json>
verify  --input <foa-asset-browser-pane-ui-render.json>
fixture --output <directory>
```

## Output

Default output root:

```text
<ExtractedDataPath>/PreviewArtifacts/AssetBrowserUI/<render-id>/
```

Generated files:

```text
foa-asset-browser-pane-ui-render.json
ui/asset-browser-pane.html
ui/asset-browser-pane-data.json
```

`foa-asset-browser-pane-ui-render.json` is the authoritative manifest. The HTML and data files are local-only render artefacts.

## Contract

The renderer accepts only a pane model:

```text
DocumentKind = foa-asset-browser-pane-model
```

It rejects import-proof documents, raw O3DE conversion files, and raw O3DE preview source files. It validates that the input model itself consumed import-proof evidence and did not consume raw conversion/source files.

## Boundary

This stage does not:

- invoke Unity;
- run FoA;
- invoke O3DE Asset Processor;
- mutate O3DE's Asset Browser;
- create real O3DE Asset Browser entries;
- create typed item or recipe selectors;
- create typed authoring bindings;
- mutate catalogs;
- grant runtime permission;
- deploy, sign, or publish;
- mark the workflow function-complete.

Every rendered UI entry preserves:

```text
CanCreateTypedAuthoringBinding = false
RequiresExplicitBindingStep = true
CatalogPromotionAllowed = false
RuntimePermissionGranted = false
```

## Validation

Focused scratch validation used:

```text
PYTHONPATH=Gems/TaintedGrailModdingSDK/Tools python -m py_compile \
  Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_ui_render.py \
  Gems/TaintedGrailModdingSDK/Tools/validate_foa_asset_browser_pane_ui_render.py

PYTHONPATH=Gems/TaintedGrailModdingSDK/Tools python -m unittest discover \
  -s Gems/TaintedGrailModdingSDK/Tools/tests \
  -p 'test_foa_asset_browser_pane_ui_render.py' \
  -v

PYTHONPATH=Gems/TaintedGrailModdingSDK/Tools \
python Gems/TaintedGrailModdingSDK/Tools/validate_foa_asset_browser_pane_ui_render.py
```

Observed:

```text
Ran 8 tests in 1.247s
OK
FoA Asset Browser UI render boundary passed.
```

Not validated in this environment: full repository validation, O3DE compiled tests, live O3DE editor pane rendering, or real FoA installation input.

## Next step

After this slice merges, the next documented unit is a bounded 3D preview viewport model/render path. It should consume the UI render/model evidence and preview product evidence explicitly; it must not infer item or recipe authoring bindings from product existence.
