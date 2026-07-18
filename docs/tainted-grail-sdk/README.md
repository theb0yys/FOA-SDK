# Tainted Grail Modding Editor and SDK Documentation

This directory contains the public documentation for the TG SDK layer in this O3DE fork.

## Start here

### For users

- [Open and Test the Actual Editor](OPEN_AND_TEST_EDITOR.md) — shortest Windows path from a fresh pull to the dedicated O3DE project, source build, trusted clickable entry, native log, and synthetic workspace.
- [Developer Preview 0](DEVELOPER_PREVIEW_0.md) — Windows x64 prerequisites, configure/build/validate commands, deterministic fixture, persistence smoke, controlled Editor launch, redacted diagnostics, manual UI evidence tooling, and current limitations.
- [Windows Manual UI Smoke](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md) — real Windows checklist, screenshot-evidence initializer/verifier, exact-commit binding, privacy attestation, and evidence handling.
- [Developer Preview Troubleshooting](DEVELOPER_PREVIEW_TROUBLESHOOTING.md) — missing Editor output, path-policy and clickable-entry failures, absent TG SDK panes, native log locations, diagnostics failures, and verification guidance.
- [User Guide](USER_GUIDE.md) — build, open, configure, and use the foundation editor tools.
- [Catalog Guide](CATALOG_GUIDE.md) — canonical search, record inspection, evidence promotion, relationships, validation, and blockers.
- [Governance Engine Guide](GOVERNANCE_ENGINE_GUIDE.md) — independent maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession decisions.
- [Item and Recipe Editor Guide](ITEM_RECIPE_EDITOR_GUIDE.md) — typed item and recipe profiles, ingredients, outputs, stations, acquisition relationships, and governed action lanes.
- [Recipe Station Evidence View](RECIPE_STATION_EVIDENCE_VIEW.md) — read-only station visibility and learnability research, evidence health, governance, and blockers.
- [Economy Acquisition Coverage](ECONOMY_ACQUISITION_COVERAGE.md) — read-only vendor, loot, reward, learnability, and crafting coverage derived from canonical relationships, exact evidence binding, governance, and blockers.
- [Economy Cross-Pack Duplicate Report](ECONOMY_CROSS_PACK_DUPLICATES.md) — exact cross-pack subject and recipe-key candidates, evidence health, governance state, blockers, and read-only review results.
- [FoA Adapter Contracts](FOA_ADAPTER_CONTRACTS.md) — typed adapter identity, semantic versions, capabilities, permission/proof gates, and the read-only compatibility matrix.
- [Data Formats](DATA_FORMATS.md) — workspace, pack, source, evidence, issue, catalog, validation, governance-history, and typed-domain contracts.
- [Support](../../SUPPORT.md) — where and how to ask for help.
- [Security](../../SECURITY.md) — private reporting and secure-use guidance.
- [Privacy](PRIVACY.md) — local data handling and telemetry requirements.
- [Accessibility](ACCESSIBILITY.md) — user-interface accessibility standard.
- [Glossary](GLOSSARY.md) — shared project terminology.

### For contributors

- [Contributing](../../CONTRIBUTING.md) — contribution lifecycle and DCO requirements.
- [Development Guide](DEVELOPMENT_GUIDE.md) — repository setup, build, validation, and implementation workflow.
- [Developer Preview 0 Design](DEVELOPER_PREVIEW_0_DESIGN.md) — approved scope, acceptance criteria, safety boundary, and implementation sequence.
- [Dedicated Editor Entry Architecture](DEDICATED_EDITOR_ENTRY.md) — product-host project, project-owned icons, trusted generated Windows shortcut, and separation from engine testing projects.
- [Canonical Path and Executable Trust Policy](PATH_POLICY.md) — canonical resolution, component containment, workspace/pack persistence boundaries, source-build provenance, and diagnostic overrides.
- [Atomic Workspace Transition and Schema Contract](WORKSPACE_ATOMICITY_AND_SCHEMA.md) — schema-0 migration, durable schema 1, candidate construction, cross-binding validation, atomic publication, rollback, and failure-stage tests.
- [Core and Framework Build Graph](CORE_FRAMEWORK_BUILD_GRAPH.md) — internal static targets, unique source ownership, dependency direction, test linkage, enforcement, and runtime boundary.
- [Economy Acquisition Coverage Contract](ECONOMY_ACQUISITION_COVERAGE.md) — Core analysis ownership, lane/status semantics, evidence and blocker handling, determinism, non-mutation, and runtime boundary.
- [Economy Cross-Pack Duplicate Contract](ECONOMY_CROSS_PACK_DUPLICATES.md) — exact signal semantics, pack gating, candidate health, determinism, non-mutation, and runtime boundary.
- [FoA Adapter Contracts](FOA_ADAPTER_CONTRACTS.md) — Core registry and compatibility ownership, typed capability vocabulary, SemVer policy, proof ordering, and Slice 9 boundary.
- [Architecture](ARCHITECTURE.md) — layers, responsibilities, invariants, and data flow.
- [Governance Reliability Baseline](GOVERNANCE_HARDENING.md) — typed transitions, shared record/relationship logic, intrinsic audit atomicity, publish-after-save persistence, and required failure tests.
- [Code Quality](CODE_QUALITY.md) — mandatory C++, Qt, persistence, UI, testing, and evidence standards.
- [Review and Merge Policy](REVIEW_AND_MERGE_POLICY.md) — design review, self-review, pull-request review, and merge gates.
- [Release Process](RELEASE_PROCESS.md) — versioning, release evidence, packaging, and rollback.
- [Maintainer Checklist](MAINTAINER_CHECKLIST.md) — recurring repository and release responsibilities.
- [Legal and Content Policy](LEGAL_AND_CONTENT_POLICY.md) — redistribution and proprietary-content boundaries.

### Project governance

- [Governance](../../GOVERNANCE.md)
- [Code of Conduct](../../CODE_OF_CONDUCT.md)
- [Roadmap](../../ROADMAP.md)
- [Changelog](../../CHANGELOG.md)

## Documentation authority

When documents conflict, use this order:

1. security policy and licence obligations;
2. governance and review policy;
3. architecture and data-format contracts;
4. code-quality standard;
5. user and development guides;
6. examples and issue discussions.

A merged architecture decision or schema migration may supersede an older section. The change must update the affected documentation in the same pull request.

## Current product status

The project is pre-alpha. Current implemented editor workflows cover:

- internal `Core.Static` and `Framework.Static` build targets with one-way dependencies, unique production-source ownership, and tests linked to production libraries instead of recompiling them;
- durable workspace schema 1 with validated schema-0 migration and atomic all-or-nothing workspace transitions;
- workspace and exact game profile;
- mod and content-pack projects;
- source and evidence intake;
- canonical catalog browsing, relationships, validation history, governance history, and reviewed claim promotion;
- independent maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession;
- typed atomic governance and publish-after-save catalog transactions;
- typed item and recipe profiles, joins, station references, and acquisition relationships;
- read-only action lanes, station/learnability evidence rows, economy acquisition coverage, and exact cross-pack duplicate candidate reports;
- typed transient adapter declarations, semantic-version compatibility, capability/permission/proof evaluation, and a read-only adapter capability matrix;
- Developer Preview 0 prerequisite checks, configure/build wrapping, deterministic synthetic fixture, SHA-256 verification, and service-level load/save/reopen smoke coverage;
- canonical workspace and pack path enforcement with component containment and filesystem-link escape rejection;
- dedicated `TaintedGrailModdingEditor` O3DE project with `TaintedGrailModdingSDK` enabled and project-owned PNG/ICO assets;
- generated `Tainted Grail Modding Editor.lnk` whose trusted target, project, working directory and icon are derived from repository-owned policy rather than its sidecar;
- explicit diagnostic-only shortcut overrides that cannot replace or verify as the standard entry;
- command-line opening through `developer_preview_open.py`;
- explicit local diagnostics collection with path/secret redaction, allow-listed files, log excerpts, workspace-relative hashes, manifest verification, and no automatic upload;
- Windows manual UI checklist and screenshot-evidence tooling with exact-commit binding, PNG hashes, required coverage, and privacy/runtime attestations.

The actual Windows screenshot pass remains pending. Work-order generation, runtime adapters, production deployment, and remaining specialised domain tools are not complete.

## Documentation contribution rules

Documentation changes must describe current behavior honestly, distinguish implemented from planned behavior, avoid proprietary content and private paths, use stable links, and include migration notes for breaking format changes.
