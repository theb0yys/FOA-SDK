# Installed launcher

This directory owns launchers shipped as part of an installed FOA-SDK suite.

The current Windows launcher shipped to users is:

```text
<install-root>\bin\Windows\profile\Default\FOA-SDK.exe
```

It walks upward from its own path to find the self-contained product root, then
requires `INSTALL_MANIFEST.json`, root `engine.json`,
`TaintedGrailModdingEditor\project.json`, the bundled `Editor.exe`, and the
packaged `Levels\DefaultLevel\DefaultLevel.prefab` before starting the Editor.
The launched command uses `--engine-path <install-root>`,
`--project-path %LOCALAPPDATA%\O3DE\TGEditor\installed\project`, writable
`--project-cache-path`, `--project-user-path`, and `--project-log-path` folders
beneath that materialized project, and the packaged default level. The launcher
copies the packaged `External` Gem roots and project into LocalAppData,
including `asset_processor.setreg`, so Asset Processor writes outside the
installed payload and on the same drive as the launched project. It supports a
bounded `--self-test` used by staging and installer smoke validation.

An installed launcher may open the FOA-SDK Editor only. It may not launch FoA, deploy mods, invoke runtime adapters, mutate saves, sign artifacts, publish releases, or bypass installed-payload verification.
