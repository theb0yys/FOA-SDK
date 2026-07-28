# FoA Neutral Preview Handoff

Status: Alpha visual-preview handoff slice.

This tool converts a profile-bound visual asset discovery index plus thumbnail artefact evidence into a manifest-first neutral preview handoff package. It implements only this identity chain:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
```

It does not create generated O3DE preview products, invoke Unity, invoke O3DE Asset Processor, run FoA, run BepInEx/Harmony, mutate the catalog, grant runtime permission, deploy, sign, publish, or create typed authoring bindings.

## Commands

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_neutral_preview_handoff.py handoff \
  --workspace <workspace.tgworkspace.json> \
  --index <ExtractedDataPath>/foa-visual-asset-index.json \
  --thumbnails <ExtractedDataPath>/PreviewArtifacts/Thumbnails/foa-thumbnail-artifacts.json
```

Optional arguments:

```text
--output-root <ExtractedDataPath>/PreviewArtifacts/Handoffs/<handoff-id>
--captured-at YYYY-MM-DDTHH:MM:SSZ
--replace
```

Verification:

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_neutral_preview_handoff.py verify \
  --input <handoff-root>/foa-preview-handoff.json \
  --workspace <workspace.tgworkspace.json> \
  --index <ExtractedDataPath>/foa-visual-asset-index.json \
  --thumbnails <ExtractedDataPath>/PreviewArtifacts/Thumbnails/foa-thumbnail-artifacts.json
```

Fixture:

```text
python Gems/TaintedGrailModdingSDK/Tools/foa_neutral_preview_handoff.py fixture --output <temp-output>
```

## Output shape

The output is a local-only handoff package under `ExtractedDataPath`:

```text
PreviewArtifacts/Handoffs/<handoff-id>/
  foa-preview-handoff.json
  payloads/
    thumbnails/
    metadata/
```

The authoritative document is `foa-preview-handoff.json`. Payloads are local preview material only. They are not source assets, runtime payloads, redistributable content, catalog truth, or proof of O3DE import compatibility.

## Source model

The handoff does not assume one source asset. It records both a primary source and a source dependency collection:

```text
PrimarySourceAssetRecordId
SourceAssetRecordIds[]
SourceDependencies[]
PreviewEntries[].PrimarySourceAssetRecordId
PreviewEntries[].SourceDependencies[]
```

This keeps the current single-icon case simple while preserving the later path for mesh, material, texture, skeleton, and metadata dependencies.

## Coordinate model

Coordinate declarations are intentionally separate from conversion evidence.

`CoordinateDeclaration` records declared source and intended target coordinate systems. This is not proof of a correct transformed payload.

`CoordinateConversionEvidence` records the actual conversion operation state:

```text
ConversionToolId
ConversionToolVersion
TransformPolicyId
ConversionMatrix
ConversionOperationPerformed
VerificationState
VerificationEvidenceIds
VerificationEvidenceRequired
```

In this Alpha slice `ConversionOperationPerformed=false` and `VerificationState=not-verified`. A top-level `TransformVerified` field is rejected because it blurs declaration and proof.

## Authority boundary

The manifest and every payload keep authority disabled:

```text
RuntimeInvocationAllowed=false
GameMutationAllowed=false
SaveAccessAllowed=false
CatalogPromotionAllowed=false
RuntimePermissionGranted=false
O3deAssetProcessorInvoked=false
UnityInvoked=false
DeploymentAllowed=false
RepositoryCommitAllowed=false
RedistributionAllowed=false
GeneratedO3dePreviewProduct=false
TypedAuthoringBindingCreated=false
FunctionCompleteAllowed=false
```

## Acceptance criteria

The handoff is valid only when:

- workspace, visual index, thumbnail evidence, and handoff profile fields match exactly;
- every source dependency references a known visual asset record;
- the primary source appears in the source dependency collection;
- every payload uses `$handoff` paths and has SHA-256 plus byte size;
- copied payloads match the source thumbnail artefact hash;
- unsupported sources emit JSON receipt payloads instead of fake conversions;
- coordinate declaration and coordinate conversion evidence remain separate;
- generated files stay under `ExtractedDataPath`;
- no absolute or private paths are written;
- all authority flags remain false.

## Next stage

The next visual-preview unit is neutral-to-O3DE preview conversion. That later stage may consume this handoff and attempt to generate O3DE preview products, but this handoff stage itself cannot claim import, rendering, selector binding, deployment, or function completeness.
