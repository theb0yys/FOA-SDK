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

FOA-SDK and O3DE have separate identities and checkouts:

```text
Development/
├── FOA-SDK/      product checkout
├── o3de/         exact O3DE checkout
└── foa-build/    generated build and evidence output
```

The FOA-SDK working tree contains only the product project, the two product Gems,
research, documentation, and governance. Stock O3DE source and engine Gems are
provided only by the external engine checkout.

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

This binds O3DE to the product-owned editor project and its `Atom`,
`DiffuseProbeGrid`, `ExternalToolchain`, and `TaintedGrailModdingSDK` Gems. Use
`--dry-run` to inspect the exact `windows-vs-unity` x64 command.

`--product-root` is available for non-standard checkouts. `--repo-root` remains
a deprecated compatibility alias for existing command invocations; it refers to
the FOA-SDK product root and never to the O3DE engine root.

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
is loaded from the pinned engine checkout and scans both product Gems. Compiled
tests run from the build root with `--no-tests=error`, so a missing or misnamed
Catalog target is a failure. Validation stops on the first failure and writes:

```text
<build-root>/tg-sdk-developer-preview-validation.json
```

The result records `product_root`, `engine_root`, `build_root`, and the pinned
engine commit.

Run the complete local product gate with:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py `
  --engine-root ..\o3de `
  --ctest-build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

## Synthetic fixture

Generate the deterministic synthetic fixture and verify byte-for-byte deterministic project-owned data outside the source tree:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output ..\foa-build\tg-sdk-developer-preview-0-fixture

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output ..\foa-build\tg-sdk-developer-preview-0-fixture
```

The generated `preview-fixture.manifest.json` binds every fixture file by exact
byte count and SHA-256 fingerprint. The fixture uses reserved `preview.*`
identities and contains no proprietary game data, native FoA identifiers,
private paths, credentials, saves, or extracted assets.

The fixture is project-owned synthetic data. It does not prove Editor load/save/reopen behavior.

## Compiled persistence smoke

```powershell
ctest --test-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile `
  -C profile `
  --output-on-failure `
  --no-tests=error `
  -R "TaintedGrailModdingSDK\.Catalog\.Tests"
```

This service-level persistence smoke proves load, save, close-equivalent, and reopen behavior plus canonical state equivalence. It preserves proof-backed allowed usages, while legacy unproven allowances remain fail-closed. It does not prove FoA runtime compatibility.

## Launch and diagnostics

The existing launch wrapper continues to accept an explicit build root and a
bounded `--log-dir`:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile `
  --log-dir ..\foa-build\tg-sdk-developer-preview-0-launch-logs `
  --dry-run
```

It uses O3DE’s `--project-path` switch, exposes no arbitrary Editor passthrough
arguments, waits for the Editor process, and returns its exit code.

Collect and verify a redacted diagnostic bundle with:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_diagnostics.py collect `
  --repo-root . `
  --output ..\foa-build\tg-sdk-developer-preview-0-diagnostics

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_diagnostics.py verify `
  --output ..\foa-build\tg-sdk-developer-preview-0-diagnostics
```

Diagnostics and screenshot evidence must remain under the external build root or
another reviewed output directory. Nothing is uploaded automatically; review every generated file before sharing.

## Windows manual UI smoke

The [Windows Manual UI Smoke and Screenshot Evidence](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md)
defines the accepted real-session pass. Initialize and verify that evidence with
`developer_preview_ui_evidence.py`; the tool binds results to an exact source
commit, hashes reviewed PNG files, checks required coverage, and enforces privacy
and runtime-boundary attestations.

manual screenshot capture remains an operator action. The tooling does not take
screenshots automatically, inspect pixels, use OCR, or fabricate release
evidence.

## Failure behavior

The preview fails closed for:

- a missing, invalid, or wrongly pinned O3DE checkout;
- an invalid or engine-contaminated FOA-SDK product checkout;
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

The visible working tree is extracted into the product-only structure. Earlier
Git commits still contain the inherited O3DE fork until the separately reviewed
filtered-history migration is completed and content-equivalence is proven.

The actual Windows pass remains pending, and the preview still lacks a
CI-produced runnable Windows archive. Until those gates pass, it remains a
source-built pre-alpha editor.
