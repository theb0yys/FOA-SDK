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
