# FoA O3DE Preview Conversion

Status: Alpha staging slice.

This document describes the neutral-to-O3DE preview conversion stage of the visual game-content browser and preview pipeline.

The implemented chain is now:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ local O3DE preview source staging evidence
```

This is still not function-complete. It does not create a rendered editor preview, an Asset Browser entry, or a typed authoring binding.

## Purpose

`foa_o3de_preview_conversion.py` consumes a profile-bound neutral preview handoff and emits a local-only O3DE preview conversion manifest. The tool stages handoff payloads into an O3DE-oriented generated-output layout under `ExtractedDataPath` and records product-evidence placeholders that explicitly state that O3DE Asset Processor was not invoked.

The output is evidence for the next step, not a runtime asset and not an O3DE product-cache proof.

## Commands

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_o3de_preview_conversion.py fixture --output <dir> --replace
python Gems/TaintedGrailModdingSDK/Tools/foa_o3de_preview_conversion.py convert --workspace <workspace.tgworkspace.json> --handoff <foa-preview-handoff.json> --replace
python Gems/TaintedGrailModdingSDK/Tools/foa_o3de_preview_conversion.py verify --input <foa-o3de-preview-conversion.json> --workspace <workspace.tgworkspace.json> --handoff <foa-preview-handoff.json>
```

## Default output

```text
<ExtractedDataPath>/PreviewArtifacts/O3DE/<conversion-id>/foa-o3de-preview-conversion.json
<ExtractedDataPath>/PreviewArtifacts/O3DE/<conversion-id>/SourceAssets/Textures/...
<ExtractedDataPath>/PreviewArtifacts/O3DE/<conversion-id>/SourceAssets/Metadata/...
```

The manifest uses `$o3depreview` token paths. No absolute or private paths may appear in the document.

## Evidence model

The conversion manifest records:

- exact workspace profile binding;
- `SourceHandoffId`;
- source index and thumbnail manifest IDs carried forward from the handoff;
- primary and dependency source asset record IDs;
- O3DE preview source files staged from the neutral handoff;
- SHA-256 and byte size for every staged preview source;
- product-evidence placeholders showing Asset Processor was not invoked;
- coordinate declaration and conversion evidence carried forward without claiming verification;
- explicit false authority flags.

## Boundary

This stage does **not**:

- invoke Unity;
- invoke O3DE Asset Processor;
- generate O3DE product assets;
- create Asset Browser entries;
- create typed authoring bindings;
- mutate the catalog;
- mutate game files or saves;
- grant runtime permission;
- deploy, sign, or publish anything.

Every generated file remains local to `ExtractedDataPath`. Generated preview sources are not source assets for redistribution, not runtime payloads, and not proof that the asset renders in O3DE.

## Validation

The focused validation command is:

```text
PYTHONPATH=Gems/TaintedGrailModdingSDK/Tools \
python Gems/TaintedGrailModdingSDK/Tools/validate_foa_o3de_preview_conversion.py
```

The validator checks that:

- authority flags remain false;
- generated O3DE preview products are not claimed;
- Asset Processor invocation is not claimed;
- payload hashes match staged preview source files;
- top-level `TransformVerified` is rejected;
- product evidence remains `asset-processor-not-invoked`.

## Next required stage

The next implementation unit is a bounded O3DE Asset Processor import proof. That later stage may consume this conversion manifest and attempt an actual O3DE import, but it must record tool identity, product IDs, cache paths, logs, failures, and verification evidence separately.
