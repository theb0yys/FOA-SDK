# Developer Preview 0 Windows Manual UI Smoke and Screenshot Evidence

Status: checklist and evidence tooling implemented; the actual Windows screenshot pass remains pending.

## Purpose

This checklist proves the source-built Tainted Grail SDK panes are usable in a **real Windows x64 Profile** O3DE Editor session. It complements the automated service-level persistence smoke. It does not replace compiled tests, and it does not prove FoA runtime compatibility.

The accepted evidence must be tied to the **exact source commit** being reviewed. Do not reuse screenshots from an older commit.

## Safety and privacy boundary

Use only the project-owned synthetic Developer Preview 0 fixture and the project-owned duplicate-review companion. Do not display or capture:

- proprietary game files, assets, saves, identifiers, or screenshots;
- credentials, tokens, user names, private absolute paths, or unrelated desktop content;
- FoA, BepInEx, Harmony, Avalon Core, runtime adapters, work-order execution, deployment, injection, or save-mutation tooling.

The evidence tool does not capture the screen and does not inspect screenshot pixels. The tester must review each image before attaching it.

Nothing is uploaded automatically. **Do not commit screenshots.** Keep the evidence directory beneath `build/` or outside the repository.

## Required environment

- Windows x64;
- Profile configuration;
- source commit checked out exactly;
- `Editor.exe` built successfully;
- deterministic synthetic fixture generated and verified;
- focused validators and compiled catalog tests green;
- normal Windows display scaling between **100–200%**;
- a project or engine setup that loads the `TaintedGrailModdingSDK` Gem.

Record the Windows version, display scale, tester alias, exact commit, Editor launch result, and TG SDK activation-log confirmation.

## Prepare the fixture

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

Prepare the project-owned duplicate-review companion before opening the workspace:

```powershell
Copy-Item `
  Gems/TaintedGrailModdingSDK/Preview/DuplicateReview/preview.duplicate-companion.tgpack.json `
  build/tg-sdk-developer-preview-0-fixture/Packs/preview.duplicate-companion.tgpack.json
```

The canonical fixture verifier intentionally covers the unchanged generated fixture only. The copied companion is separately repository-owned and must match the reviewed source commit. Follow `Gems/TaintedGrailModdingSDK/Preview/DuplicateReview/README.md` during the duplicate-report step.

Slice 8 does not register an adapter declaration. The expected Adapter Capability Matrix state for this manual pass is therefore the deterministic fail-closed no-declaration state.

## Launch the Editor

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --project C:\O3DE\Projects\MyProject `
  --log-dir build/tg-sdk-developer-preview-0-launch
```

Confirm the launch result is zero and the native Editor log contains the TG SDK activation message.

## Initialize the evidence directory

Use a non-personal tester alias and the exact 40-character commit SHA:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py init `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --source-commit <40-character-commit> `
  --tester-alias windows-reviewer `
  --windows-version "Windows 11 23H2" `
  --display-scale 125
```

Initialization creates a pending checklist. It does not claim a pass and does not create screenshots.

## Manual checklist

Complete every item in order.

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
- Tainted Grail Adapter Capability Matrix.

Confirm every pane opens without an error and remains interactive. **All nine TG SDK panes** must be present.

Record:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py record `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --check all-panes-open `
  --status pass `
  --notes "All nine TG SDK panes opened from the Tools menu."
```

Screenshot required.

### 2. Readability at normal scaling

At the configured **100–200%** Windows display scale, confirm:

- labels are not clipped;
- tables and form fields remain readable;
- primary buttons are visible;
- no pane requires extreme resizing to expose core controls.

Screenshot required.

### 3. Keyboard traversal

Use `Tab`, `Shift+Tab`, arrow keys, and activation keys where appropriate. Confirm keyboard traversal reaches primary controls in each pane and the current focus is visible.

A screenshot is not mandatory because focus traversal is temporal, but detailed notes are required.

### 4. Preview workspace, economy analyses, and adapter readiness

Open the generated synthetic workspace after the companion pack has been copied. Confirm both owner packs load:

- `preview.developer-preview-0`;
- `preview.duplicate-companion`.

Import `Gems/TaintedGrailModdingSDK/Preview/DuplicateReview/preview-duplicate-source.json` through **Tainted Grail Source Intake** using the structured JSON importer. Then promote the two project-owned claims exactly as documented in `Preview/DuplicateReview/README.md`:

- `preview.evidence.duplicate.primary` → `preview.item.duplicate.primary`, owner `preview.developer-preview-0`;
- `preview.evidence.duplicate.companion` → `preview.item.duplicate.companion`, owner `preview.duplicate-companion`;
- domain `economy`, kind `item`, subject `subject:preview:item:duplicate-review`, identity `synthetic` for both.

Confirm:

- the active preview profile is visible;
- source and evidence documents are present;
- the catalog shows the expected synthetic records and relationships;
- blocked, stale, allowed, forbidden, and unresolved states are distinguishable;
- the Economy Acquisition Coverage pane lists canonical economy items and recipes;
- vendor, loot, reward, learnability, and crafting columns show only applicable lanes;
- relationship, evidence, blocker, and reason details are visible without an editing control;
- the Economy Cross-Pack Duplicates pane displays one exact `subject_ref` group for the two promoted cross-pack duplicate candidates;
- the group shows `partial`, because neither promoted candidate has a typed item profile;
- exact signal, exact match key, owner packs, canonical records, evidence, blockers, and status are visible;
- same-pack repeats, case-different keys, and display-name similarity do not appear as duplicate groups;
- the Adapter Capability Matrix lists all eleven typed capabilities for each loaded pack;
- the matrix shows **unsupported rows when no adapter declaration is registered**;
- required adapter version, active runtime target, capability, status, and actionable reason are visible;
- no adapter declaration, registration, work-order, execution, deployment, launch, or save control exists;
- all three analysis panes remain **non-editable**.

Record this observation under `preview-data-displayed`. Screenshot required.

### 5. Item and Recipe Editor

Confirm the Item and Recipe Editor displays:

- two original synthetic item profiles;
- one original synthetic recipe profile;
- the ingredient and output joins;
- the expected station and unlock references;
- read-only action-lane status.

The two duplicate-review records intentionally have no typed profiles and must not be mistaken for the two original typed items.

Screenshot required.

### 6. Station visibility and learnability

Select the synthetic recipe and confirm the station visibility and learnability rows show:

- the resolved workbench association;
- the resolved learnability source;
- the explicitly unresolved learnability source;
- status, governance, evidence, blockers, and reasons.

Screenshot required.

### 7. Save, close, reopen, and reload

This step proves the full save, close, reopen sequence on the accepted commit.

Save durable documents, close the Editor, relaunch the same commit, reopen the workspace, and reload the catalog.

Confirm the reviewed state survives:

- workspace and profile;
- both packs;
- source and evidence;
- catalog records and relationships;
- governance and validation history;
- economy profiles and joins;
- station and learnability evidence rows;
- derived economy acquisition coverage rows and statuses;
- the derived partial cross-pack duplicate group, exact key, candidate health, and status.

Also confirm the Adapter Capability Matrix returns to the same deterministic `unsupported` state after relaunch because its declaration registry is transient and no declaration is registered.

Screenshot required after reopen.

### 8. Actionable failure message

Trigger one safe, reversible validation failure using synthetic data, such as an invalid path or a missing synthetic subject reference.

Confirm the failure message identifies the affected path or subject and does not expose a private path or secret.

Screenshot required.

### 9. Runtime boundary remains absent

Inspect every TG SDK pane and confirm no runtime, deployment, injection, save-mutation, work-order generation/execution, adapter loading/registration, automatic duplicate merge, pack rejection, or winner-selection action is exposed.

Record detailed notes. A screenshot is optional because absence is established across the full pass, not by a single image.

## Attach reviewed screenshots

Capture screenshots manually with Windows tools. Crop to the O3DE Editor content when practical. Review each image before attaching it.

A single screenshot may cover multiple checks:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py attach `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --screenshot C:\Evidence\catalog-and-governance.png `
  --check all-panes-open `
  --check preview-data-displayed `
  --title "Catalog, economy analysis, and adapter-capability panes" `
  --description "Project-owned synthetic preview data and fail-closed adapter rows in the O3DE Editor." `
  --privacy-reviewed `
  --project-owned-only
```

The tool accepts PNG files only, requires at least 800×600 pixels, records dimensions, size, and SHA-256, and copies the reviewed image beneath `screenshots/`.

Required screenshot coverage:

- `all-panes-open`;
- `normal-scaling-readable`;
- `preview-data-displayed`;
- `item-recipe-data-displayed`;
- `station-learnability-displayed`;
- `save-close-reopen`;
- `failure-message-actionable`.

## Final attestation

After every checklist item is recorded as `pass` and every image has been reviewed:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py attest `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --tested-at 2026-07-18T12:00:00Z `
  --launch-exit-code 0 `
  --activation-log-confirmed `
  --reviewed-for-private-data `
  --project-owned-only `
  --no-game-files-or-saves `
  --no-credentials-or-private-paths `
  --no-runtime-or-deployment `
  --tester-confirmed
```

## Verify the evidence

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py verify `
  --output build/tg-sdk-developer-preview-0-ui-evidence `
  --expected-commit <40-character-commit>
```

Verification checks:

- exact commit binding;
- Windows x64 Profile metadata;
- successful Editor launch and activation-log confirmation;
- every checklist item recorded as `pass`;
- required screenshot coverage;
- PNG signature, dimensions, sizes, and SHA-256 hashes;
- relative paths, exact file allow-list, traversal rejection, and symlink rejection;
- textual metadata for private paths and secret-like material;
- disclosure and runtime-boundary attestations.

Verification does not inspect screenshot pixels. A successful result therefore requires both machine checks and the tester’s privacy review.

## Evidence handling

The evidence directory may be attached privately to a review or release record after verification and human review. It must not be committed to the repository.

The PR or release record should state:

- exact source commit;
- Windows version and display scale;
- tester alias;
- checklist result;
- screenshot count and hashes;
- evidence verifier result;
- any limitations or blocked observations.

A checklist failure or privacy concern blocks acceptance. Correct the issue, start a new evidence directory, and repeat the pass.
