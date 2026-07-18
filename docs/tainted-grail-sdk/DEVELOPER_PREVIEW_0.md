# Developer Preview 0

Status: third implementation slice — command layer, deterministic synthetic fixture, and service-level persistence smoke available; launch wrapper, diagnostics bundle, manual UI evidence, and distributable artifact are not implemented yet.

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

### 2. Configure

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

Use `--dry-run` to inspect the exact `windows-vs-unity` x64 CMake command without executing it.

### 3. Build

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

This builds the Profile configuration for exactly these targets:

```text
Editor
TaintedGrailModdingSDK.Catalog.Tests
```

### 4. Validate

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The command stops on the first failure, preserves that command’s exit code, and writes:

```text
<build-dir>/tg-sdk-developer-preview-validation.json
```

The deterministic fixture also has local tests and contract validation:

```powershell
python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p "test_developer_preview_fixture.py" `
  -v

python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_fixture.py
python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_smoke.py
```

## Generate the deterministic synthetic fixture

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

Generation is byte-for-byte deterministic. The generator refuses to overwrite a non-empty directory. `--replace` works only when the existing directory first passes complete verification.

## Verify the fixture and SHA-256 manifest

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

`preview-fixture.manifest.json` records the relative path, byte size, and SHA-256 digest of every payload file. Verification fails closed for missing, extra, modified, traversing, symlinked, private-path, ownership, evidence, relationship, governance, economy, or binding defects.

The committed workspace, pack, source, evidence, and catalog fixture files remain canonical plain schema-1 JSON. The persistence services accept both these documented plain objects and existing O3DE `JsonSerialization` envelopes through the same reflected model types. Files saved by the current services use the O3DE envelope and are reopened through the same compatibility path.

## Service-level persistence smoke

The compiled `TaintedGrailModdingSDK.Catalog.Tests` target contains a service-level persistence smoke test using the committed fixture and the real editor persistence classes:

- `WorkspacePersistenceService`;
- `PackPersistenceService`;
- `SourceEvidencePersistenceService`;
- `CatalogPersistenceService`;
- `SourceEvidenceRegistry` and `CatalogDatabase`;
- `EconomyAuthoringService::BuildRecipeStationEvidence`.

The smoke test:

1. loads the plain schema fixture through those services;
2. verifies expected workspace/profile, source/evidence, catalog, governance, economy, stale, blocked, and unresolved state;
3. saves all durable documents to a temporary workspace;
4. discards the in-memory state as a close-equivalent step;
5. reopens the saved documents through the same services;
6. compares canonical state before and after.

The canonical state comparison covers workspace/profile data, pack data, source/evidence documents, catalog records and relationships, validation and governance history, item/recipe profiles, joins, permissions, and recipe station/learnability evidence rows.

The smoke target also proves that reviewed, proof-backed allowed usages survive catalog loading while legacy unproven allowances and newly injected allowances without matching reviewed validation proof are cleared and marked `legacy_permission_review_required`.

Run the compiled smoke with the other focused C++ tests:

```powershell
ctest --test-dir build/tg-sdk-developer-preview-0-windows-profile `
  -C profile `
  --output-on-failure `
  -R "TaintedGrailModdingSDK\.Catalog\.Tests"
```

This is service-level load, save, close-equivalent, and reopen proof. It does not replace the later manual Editor UI smoke pass or prove FoA runtime compatibility.

## Failure behavior

The command layer, fixture tooling, and persistence compatibility path fail closed for:

- unsupported hosts;
- invalid repository or build roots;
- prerequisite, configure, build, validator, source-policy, or compiled-test failures;
- unsafe fixture output or overwrite attempts;
- malformed, tampered, mismatched, non-portable, or non-synthetic fixture data;
- altered, partial, or halted plain-schema deserialization;
- catalog permissions without current reviewed proof for the same subject.

No command launches the game, performs deployment, mutates a save, installs a dependency silently, or uploads data.

## Current limitations

This third slice does not yet provide:

- an Editor launch wrapper;
- redacted diagnostics collection;
- manual UI smoke evidence;
- a CI-produced runnable Windows archive or source-build support bundle.

Those capabilities remain gated by the approved [Developer Preview 0 design](DEVELOPER_PREVIEW_0_DESIGN.md). Until they are complete and verified, the project remains a source-built pre-alpha editor and must not be presented as a finished Developer Preview release.
