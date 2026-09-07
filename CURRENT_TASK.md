# Current Task

## Status

`Installer correction and item-viewer repair` — local implementation and exact-pin Editor validation are complete, including the owner's category correction. Game discovery, registration, installed-item icon previews, and category/search/reload behavior have operational evidence. PR #248 is ready for maintainer audit; release acceptance remains separate.

## Goal

Restore `FOA-SDK.exe` as the only installed user-facing SDK application entry point, repair game discovery and registration, and make the item viewer populate native icon previews from the registered installation in the compiled exact-pin Editor.

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
- make the asset viewer discover installed items and generate local icon previews through a bounded read-only Unity provider;
- run extraction outside the UI thread, report progress/failures, reload results on Refresh, and resolve custom Assets from the active O3DE project;
- package the pinned preview-reader dependencies and verify real installed-game item selection in the compiled Editor.
- replace internal folder categories with readable item groups and subcategories, preserve all records, and validate category/search/reload behavior in the compiled Editor.

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
- Refresh produces visible installed-item rows and selecting an item displays its decoded native icon; no pre-generated fixture is required;
- stale, malformed, excessive, or escaping inputs fail closed; cancellation preserves previous completed previews; no game files or saves are modified.
- category counts cover every item; category and subcategory filters match their rows, and reload preserves valid filter selections.

## Current branch

`codex/installer-control-panel-completion`

## Next action

Maintainer audit of PR #248 and the open corrected Editor. The live item-viewer smoke passed Refresh, category/subcategory filtering, search, selection, cancellation, and reload with 3,914 item rows and 3,912 decoded icons; two items have no supported icon reference. Thirteen readable main categories account for every row, and valid filters survive reload. The viewer also opens its saved workspace without requiring System Details first. The final refresh took 73.219 seconds with a maximum measured UI timer gap of 1.329 seconds. Both mandatory compiled suites passed (462 tests passed, two explicit symlink-privilege skips). Static validation passed (803 tests passed, nine explicit skips), and all ten enabled source-policy checks passed. Private source/binary hashes, measurements, logs, and screenshots remain outside the checkout. Exact full-product packaging and clean-machine installer evidence remain maintainer-controlled release gates.
