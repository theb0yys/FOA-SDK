# Developer Preview 0 Windows Manual UI Smoke and Screenshot Evidence

Status: checklist and evidence tooling implemented; the actual Windows screenshot pass remains pending.

## Purpose

This checklist proves the source-built TG SDK panes are usable in a **real Windows x64 Profile** Editor session. It complements automated and compiled tests and does not prove FoA runtime compatibility.

Evidence binds to the **exact source commit** under review. Historical contracts required **All eight TG SDK panes**, **All nine TG SDK panes**, **All ten TG SDK panes**, **All eleven TG SDK panes**, **All twelve TG SDK panes**, and **All thirteen TG SDK panes** as earlier slices landed. Slice 13 requires **All fourteen TG SDK panes**.

## Safety and privacy boundary

Use only project-owned synthetic data. Do not display or capture game files/assets/saves, credentials, private paths, unrelated desktop content, FoA, BepInEx/Harmony runtime tooling, **work-order execution, deployment, injection, or save-mutation**.

The evidence tool does not capture the screen and does not inspect screenshot pixels. Nothing is uploaded automatically. **Do not commit screenshots.** Keep evidence beneath `build/` or outside the repository.

## Required environment

- Windows x64 Profile;
- exact reviewed commit;
- successful `Editor.exe` build and launch;
- verified synthetic fixture;
- focused validators and compiled catalog tests green;
- **100–200%** display scaling;
- a project loading `TaintedGrailModdingSDK`.

Record Windows version, display scale, tester alias, exact commit, Editor launch result, and activation-log confirmation.

## Prepare and launch

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --project C:\O3DE\Projects\MyProject `
  --log-dir build/tg-sdk-developer-preview-0-launch
```

Prepare `preview.duplicate-companion`, then import `preview.evidence.duplicate.primary` and `preview.evidence.duplicate.companion` exactly as documented in `Preview/DuplicateReview/README.md`.

The normal fixture has no adapter declaration, runtime-result envelope, package-preview input, or staging/deployment-preview input. Expected defaults are:

- the Adapter Capability Matrix shows `unsupported` rows when no adapter declaration is registered;
- Work-Order Plans shows **one refused plan group and zero generated steps** per loaded pack;
- Runtime Result Evidence shows zero registered envelopes;
- Adapter Build Manifests shows **zero ready build definitions**;
- Package Assembly Preview shows **zero registered package-preview inputs**;
- Staging and Deployment Preview shows **zero registered staging/deployment-preview inputs**.

## Initialize evidence

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py init `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --source-commit <40-character-commit> `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

Initialization creates pending evidence and captures nothing.

## Manual checklist

### 1. All panes open

From **Tools → Tainted Grail SDK**, open:

- Tainted Grail SDK Status;
- Tainted Grail Pack Manager;
- Tainted Grail Source Intake;
- Tainted Grail Catalog Browser;
- Tainted Grail Catalog Governance;
- Tainted Grail Item and Recipe Editor;
- Tainted Grail Economy Acquisition Coverage;
- Tainted Grail Economy Cross-Pack Duplicates;
- Tainted Grail Adapter Capability Matrix;
- Tainted Grail Adapter Work-Order Plans;
- Tainted Grail Adapter Runtime Result Evidence;
- Tainted Grail Adapter Build Manifests;
- Tainted Grail Package Assembly Preview;
- Tainted Grail Staging and Deployment Preview.

Confirm every pane opens and remains interactive. **All fourteen TG SDK panes** must be present.

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py record `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --check all-panes-open `
  --status pass `
  --notes "All fourteen TG SDK panes opened from the Tools menu."
```

Screenshot required.

### 2. Readability at normal scaling

At **100–200%**, confirm labels, tables, fields, and primary controls remain readable without extreme resizing. Screenshot required.

### 3. Keyboard traversal

Use `Tab`, `Shift+Tab`, arrows, and activation keys. Confirm keyboard traversal reaches primary controls and focus remains visible. Notes required.

### 4. Preview data and pipeline views

Open the synthetic workspace and duplicate companion. Confirm:

- catalog/economy data and blockers are visible;
- coverage lanes are correct;
- **cross-pack duplicate candidates** include the exact `preview.duplicate-companion` subjects produced from `preview.evidence.duplicate.primary` and `preview.evidence.duplicate.companion`;
- no display-name/fuzzy candidates appear;
- the Adapter Capability Matrix shows `unsupported` rows when no adapter declaration is registered;
- Work-Order Plans shows **one refused plan group and zero generated steps** per pack;
- Runtime Result Evidence shows zero envelopes;
- Adapter Build Manifests shows **zero ready build definitions**;
- **Tainted Grail Package Assembly Preview** shows **zero registered package-preview inputs**;
- package-preview columns expose reviewed manifest, staging inventory, status, package root, derived layout/output digests, omissions, collisions, redistribution/trust blockers, and canonical JSON;
- **Tainted Grail Staging and Deployment Preview** shows **zero registered staging/deployment-preview inputs**;
- staging/deployment-preview columns expose package and target inventory identities, status, target/backup roots, additions, replacements, removals, unchanged paths, conflicts, backup requirements, rollback steps, blockers, and canonical JSON;
- every analysis/planning/result/build/package/deployment pane is **non-editable** and exposes no registration, import, save/export, compiler, copy, replace, delete, backup, restore, archive, deploy, launch, execution, or save-mutation action.

Record under `preview-data-displayed`. Screenshot required.

### 5. Item and Recipe Editor

Confirm typed items/recipe, joins, station/unlock references, and read-only action lanes. Screenshot required.

### 6. Station visibility and learnability

Confirm resolved and intentionally unresolved station/learnability rows, evidence, governance, blockers, and reasons. Screenshot required.

### 7. Save, close, reopen

Perform the full **save, close, reopen** sequence. Durable workspace, pack, source/evidence, catalog, governance, economy, and derived analysis state must survive. Transient adapter, runtime-result, package-preview, and staging/deployment-preview registries must reset; no adapter/work-order/runtime-result/build-manifest/staging-inventory/package-preview/target-inventory/deployment-preview/backup/rollback file may appear. Screenshot required.

### 8. Actionable failure message

Trigger a safe reversible validation failure. Confirm the **failure message** identifies the affected subject/path without exposing private data. Screenshot required.

### 9. Runtime/deployment boundary absent

Confirm no runtime, deployment, injection, save mutation, duplicate merge, pack rejection, winner selection, adapter loading, work-order execution, result promotion, compiler, file copy/replace/delete, backup/restore, archive creation, package assembly, deployment-directory mutation, rollback execution, or launch action exists. Record under `runtime-deployment-absent`.

## Attach reviewed screenshots

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py attach `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --screenshot C:\Evidence\tg-sdk-panes.png `
  --check all-panes-open `
  --check preview-data-displayed `
  --title "TG SDK governed authoring and previews" `
  --description "Project-owned synthetic data and fail-closed pipeline states." `
  --privacy-reviewed `
  --project-owned-only
```

Required screenshot coverage:

- `all-panes-open`;
- `normal-scaling-readable`;
- `preview-data-displayed`;
- `item-recipe-data-displayed`;
- `station-learnability-displayed`;
- `save-close-reopen`;
- `failure-message-actionable`.

## Final attestation

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py attest `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --tested-at 2026-07-19T12:00:00Z `
  --launch-exit-code 0 `
  --activation-log-confirmed `
  --reviewed-for-private-data `
  --project-owned-only `
  --no-game-files-or-saves `
  --no-credentials-or-private-paths `
  --no-runtime-or-deployment `
  --tester-confirmed
```

## Verify evidence

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py verify `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --expected-commit <40-character-commit>
```

Verification checks exact commit binding, Windows/Profile metadata, launch, checklist results, screenshot coverage, PNG integrity/dimensions/hashes, allow-listed relative paths, traversal/symlink rejection, textual privacy checks, and attestations.

Verification does not inspect screenshot pixels. Human privacy review remains mandatory.

## Evidence handling

Verified evidence may be attached privately to a review/release record and **must not be committed**. Record exact source commit, Windows version/scale, tester alias, screenshot hashes, verifier result, limitations, and blocked observations.
