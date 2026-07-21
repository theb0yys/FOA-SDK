# Developer Preview 0 Design

Status: proposed for design review

Target: end-to-end usability and source-built developer preview milestone

## Decision requested

Approve a focused **Developer Preview 0** milestone that proves the existing Tainted Grail SDK editor can be built, launched, exercised, closed, reopened, and verified by a fresh developer without adding another domain feature or crossing the runtime boundary.

The milestone will make the current editor reproducibly usable. It will not claim production readiness, a supported public binary release, FoA runtime execution, deployment, save mutation, or adapter capability.

## Why this milestone comes next

The repository now contains a governed editor foundation and a usable Item and Recipe Editor, including:

- workspace and exact game-profile management;
- pack manifests;
- source and evidence intake;
- canonical catalog browsing;
- governance, validation, permissions, prohibitions, and blockers;
- typed item and recipe profiles;
- ingredients, outputs, stations, and acquisition relationships;
- read-only station visibility and learnability evidence views.

Those capabilities have passed repository validation and host-build gates, but using them still requires a knowledgeable O3DE developer to infer build targets, launch paths, setup order, sample data, persistence expectations, and failure recovery.

The next risk is therefore usability and reproducibility rather than missing economy fields.

## Product definition

Developer Preview 0 is a source-built, pre-alpha editor preview for contributors and technically capable testers.

It must provide one documented path from a clean checkout to a verified editor session:

```text
prepare prerequisites
→ configure
→ build
→ validate
→ launch
→ create or open preview workspace
→ exercise governed authoring flow
→ save
→ close
→ reopen
→ verify durable state
→ collect diagnostics
```

The preview remains an O3DE-hosted tool. It is not a standalone end-user installer in this milestone.

## Primary host

The first supported runnable preview target is **Windows x64 Profile**.

Reasons:

- it is the most practical primary host for current FoA mod-development workflows;
- it provides a concrete target for launch scripts, screenshots, troubleshooting, and smoke evidence;
- it avoids claiming equal packaging support across platforms before those paths have been exercised end to end.

Linux and macOS host builds remain required compatibility signals where current CI supports them, but Developer Preview 0 acceptance requires one fully verified Windows path. Additional runnable hosts may be added only after the same smoke criteria pass.

## User outcomes

A fresh developer following the preview guide must be able to:

1. verify required tools and repository state;
2. configure the supported O3DE build;
3. build the Editor and TG SDK catalog tests;
4. run focused validators and unit tests;
5. launch the O3DE Editor with the TG SDK Gem available;
6. open every registered TG SDK pane;
7. generate or copy a legally distributable example workspace;
8. import deterministic sample evidence;
9. inspect and author canonical economy data;
10. save all durable documents;
11. close and reopen the Editor;
12. verify that the workspace, pack, evidence, catalog, governance history, economy profiles, joins, and relationships survived unchanged;
13. collect a diagnostics bundle when any step fails.

## Deliverables

### 1. Prerequisite checker

Add a repository-owned command that reports, without mutating the machine:

- supported operating system and architecture;
- Python version;
- CMake version;
- compiler/toolchain availability;
- Git and Git LFS availability;
- repository root detection;
- required O3DE third-party/package prerequisites where they can be checked safely;
- configured build-directory state;
- expected Editor executable state;
- clear remediation for every failed check.

The checker must never install software silently.

### 2. Configure and build wrapper

Add a thin, reviewable wrapper around the repository's existing O3DE CMake workflow.

The wrapper must:

- accept an explicit build directory;
- default to the approved Windows Profile configuration;
- print the exact underlying commands before executing them;
- support a dry-run mode;
- build the Editor host target and `TaintedGrailModdingSDK.Catalog.Tests`;
- fail with the original command exit code;
- avoid downloading or executing unreviewed third-party scripts;
- avoid storing absolute private paths in tracked files.

The wrapper must not replace or fork O3DE's build system.

### 3. Validation command

Provide one documented command that runs, in deterministic order:

- `validate_foundation.py`;
- `validate_governance_hardening.py`;
- `validate_catalog_tests.py`;
- O3DE source-policy validation for the Gem;
- the compiled catalog/governance/economy test target.

The command must produce a concise terminal summary and a machine-readable result file suitable for CI artifact collection.

### 4. Launch wrapper

Add a launch command that:

- accepts an explicit Editor executable or approved build directory;
- validates that the executable exists;
- prints the resolved command;
- supports optional project and log-directory arguments;
- never injects FoA, BepInEx, Harmony, Avalon Core runtime, or game-launch arguments;
- returns the Editor process exit code;
- provides clear guidance when the TG SDK panes are not registered.

### 5. Legally distributable preview fixture

Add a small deterministic fixture containing only project-owned synthetic data.

It must include enough data to exercise:

- one workspace document;
- one placeholder game profile with no private installation path;
- one pack manifest;
- one structured source document and evidence document;
- canonical item, recipe, station, and learnability-source records;
- at least one `crafted_at` relationship;
- at least one `learned_from` relationship;
- one item profile;
- one recipe profile;
- ingredient and output joins;
- validation and governance history sufficient to demonstrate allowed, forbidden, blocked, stale, and unresolved states.

Fixture identities must use a reserved namespace such as `preview.*` and must never resemble or claim real proprietary FoA identifiers.

The fixture must not contain:

- extracted game assets;
- decompiled game code;
- screenshots from private sources;
- private filesystem paths;
- real user names;
- real save data;
- credentials or tokens;
- redistributable-content assumptions not covered by project ownership.

### 6. Fixture generator and verifier

Prefer a deterministic generator over manually maintained duplicated JSON where practical.

The generator must:

- write only beneath an explicit output directory;
- refuse to overwrite unrelated files;
- produce stable content and ordering;
- support verification without rewriting;
- validate generated documents through the same persistence and schema rules used by the editor;
- emit a manifest containing file paths and SHA-256 hashes.

The generated fixture must remain readable by the current schema without migration.

### 7. Automated smoke harness

Add a smoke harness covering service and persistence behavior without requiring UI-coordinate automation.

The automated portion must prove:

- preview documents load successfully;
- the active workspace and profile bind correctly;
- sources and evidence register correctly;
- the catalog loads with expected records and relationships;
- typed item and recipe profiles are present;
- ingredients and outputs are present;
- station/learnability evidence rows are deterministic;
- expected blockers and governance states are present;
- saving to a temporary copy succeeds;
- reopening the saved copy produces an equivalent canonical snapshot;
- the smoke process does not invoke runtime, deployment, or game-state behavior.

Do not add brittle screen-coordinate automation as a merge requirement.

### 8. Manual UI smoke checklist

Provide a short manual checklist for the supported Windows host that verifies:

- all TG SDK panes open from the Tools menu;
- labels and controls are readable at normal Windows scaling;
- keyboard traversal reaches primary controls;
- preview workspace and catalog data display correctly;
- Item and Recipe Editor selections display expected typed data;
- station visibility and learnability rows display expected status and reasons;
- save, close, reopen, and reload behavior works;
- failure messages identify the affected path or subject;
- no runtime or deployment action is exposed.

A completed checklist and screenshots from the accepted head become release evidence, not committed proprietary fixtures.

### 9. Diagnostics bundle

Add a command that collects only project-owned troubleshooting information into an explicit output directory:

- preview script version and arguments with secrets redacted;
- operating system and tool versions;
- build configuration summary;
- validator and test summaries;
- relevant Editor log excerpts;
- workspace-relative document inventory and hashes;
- no source artifact contents unless explicitly requested;
- no game files, private saves, credentials, or unrestricted absolute-path dumps.

The user must review the bundle before sharing it.

### 10. CI and artifact flow

Add focused CI for the preview scripts, fixture, and smoke harness.

Pull requests must run:

- script unit tests;
- fixture generation and verification;
- focused validators;
- smoke load/save/reopen verification;
- existing catalog/governance/economy tests;
- applicable host builds.

A manually dispatched or approved integration workflow should attempt to produce:

```text
tg-sdk-developer-preview-0-windows-profile.zip
```

The archive should contain only files that are legal and practical to redistribute, plus:

- a build manifest;
- exact source commit;
- toolchain/configuration summary;
- checksums;
- preview fixture;
- launch and validation instructions;
- known limitations.

If a runnable Editor archive cannot be produced safely because of third-party package redistribution, artifact size, licensing, or O3DE packaging constraints, implementation must stop and return to design review. It must not quietly downgrade the milestone while still calling it a runnable binary preview.

A source-build support bundle may be produced separately, but it must be labelled accurately.

## Repository layout

Final paths may follow existing conventions, but the intended ownership is:

```text
Gems/TaintedGrailModdingSDK/Preview/
    fixture source or generator inputs
    smoke expectations

Gems/TaintedGrailModdingSDK/Tools/
    preview prerequisite, validation, fixture, smoke, and diagnostics commands

docs/tainted-grail-sdk/
    Developer Preview guide
    troubleshooting
    manual smoke checklist

.github/workflows/
    focused preview validation
    manually dispatched preview artifact workflow
```

The preview tooling belongs to the TG SDK layer and must not modify stock O3DE behavior unnecessarily.

## Persistence and compatibility

Developer Preview 0 should use existing durable formats.

No schema change is authorised by this design.

If the smoke workflow cannot faithfully represent its fixture using current workspace, pack, source, evidence, catalog, governance, and economy documents, implementation must stop and request a separate schema review.

The reload comparison must ignore only fields that are intentionally generated at runtime and documented as nondeterministic. It must not hide missing records, changed identities, lost relationships, lost history, permission changes, or evidence changes.

## Security, privacy, and legal boundary

The milestone must not:

- scan a user's machine beyond explicitly supplied paths;
- discover or upload FoA installations automatically;
- download private dependencies without explicit user action;
- launch FoA;
- invoke BepInEx, Harmony, Avalon Core runtime, or game APIs;
- deploy files into the game;
- modify saves;
- collect telemetry;
- upload diagnostics automatically;
- commit copyrighted game data;
- package files whose redistribution rights are unclear.

Every generated path must be user-controlled, temporary, or workspace-relative.

## Failure and rollback behavior

Scripts must fail closed and leave actionable messages.

Required failure coverage includes:

- missing prerequisite;
- unsupported host;
- invalid repository root;
- invalid or unwritable build directory;
- configure failure;
- compile failure;
- missing Editor executable;
- validator failure;
- unit-test failure;
- malformed fixture;
- fixture hash mismatch;
- save failure;
- reload mismatch;
- unexpected absolute/private path in an artifact;
- prohibited or oversized artifact content;
- diagnostics redaction failure.

Rollback is deletion of generated preview outputs and reversion of the preview implementation commits. Existing workspaces and catalogs must not require migration.

## Test strategy

### Script tests

Use Python unit tests for:

- argument parsing;
- dry-run command construction;
- path validation;
- overwrite protection;
- process exit-code propagation;
- manifest generation;
- hash verification;
- redaction;
- artifact allow-list enforcement.

### Service and persistence tests

Extend focused C++ tests only where the smoke path exposes missing testable behavior in current services.

Do not duplicate domain rules inside Python solely to make the smoke harness pass.

### CI contract

Extend repository validators so the preview cannot silently lose:

- prerequisite checks;
- fixture verification;
- smoke load/save/reopen proof;
- artifact manifest and allow list;
- legal/safety boundary text;
- supported-host declaration;
- troubleshooting and manual smoke documentation.

## Documentation

Implementation must update:

- root README capability and status text;
- TG SDK documentation hub;
- User Guide build/open section;
- Development Guide;
- Release Process;
- Roadmap;
- Changelog;
- Maintainer Checklist;
- a new Developer Preview 0 guide;
- a troubleshooting guide or dedicated troubleshooting section.

Documentation must distinguish:

- source-build preview;
- CI support bundle;
- runnable binary artifact, if approved and produced;
- unsupported platforms;
- editor research/authoring capability;
- absent runtime/deployment capability.

## Implementation sequence

After design approval, implementation should proceed in focused reviewable commits:

1. prerequisite/configure/build/validate command contracts and tests;
2. deterministic fixture generator, manifest, and verification tests;
3. service-level load/save/reopen smoke harness;
4. launch and diagnostics commands;
5. focused preview CI;
6. Windows manual smoke evidence and screenshot pass;
7. artifact workflow and allow-list verification;
8. documentation, roadmap, changelog, and release-readiness review.

Each commit must pass the relevant local focused tests before it is created. GitHub Actions must validate each pushed head. No preview artifact or PR may merge with queued, cancelled, skipped unexpectedly, or failing required checks.

## Non-goals

Developer Preview 0 does not include:

- vendor, loot, reward, or acquisition dashboards;
- duplicate detection across packs;
- actor, spawn, world, faction, quest, asset, or localisation editors;
- adapter contracts;
- work-order generation;
- runtime execution;
- FoA launch;
- deployment;
- save mutation;
- automatic updater;
- installer signing;
- stable release support promises.

## Acceptance criteria

Developer Preview 0 is complete only when:

- a clean supported Windows environment can follow one documented configure/build/validate/launch path;
- the Editor exposes every current TG SDK pane;
- the project-owned preview fixture is deterministic, legal, validated, and hash-manifested;
- automated smoke tests prove load, save, close-equivalent, reopen, and canonical-state equivalence;
- the manual UI checklist passes on the accepted commit;
- focused validators, script tests, C++ tests, source policy, repository validation, and applicable host builds pass;
- diagnostics are explicit, reviewable, and redacted;
- documentation accurately describes limitations;
- no runtime, deployment, telemetry, private-data, save-mutation, or proprietary-content boundary is crossed;
- a runnable Windows preview archive is produced and verified, or the project returns to design review before claiming binary-preview completion.

## Review boundary

Approval authorises implementation of this Developer Preview 0 usability milestone on `foa-development`.

It does not authorise schema changes, runtime adapters, game launch, deployment, save mutation, telemetry, proprietary fixtures, automatic dependency installation, or redistribution of files without confirmed rights.

The later approved [Windows Installer and Prebuilt Artifact Workflow Design](WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md)
adds a separate installed-artifact path. It does not retroactively turn this
source-built milestone or its shortcut into release evidence.
