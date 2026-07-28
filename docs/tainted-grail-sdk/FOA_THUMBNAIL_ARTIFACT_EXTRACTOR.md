# FoA Thumbnail Artefact Extractor

Status: Alpha preview pipeline slice 2.

This tool consumes `foa-visual-asset-index.json` from the visual asset discovery index and emits local-only native icon/thumbnail artefact evidence. It advances the visual preview chain from:

```text
FoA native asset reference
→ version-bound discovery record
```

to:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
```

It does not invoke Unity, does not invoke O3DE Asset Processor, does not parse Unity bundles, does not run FoA, does not run BepInEx or Harmony, does not mutate catalogues, does not inspect saves, does not deploy, and does not grant runtime permission. `FunctionCompleteAllowed` remains `false`.

## Commands

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor.py extract \
  --workspace <workspace.tgworkspace.json> \
  --index <ExtractedDataPath>/foa-visual-asset-index.json \
  --preview-root <ExtractedDataPath>/PreviewArtifacts/Thumbnails \
  --manifest <ExtractedDataPath>/PreviewArtifacts/Thumbnails/foa-thumbnail-artifacts.json

python Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor.py verify \
  --manifest <ExtractedDataPath>/PreviewArtifacts/Thumbnails/foa-thumbnail-artifacts.json \
  --workspace <workspace.tgworkspace.json> \
  --index <ExtractedDataPath>/foa-visual-asset-index.json \
  --preview-root <ExtractedDataPath>/PreviewArtifacts/Thumbnails

python Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor.py fixture \
  --output <temporary-output>
```

## Inputs

The extractor requires a workspace-bound visual discovery index with:

- `DocumentKind: foa-visual-asset-discovery-index`;
- exact `ProfileId`, `GameVersion`, `Branch`, and `RuntimeTarget` matching the active workspace profile;
- tokenized `$install/...` native asset references;
- `PreviewEligibility.ThumbnailCandidate: true` for candidate icon files;
- false discovery authority flags.

Only loose local image candidates are handled in this slice:

- supported generated artefacts: `.png`, `.jpg`, `.jpeg`, `.webp`;
- unsupported receipts: `.dds`, `.tga`.

The tool does not extract from AssetBundles or Unity serialized object containers. Those require later Unity-to-neutral or extractor-specific preview handoff work.

## Outputs

The extractor writes:

```text
<ExtractedDataPath>/PreviewArtifacts/Thumbnails/foa-thumbnail-artifacts.json
<ExtractedDataPath>/PreviewArtifacts/Thumbnails/<thumbnail-artifact-id>.<ext>
```

The manifest uses `DocumentKind: foa-thumbnail-artifact-evidence` and records:

- `SourceIndexId`;
- exact workspace/game/runtime binding;
- per-artifact source hash and output hash;
- `$preview/...` artefact paths only;
- `GenerationMethod`;
- fidelity status;
- `RepositoryCommitAllowed: false`;
- `RedistributionAllowed: false`;
- `RuntimePermissionGranted: false`;
- `GeneratedO3dePreviewProduct: false`.

## Boundary

The generated artefacts are local preview artefacts only. They may be used by later browser and selector tooling but are not O3DE preview products yet and are not runtime assets.

The following remain out of scope:

- Unity batch extraction;
- AssetBundle parsing;
- texture transcode or decompression for DDS/TGA;
- generated O3DE preview products;
- Asset Browser pane integration;
- Item/Recipe selector binding;
- actor or troop preview;
- drag-and-drop placement;
- runtime adapters;
- catalog mutation.

## Next stage

The next researched unit is Unity-to-neutral preview handoff. That stage consumes discovery records and local preview artefact evidence, then emits a neutral preview conversion result with fingerprints, losses, and warnings before O3DE import is attempted.
