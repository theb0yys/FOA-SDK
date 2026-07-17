# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows the principles of Keep a Changelog. Version numbers follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe capability slices before a tagged release exists.

## Unreleased

### Added

- O3DE `TaintedGrailModdingSDK` editor Gem and host-tool registration.
- Foundation contract validator and focused GitHub Actions workflow.
- Workspace and exact FoA game-profile management with durable `*.tgworkspace.json` documents.
- Mod/content-pack project management with durable `*.tgpack.json` manifests.
- Pack ownership, compatibility, dependency, save-impact, resource, build, and release declarations.
- Source and evidence intake with structured JSON/CSV import, generic fingerprinted artifacts, SHA-256 provenance, exact profile binding, and durable source/evidence documents.
- Canonical catalog browser and record inspector.
- Durable `Catalog/catalog.tgcatalog.json` documents bound to workspace and exact game profile.
- Exact-reference/GUID, alias, subject, domain, kind, pack, evidence, maturity, confidence, risk, validation, staleness, permission, blocker, and supersession search.
- First-class catalog relationships, validation history, and governance history.
- Evidence-backed claim promotion into reviewed-but-unvalidated, staleness-unknown records.
- Native exact-reference uniqueness and synthetic pack-ownership rules.
- **Tainted Grail Catalog Governance** editor pane.
- Independent maturity, confidence, operational-risk, validation, staleness, permission, prohibition, and supersession authoring for records and relationships.
- Validation decisions bound to exact profile/version/branch, evidence, method, and named validator.
- Proof-backed named usage permissions with separate allow, forbid, and clear outcomes.
- Automatic permission revocation for stale, failed, blocked, and superseded subjects.
- Durable governance events containing previous/new values, evidence, validation proof IDs, reviewer, time, and notes.
- Governance blockers for unknown assessments, stale state, missing permission proof, missing history references, and profile mismatch.
- Strong internal governance types for subject kind, axis, maturity, confidence, risk, validation, staleness, and permission decisions while retaining schema-1 string compatibility.
- Shared `GovernedSubjectState` transition handling for records and relationships.
- Intrinsically atomic governance and validation services that return complete candidates without mutating caller-owned catalogs.
- Testable publish-after-save catalog transaction coordination.
- Governance hardening tests for malformed or mismatched evidence, wrong profiles, duplicate audit IDs, persistence failure, rollback, corrupted documents, and typed-value misspellings.
- Governance Reliability Baseline contributor documentation.
- **Tainted Grail Item and Recipe Editor** editor pane.
- Typed economy item profiles keyed to canonical `economy/item` records.
- Typed recipe profiles keyed to canonical `economy/recipe` records.
- Canonical station references and typed ingredient, output, and by-product joins.
- Item quest/unique/hidden flags plus localisation, icon, asset, value, weight, quality, rarity, stack, and durability fields.
- Recipe type, tab, unlock, duplicate-key, persistence-description, and hidden-state fields.
- First-class economy acquisition relationships: `sold_by`, `dropped_by`, `found_at`, `rewarded_by`, `learned_from`, `granted_by`, and `crafted_at`.
- Read-only item and recipe action-lane matrices derived from catalog governance.
- Economy blockers for missing profiles/evidence/stations/outputs, unresolved joins, quest/unique risks, and identity-incompatible lanes.
- Economy authoring unit tests covering profile identity, station references, joins, quantity/chance constraints, canonical round trips, acquisition defaults, and lane status.
- Item and Recipe Editor user guide and typed economy data-format documentation.
- Full public project README, user, catalog, governance-engine, architecture, development, data-format, quality, review, governance, security, support, conduct, legal/content, privacy, accessibility, release, roadmap, glossary, changelog, and maintainer documentation.
- GitHub issue forms, pull-request template, TG SDK CODEOWNERS rules, and CI-enforced documentation contracts.

### Changed

- Catalog records now carry an explicit staleness state.
- Catalog relationships now use the same independent maturity, confidence, risk, validation, staleness, permission, prohibition, conflict, missing-reference, and supersession model as records.
- Record and relationship governance transitions now execute through one typed state machine instead of duplicated branches.
- Governance and validation APIs now accept immutable catalog inputs and return complete state-plus-history candidates only after all in-memory checks succeed.
- Catalog publication now occurs only from a successful `CatalogTransactionService` result after persistence completes.
- Validation events can target records or relationships while retaining the legacy record-only binding for schema-1 compatibility.
- Canonical catalog schema 1 accepts optional governance-history, typed economy arrays, and extended state fields without reinterpreting existing identity data.
- Foundation status snapshots now count relationships, validation events, governance decisions, typed item/recipe profiles, recipe joins, stale subjects, allowed usages, and prohibitions.
- Catalog blockers now cover missing evidence, missing pack ownership, conflicts, missing refs, unvalidated state, permission-before-validation, invalid supersession, relationship evidence, validation-profile mismatches, governance proof gaps, and economy authoring gaps.
- The roadmap advances Phase 6 with the Item and Recipe Editor as the first specialised domain tool.

### Security

- Runtime execution remains disabled in editor-owned workspace, pack, source/evidence, catalog, validation, governance, and typed economy workflows.
- Source intake rejects missing or mismatched profile and fingerprint bindings.
- Structured imports are size-limited and fail closed on malformed schemas.
- Catalog promotion cannot grant allowed usages and adds `no_unvalidated_runtime_use`.
- Validation never grants permission automatically.
- Allowed usage requires a separate reviewed permission event backed by validated proof for the same subject.
- Stale, failed, blocked, and superseded transitions remove existing allowed usages.
- A failed governance-history append cannot leave changed state in a caller-owned catalog.
- A failed catalog save cannot publish a candidate into the live editor state.
- Item/recipe profile and join writes reject missing or unrelated evidence before persistence.
- Native and synthetic item/recipe lanes are checked independently.
- Quest-sensitive and unique item distribution lanes receive explicit blockers.
- The Item and Recipe Editor exposes governance lanes as read-only status and cannot grant permission.
- Public contribution rules require design review, pre-commit self-review, required CI, resolved review threads, and maintainer approval before merge.

### Known limitations

- The Item and Recipe Editor does not yet generate adapter work orders.
- Station visibility, recipe learnability, runtime append, custom registration, vendor/loot mutation, reward mutation, persistence, cleanup, and rollback remain adapter-side research or implementation work.
- Permission usage names remain free-form pending typed adapter-capability contracts; the decision operation itself is strongly typed.
- Remaining actor, spawn, world, faction, quest/state, asset, and localisation authoring tools are not implemented.
- FoA runtime adapters and production deployment are not implemented.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
