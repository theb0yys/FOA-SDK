# Developer Preview Exact-Head Verification

## Purpose

This runbook coordinates the repository's existing exact-head build, compiled-test,
validation-receipt, and Windows UI-evidence process. It does not define a second
acceptance standard. `developer_preview_verification.py` delegates to
`developer_preview.py`, `run_local_validation.py`, `validation_receipt.py`, CTest,
and `developer_preview_ui_evidence.py`.

The coordinator does not capture screenshots, automate Editor input, launch FoA,
invoke BepInEx or Harmony, deploy files, load keys, sign or verify data, upload
evidence, or modify saves.

## Required environment and synchronized refs

Use a clean Windows x64 checkout at the exact commit under review. The supported
configuration is Profile with Visual Studio 2022 C++ tools, CMake, Python, Git, and
Git LFS available.

Run from the repository root:

```powershell
git fetch --prune origin
git checkout FOA-plug-in-development
git pull --ff-only
git status --short
git rev-parse HEAD
```

`git status --short` must print nothing. By default, the coordinator requires both
`origin/main` and `origin/foa-development` to be ancestors of the exact HEAD. It
uses `origin/main` as the committed review range base. A stale or diverged branch
is rejected before evidence directories or build output are created.

Use `--base-ref` to select a different reviewed base and repeat `--sync-ref` only
for a deliberately reviewed alternative branch topology.

## Storage defaults

For commit `<head>`, the coordinator uses:

```text
build/tg-sdk-developer-preview-0-windows-profile
../tg-sdk-exact-head-receipt-<first-12-characters-of-head>
build/tg-sdk-developer-preview-0-ui-evidence-<first-12-characters-of-head>
build/tg-sdk-developer-preview-0-verification-<first-12-characters-of-head>.json
build/tg-sdk-developer-preview-0-verification-<first-12-characters-of-head>-prerequisites.json
```

The prerequisite result is deliberately outside the O3DE build directory. It
cannot make a fresh unconfigured build directory non-empty before CMake runs.

The validation receipt and captured gate logs stay outside the repository. UI
evidence, prerequisite output, and coordinator status may live inside the checkout
only beneath `build/`. Do not commit receipts, logs, screenshots, generated build
output, prerequisite JSON, or verification-state JSON.

Custom paths are supported with `--build-dir`, `--receipt-dir`,
`--ui-evidence-dir`, and `--state-output`. A new
exact-head run requires absent or empty receipt and UI-evidence directories;
existing evidence is never overwritten.

## 1. Inspect the exact command plan

This prints every established command without running O3DE, validation, CTest, or
UI verification:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py plan `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

The plan includes the exact resolved `origin/main` commit used by:

```text
git diff --check <resolved-origin-main-commit> HEAD
```

That checks committed changes in the reviewed range. It does not mistake an empty
clean-working-tree diff for committed whitespace validation.

## 2. Prepare exact-head evidence directories

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py prepare `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

`prepare` first runs the existing Developer Preview prerequisite command and writes
its prerequisite result beside the coordinator state under `build/`. Only after
required prerequisites pass does it initialize:

- the external exact-head `validation_receipt.py` directory;
- the commit-bound `developer_preview_ui_evidence.py` directory.

No build or UI pass is claimed by preparation. No screenshot is captured.

## 3. Run and record all automated gates

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py automated
```

Before running a gate, the coordinator invokes the authoritative receipt verifier.
It then rechecks prerequisites and uses `validation_receipt.py record` to execute
and capture these mandatory gates in order:

1. `git-diff-check` — committed review-range `git diff --check`;
2. `local-validation` — `run_local_validation.py --keep-going`;
3. `o3de-configure` — approved Windows x64 Developer Preview configure;
4. `o3de-build` — Profile `Editor` and
   `TaintedGrailModdingSDK.Catalog.Tests`;
5. `compiled-tests` — CTest filtered to
   `TaintedGrailModdingSDK.Catalog.Tests` with output on failure.

The command stops at the first non-zero result and preserves its exit code. A
failed recorded gate requires a new receipt after remediation. Only gates already
verified as passed are skipped when an unfinalized run resumes.

## 4. Inspect current progress

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py status
```

The report shows exact HEAD, synchronized refs, review base, evidence paths, gate
metadata, and the next honest action. Raw JSON metadata is never enough to report
completion:

- the authoritative receipt verifier checks schema, commands, timings, logs, hashes,
  exact commit, and finalization rules;
- the authoritative Windows UI evidence verifier checks the exact commit,
  checklist, PNG coverage and hashes, privacy declarations, and attestations;
- merge-ready completion requires both verifiers to pass.

A tampered log, receipt, screenshot, or UI document is reported as not verified,
even when its raw status field says `passed`.

## 5. Perform the Windows twenty-two-pane pass

Follow [Windows Manual UI Smoke](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md) using the
commit-specific UI-evidence directory printed by the coordinator.

All twenty-two TG SDK panes must open from **Tools → Tainted Grail SDK**, including
**Tainted Grail Release Signing Results**. Confirm the documented synthetic zero
state, non-editable behavior, exact supplied metadata, `contract status=not
evaluated`, transient reset behavior, and complete no-operation boundary.

Record all nine checklist items, attach the seven required reviewed PNG coverage
groups, and complete the final attestation. Screenshot capture and privacy review
remain manual; the tools do not inspect screenshot pixels or upload anything.

## 6. Verify UI evidence and finalize the receipt

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py finalize
```

For an unfinalized receipt, this records the real UI verifier as the `windows-ui`
gate, finalizes the receipt, verifies it as merge-ready, and prints its summary.

For an already-finalized receipt, `finalize` is not a no-op. It reruns the
authoritative Windows UI evidence verifier, the merge-ready receipt verifier, and
the receipt summary. Deleted or modified logs and screenshots therefore fail
instead of being presented as complete.

A successful result means:

- all five mandatory automated gates executed with exit code zero;
- the twenty-two-pane Windows evidence verifier passed for the same commit;
- the receipt was finalized and verified as merge-ready;
- captured logs and screenshot hashes remain available for review.

## Recovery and reruns

Do not edit receipt JSON, captured logs, UI JSON, or screenshot hashes by hand.

- A changed source commit or synchronized base requires new commit-specific evidence.
- A failed recorded gate requires a new receipt after remediation.
- An incomplete UI checklist may continue in the same unfinalized evidence directory.
- A finalized receipt remains immutable and is reverified rather than rewritten.
- An old or nonempty default evidence directory is rejected instead of overwritten.
- A branch behind either required synchronization ref is rejected before execution.
