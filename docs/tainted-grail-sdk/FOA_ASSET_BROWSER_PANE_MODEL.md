# FOA Asset Browser Pane Model

Status: Alpha evidence-view model.

This slice consumes bounded O3DE Asset Processor import-proof evidence and emits an Asset Browser pane data model for editor UI work. It is intentionally not an O3DE Asset Browser mutation and not a typed authoring binding step.

Implemented chain:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ local O3DE preview source staging evidence
→ bounded O3DE Asset Processor import proof
→ Asset Browser pane model
```

## Commands

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_model.py build \
  --workspace <workspace.tgworkspace.json> \
  --import-proof <foa-o3de-asset-processor-import-proof.json>

python Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_model.py verify \
  --input <foa-asset-browser-pane-model.json> \
  --workspace <workspace.tgworkspace.json> \
  --import-proof <foa-o3de-asset-processor-import-proof.json>

python Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_model.py fixture \
  --output <dir>
```

## Output

The default output is written under:

```text
<ExtractedDataPath>/PreviewArtifacts/AssetBrowser/<asset-browser-model-id>/foa-asset-browser-pane-model.json
```

The document kind is:

```text
foa-asset-browser-pane-model
```

## Input rule

This slice consumes only import-proof evidence:

```text
foa-o3de-asset-processor-import-proof.json
```

It rejects raw conversion documents such as:

```text
foa-o3de-preview-conversion.json
```

The pane model must not infer capability directly from raw conversion files, raw preview source files, product cache paths, or product existence alone.

## Entry model

Each pane entry is an evidence-backed row with:

- a stable `PaneEntryId`;
- source import proof and conversion identifiers;
- product evidence or failure evidence;
- tokenized `$assetcache/...` product cache paths for successful products;
- error issues for failed import records;
- a selection policy that requires a later explicit binding step.

## Boundary

This PR does not:

- invoke Unity;
- run FoA;
- invoke O3DE Asset Processor;
- mutate O3DE's Asset Browser;
- create O3DE Asset Browser entries;
- create typed item or recipe bindings;
- mutate catalogues;
- grant runtime permission;
- deploy, sign, publish, or commit generated product content;
- mark any workflow function-complete.

The following remain false:

```text
O3deAssetBrowserMutated=false
AssetBrowserEntryCreated=false
TypedAuthoringBindingCreated=false
CatalogPromotionAllowed=false
RuntimePermissionGranted=false
FunctionCompleteAllowed=false
```

## Product existence is not authoring authority

A successful product import can make a pane row selectable and viewable as evidence. It cannot create a typed authoring binding. Every pane entry sets:

```text
CanCreateTypedAuthoringBinding=false
RequiresExplicitBindingStep=true
```

The next stage is a bounded UI rendering pass that presents these rows without creating item/recipe selectors or authoring bindings.
