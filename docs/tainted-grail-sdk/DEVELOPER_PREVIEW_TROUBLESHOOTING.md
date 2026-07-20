# Developer Preview Troubleshooting

This guide covers the source-built Developer Preview 0 path. It does not cover
FoA launch, BepInEx, Harmony, deployment, save mutation, or runtime adapters.

## Editor executable is missing

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The trusted clickable entry expects `Editor.exe` beneath `bin/profile` or
`bin/Profile` in the exact approved Developer Preview build directory.

## Build directory is rejected

The standard clickable entry accepts only:

```text
build/tg-sdk-developer-preview-0-windows-profile
```

It also requires `CMakeCache.txt` to bind that directory to the current
repository through `CMAKE_HOME_DIRECTORY`. Re-run the configure command if the
cache is missing or belongs to another checkout. A renamed, empty, truncated,
non-x64, or non-GUI executable is rejected even when it is named `Editor.exe`.

## Clickable entry is missing

Create it after a successful build:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create
```

The output is:

```text
build/Tainted Grail Modding Editor.lnk
build/Tainted Grail Modding Editor.shortcut.json
```

The sidecar is evidence about the generated file; trusted target paths are
derived from the repository and build policy.

## Existing shortcut cannot be replaced

Use `--replace` only for a shortcut that was previously created and still
passes complete verification:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create `
  --replace
```

An unrelated link, a missing sidecar, changed hash, unapproved target, or
semantic shortcut mismatch blocks replacement.

## An explicit Editor.exe is rejected

`--editor` is diagnostic-only and must never replace the standard verified
entry. Use a separate output:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py create `
  --editor C:\diagnostics\Editor.exe `
  --diagnostic-override `
  --output build\diagnostic-entries\External Editor.lnk
```

Normal verification rejects diagnostic overrides. Deliberate inspection needs
`--allow-diagnostic-override` and still does not promote the link to a verified
source-built entry.

## The canonical path-policy contract fails

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/validate_path_policy.py
```

The contract requires canonical filesystem resolution, component-based
containment, Windows case folding, symlink/junction escape rejection,
workspace-relative root resolution, service-boundary pack enforcement, and
repository-derived shortcut trust.

## The dedicated project contract fails

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_project.py
```

The contract requires the dedicated project, Gem enablement, project-owned
icons, opener, and clickable-entry integration.

## The Editor exits immediately

Use the command-line fallback to capture wrapper logs:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py
```

Review:

- `build/tg-sdk-developer-preview-0-launch/editor-launch.stderr.log`;
- `build/tg-sdk-developer-preview-0-launch/tg-sdk-developer-preview-launch.json`;
- `TaintedGrailModdingEditor/user/log/Editor.log`.

## The TG SDK panes are missing

After the Editor opens, use **Tools → Tainted Grail SDK**.

If that menu group is absent:

1. validate the dedicated project and path-policy contracts;
2. reconfigure after pulling project or engine-manifest changes;
3. rebuild `Editor`;
4. recreate and verify the standard shortcut;
5. close other Editor or Asset Processor instances;
6. search `TaintedGrailModdingEditor/user/log/Editor.log` for activation errors.

## Shortcut verification fails

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_entry.py verify
```

Verification fails when the hash changes, repository-derived paths do not match
the sidecar, the approved build is missing or bound to another source tree, or
the actual `.lnk` properties differ. Correct the build or path issue and create
a new entry deliberately.

## Native Editor logs and wrapper logs differ

The `.lnk` opens the Editor directly. O3DE’s native log is:

```text
TaintedGrailModdingEditor/user/log/Editor.log
```

`developer_preview_open.py` additionally captures wrapper-owned process output
under `build/tg-sdk-developer-preview-0-launch`.

## Diagnostics collection fails

Diagnostics output must be empty, or a previously verified bundle used with
`--replace`. It must not be inside the project or workspace being inventoried.

Inspect `diagnostics-manifest.json` to confirm the bundle contains only the
documented allow-listed files and that redaction completed before sharing it.

Nothing is uploaded automatically. Review every generated file before sharing.
