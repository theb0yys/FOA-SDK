# FoA Thumbnail Artefact Extractor

Status: Alpha preview pipeline slice 2, bounded loose-texture cohort implemented.

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
- a valid byte size and SHA-256 source fingerprint;
- false discovery authority flags.

Before copying or decoding a file, the extractor rereads it and compares its byte size and SHA-256 with the discovery record. source fingerprint drift fails closed and produces no replacement evidence.

## Bounded format cohort

The bounded DDS and TGA decoder is dependency-free and handles the documented loose-texture cohort.

Loose image payloads are handled as follows:

- `.png`, `.jpg`, `.jpeg`, and `.webp` are copied byte-for-byte;
- `.tga` is decoded to deterministic RGBA PNG for:
  - uncompressed true-colour at 16, 24, or 32 bits;
  - RLE true-colour at 16, 24, or 32 bits;
  - uncompressed or RLE greyscale at 8 or 16 bits;
  - top/bottom and left/right origin flags;
- `.dds` first-mip decoding supports:
  - BC1 / DXT1;
  - BC2 / DXT3;
  - BC3 / DXT5;
  - BC4 / ATI1;
  - BC5 / ATI2 as a partial two-channel preview;
  - bounded legacy masked RGB/luminance payloads;
  - selected DX10 formats: RGBA8, BGRA8, R8, BC1, BC2, BC3, BC4, and BC5.

Decoded DDS and TGA artefacts are emitted as `.png` with `OutputMediaType: image/png`, dimensions, source texture format, generation method, and fidelity evidence.

Unsupported DDS/TGA sub-formats produce an explicit `unsupported-receipt` and warning. They do not fall back to invented pixels or external tools. Arrays, cubemaps, volume textures, colour-mapped TGA files, unsupported DXGI/FourCC values, excessive dimensions, truncated streams, and other out-of-cohort forms remain unsupported.

The tool does not extract from AssetBundles or Unity serialized object containers. Those require separately reviewed Unity-to-neutral or extractor-specific handoff work.

## Safety bounds

The extractor applies deterministic resource limits:

- maximum source payload: 16 MiB;
- maximum dimension: 8192 pixels;
- maximum decoded pixel count: 16,777,216;
- maximum artefact count: 10,000;
- output root must remain below the active profile `ExtractedDataPath`;
- generated paths use `$preview/...` tokens only.

No decoder command, subprocess, native DLL, third-party image package, Unity executable, or O3DE executable is invoked.

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
- `Fidelity` and `FidelityDetail`;
- decoded width, height, and source texture format where applicable;
- generated and unsupported counts;
- `RepositoryCommitAllowed: false`;
- `RedistributionAllowed: false`;
- `RuntimePermissionGranted: false`;
- `GeneratedO3dePreviewProduct: false`.

Verification rereads every generated payload, verifies size and SHA-256, checks decoded DDS/TGA outputs are PNG, binds artefacts back to the exact discovery record, and rejects path, profile, source, count, media-type, or authority drift. Legacy `0.1.0` manifests remain verifiable under their original boundary.

## Boundary

The generated artefacts are local preview artefacts only. They may be used by later browser and selector tooling but are not O3DE preview products and are not runtime assets.

The following remain out of scope:

- Unity batch extraction;
- AssetBundle or serialized-object parsing;
- arbitrary or vendor-specific DDS/TGA encodings outside the documented cohort;
- mip-chain, cubemap, array, or volume preview selection;
- generated O3DE preview products;
- runtime adapters;
- catalog mutation;
- redistribution of extracted commercial content.

The downstream neutral preview handoff consumes the generated local preview artefacts and records fingerprints, losses, warnings, and dependency identities before O3DE import is attempted.
