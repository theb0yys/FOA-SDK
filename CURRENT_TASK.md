# Current Task

## Status

`Installer + Control Panel completion` — implementation complete; maintainer review candidate.

## Goal

Deliver a polished, usable Windows installer and a separate installed FOA-SDK Control Panel that implements the accepted first-release setup boundary from the multi-game installer research.

## Classification

**Critical/Runtime** for the installer lifecycle, plus **Significant** for the new installed profile, compatibility, diagnostics, and UI surfaces.

## Completed scope

- self-contained `FOA-SDK-ControlPanel.exe` with Home, Setup, Compatibility, and Diagnostics pages;
- explicit external workspace and game-folder selection without broad disk or network scanning;
- versioned `foa.sdk.setup_profile.v1` persistence with bounded legacy Tool Wizard import;
- separate Mono/IL2CPP/unknown route indications without runtime-proof claims;
- non-mutating plan preview and path-redacted `foa.sdk.support_report.v1` export;
- installer finish flow, MSI payload, Start Menu, inventory, smoke, and functional-readiness integration;
- preserved legacy Tool Setup Wizard command-line compatibility;
- focused static, build, self-test, and bounded Windows installer evidence.

## Explicit boundary

This milestone does not install loaders, convert or deploy assets, write game files, launch Fall of Avalon, inspect saves, sign artifacts, publish a release, or promote local observations to runtime compatibility evidence.

## Current branch

`codex/installer-control-panel-completion`

## Next action

Maintainer audit of this focused installer/Control Panel change. Exact full-product package and clean-machine evidence remains tied to a reviewed canonical inventory and must be recorded separately when that package lane is run.

## Next product task — do not start implicitly

After this change is accepted, the next task is the **zero-configuration Highmap Importer experience** described in `Research/world-authoring-terrain-heightmap/`.

That task must build the two-action Terrain Authoring pane and coordinator over the existing `TerrainHeightmapDocumentV1` backend and current command-line importer. The ordinary path remains **Edit Vanilla Map** or **Import New Map** without a technical path/metadata wizard. The production vanilla-map provider remains `BLOCKED` until an exact lawful CampaignMap-to-terrain source binding is established; missing provider facts must not be guessed or transferred to the user as configuration fields.
