# Development Guide

## Purpose

This guide covers local development for the `TaintedGrailModdingSDK` Gem. It supplements the upstream O3DE source-build documentation.

## Repository model

Long-lived branches:

- `main` — reviewed integration state;
- `foa-development` — active development.

Work directly on `foa-development`. Do not commit directly to `main`. Significant changes require design review before implementation.

## Prerequisites

Use the compiler, CMake, Python, Git LFS, and third-party package configuration required by the O3DE revision in this repository.

At minimum:

- Git and Git LFS;
- Python supported by repository scripts;
- CMake supported by this O3DE revision;
- a platform compiler supported by O3DE;
- a writable O3DE third-party package cache;
- sufficient disk space for an O3DE source build.

## Clone and configure

```shell
git clone https://github.com/theb0yys/o3de.git
git -C o3de lfs install
cd o3de
git checkout foa-development
```

Configure the engine using the standard O3DE source-build instructions for your platform. Keep build output outside the source tree when practical.

## Gem location

```text
Gems/TaintedGrailModdingSDK/
├── Code/
│   ├── Source/
│   ├── Tests/
│   ├── CMakeLists.txt
│   ├── taintedgrailmoddingsdk_editor_files.cmake
│   └── taintedgrailmoddingsdk_catalog_tests_files.cmake
├── Tools/
│   ├── validate_foundation.py
│   └── validate_catalog_tests.py
├── README.md
└── gem.json
```

The Gem is host-tools-only. Do not add Clients, Servers, or Unified runtime aliases.

## Build target

Build an O3DE Editor or applicable host-tools target that resolves the Gem's `Tools` variant. Builder hosts resolve the `Builders` alias.

The catalog unit target is registered as:

```text
Gem::TaintedGrailModdingSDK.Catalog.Tests
```

A full build depends on your O3DE project and platform configuration. The focused validators do not replace compilation.

## Focused validation

Run from the repository root:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
python Gems/TaintedGrailModdingSDK/Tools/validate_catalog_tests.py
```

The validators check:

- engine and Gem registration;
- host-tool variants;
- source manifest completeness;
- required public documentation and governance;
- workspace, pack, source/evidence, catalog, and blocker contracts;
- transactional persistence ordering;
- catalog test registration and core identity cases;
- editor/runtime separation.

Update the validators whenever a required source file, document, test, or architectural contract changes.

## Source organisation

Use focused service boundaries:

- `FoundationModels.*` — reflected durable and query models;
- `FoundationService.*` — orchestration and shared state;
- `FoundationCatalogService.cpp` — transactional catalog orchestration;
- `*PersistenceService.*` — versioned durable reads and writes;
- `SourceImportService.*` — bounded parsing and extraction;
- `SourceEvidenceRegistry.*` — in-memory identity and linkage rules;
- `CatalogDatabase.*` — canonical record/query/relationship storage;
- `CatalogPromotionService.*` — evidence-backed reviewed-record creation;
- `FoundationValidationService.*` — blockers and policy checks;
- `*Widget.*` — Qt views and user commands;
- `TaintedGrailModdingSDKSystemComponent.*` — editor lifecycle and pane registration.

New domain tools should follow the same pattern instead of placing logic in widgets.

## Adding a reflected model

1. Add a stable `AZ_TYPE_INFO` UUID.
2. Add fields with clear ownership and defaults.
3. Reflect the type in `FoundationModels.cpp`.
4. Register reflection from the system component when required.
5. Set or increment the serialization version.
6. Define migration or rejection behavior for incompatible changes.
7. Document the format in `DATA_FORMATS.md`.
8. Add round-trip and malformed-input coverage.

Do not reuse type IDs or silently reinterpret an existing field.

## Adding a persistence format

A durable format must have:

- explicit schema version;
- stable suffix and workspace location;
- load and save service;
- path-boundary validation;
- clear error reporting;
- migration or unsupported-version rejection;
- transactional behavior for related files when practical;
- documentation and examples without proprietary content.

File writes must not be scattered through UI code.

## Adding an importer

An importer contract must declare:

- stable importer ID and version;
- supported source kinds and extensions;
- whether it extracts evidence;
- size and resource limits;
- exact profile binding;
- deterministic fingerprint behavior;
- schema and issue reporting;
- non-inference behavior for unknown fields;
- legally distributable fixtures and tests.

Importers may create source, evidence, and issue records. They may not promote canonical catalog records or runtime permission automatically.

## Adding a catalog capability

A catalog change must preserve:

- stable record IDs;
- exact native-reference uniqueness;
- pack ownership for synthetic records;
- duplicate display-name independence;
- evidence linkage;
- first-class relationship identity;
- validation history;
- deterministic result ordering;
- save-before-publish persistence;
- reviewed promotion separate from validation and permission.

Add or update unit tests for identity, queries, relationships, and negative cases.

## Adding a dock window

1. Create a focused `QWidget` subclass.
2. Keep domain logic in services.
3. Connect to `FoundationNotificationBus` when live refresh is needed.
4. Register and unregister the pane in the system component.
5. Use a stable pane name and save key.
6. Provide accessible labels and actionable errors.
7. Update the source manifest, validators, README, and user documentation.

Avoid `Q_OBJECT` unless signals, slots, properties, or Qt meta-object features are required.

## C++ and include discipline

- Use explicit includes for every directly used type or function.
- Do not rely on transitive includes.
- Prefer O3DE/AZ types where required by O3DE APIs and serialization.
- Use Qt types at the UI and importer boundary where appropriate.
- Convert strings deliberately and preserve UTF-8.
- Avoid unchecked narrowing conversions.
- Move large objects deliberately and include the correct move utility.
- Keep ownership obvious; prefer values, references, and RAII.

See `CODE_QUALITY.md` for mandatory details.

## Testing strategy

### Contract tests

Python validators ensure repository structure and architecture invariants.

### Unit tests

Current catalog tests cover:

- duplicate display names remaining independent;
- duplicate exact native references being rejected;
- synthetic pack ownership;
- evidence/permission/validation search filters;
- relationship target and evidence requirements.

Add O3DE test coverage for:

- identity validation;
- registry duplicate and mismatch handling;
- catalog queries;
- blocker generation;
- schema version handling;
- importer parsing and issue reporting;
- persistence round trips and migrations;
- promotion profile, pack, and permission failures.

### Integration tests

Cover:

- workspace open/save/reload;
- pack open/save/reload;
- source intake and paired document persistence;
- catalog reload and exact-reference search;
- promotion save-before-publish behavior;
- editor pane registration;
- notification-driven UI refresh.

### Manual UI validation

Record:

- build configuration;
- steps performed;
- screenshots when useful;
- expected and observed blockers;
- generated workspace files.

Do not include private paths or copyrighted game data in screenshots.

## Documentation changes

Behavior and documentation ship together.

Update:

- root README for public capability changes;
- User or Catalog Guide for user-facing workflows;
- Data Formats for schema changes;
- Architecture for boundary changes;
- Changelog for notable changes;
- Roadmap status when a phase changes.

## Commit workflow

Before committing:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
python Gems/TaintedGrailModdingSDK/Tools/validate_catalog_tests.py
git diff --check
git status
```

Review the full diff, then create a DCO-signed commit:

```shell
git commit -s -m "Add concise imperative summary"
```

## Pull-request workflow

1. Synchronize `foa-development` to the accepted `main` merge commit.
2. Obtain design review for significant changes.
3. Implement and perform pre-commit self-review.
4. Run focused and relevant local tests.
5. Update documentation and changelog.
6. Open a pull request from `foa-development` to `main`.
7. Complete every PR-template section.
8. Resolve review threads and CI failures.
9. Obtain maintainer approval.
10. Merge through GitHub only after required checks pass.
11. Synchronize `foa-development` to the new merge commit before new work.

## Debugging

Useful first checks:

- confirm the Gem is listed in `engine.json` external subdirectories;
- confirm the source is in the CMake file manifest;
- confirm required AzToolsFramework and Qt dependencies are linked;
- inspect O3DE Editor logs for module load and pane-registration errors;
- isolate serialization errors with a minimal document;
- validate workspace-root and path-containment behavior;
- check exact profile and fingerprint binding for source/evidence failures;
- inspect exact native-ref and record-ID collisions for catalog failures.

## Upstream O3DE changes

Keep TG SDK changes isolated from unrelated O3DE modifications. When an upstream change is genuinely required:

- explain why the Gem cannot solve the problem alone;
- minimise the upstream surface;
- identify divergence and future merge cost;
- follow applicable O3DE contribution and licence requirements;
- document the change in the PR and architecture guide.
