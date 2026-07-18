# Developer Preview 0

Status: second implementation slice — command layer and deterministic synthetic fixture available; launch wrapper, load/save/reopen smoke harness, diagnostics bundle, manual UI evidence, and distributable artifact are not implemented yet.

## Purpose

Developer Preview 0 is the project’s source-built, pre-alpha usability milestone. Its first supported runnable target is **Windows x64 Profile**.

This milestone is not a standalone installer or a production mod toolchain. The command layer and fixture tooling do not launch FoA, invoke BepInEx or Harmony, deploy files, modify saves, collect telemetry, or install prerequisites automatically.

## Build and validation commands

Run all commands from the repository root. The default build directory is:

```text
build/tg-sdk-developer-preview-0-windows-profile
```

An explicit `--build-dir` may point elsewhere, but it must not be the repository root, contain the repository, live inside `.git`, or contain unrelated files without a matching O3DE `CMakeCache.txt`.

### 1. Check prerequisites

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --json-output build/tg-sdk-developer-preview-0-windows-profile/prerequisites.json
```

Required checks cover:

- Windows x64 host;
- Python 3.10 or newer;
- CMake 3.23 or newer;
- Git and Git LFS;
- Visual Studio 2022 or Build Tools with **Desktop development with C++**;
- a valid O3DE repository root.

The checker is read-only. It reports whether the build directory is configured and whether `bin/profile/Editor.exe` exists, but those are informational until the configure and build steps run.

### 2. Preview the configure command

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --dry-run
```

The approved configure command uses the repository’s `windows-vs-unity` CMake preset, an explicit build directory, and x64 architecture. Remove `--dry-run` to execute it:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The wrapper prints the exact CMake command before execution and returns CMake’s original exit code.

### 3. Build the Editor and focused test target

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

This builds the Profile configuration for exactly these targets:

```text
Editor
TaintedGrailModdingSDK.Catalog.Tests
```

Use `--dry-run` to inspect the command without invoking the build.

### 4. Run the focused command-layer validation

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The validation command currently runs, in order:

1. Developer Preview command unit tests;
2. Developer Preview command contract validation;
3. TG SDK foundation validation;
4. governance-hardening validation;
5. catalog/governance/economy contract validation;
6. O3DE source-policy validation for the TG SDK Gem;
7. compiled `TaintedGrailModdingSDK.Catalog.Tests` through CTest.

It stops on the first failure and propagates that command’s exit code. It writes a machine-readable summary to:

```text
<build-dir>/tg-sdk-developer-preview-validation.json
```

Use `--result <path>` to select another explicit result path. Use `--dry-run` to print the ordered plan and write a planned-result document without executing tests.

The deterministic fixture has its own unit and contract validation commands in this slice. They run automatically in GitHub Actions and can be run locally with:

```powershell
python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p "test_developer_preview_fixture.py" `
  -v

python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_fixture.py
```

## Generate the deterministic synthetic fixture

Choose an explicit output directory:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture
```

The generator creates a portable workspace tree containing only project-owned synthetic data. Every canonical ID uses `preview.*`; every subject uses `subject:preview:*`; synthetic records are owned by `preview.developer-preview-0`; native references remain empty.

The fixture includes:

- one workspace and placeholder game profile;
- one runtime-disabled pack;
- one structured source input, source document, and evidence document;
- five canonical records: two items, one recipe, one station, and one learnability source;
- resolved `crafted_at` and `learned_from` relationships;
- one explicitly unresolved `learned_from` relationship;
- validation and governance histories;
- allowed, forbidden, blocked, stale, and unresolved states;
- item and recipe profiles, one ingredient join, and one output join.

Generation is byte-for-byte deterministic. Repeating the command in two clean directories produces identical payload and manifest bytes.

The generator refuses to overwrite a non-empty directory. `--replace` works only when the existing directory first passes complete verification. It will not replace a modified, partial, unrelated, or symlink-containing directory.

## Verify the fixture and SHA-256 manifest

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

The fixture contains:

```text
preview-fixture.manifest.json
```

The manifest records the relative path, byte size, and SHA-256 digest of every payload file. Verification fails closed for:

- missing, extra, non-canonical, or modified files;
- SHA-256 or size mismatches;
- path traversal or symbolic links;
- absolute or private filesystem paths;
- secret-like material;
- workspace, profile, game-version, or branch binding mismatches;
- source/evidence fingerprint mismatches;
- non-synthetic or non-preview identities;
- synthetic records without the preview pack owner;
- borrowed native references;
- unknown evidence or catalog references;
- invalid relationship target cardinality;
- unresolved relationships that lose their explicit subject or missing-reference state;
- invalid item, recipe, ingredient, output, validation, or governance links;
- loss of the expected allowed, forbidden, blocked, stale, or unresolved states.

The generated fixture is compatible with the current schema-1 field contracts and is intended as the input to the next service-level smoke slice. This slice does not prove Editor load/save/reopen behavior; that proof requires the approved C++ service and persistence smoke harness.

## Failure behavior

The command layer and fixture tooling fail closed for:

- unsupported hosts;
- invalid repository roots;
- unsafe or unrelated build directories;
- CMake caches bound to another source tree;
- missing build configuration before a real build or validation run;
- prerequisite, configure, build, validator, source-policy, or compiled-test failures;
- unsafe fixture output paths or overwrite attempts;
- malformed, tampered, mismatched, non-portable, or non-synthetic fixture data.

No command uses a shell command string. Child processes receive explicit argument arrays, no prerequisite is installed silently, and fixture generation performs no network access or process launch.

## Current limitations

This second slice does not yet provide:

- service-level load, save, close-equivalent, and reopen smoke automation;
- an Editor launch wrapper;
- redacted diagnostics collection;
- manual UI smoke evidence;
- a CI-produced runnable Windows archive or source-build support bundle.

Those capabilities remain gated by the approved [Developer Preview 0 design](DEVELOPER_PREVIEW_0_DESIGN.md). Until they are complete and verified, the project remains a source-built pre-alpha editor and must not be presented as a finished Developer Preview release.
