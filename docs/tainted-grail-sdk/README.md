# Tainted Grail Modding Editor and SDK Documentation

This directory contains the public documentation for the TG SDK layer in this O3DE fork.

## Start here

### For users

- [Developer Preview 0](DEVELOPER_PREVIEW_0.md) — Windows x64 prerequisite, configure, build, and full focused-validation commands plus current preview limitations.
- [User Guide](USER_GUIDE.md) — build, open, configure, and use the foundation editor tools.
- [Catalog Guide](CATALOG_GUIDE.md) — canonical search, record inspection, evidence promotion, relationships, validation, and blockers.
- [Governance Engine Guide](GOVERNANCE_ENGINE_GUIDE.md) — independent maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession decisions.
- [Item and Recipe Editor Guide](ITEM_RECIPE_EDITOR_GUIDE.md) — typed item and recipe profiles, ingredients, outputs, stations, acquisition relationships, and governed action lanes.
- [Recipe Station Evidence View](RECIPE_STATION_EVIDENCE_VIEW.md) — read-only station visibility and learnability research, evidence health, governance, and blockers.
- [Data Formats](DATA_FORMATS.md) — workspace, pack, source, evidence, issue, catalog, validation, governance-history, and typed-domain contracts.
- [Support](../../SUPPORT.md) — where and how to ask for help.
- [Security](../../SECURITY.md) — private reporting and secure-use guidance.
- [Privacy](PRIVACY.md) — local data handling and telemetry requirements.
- [Accessibility](ACCESSIBILITY.md) — user-interface accessibility standard.
- [Glossary](GLOSSARY.md) — shared project terminology.

### For contributors

- [Contributing](../../CONTRIBUTING.md) — contribution lifecycle and DCO requirements.
- [Development Guide](DEVELOPMENT_GUIDE.md) — repository setup, build, validation, and implementation workflow.
- [Developer Preview 0 Design](DEVELOPER_PREVIEW_0_DESIGN.md) — approved scope, acceptance criteria, safety boundary, and implementation sequence for the usability milestone.
- [Architecture](ARCHITECTURE.md) — layers, responsibilities, invariants, and data flow.
- [Governance Reliability Baseline](GOVERNANCE_HARDENING.md) — typed transitions, shared record/relationship logic, intrinsic audit atomicity, publish-after-save persistence, and required failure tests.
- [Code Quality](CODE_QUALITY.md) — mandatory C++, Qt, persistence, UI, testing, and evidence standards.
- [Review and Merge Policy](REVIEW_AND_MERGE_POLICY.md) — design review, self-review, pull-request review, and merge gates.
- [Data Formats](DATA_FORMATS.md) — versioned durable-document requirements.
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

- workspace and exact game profile;
- mod and content-pack projects;
- source and evidence intake;
- canonical catalog browser, record inspector, first-class relationships, validation history, and reviewed claim promotion;
- independent maturity, confidence, operational-risk, validation, staleness, permission, prohibition, and supersession authoring;
- typed and intrinsically atomic governance transitions shared by records and relationships;
- publish-after-save catalog transactions and rollback-focused governance tests;
- durable governance history and proof-linked permission decisions;
- typed item and recipe profiles, ingredient/output joins, station references, and acquisition relationships;
- read-only item/recipe action-lane matrices backed by catalog governance;
- read-only recipe station visibility and learnability evidence rows derived from exact identities, relationships, evidence, governance, and blockers;
- foundation status, coverage, import issues, governance and economy blockers, and usage totals;
- the first Developer Preview 0 command layer for Windows x64 prerequisite checks, O3DE configure/build wrapping, ordered focused validation, unit tests, and machine-readable results.

The preview fixture, automated reload smoke harness, launch wrapper, diagnostics bundle, verified runnable archive, runtime adapters, production deployment, and the remaining specialised domain tools are not complete.

## Documentation contribution rules

Documentation changes must:

- describe current behavior honestly;
- distinguish implemented, planned, and experimental behavior;
- avoid publishing proprietary game content or private paths;
- use stable internal links;
- update examples when schemas change;
- include migration notes for breaking changes;
- use clear, direct language and define project-specific terms.
