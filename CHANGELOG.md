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
- Governance transition unit tests covering validation/permission separation, proof requirements, stale-state revocation, and supersession.
- Full public project README, user, catalog, governance-engine, architecture, development, data-format, quality, review, governance, security, support, conduct, legal/content, privacy, accessibility, release, roadmap, glossary, changelog, and maintainer documentation.
- GitHub issue forms, pull-request template, TG SDK CODEOWNERS rules, and CI-enforced documentation contracts.

### Changed

- Catalog records now carry an explicit staleness state.
- Catalog relationships now use the same independent maturity, confidence, risk, validation, staleness, permission, prohibition, conflict, missing-reference, and supersession model as records.
- Validation events can target records or relationships while retaining the legacy record-only binding for schema-1 compatibility.
- Canonical catalog schema 1 accepts optional governance-history and extended state fields without reinterpreting existing identity data.
- Foundation status snapshots now count relationships, validation events, governance decisions, stale subjects, allowed usages, and prohibitions.
- Catalog blockers now cover missing evidence, missing pack ownership, conflicts, missing refs, unvalidated state, permission-before-validation, invalid supersession, relationship evidence, validation-profile mismatches, and governance proof gaps.
- The roadmap advances specialised domain authoring tools to the next capability area after governance-engine integration.

### Security

- Runtime execution remains disabled in editor-owned workspace, pack, source/evidence, catalog, validation, and governance workflows.
- Source intake rejects missing or mismatched profile and fingerprint bindings.
- Structured imports are size-limited and fail closed on malformed schemas.
- Catalog promotion cannot grant allowed usages and adds `no_unvalidated_runtime_use`.
- Validation never grants permission automatically.
- Allowed usage requires a separate reviewed permission event backed by validated proof for the same subject.
- Stale, failed, blocked, and superseded transitions remove existing allowed usages.
- Catalog persistence publishes in-memory changes only after a successful workspace-contained document write.
- Public contribution rules require design review, pre-commit self-review, required CI, resolved review threads, and maintainer approval before merge.

### Known limitations

- The first specialised domain authoring tools are not implemented.
- Permission vocabulary is currently free-form and will become adapter-capability aware in later slices.
- Governance decisions do not generate runtime work orders or invoke FoA adapters.
- Domain authoring tools and FoA runtime adapters are not implemented.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
