# Developer Preview Exact-Head Verification

## Purpose

`developer_preview_verification.py` coordinates the existing Developer Preview,
validation-receipt, CTest, and Windows UI-evidence tools. It does not define a
second acceptance standard, capture screenshots, launch FoA, deploy files, sign
content, upload evidence, or modify saves.

A completion result is authoritative only when both existing verifiers succeed
again against the same clean exact head:

```powershell
validation_receipt.py verify --require-merge-ready
developer_preview_ui_evidence.py verify
```

Recorded JSON strings such as `passed` and a finalization timestamp are progress
metadata, not proof.

## Required inputs

Use a clean Windows x64 checkout and identify the full reviewed base commit. The
base must be an ancestor of `HEAD`, must differ from `HEAD`, and must be the base
of the exact commit range being reviewed.

```powershell
git status --short
git rev-parse HEAD
git rev-parse <reviewed-base>
```

Every coordinator command requires:

```text
--review-base <full-40-character-commit>
```

## Storage defaults

For an exact head `<head>`, the coordinator uses:

```text
release/revisions/tg-sdk-developer-preview-0-windows-profile
../tg-sdk-exact-head-receipt-<first-12-of-head>
../foa-build/tg-sdk-developer-preview-0-ui-evidence-<first-12-of-head>
../foa-build/tg-sdk-developer-preview-0-verification-<first-12-of-head>
```

The prerequisite report is written to the separate verification directory as
`prerequisites.json`. It is never written into the O3DE build directory. This
preserves the existing fail-closed rule that rejects a non-empty, unconfigured
build directory.

The receipt and captured logs stay outside the repository. UI evidence and
verification state use `../foa-build/` or another reviewed output root and must
remain outside the O3DE build directory. Do not commit generated output,
receipts, logs, screenshots, or verification state.

## 1. Inspect the command plan

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py plan `
  --review-base <reviewed-base> `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

The plan is dry-run output only.

## 2. Prepare the run

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py prepare `
  --review-base <reviewed-base> `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

Preparation checks prerequisites, then creates a new external receipt and a new
commit-bound UI-evidence directory. Existing non-empty run directories are
rejected rather than overwritten.

## 3. Record automated gates

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py automated `
  --review-base <reviewed-base>
```

A fresh receipt is required. The coordinator records these gates in order:

1. `git diff --check <reviewed-base> HEAD`;
2. `run_local_validation.py --keep-going`;
3. approved O3DE configure;
4. Profile Editor and Catalog-test build;
5. production-linked Catalog CTest.

The whitespace gate therefore checks the reviewed commits, not the empty working
copy diff of an already-clean tree. The exact range command is retained in the
receipt and must still match the requested base before finalization or completion.

A failed gate requires a new receipt after remediation. The coordinator does not
rewrite failed evidence or implement a second receipt-resume policy.

## 4. Inspect status

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py status `
  --review-base <reviewed-base>
```

For machine-readable output, add `--json`.

Status reruns the merge-ready receipt verifier and the UI-evidence verifier. It
reports `complete_verified: true` only when:

- the receipt verifier succeeds with `--require-merge-ready`;
- the UI-evidence verifier succeeds for the same head;
- the receipt contains exactly the requested reviewed-range whitespace command.

Gate strings read from the receipt are labelled unverified metadata. Deleted or
altered logs, changed screenshots, hash mismatches, malformed evidence, missing
attestations, and a mismatched reviewed range all prevent completion.

## 5. Complete Windows evidence

Follow [Windows Manual UI Smoke](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md). Record all
checklist items, attach the required reviewed PNG coverage, and complete the
privacy and no-operation attestations for the same exact commit.

Screenshot capture and review remain manual. The repository tools do not inspect
screenshot pixels or upload evidence.

## 6. Finalize and reverify

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py finalize `
  --review-base <reviewed-base>
```

For an unfinalized run, this records the authoritative UI verifier as the
`windows-ui` gate, finalizes the receipt, and then runs both authoritative
verifiers plus the receipt summary.

For an already-finalized receipt, `finalize` is deliberately not a successful
no-op. It reruns receipt verification, UI-evidence verification, and summary.
Both verifiers are attempted so one integrity failure does not hide the other.

## Recovery

- A changed head or reviewed base requires a new run.
- A failed recorded gate requires a new receipt.
- Incomplete UI evidence may continue before finalization.
- Never edit receipt JSON, logs, UI JSON, or hashes by hand.
- Do not represent planned, pending, raw, or previously verified metadata as a
  current pass.
