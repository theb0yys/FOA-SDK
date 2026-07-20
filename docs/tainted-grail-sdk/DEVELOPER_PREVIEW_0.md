# Developer Preview 0

Status: fifth implementation slice — command layer, deterministic synthetic fixture, service-level persistence smoke, Windows x64 Editor launch wrapper, redacted local diagnostics, and manual UI evidence tooling available; the actual Windows screenshot pass and a distributable artifact are not complete yet.

## Purpose

Developer Preview 0 is the project’s source-built, pre-alpha usability milestone. Its first supported runnable target is **Windows x64 Profile**.

This milestone is not a standalone installer or a production mod toolchain. The preview commands do not launch FoA, invoke BepInEx or Harmony, deploy files, modify saves, collect telemetry, upload diagnostics or evidence, or install prerequisites automatically.

Run all commands from the repository root. The default build directory is:

```text
build/tg-sdk-developer-preview-0-windows-profile
```

## Build and validation commands

### 1. Check prerequisites

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --json-output build/tg-sdk-developer-preview-0-windows-profile/prerequisites.json
```

Required checks cover Windows x64, Python 3.10 or newer, CMake 3.23 or newer, Git, Git LFS, Visual Studio C++ tools, repository identity, build-directory state, and the expected Editor output.

### 2. Configure

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The configure command binds `LY_PROJECTS` to the repository-owned
`TaintedGrailModdingEditor` project so the generated Editor runtime registry
loads `ExternalToolchain` and `TaintedGrailModdingSDK` for the supported host.

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

Run the manual UI evidence unit tests and contract validator directly when working on this slice:

```powershell
python -m unittest discover `
  -s Gems/TaintedGrailModdingSDK/Tools/tests `
  -p "test_developer_preview_ui_evidence.py" `
  -v

python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_ui_evidence.py
```

## Generate and verify the synthetic fixture

Generate the deterministic synthetic fixture from project-owned data:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture
```

Verify its exact file set and SHA-256 manifest:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

The generated `preview-fixture.manifest.json` makes the output byte-for-byte deterministic.
The project-owned synthetic data does not prove Editor load/save/reopen behavior; that proof belongs to the compiled service-level smoke below.

The fixture contains one portable workspace and profile, one runtime-disabled pack, source/evidence documents, five synthetic canonical records, resolved and unresolved relationships, validation and governance history, economy profiles and joins, and allowed, forbidden, blocked, stale, and unresolved states.

Every identity uses the reserved `preview.*` or `subject:preview:*` namespace. The fixture contains no proprietary game data, native FoA identifiers, private paths, credentials, saves, or extracted assets.

## Service-level persistence smoke

The compiled `TaintedGrailModdingSDK.Catalog.Tests` target uses the real workspace, pack, source/evidence, catalog, registry, database, and economy services to prove load, save, close-equivalent, and reopen behavior, including proof-backed allowed usages while legacy unproven allowances fail closed:

1. the plain schema fixture loads;
2. canonical state matches the reviewed expectations;
3. every durable document saves to a temporary workspace;
4. in-memory state is discarded as a close-equivalent step;
5. the saved copy reopens through the same services;
6. the canonical state remains equivalent.

Run it with:

```powershell
ctest --test-dir build/tg-sdk-developer-preview-0-windows-profile `
  -C profile `
  --output-on-failure `
  -R "TaintedGrailModdingSDK\.Catalog\.Tests"
```

This is service-level persistence proof. It does not replace the Windows manual UI smoke or prove FoA runtime compatibility.

## Launch the O3DE Editor

Preview the resolved launch command without starting a process:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --dry-run
```

Launch the source-built Editor and capture wrapper-owned stdout, stderr, and the process result:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --log-dir build/tg-sdk-developer-preview-0-launch
```

To select an O3DE project explicitly, pass a directory containing `project.json`:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --project C:\O3DE\Projects\MyProject `
  --log-dir build/tg-sdk-developer-preview-0-launch
```

The wrapper uses O3DE’s documented `--project-path` switch. It accepts no arbitrary Editor passthrough arguments and does not launch FoA, BepInEx, Harmony, Avalon Core, deployment, or injection commands. It waits for the Editor process and returns the Editor’s exit code.

An explicit `--editor C:\path\to\Editor.exe` can be used instead of `--build-dir`. The executable must be named `Editor.exe` and must already exist.

The wrapper log directory contains:

```text
editor-launch.stdout.log
editor-launch.stderr.log
tg-sdk-developer-preview-launch.json
```

These are wrapper-owned captures. O3DE’s native Editor log remains in:

- `<PROJECT_ROOT>/user/log/Editor.log` when `--project` is supplied;
- `<ENGINE_ROOT>/user/log/Editor.log` when no project path is selected.

After launch, open **Tools → Tainted Grail SDK**. If the panes are missing, see the [Developer Preview Troubleshooting Guide](DEVELOPER_PREVIEW_TROUBLESHOOTING.md).

## Collect a redacted diagnostics directory

Diagnostics collection is explicit, local-only, and reviewable. Choose an output directory outside the project or workspace being inventoried:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_diagnostics.py collect `
  --repo-root . `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --project C:\O3DE\Projects\MyProject `
  --workspace-root build/tg-sdk-developer-preview-0-fixture `
  --validation-result build/tg-sdk-developer-preview-0-windows-profile/tg-sdk-developer-preview-validation.json `
  --launch-result build/tg-sdk-developer-preview-0-launch/tg-sdk-developer-preview-launch.json `
  --editor-log C:\O3DE\Projects\MyProject\user\log\Editor.log `
  --output build/tg-sdk-developer-preview-0-diagnostics
```

Arguments are optional except `--output`. When `--project` is supplied and no `--editor-log` is specified, the collector uses the known project `user/log/Editor.log` location when present.

The directory can contain only:

```text
README.txt
diagnostics-summary.json
system.txt
build-summary.json
validation-summary.json          optional
launch-summary.json              optional
workspace-inventory.json         optional
logs/*.log                       optional redacted tail excerpts
diagnostics-manifest.json
```

The collector includes tool versions, an allow-listed CMake summary, supplied validation and launch summaries, redacted Editor log excerpts, and workspace-relative durable-document paths, sizes, and SHA-256 hashes. It does not include source artifact contents, game files, saves, environment variables, credentials, unrestricted directory listings, or binary logs.

Paths and secret-like values are redacted. Log excerpts are tail-limited. Output size and file names are allow-listed. Nothing is uploaded automatically. You must review every generated file before sharing it.

Verify the bundle before sharing:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_diagnostics.py verify `
  --output build/tg-sdk-developer-preview-0-diagnostics
```

Verification fails for unexpected files, symlinks, path traversal, oversized files, invalid UTF-8, hash or size changes, remaining private absolute paths, or remaining secret-like material.

`--replace` is accepted only when the existing output first passes complete verification. An unrelated or modified directory is never overwritten silently.

## Windows manual UI smoke

The [Windows Manual UI Smoke and Screenshot Evidence](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md) checklist defines the accepted manual pass for a real Windows x64 Profile session.

The evidence command supports:

```text
developer_preview_ui_evidence.py init
developer_preview_ui_evidence.py record
developer_preview_ui_evidence.py attach
developer_preview_ui_evidence.py attest
developer_preview_ui_evidence.py verify
```

It binds the run to an exact commit, records every checklist result, copies only manually reviewed PNG screenshots, stores dimensions, sizes, and SHA-256 hashes, enforces required screenshot coverage, and checks textual metadata for private paths or secret-like material.

The tool performs no manual screenshot capture, UI-coordinate automation, OCR, network access, or upload. It does not inspect screenshot pixels. A tester must review every image and provide the disclosure attestations.

Keep the evidence under `build/` or outside the repository. Do not commit screenshots.

The checklist and screenshot-evidence verifier are implemented. The **actual Windows pass remains pending** until a tester runs the accepted commit, attaches reviewed screenshots, records all checks as `pass`, attests the privacy and runtime boundary, and verifies the evidence against the same commit.

## Failure behavior

The preview tooling fails closed for:

- unsupported launch hosts;
- a missing or wrongly named Editor executable;
- invalid repository, build, project, workspace, log, evidence, or output paths;
- prerequisite, configure, build, validator, source-policy, or compiled-test failures;
- unsafe fixture, diagnostics, or screenshot-evidence overwrite attempts;
- malformed or tampered fixture documents;
- persistence or canonical-state mismatches;
- diagnostics path traversal, symlinks, oversized content, unexpected files, or redaction failures;
- incomplete manual UI checklist items;
- missing screenshot coverage, invalid PNGs, screenshot tampering, or commit mismatches;
- absent privacy, disclosure, activation-log, or runtime-boundary attestations;
- child-process launch errors.

No command performs runtime deployment, game launch, save mutation, telemetry, automatic upload, automatic screenshot capture, or automatic dependency installation.

## Current limitations

This fifth slice does not yet provide:

- completed Windows manual UI smoke evidence and screenshots from an accepted commit;
- a CI-produced runnable Windows archive or source-build support bundle.

The checklist and verifier make the manual pass reproducible and reviewable, but they do not create or fabricate release evidence. The actual Windows screenshot pass remains pending.

The remaining artifact capability is gated by the approved [Developer Preview 0 design](DEVELOPER_PREVIEW_0_DESIGN.md). Until the manual evidence and artifact gates are complete, the project remains a source-built pre-alpha editor and must not be presented as a finished Developer Preview release.
