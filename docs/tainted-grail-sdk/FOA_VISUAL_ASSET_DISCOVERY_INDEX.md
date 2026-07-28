# FoA Visual Asset Discovery Index

Status: Alpha implementation slice; not function-complete.

This document defines the first development unit after the visual game-content browser and preview pipeline gate: read-only asset discovery and indexing.

The output is `foa-visual-asset-index.json` under the active profile's `ExtractedDataPath`. It is a local evidence index for the visual browser pipeline. It is not a preview product, not a Unity extraction result, not an O3DE Asset Processor result, not a catalog mutation, and not runtime permission.

## Required gate chain

This slice produces only the first part of the required visual-preview chain:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ generated O3DE preview product
→ typed authoring binding
```

This implementation covers:

```text
FoA native asset reference
→ version-bound discovery record
```

It does not yet cover:

```text
local preview artefact
→ generated O3DE preview product
→ typed authoring binding
```

## Command

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_visual_asset_discovery_index.py index `
  --workspace D:\FOA-SDK\workspace.tgworkspace.json `
  --captured-at 2026-07-28T00:00:00Z `
  --replace
```

By default the tool writes:

```text
<ExtractedDataPath>/foa-visual-asset-index.json
```

Verification:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_visual_asset_discovery_index.py verify `
  --workspace D:\FOA-SDK\workspace.tgworkspace.json `
  --input <ExtractedDataPath>\foa-visual-asset-index.json
```

Fixture generation:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/foa_visual_asset_discovery_index.py fixture `
  --output D:\FOA-SDK-Out\visual-index-fixture
```

## Bounded discovery rules

The indexer:

- reads only the active workspace game profile;
- enumerates only files under the configured `InstallPath`;
- writes only under `ExtractedDataPath`;
- records tokenized `$install` and `$extracted` locators instead of private paths;
- scans only allowlisted candidate content extensions;
- hashes bounded files without copying file contents;
- records preview eligibility as a planning signal only;
- keeps every operational authority false.

Generated outputs remain outside repository and engine source trees. No proprietary game payloads may be committed.

## Explicit non-authority

This slice performs no preview product generation. It includes:

- no preview product;
- no Unity invocation;
- no Asset Processor invocation;
- no BepInEx or Harmony execution;
- no FoA launch;
- no save inspection;
- no copied game payload;
- no catalog promotion;
- no runtime permission.

The document includes `FunctionCompleteAllowed: false` because this stage is still only an evidence-producing prerequisite.

## Output shape

The output document uses:

```json
{
  "SchemaVersion": 1,
  "DocumentKind": "foa-visual-asset-discovery-index",
  "IndexId": "visual.index.<profile>.<fingerprint>",
  "ProfileId": "...",
  "GameVersion": "...",
  "Branch": "...",
  "RuntimeTarget": "Mono",
  "InstallRoot": "$install",
  "OutputRoot": "$extracted",
  "PreviewGateStatus": {
    "VisualPreviewGateRequired": true,
    "FunctionCompleteAllowed": false,
    "Stage": "alpha.discovery-index"
  },
  "AssetRecords": [],
  "OperationalAuthority": {
    "RuntimeInvocationAllowed": false,
    "GameMutationAllowed": false,
    "SaveAccessAllowed": false,
    "CatalogPromotionAllowed": false,
    "RuntimePermissionGranted": false,
    "PreviewProductGenerated": false,
    "O3deAssetProcessorInvoked": false,
    "UnityInvoked": false,
    "PayloadCopied": false
  }
}
```

## Next slice

The next researched implementation unit is native icon and thumbnail extraction. That unit may consume this index, but it must produce a separate local preview artefact document and still keep generated payloads outside repository and engine source trees.
