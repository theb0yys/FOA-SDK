# Developer Preview 0 Windows Manual UI Smoke and Screenshot Evidence

Status: checklist and evidence tooling implemented; the actual Windows screenshot pass remains pending.

## Purpose

This checklist proves the source-built TG SDK panes are usable in a **real Windows x64 Profile** Editor session. It complements automated and compiled tests and does not prove FoA runtime compatibility.

Evidence binds to the **exact source commit** under review. Historical contracts required **All eight TG SDK panes**, **All nine TG SDK panes**, **All ten TG SDK panes**, **All eleven TG SDK panes**, **All twelve TG SDK panes**, **All thirteen TG SDK panes**, **All fourteen TG SDK panes**, and **All fifteen TG SDK panes** as earlier slices landed. Slice 15 requires **All sixteen TG SDK panes**. Slice 16 requires **All seventeen TG SDK panes**. Slice 17 requires **All eighteen TG SDK panes**. Slice 18 requires **All nineteen TG SDK panes**. The release-artifact provenance/signing-intent slice requires **All twenty TG SDK panes**. The release-assembly/checksum-result slice requires **All twenty-one TG SDK panes**. The release-signing result slice requires **All twenty-two TG SDK panes**.

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

The normal fixture has no adapter declaration, runtime-result envelope, package-preview input, staging/deployment-preview input, deployment confirmation/work-order input, deployment execution-result envelope, independent-verifier envelope, verifier-reconciliation request, release-artifact envelope, release assembly-result envelope, or release signing-result envelope. It therefore has no post-deployment report, independent-verifier result, release-decision reconciliation, release-artifact metadata, archive/checksum result evidence, or release-signing result evidence. Expected defaults are:

- the Adapter Capability Matrix shows `unsupported` rows when no adapter declaration is registered;
- Work-Order Plans shows **one refused plan group and zero generated steps** per loaded pack;
- Runtime Result Evidence shows zero registered envelopes;
- Adapter Build Manifests shows **zero ready build definitions**;
- Package Assembly Preview shows **zero registered package-preview inputs**;
- Staging and Deployment Preview shows **zero registered staging/deployment-preview inputs**;
- Deployment Confirmation and Work Orders shows **zero registered deployment confirmation/work-order inputs**;
- Deployment Execution Result Evidence shows **zero registered deployment execution-result envelopes**;
- Post-Deployment Verification and Release Blockers shows **zero reports**;
- Independent Post-Deployment Verifier Results shows **zero registered verifier envelopes**;
- Verifier Evidence Reconciliation and Release Decision shows **zero registered reconciliation requests**;
- Release Artifact Provenance and Signing Intent shows **zero registered release-artifact envelopes**;
- Release Assembly and Checksum Results shows **zero registered release assembly-result envelopes**;
- Release Signing Results shows **zero registered release signing-result envelopes**.

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
- Tainted Grail Staging and Deployment Preview;
- Tainted Grail Deployment Confirmation and Work Orders;
- Tainted Grail Deployment Execution Result Evidence;
- Tainted Grail Post-Deployment Verification and Release Blockers;
- Tainted Grail Independent Post-Deployment Verifier Results;
- Tainted Grail Verifier Evidence Reconciliation and Release Decision;
- Tainted Grail Release Artifact Provenance and Signing Intent;
- Tainted Grail Release Assembly and Checksum Results;
- Tainted Grail Release Signing Results.

Confirm every pane opens and remains interactive. **All twenty-two TG SDK panes** must be present.

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py record `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --check all-panes-open `
  --status pass `
  --notes "All twenty-two TG SDK panes opened from the Tools menu."
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
- **Tainted Grail Deployment Confirmation and Work Orders** shows **zero registered deployment confirmation/work-order inputs**;
- confirmation/work-order columns expose exact preview and pack identity, named reviewer, typed scope and expiry, maintenance window, status, typed steps, operator checklist, blockers, prohibited permissions, and canonical JSON;
- **Tainted Grail Deployment Execution Result Evidence** shows **zero registered deployment execution-result envelopes**;
- deployment execution-result columns expose result/work-order identity, reviewed executor, attempted steps, outcomes, observed fingerprints, backup results, target verification, rollback/restore outcomes, failures/logs, candidate evidence counts, and contract issues;
- **Tainted Grail Post-Deployment Verification and Release Blockers** shows **zero reports**;
- post-deployment report columns expose result/report/work-order identity, exact profile/game/branch/runtime context, candidate source/evidence IDs and counts, step outcomes, target-verification states, rollback completeness, failures/diagnostics, compatibility blockers, and release blockers;
- **Tainted Grail Independent Post-Deployment Verifier Results** shows **zero registered verifier envelopes**;
- independent-verifier columns expose verifier-result/report identity, structural status, reviewed verifier/capabilities, exact expected checks, independently supplied observations, failures/diagnostics, candidate evidence counts, exact context, safety boundary, and issues;
- **Tainted Grail Verifier Evidence Reconciliation and Release Decision** shows **zero registered reconciliation requests**;
- reconciliation columns expose exact reconciliation/report/verifier/execution/work-order identity, contract status, separate compatibility assessment, release decision, human-review state, preserved/adverse/contradictory findings, disposition coverage, blocker counts, candidate evidence counts, exact pack/preview/target context, safety boundary, and issues;
- **Tainted Grail Release Artifact Provenance and Signing Intent** shows **zero registered release-artifact envelopes**;
- release-artifact columns expose exact reconciliation/package/manifest/pack binding, content paths/roles/media types/sizes, declared SHA-256 values, provenance, legal dispositions, signing intent, publication targets, status, safety flags, and blockers;
- **Tainted Grail Release Assembly and Checksum Results** shows **zero registered release assembly-result envelopes**;
- release assembly-result columns expose exact artifact/result fingerprints, reviewed assembler/checksummer identity, archive attempt/outcome/path/size/fingerprint, per-content expected/observed checksums and outcomes, failures, safe diagnostics, exact release context, and the SDK safety boundary;
- **Tainted Grail Release Signing Results** shows **zero registered release signing-result envelopes**;
- release signing-result columns expose exact result/artifact/assembly/archive fingerprints and context, supplied signer review/tool/version/fingerprint/reviewer/timestamp/capabilities/evidence, approved signing identity, attempted outcome/completion/capture time, signature artifacts, failures/retryable state, safe diagnostics, `contract status=not evaluated`, and explicit SDK no-operation state;
- every analysis/planning/result/build/package/deployment/work-order/evidence/report/verifier/reconciliation/release-artifact/release-assembly/release-signing pane is **non-editable** and exposes no registration, review authoring, acknowledgement, import, promotion, save/export, compiler, executor, verifier, target-filesystem access, file read/hash/copy, checksum generation, replace, delete, backup, restore, archive access, key loading, identity resolution, signing, signature verification, signature-artifact writing, upload, publication, deploy, launch, execution, or save-mutation action.

Record under `preview-data-displayed`. Screenshot required.

### 5. Item and Recipe Editor

Confirm typed items/recipe, joins, station/unlock references, and read-only action lanes. Screenshot required.

### 6. Station visibility and learnability

Confirm resolved and intentionally unresolved station/learnability rows, evidence, governance, blockers, and reasons. Screenshot required.

### 7. Save, close, reopen

Perform the full **save, close, reopen** sequence. Durable workspace, pack, source/evidence, catalog, governance, economy, and derived analysis state must survive. Transient adapter, runtime-result, package-preview, staging/deployment-preview, deployment-work-order, deployment-execution-result, independent-verifier, verifier-reconciliation, release-artifact, release-assembly-result, and release-signing-result registries and derived post-deployment reports must reset; no adapter/work-order/runtime-result/build-manifest/staging-inventory/package-preview/target-inventory/deployment-preview/backup/rollback/confirmation/window/preflight/checklist/deployment-execution-result/post-deployment-report/independent-verifier-result/verifier-reconciliation/release-artifact/release-assembly-result/release-signing-result file may appear. Screenshot required.

### 8. Actionable failure message

Trigger a safe reversible validation failure. Confirm the **failure message** identifies the affected subject/path without exposing private data. Screenshot required.

### 9. Runtime/deployment boundary absent

Confirm no runtime, deployment, injection, save mutation, duplicate merge, pack rejection, winner selection, adapter loading, work-order execution, result promotion, executor invocation, independent verifier execution or target access, release-review authoring, file read/hash/copy, checksum generation, archive creation/opening, key or credential loading, identity resolution/authentication, signing, signature verification, signature-artifact copying/writing, upload, publication, compiler, replace/delete, backup/restore, package assembly, deployment-directory mutation, checklist acknowledgement, rollback execution, automatic evidence promotion, release action, or launch action exists. Record under `runtime-deployment-absent`.

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

Verified evidence may be attached privately to a review/release record and **must not be committed**. Record exact source commit, Windows version/scale, tester alias, screenshot hashes, verifier result, reconciliation result, release-artifact result, release-assembly result, release-signing result, limitations, and blocked observations.
