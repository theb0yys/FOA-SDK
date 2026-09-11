# Development Guide

## Purpose

This guide covers local FOA-SDK development. The authoritative workflow is `ENGINEERING_PROCESS.md`; validation requirements are in `CI_AND_LOCAL_VALIDATION.md`.

## Repository and engine layout

FOA-SDK is the product repository. O3DE is an external pinned dependency.

Recommended layout:

```text
Development/
├── FOA-SDK/
├── o3de/
└── foa-build/
```

Clone the product and upstream engine separately:

```shell
git clone https://github.com/theb0yys/FOA-SDK.git FOA-SDK
git clone https://github.com/o3de/o3de.git o3de
```

From `FOA-SDK`, read the exact engine commit from `o3de.lock.json` and check out that commit in the sibling `o3de` repository. Do not substitute a branch tip.

At the time this guide was updated, the pinned commit is:

```text
68683f23fb747380d3efa2424bd5f30242e9c5a2
```

`developer_preview.py` verifies the lock before using the engine.

Keep generated build output outside both source checkouts, normally under sibling `foa-build/`.

## Branch model

`main` is the reviewed integration branch. Create a focused non-`main` branch for each review unit.

`foa-development` may exist as a maintainer convenience branch but is not required for normal work and is not the engine source branch.

## Prerequisites

Use the compiler, CMake, Python, Git LFS, and O3DE third-party package configuration required by the pinned O3DE revision.

On Windows, use the Developer Preview prerequisite command from the repository root:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

## Configure

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

The external engine is the CMake source and `TaintedGrailModdingEditor` is the product project.

## Build

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --engine-root ..\o3de `
  --build-dir ..\foa-build\tg-sdk-developer-preview-0-windows-profile
```

Use focused targets while developing. Build the complete host only when the changed surface requires it.

## Validate

Choose the required layer from `CI_AND_LOCAL_VALIDATION.md`.

A static pass:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going --static-only --skip-source-policy
```

A broad compiled local pass, when applicable:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --engine-root ../o3de \
  --ctest-build-dir ../foa-build/tg-sdk-developer-preview-0-windows-profile
```

Prefer the focused unit/compiled target for the code you changed before running broader suites.

## Source organization

The required product foundation remains under `Gems/TaintedGrailModdingSDK/`.

Use existing ownership boundaries:

- Core value/contracts and pure domain logic in Core-owned sources;
- Framework orchestration and persistence in Framework-owned services;
- Editor/Qt behavior in Editor-owned sources;
- optional systems beneath `Plugins/`;
- external-tool provider contracts in `Gems/ExternalToolchain`;
- runtime adapters beneath their separately governed plug-in paths.

Do not put persistence or execution side effects into UI classes.

## Public models and durable formats

When adding or changing a durable/reflected model:

1. preserve stable identity/type IDs;
2. use an explicit schema/serialization version;
3. define migration or unsupported-version rejection;
4. preserve exact native references;
5. validate path and ownership boundaries;
6. document the format;
7. add round-trip and malformed-input coverage.

Significant schema/persistence changes require a reviewed design before implementation.

## Importers and evidence

Importers must declare bounded source kinds, size/resource limits, exact profile binding, deterministic fingerprint behavior, and issue reporting. They may produce source/evidence candidates but must not silently promote catalog records or runtime permission.

## UI changes

Keep domain logic in services, use stable pane identities, provide accessible/actionable UI, and update registration/manifest ownership explicitly.

UI behavior needs L3 evidence only when the change can affect actual interaction/rendering.

### Pack Manager shutdown acceptance (Windows)

The pinned Editor's main-window close handler calls `ClosePanesWithRollback`,
which sends the normal close event to Pack Manager. Its existing draft guard
therefore protects both pane close and Editor exit. Keep this host route covered
when changing draft tracking, saving, pane registration, or shutdown integration.

With a built SDK Editor and a prepared engine-asset cache, run from the product
checkout:

```powershell
./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_pack_editor_exit_smoke.ps1 -EditorExecutable <build>/bin/profile/Editor.exe -EngineRoot <pinned-engine> -CacheRoot <prepared-cache> -OutputRoot <fresh-external-output>
```

The runner checks the engine pin, creates synthetic workspaces, and starts nine
separate Editor processes with normal modal dialogs and NullRenderer. It exercises
File > Exit, the main-window Close button, and the ordinary Python exit command;
Cancel/Escape/prompt dismissal, invalid input and a real locked-file save failure;
save retry, new/saved drafts, discard, clean exit, and three fresh-process reopens.

A passing result requires both the in-process assertions and an actual process
exit code of zero with an about-to-quit notification. A timeout or forced test
cleanup is a failure. Inspect `suite-result.json`, each `process-result.json`,
and the saved prompt/draft images under the output directory. The temporary
workspace, project user/log data, and environment are isolated; Qt uses normal
desktop layout preferences. Startup may read installed-game discovery metadata;
game files and saves are not modified.

The existing `pack_unsaved_live_smoke.py` separately covers New/Open and pane-close
regressions. These are Editor authoring checks; they do not prove game runtime,
forced-termination recovery, or another pane's unsaved-state handling.

### Pack Manager workspace-switch acceptance (Windows)

Foundation owns workspace replacement. `LoadWorkspace` builds and validates its
candidate before calling `FoundationNotifications::CanChangeWorkspace` on trusted
host handlers. Each handler sees the original service/workspace and may save a draft.
The first veto stops the switch; nested LoadWorkspace/SetWorkspace requests fail
closed during admission and publication. With no draft owner, loading works normally.
`OnWorkspaceChanged` runs only after publication, including same-workspace reloads,
and Pack Manager resets its form/baseline from the new profile.

These are synchronous Editor-thread host APIs, not ExtensionAPI. Handlers must
ignore service instances they do not own. Admission must not clear an unsaved draft:
another handler may still veto. Discard authorizes replacement; it does not reset the
form until the commit notification. A completed Save remains saved if a later owner
vetoes. `SetWorkspace` now returns success; existing call statements remain valid.
`LoadWorkspace` retains bool/error reporting and adds an optional `bool* cancelled`
output, initialized on each call. The two opening panes use it to avoid showing a
load-error dialog for Cancel or failed draft Save; Pack Manager retains its save
error. No serialized schema, extension contract, stable ID or migration changes.

Run with a built SDK Editor and a prepared engine-asset cache:

```powershell
./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_pack_workspace_switch_smoke.ps1 -EditorExecutable <build>/bin/profile/Editor.exe -EngineRoot <pinned-engine> -CacheRoot <prepared-cache> -OutputRoot <fresh-external-output>
```

The runner opens two compiled Editor processes, one per workspace-opening pane.
Each uses two synthetic roots and the actual Qt file picker and Save/Discard/Cancel
dialog. It checks Cancel/Escape/dismissal, cancelling the picker, invalid destination,
invalid input, actual locked-file save failure, retry, old-root writes, new/saved
drafts, new-profile baselines, saved-mod reopening and same-workspace reloads.
It requires passing assertions, an about-to-quit signal and exit code zero; a timeout
or forced cleanup fails. Inspect suite/process/result JSON and prompt/draft screenshots.
The picker uses Qt's non-native dialog in the test process; production dialog
settings are unchanged. Catalog Browser's remembered workspace setting is restored.
Other panes' drafts and local setup's game-profile detection are separate scopes.

## Runtime and external operations

Editor contracts, previews, plans, hashes, receipts, and research are not runtime authority. Process execution, deployment, saves, runtime adapters, signing, and publication are Critical/Runtime work and use their specific designs and L4 evidence.

## Commit and pull request workflow

1. define/classify the change;
2. inspect the owner surface and existing tests;
3. implement on a focused non-`main` branch;
4. run applicable validation;
5. review the complete diff;
6. create DCO-signed commits;
7. open a pull request to `main`;
8. resolve blocking review findings and required checks;
9. leave final merge to the maintainer unless explicitly authorized otherwise.

Example:

```shell
git diff --check
git status
git commit -s -m "Fix concise behavior"
```

## Debugging

Useful first checks:

- verify `o3de.lock.json` matches the selected external engine checkout;
- confirm source manifest/build-target ownership;
- inspect O3DE Editor logs for module/pane registration problems;
- isolate serialization failures with a minimal document;
- verify workspace-root containment and exact profile/fingerprint binding;
- confirm compiled test selection is non-zero;
- separate product defects from missing host/runtime evidence.
