# Open and Test the Actual Editor

This is the supported Windows path from a fresh checkout to the dedicated,
source-built **Tainted Grail Modding Editor**.

The editor is an O3DE project named `TaintedGrailModdingEditor`. It is separate
from `AutomatedTesting`, enables `TaintedGrailModdingSDK` and
`ExternalToolchain` directly, and owns the project and Windows shortcut icons.
The project also enables O3DE's `Atom` and `DiffuseProbeGrid` Editor host Gems.
Its `ShaderLib` supplies the required empty Scene and View SRG extensions so
Atom can compile the project shader registry.

## Requirements

- Windows x64;
- Visual Studio 2022 or Build Tools with **Desktop development with C++**;
- Python 3.10 or newer;
- CMake 3.23 or newer;
- Git and Git LFS.

Run every command from the repository root.

## 1. Prepare the checkout

```powershell
git checkout main
git pull --ff-only
git lfs install
git lfs pull
```

Confirm the dedicated project and canonical path-policy contracts:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_project.py
python Gems/TaintedGrailModdingSDK/Tools/validate_path_policy.py
```

The commands must report that the dedicated project, icons, opener, persistence
boundaries, repository-owned path policy, and repository-derived clickable-entry
trust contract are complete.

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

The supported clickable entry accepts only that exact configured build
directory. Its `CMakeCache.txt` must identify the current repository as
`CMAKE_HOME_DIRECTORY`, and the target must have a valid x64 PE32+ Windows GUI
executable header.

The actual O3DE Editor executable is expected at:

```text
build/tg-sdk-developer-preview-0-windows-profile/bin/profile/Editor.exe
```

## 4. Create the clickable entry

Use the hardened entry command:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create
```

This creates and immediately verifies:

```text
build/Tainted Grail Modding Editor.lnk
build/Tainted Grail Modding Editor.shortcut.json
```

The trusted target, project, working directory, and icon are derived from the
repository and approved build policy. The sidecar records the generated file
size and SHA-256; it is not the root of trust.

The command inspects the real Windows shortcut through `WScript.Shell`. It
confirms the target, project argument, working directory, icon, and description
before reporting success.

The shortcut has the project-owned icon and launches the source-built
`Editor.exe` with:

```text
--project-path <repository>/TaintedGrailModdingEditor
```

Double-click **Tainted Grail Modding Editor.lnk**.

To inspect the trusted shortcut plan without writing files:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create `
  --dry-run
```

To replace an existing generated shortcut deliberately:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create `
  --replace
```

Replacement is allowed only when the existing `.lnk` and sidecar pass complete
repository-derived verification. An unrelated or modified shortcut is not
deleted.

Verify the source-built clickable entry again at any time:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py verify
```

`developer_preview_shortcut.py` is the lower-level generator used by the
hardened command. It is not the supported user entry point.

### Deliberate diagnostic override

An external `Editor.exe` is not a verified source-built entry. For a deliberate
diagnostic-only link, provide all three explicit controls:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create `
  --editor C:\diagnostics\Editor.exe `
  --diagnostic-override `
  --output build\diagnostic-entries\External Editor.lnk
```

Normal verification rejects that link. To inspect it deliberately without
promoting it to a verified entry:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py verify `
  --shortcut build\diagnostic-entries\External Editor.lnk `
  --allow-diagnostic-override
```

## 5. Command-line fallback

The dedicated opener uses the same project and restricted launcher:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py
```

Use this when collecting wrapper-owned stdout, stderr, and launch-result data.

The tracked `TaintedGrailModdingEditor/Levels` root must remain present. The
Editor creates new levels beneath that directory and cannot create a level when
the parent is missing.

On Windows, Controlled Folder Access can still deny the Editor permission to
write beneath a protected checkout even when the directory exists. Event Viewer
records this as Windows Defender event 1123. If that event names `Editor.exe` or
`AssetProcessor.exe`, either move the checkout outside a protected folder or
use Windows Security's **Allow an app through Controlled folder access** flow
for the exact source-built executables. Do not disable Controlled Folder Access
globally, and do not treat running the Editor as administrator as the normal
solution.

## 6. Open the TG SDK panes

After the Editor opens, use:

```text
Tools → Tainted Grail SDK
```

Open:

- Tainted Grail SDK Status;
- Tainted Grail Pack Manager;
- Tainted Grail Source Intake;
- Tainted Grail Catalog Browser;
- Tainted Grail Catalog Governance;
- Tainted Grail Item and Recipe Editor.

The native Editor log is:

```text
TaintedGrailModdingEditor/user/log/Editor.log
```

It should contain `TaintedGrailModdingSDK` and
`Editor foundation activated`.

## 7. Generate the synthetic workspace

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture

python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

Use only that project-owned fixture for the manual UI evidence pass.

## Architecture boundary

`AutomatedTesting` remains an upstream engine test project and does not enable
the TG SDK Gem. The dedicated project is the only supported preview host.

The shortcut launches only the source-built O3DE Editor. It does not launch
FoA, BepInEx, Harmony, Avalon Core, deployment, injection, or save-mutation
tools.
