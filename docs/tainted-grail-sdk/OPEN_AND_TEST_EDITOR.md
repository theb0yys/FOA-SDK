# Open and Test the Actual Editor

This is the shortest supported path from a fresh Windows checkout to the real
source-built O3DE Editor with the `TaintedGrailModdingSDK` Gem loaded.

The repository-owned `AutomatedTesting` project enables the Gem in its committed
`project.json`. Do not run a local `enable-gem` command for this path.

## Requirements

- Windows x64;
- Visual Studio 2022 or Build Tools with **Desktop development with C++**;
- Python 3.10 or newer;
- CMake 3.23 or newer;
- Git and Git LFS.

Run every command from the repository root.

## 1. Prepare the checkout

```powershell
git checkout foa-development
git pull --ff-only
git lfs install
git lfs pull
```

After the pull, confirm the integration contract:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_project.py
```

The command must report that `AutomatedTesting` enables
`TaintedGrailModdingSDK`.

## 2. Check prerequisites

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

Resolve every required failure before continuing.

## 3. Configure and build

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile

python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The actual Editor executable is expected at:

```text
build/tg-sdk-developer-preview-0-windows-profile/bin/profile/Editor.exe
```

## 4. Run the focused tests

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

## 5. Open the actual Editor

Use the repository-owned opener:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py
```

It always selects:

```text
AutomatedTesting
```

and delegates process handling to the reviewed launch wrapper. Optional examples:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py --dry-run

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile `
  --log-dir build/tg-sdk-developer-preview-0-launch
```

After the Editor opens, use:

```text
Tools → Tainted Grail SDK
```

The menu contains:

- Tainted Grail SDK Status;
- Tainted Grail Pack Manager;
- Tainted Grail Source Intake;
- Tainted Grail Catalog Browser;
- Tainted Grail Catalog Governance;
- Tainted Grail Item and Recipe Editor.

The native Editor log is:

```text
AutomatedTesting/user/log/Editor.log
```

It should contain `TaintedGrailModdingSDK` and
`Editor foundation activated`.

## 6. Generate the synthetic test workspace

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

In **Tainted Grail SDK Status**, click **Open Workspace…** and select:

```text
build/tg-sdk-developer-preview-0-fixture/preview.tgworkspace.json
```

The fixture stores `RootPath` as `.` for portability. For an editable UI test
copy, set **Workspace root** to the absolute fixture directory, click
**Apply Configuration**, save the workspace, and reopen it.

In **Tainted Grail Pack Manager**, click **Open…** and select:

```text
build/tg-sdk-developer-preview-0-fixture/Packs/preview.developer-preview-0.tgpack.json
```

Then inspect the catalog, governance, Item and Recipe Editor, and
station/learnability evidence panes.

## Boundaries

This path launches only the O3DE Editor. It does not launch FoA, invoke BepInEx
or Harmony, deploy files, inject code, modify saves, collect telemetry, or
upload diagnostics or screenshots.
