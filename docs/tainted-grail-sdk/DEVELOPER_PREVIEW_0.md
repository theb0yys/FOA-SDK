# Developer Preview 0

Status: source-built pre-alpha command, fixture, persistence, launch,
diagnostics, and manual-evidence tooling. The Windows screenshot pass and a
distributable artifact remain incomplete.

## Purpose

Developer Preview 0 is the first usability milestone for the Tainted Grail
Modding Editor. Its supported runnable target is **Windows x64 Profile**.

It is not a standalone installer or production mod toolchain. It does not launch FoA,
invoke BepInEx or Harmony, deploy files, modify saves, collect telemetry, upload
diagnostics, or install prerequisites automatically.

FOA-SDK and O3DE now have separate identities:

```text
Development/
├── FOA-SDK/      product checkout
├── o3de/         exact O3DE checkout
└── foa-build/    generated build and evidence output
```

The exact engine dependency is recorded in `o3de.lock.json`. The command checks
the engine name, version, and full Git commit before configure, build, or
validation.

Set `FOA_O3DE_ROOT` when the O3DE checkout is not the sibling `o3de/` directory.
Set `FOA_BUILD_ROOT` to change the generated-output root. Explicit
`--engine-root` and `--build-dir` arguments take precedence.

## Obtain the pinned engine

From the directory containing FOA-SDK:

```powershell
git clone https://github.com/o3de/o3de.git o3de
git -C o3de checkout 68683f23fb747380d3efa2424bd5f30242e9c5a2
```

Do not substitute a branch tip. A dependency update requires its own reviewed
change to `o3de.lock.json`.

## Build and validation commands

Run commands from `FOA-SDK`.

### 1. Check prerequisites

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile `
  --json-output ..\foa-build\tg-sdk-developer-preview-0-windows-profile\prerequisites.json
```

Required checks cover Windows x64, Python 3.10 or newer, CMake 3.23 or newer,
Git, Git LFS, Visual Studio C++ tools, product identity, O3DE identity, the exact
pinned engine commit, and build-directory state.

### 2. Configure

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The generated CMake command uses:

```text
-S <engine-root>
-B <build-root>
-DLY_PROJECTS=<product-root>/TaintedGrailModdingEditor
```

This binds O3DE to the repository-owned editor project and its `Atom`,
`DiffuseProbeGrid`, `ExternalToolchain`, and `TaintedGrailModdingSDK` Gems. Use
`--dry-run` to inspect the exact `windows-vs-unity` x64 command.

`--product-root` is available for non-standard checkouts. `--repo-root` remains
a temporary compatibility alias during extraction preparation.

### 3. Build

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The Profile build contains exactly these command-layer targets:

```text
Editor
AssetProcessorBatch
TaintedGrailModdingSDK.Catalog.Tests
```

### 4. Validate

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

FOA validators run from the product checkout. The O3DE source-policy validator
is loaded from the pinned engine checkout. Compiled tests run from the build
root. Validation stops on the first failure and writes:

```text
<build-root>/tg-sdk-developer-preview-validation.json
```

The result records `product_root`, `engine_root`, `build_root`, and the pinned
engine commit.

## Synthetic fixture

Generate and verify project-owned deterministic data outside the source tree:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output ..\foa-build\tg-sdk-developer-preview-0-fixture

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output ..\foa-build\tg-sdk-developer-preview-0-fixture
```

The fixture uses reserved `preview.*` identities and contains no proprietary
game data, native FoA identifiers, private paths, credentials, saves, or
extracted assets.

## Compiled persistence smoke

```powershell
ctest --test-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile `
  -C profile `
  --output-on-failure `
  -R "TaintedGrailModdingSDK\.Catalog\.Tests"
```

This proves service-level load, save, close-equivalent clear, reopen, and
canonical equivalence. It does not prove FoA runtime compatibility.

## Launch and diagnostics

The existing launch wrapper continues to accept an explicit build root:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile `
  --dry-run
```

It uses O3DE’s `--project-path` switch, exposes no arbitrary Editor passthrough
arguments, waits for the Editor process, and returns its exit code.

Diagnostics and screenshot evidence must also remain under the external build
root or another reviewed output directory. Nothing is uploaded automatically.
Review every generated file before sharing.

## Windows manual UI smoke

The [Windows Manual UI Smoke and Screenshot Evidence](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md)
defines the accepted real-session pass. The evidence tool binds results to an
exact source commit, hashes reviewed PNG files, checks required coverage, and
enforces privacy and runtime-boundary attestations.

The tooling does not take screenshots automatically, inspect pixels, use OCR,
or fabricate release evidence.

## Failure behavior

The preview fails closed for:

- a missing, invalid, or wrongly pinned O3DE checkout;
- an invalid FOA-SDK product checkout;
- a build directory overlapping either source checkout;
- a build cache configured from another engine or without the FOA project;
- unsupported launch hosts;
- missing or wrongly named Editor output;
- prerequisite, configure, build, validator, source-policy, or compiled-test
  failures;
- malformed, tampered, private, or unsafe generated evidence.

No command performs runtime deployment, game launch, save mutation, telemetry,
automatic upload, automatic screenshot capture, or automatic dependency
installation.

## Current limitations

The source tree still contains inherited O3DE content while the external-engine
path is proven. The temporary in-tree fallback will be removed by the history
extraction unit.

The preview still lacks completed Windows manual UI evidence and a CI-produced
runnable Windows archive. Until those gates pass, it remains a source-built
pre-alpha editor.
