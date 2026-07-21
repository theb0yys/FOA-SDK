# Open and Test the Actual Editor

This is the supported Windows path from a fresh checkout to the dedicated,
source-built **Tainted Grail Modding Editor**.

The editor is an O3DE project named `TaintedGrailModdingEditor`. It is separate
from `AutomatedTesting`, enables `TaintedGrailModdingSDK` and
`ExternalToolchain` directly, and owns the project and Windows shortcut icons.
The project also enables O3DE's `Atom`, `DiffuseProbeGrid`, and `PhysX5` Editor
host Gems so every component in the standard default level resolves. Its
`ShaderLib` supplies the required empty Scene and View SRG extensions so Atom
can compile the project shader registry.

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

The commands must report that the dedicated source project, bounded writable
materialization, tracked default viewport level, icons, opener, persistence
boundaries, path policy, and repository-derived clickable-entry trust contract
are complete.

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

The build includes the Editor, the `AssetProcessorBatch` clean-first-run
preflight, and the compiled TG SDK catalog tests. The executables are expected
at:

```text
build/tg-sdk-developer-preview-0-windows-profile/bin/profile/Editor.exe
build/tg-sdk-developer-preview-0-windows-profile/bin/profile/AssetProcessorBatch.exe
```

## 4. Create the clickable entry

Use the hardened entry command:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create
```

This materializes the small Editor project beneath bounded per-user storage,
synchronously prepares its `pc` asset cache with `AssetProcessorBatch`, then
creates and immediately verifies:

```text
build/Tainted Grail Modding Editor.lnk
build/Tainted Grail Modding Editor.shortcut.json
```

The project is materialized beneath
`%LOCALAPPDATA%\O3DE\TGEditor`. Repository-owned
bootstrap files are managed there, while additional user-authored levels are
preserved. O3DE's conventional `Cache` and `user/log` paths remain inside that
materialized project, while wrapper logs are a sibling beneath the same bounded
root. No Defender exclusion or administrator permission is required.
Asset-preparation stdout and stderr remain beneath the same root in `launcher`.
The entry is not created if asset preparation fails.

The trusted target, engine, project, writable paths, working directory, and
icon are derived from repository and per-user path policy. The sidecar records
the generated file size and SHA-256; it is not the root of trust.

The command inspects the real Windows shortcut through `WScript.Shell`. It
confirms the target, engine, project, cache, user, log, and startup-level
arguments, working directory, icon, and description before reporting success.

The shortcut has the project-owned icon and launches the source-built
`Editor.exe` with:

```text
--project-path %LOCALAPPDATA%\O3DE\TGEditor\project
--engine-path <repository>
--project-cache-path %LOCALAPPDATA%\O3DE\TGEditor\project\Cache
--project-user-path %LOCALAPPDATA%\O3DE\TGEditor\project\user
--project-log-path %LOCALAPPDATA%\O3DE\TGEditor\project\user\log
%LOCALAPPDATA%\O3DE\TGEditor\project\Levels\DefaultLevel\DefaultLevel.prefab
```

The positional `.prefab` argument makes O3DE open the materialized standard
default level. Its Sun, sky, camera, ground, grid, and shader ball provide a
visible, lit 3D authoring viewport instead of an empty grey view.

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
repository-derived verification. Earlier verified repository-project shortcuts
may be replaced to migrate to bounded per-user storage. An unrelated or
otherwise modified shortcut is not deleted.

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

The dedicated opener refreshes the managed per-user materialization, waits for
the bounded `pc` asset cache to finish, and uses the same canonical default
level and restricted launcher:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py
```

Use this when collecting wrapper-owned stdout, stderr, and launch-result data.

The tracked source
`TaintedGrailModdingEditor/Levels/DefaultLevel/DefaultLevel.prefab` must remain
present and semantically equal to O3DE's
`Assets/Editor/Prefabs/Default_Level.prefab` template. It seeds the per-user
project only when that level is missing. Existing user-created levels and an
intentionally modified materialized default level are preserved. Managed
bootstrap files update only when their prior hashes prove they were not edited
outside the materializer; conflicts fail closed instead of overwriting work.

The Editor and Asset Processor write only beneath the per-user root during the
supported workflow. The repository checkout remains the reviewed source and
engine root; users do not need to weaken Controlled Folder Access, add security
exceptions, or run the Editor as administrator.

## 6. Open the TG SDK panes

After the Editor opens, use:

```text
Tools → Tainted Grail SDK
```

Before opening the SDK panes, confirm the viewport shows the standard default
scene's sky, ground grid, and shader ball. A blank grey viewport means the
canonical startup level was omitted or failed to load; close the Editor,
replace the verified shortcut, and inspect `Editor.log` before continuing.

Open:

- Tainted Grail SDK Status;
- Tainted Grail Pack Manager;
- Tainted Grail Source Intake;
- Tainted Grail Catalog Browser;
- Tainted Grail Catalog Governance;
- Tainted Grail Item and Recipe Editor.

The native Editor log is:

```text
%LOCALAPPDATA%\O3DE\TGEditor\project\user\log\Editor.log
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
