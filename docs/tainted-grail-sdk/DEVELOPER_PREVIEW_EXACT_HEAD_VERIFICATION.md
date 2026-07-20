# Developer Preview Exact-Head Verification

## Purpose

This runbook consolidates the repository's existing exact-head build, compiled-test,
validation-receipt, and Windows UI-evidence process. It does not define a second
acceptance standard. `developer_preview_verification.py` only coordinates the
established commands in `developer_preview.py`, `run_local_validation.py`,
`validation_receipt.py`, CTest, and `developer_preview_ui_evidence.py`.

The coordinator does not capture screenshots, automate Editor input, launch FoA,
invoke BepInEx or Harmony, deploy files, load keys, sign or verify data, upload
evidence, or modify saves.

## Required environment

Use a clean Windows x64 checkout at the exact commit under review. The supported
configuration is Profile, with Visual Studio 2022 C++ tools, CMake, Python, Git,
and Git LFS available as described in [Developer Preview 0](DEVELOPER_PREVIEW_0.md).

Run every command from the repository root. Before starting:

```powershell
git checkout FOA-plug-in-development
git pull --ff-only
git status --short
git rev-parse HEAD
```

`git status --short` must print nothing. The coordinator derives the full
40-character commit from Git and refuses a dirty tree or a receipt from another
head.

## Storage defaults

For commit `<head>`, the coordinator uses:

```text
build/tg-sdk-developer-preview-0-windows-profile
../tg-sdk-exact-head-receipt-<first-12-characters-of-head>
build/tg-sdk-developer-preview-0-ui-evidence-<first-12-characters-of-head>
build/tg-sdk-developer-preview-0-verification-<first-12-characters-of-head>.json
```

The validation receipt and captured gate logs stay outside the repository. UI
evidence and coordinator status may live inside the checkout only beneath
`build/`. Do not commit receipts, logs, screenshots, generated build output, or
verification-state JSON.

Custom paths are supported with `--build-dir`, `--receipt-dir`,
`--ui-evidence-dir`, and `--state-output`. A new exact-head run requires absent or
empty receipt and UI-evidence directories; existing evidence is never overwritten.

## 1. Inspect the exact command plan

This prints every established command without running O3DE, validation, CTest, or
UI verification. It writes only the machine-readable plan state beneath `build/`.

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py plan `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

Use the actual Windows version and display scale. The display scale must be from
100 through 200 percent.

## 2. Prepare exact-head evidence directories

`prepare` first runs the existing Developer Preview prerequisite command. Only
after required prerequisites pass does it initialize:

- the external exact-head `validation_receipt.py` directory;
- the commit-bound `developer_preview_ui_evidence.py` directory.

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py prepare `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

No build or UI pass is claimed by preparation. No screenshot is captured.

## 3. Run and record all automated gates

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py automated
```

The coordinator rechecks prerequisites, then uses `validation_receipt.py record`
to execute and capture these mandatory gates in the established order:

1. `git-diff-check` — `git diff --check`;
2. `local-validation` — `run_local_validation.py --keep-going`;
3. `o3de-configure` — the approved Windows x64 Developer Preview configure command;
4. `o3de-build` — Profile `Editor` and
   `TaintedGrailModdingSDK.Catalog.Tests` targets;
5. `compiled-tests` — CTest filtered to
   `TaintedGrailModdingSDK.Catalog.Tests` with output on failure.

The local-validation and compiled-test gates remain separate so the receipt proves
both the repository-owned static/Python gate and the production-linked compiled
suite without treating one as a substitute for the other.

The command stops at the first non-zero result and preserves that exit code. A
failed recorded gate requires a new receipt directory after the cause is fixed;
the receipt tool does not rewrite or reinterpret failed evidence. Already passed
gates may be resumed in the same unfinalized exact-head receipt.

## 4. Inspect current progress

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py status
```

The status report shows:

- current exact HEAD and evidence paths;
- every receipt gate and its recorded state;
- Windows checklist pass, pending, blocked, and failed counts;
- attached screenshot count;
- the next honest action.

It never converts pending UI evidence, a missing gate, or an unfinalized receipt
into a pass.

## 5. Perform the Windows twenty-two-pane pass

Follow [Windows Manual UI Smoke](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md) using the
commit-specific UI-evidence directory printed by the coordinator.

All twenty-two TG SDK panes must open from **Tools → Tainted Grail SDK**, including
**Tainted Grail Release Signing Results**. Confirm the documented synthetic zero
state, non-editable behavior, exact supplied metadata, `contract status=not
evaluated`, transient reset behavior, and the complete no-operation boundary.

Record all nine checklist items with `developer_preview_ui_evidence.py record`,
attach the seven required reviewed PNG coverage groups, and complete the final
attestation. Screenshot capture and privacy review remain manual; the tools do not
inspect screenshot pixels or upload anything.

Use `status` at any point to see the remaining checklist IDs.

## 6. Verify UI evidence and finalize the receipt

After every manual checklist item is `pass`, all required screenshots are attached,
and the attestation is complete:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py finalize
```

`finalize` does not trust the UI document merely because it exists. It records the
real `developer_preview_ui_evidence.py verify` command as the `windows-ui` gate,
then runs the existing receipt `finalize`, merge-ready `verify`, and Markdown
`summarize` commands against the same exact HEAD.

A successful result means:

- all five mandatory automated gates executed with exit code zero;
- the twenty-two-pane Windows evidence verifier passed for the same commit;
- the receipt was finalized and verified as merge-ready;
- captured logs and screenshot hashes remain available for review outside tracked
  source.

## Recovery and reruns

Do not edit receipt JSON, captured logs, UI JSON, or screenshot hashes by hand.

- A changed source commit requires new commit-specific receipt and UI-evidence
  directories.
- A failed recorded gate requires a new receipt directory after remediation.
- An incomplete UI checklist may continue in the same unfinalized exact-head
  evidence directory.
- A finalized receipt is immutable; use `status` to inspect it rather than rerun
  finalization.
- An old or nonempty default evidence directory is rejected instead of overwritten.

These rules preserve the repository's existing fail-closed evidence model while
making the operator sequence reproducible for the evening Windows verification
session.
