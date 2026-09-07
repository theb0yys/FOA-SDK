# Current Task

## Status

`Installer correction and game-location repair` — owner review found automatic discovery and manual registration failures; PR #248 remains under correction.

## Goal

Remove the unintended standalone Control Panel application, restore `FOA-SDK.exe` as the only installed user-facing SDK application entry point, and provide a compiled exact-pin Editor session for review.

## Classification

**Critical/Runtime** — installer UI and installed-process launch behavior require Windows operational evidence. Package layout, build registration, validator, and documentation repairs are also included. No released installer or persisted user schema requires migration.

## In scope

- remove the standalone `FOA-SDK-ControlPanel.exe` source and dedicated tests;
- remove its MSI payload requirement, Start Menu entry, installer finish option, command-line switches, build steps, and self-tests;
- keep `bin\Windows\profile\Default\FOA-SDK.exe` as the installed application entry point;
- keep the installer lifecycle, installed launcher validation, desktop shortcut, and legacy Tool Setup Wizard maintenance route;
- fix the Windows compiler blockers in Fall of Avalon install discovery that prevented the real Editor target from building;
- build the dedicated Editor against the pinned O3DE revision and open the isolated Developer Preview project for review;
- update installer documentation and validation contracts to the single-entry-point flow.
- restore misplaced root documentation and remove superseded task/decision copies;
- repair stale Foundation/catalog validation contracts and register the existing quest binding contract and tests in their owned build targets;
- correct the source-policy header and Unicode findings without changing rendered text or runtime behavior;
- validate the correction, commit it, and update PR #248 for maintainer audit.
- fix game discovery for Steam clients installed outside Program Files;
- prevent stale Tool Wizard hints without a saved workspace from redirecting fresh setup;
- honor manual game selection, rebuild dependent paths, and verify registration survives restart in the compiled Editor.

## Out of scope

- full multi-GB package production, signing, release, or publication;
- changes to protected Fall of Avalon files, installations, saves, or proprietary material;
- runtime-adapter compatibility claims;
- the zero-configuration Highmap Importer follow-on task.

## Acceptance criteria

- no tracked source, packaging, workflow, test, or public documentation requires or launches `FOA-SDK-ControlPanel.exe`;
- the installer opens `FOA-SDK.exe` directly by default after successful validation;
- the MSI exposes one Start Menu application entry for `FOA-SDK.exe`;
- focused installer source, validator, and test lanes pass;
- the exact-pin Profile Editor and required asset preflight targets build, both mandatory compiled suites pass, and the isolated review level opens in a responsive Editor window;
- generated Control Panel output is removed from the working checkout.
- automatic discovery and the manual folder picker save the selected installation and reopen it correctly; failures preserve existing configuration and report the cause.

## Current branch

`codex/installer-control-panel-completion`

## Next action

Complete the game-location repair and its compiled service and Editor workflow checks, then update PR #248 for maintainer audit. Exact full-product packaging and clean-machine installer evidence remain maintainer-controlled release gates.
