# Developer Preview Troubleshooting

This guide covers the source-built Developer Preview 0 path. It does not cover FoA launch, BepInEx, Harmony, deployment, save mutation, or runtime adapters.

## Editor executable is missing

Run the supported build command:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build `
  --build-dir build/tg-sdk-developer-preview-0-windows-profile
```

The launch wrapper expects `Editor.exe` beneath `bin/profile` or `bin/Profile`, unless an explicit `--editor` path is supplied. An explicit executable with another name is rejected.

## The Editor exits immediately

Use the repository-owned project opener:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py
```

Review:

- `build/tg-sdk-developer-preview-0-launch/editor-launch.stderr.log`;
- `build/tg-sdk-developer-preview-0-launch/tg-sdk-developer-preview-launch.json`;
- `AutomatedTesting/user/log/Editor.log`.

The opener always selects the repository-owned `AutomatedTesting` project, which is committed with `TaintedGrailModdingSDK` enabled.

## The TG SDK panes are missing

After the Editor opens, use **Tools → Tainted Grail SDK**.

If that menu group is absent:

1. run `python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_project.py`;
2. confirm `AutomatedTesting/project.json` contains `TaintedGrailModdingSDK` in `gem_names`;
3. confirm `engine.json` contains `Gems/TaintedGrailModdingSDK` in `external_subdirectories` and `AutomatedTesting` in `projects`;
4. reconfigure after pulling the project-manifest change;
5. rebuild the `Editor` target from the accepted source head;
6. ensure the Editor executable comes from the same build directory;
7. close other Asset Processor or Editor processes using another build;
8. search `AutomatedTesting/user/log/Editor.log` for `TaintedGrailModdingSDK` activation or module-load errors.

The expected activation message states that the editor foundation was activated and FoA runtime execution remains disabled.

## The repository project contract fails

The supported source-built preview uses:

```text
AutomatedTesting/project.json
```

The contract requires that project to be registered by `engine.json` and to enable `TaintedGrailModdingSDK` exactly once. Do not remove the Gem locally before configuring or building the preview.

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview_project.py
```

A failure identifies the missing project, Gem registration, opener, launcher forwarding, or quickstart contract.

## The selected project is rejected

The lower-level `developer_preview_launch.py --project` option must identify an existing directory with a valid UTF-8 `project.json` containing `project_name`. The wrapper passes the resolved directory through O3DE’s `--project-path` switch.

For the supported path, prefer `developer_preview_open.py`; it selects `AutomatedTesting` automatically. The preview fixture is a TG SDK workspace, not an O3DE project, and must not be passed as `--project`.

## Native Editor logs and wrapper logs differ

`--log-dir` belongs to the wrapper. It captures the process command, exit status, stdout, and stderr without injecting an undocumented O3DE log override.

The repository-owned opener selects `AutomatedTesting`, so O3DE’s native Editor log is:

```text
AutomatedTesting/user/log/Editor.log
```

Supply the native log explicitly to diagnostics with `--editor-log` when needed.

## Diagnostics collection fails

The diagnostics output must be an empty directory, or a previously verified bundle used with `--replace`. It must not be inside the project or workspace being inventoried.

Collection rejects:

- symlinks;
- missing requested files;
- oversized log excerpts or JSON summaries;
- binary or non-UTF-8 bundle output;
- unexpected output paths;
- path traversal;
- private absolute paths that remain after redaction;
- secret-like values that remain after redaction.

Nothing is uploaded automatically. The collector does not read source artifact contents, game files, saves, environment variables, or unrestricted filesystem trees.

## Verify a diagnostics directory

Run:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_diagnostics.py verify `
  --output build/tg-sdk-developer-preview-0-diagnostics
```

`diagnostics-manifest.json` is authoritative for the allowed file set, byte sizes, and SHA-256 hashes. Verification also repeats the redaction, UTF-8, size, symlink, and traversal checks.

A failed verification means the directory must not be shared. Recollect it into a new empty directory after fixing the reported issue.

## Review before sharing

Open every generated file. Remove anything you do not want to disclose. Confirm that paths are represented by tokens such as `<REPO_ROOT>`, `<BUILD_DIR>`, `<PROJECT_ROOT>`, `<WORKSPACE_ROOT>`, `<HOME>`, `<TEMP>`, or `<ABSOLUTE_PATH>`.

A successful verification reduces accidental disclosure risk but does not transfer responsibility for reviewing the bundle. Do not share proprietary source artifacts, FoA files, private saves, credentials, or personal information.
