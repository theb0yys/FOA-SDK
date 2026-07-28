# FoA O3DE Asset Processor Import Proof

Status: Alpha evidence stage.

This stage consumes a profile-bound `foa-o3de-preview-conversion.json` manifest and a bounded external O3DE Asset Processor observation. It emits `foa-o3de-asset-processor-import-proof.json` plus local copied import-log evidence.

Implemented chain:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ neutral preview handoff
→ local O3DE preview source staging evidence
→ bounded O3DE Asset Processor import proof
```

This stage records proof that the O3DE Asset Processor was observed externally. The tool itself does not invoke O3DE Asset Processor.

## Commands

```text
proof
verify
fixture
```

`proof` requires:

```text
--workspace <workspace.tgworkspace.json>
--conversion <foa-o3de-preview-conversion.json>
--observation <foa-o3de-asset-processor-observation.json>
```

The observation document must use `DocumentKind: foa-o3de-asset-processor-observation` and must bind to the exact active game profile and conversion ID.

## Observation boundary

The observation records a bounded Asset Processor run:

- redacted command line;
- invocation start and completion times;
- exit code and timeout state;
- imported product IDs;
- tokenized product cache paths;
- product hashes and sizes;
- copied import logs;
- import failures;
- verification evidence IDs.

The proof manifest distinguishes observed Asset Processor execution from editor authority. `O3deAssetProcessorInvocationObserved` may be true as evidence, but the proof tool still sets `AssetProcessorInvocationPerformedByThisTool=false` and keeps runtime/catalog/deployment authority false.

## Product boundary

This stage does not:

- invoke Unity;
- launch FoA;
- mutate game files or saves;
- invoke O3DE Asset Processor itself;
- create Asset Browser entries;
- create typed authoring bindings;
- mutate catalogues;
- grant runtime permission;
- deploy, sign, or publish;
- mark the workflow function-complete.

Imported products are recorded as local preview evidence only. Product cache paths use `$assetcache/...` tokens and must not leak private absolute paths.

## Output

Default output root:

```text
<ExtractedDataPath>/PreviewArtifacts/O3DE/<conversion-id>/ImportProofs/<proof-id>/
```

Main manifest:

```text
foa-o3de-asset-processor-import-proof.json
```

Copied logs:

```text
logs/<log-file>
```

## Acceptance criteria

A proof is valid only when:

- profile, game version, branch, and runtime target match the active workspace;
- `SourceConversionId` matches the input conversion manifest;
- the conversion manifest already staged O3DE preview sources;
- the observation records an Asset Processor invocation;
- product records bind to known staged source IDs;
- product cache paths use `$assetcache/...` tokens;
- copied import logs match recorded SHA-256 and byte sizes;
- failures are explicit rather than hidden;
- `FunctionCompleteAllowed=false`;
- Asset Browser and typed binding states remain false;
- runtime, catalog, deployment, repository-commit, and redistribution authority remain false.

## Next stage

After this proof stage lands, the next documented unit is the Asset Browser pane. It should consume import-proof evidence, not raw conversion files, and must not infer authoring bindings from product existence alone.
