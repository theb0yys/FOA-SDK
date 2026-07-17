# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows the principles of Keep a Changelog. Version numbers follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe capability slices before a tagged release exists.

## Unreleased

### Added

- O3DE `TaintedGrailModdingSDK` editor Gem and host-tool registration.
- Foundation contract validator and focused GitHub Actions workflow.
- Workspace and exact FoA game-profile management.
- Durable `*.tgworkspace.json` documents.
- Mod and content-pack project management.
- Durable `*.tgpack.json` manifests.
- Pack ownership, compatibility, dependency, save-impact, resource, build, and release declarations.
- Source and evidence intake tool.
- Structured JSON and CSV evidence import contracts.
- Generic fingerprinted artifact intake without inferred evidence.
- SHA-256 fingerprints and exact game-profile binding.
- Durable `source.tgsource.json` and `evidence.tgevidence.json` documents.
- Import and schema issues integrated with the blocker engine.
- Canonical catalog browser and record inspector.
- Durable `Catalog/catalog.tgcatalog.json` documents bound to workspace and exact game profile.
- Exact-reference/GUID, alias, subject, domain, kind, pack, evidence, validation, permission, blocker, and supersession search.
- First-class catalog relationships and validation history.
- Evidence-backed claim promotion into reviewed-but-unvalidated records.
- Native exact-reference uniqueness and synthetic pack-ownership rules.
- Public project README, user guide, catalog guide, architecture, development, data-format, quality, review, governance, security, support, conduct, legal/content, privacy, accessibility, release, roadmap, glossary, changelog, and maintainer policies.
- GitHub issue forms, pull-request template, and TG SDK CODEOWNERS rules.
- CI-enforced presence and key requirements for the public documentation and governance set.

### Changed

- Catalog blockers now cover missing evidence, missing pack ownership, conflicts, missing refs, unvalidated state, permission-before-validation, invalid supersession, relationship evidence, and validation-profile mismatches.
- Project roadmap advances the independent maturity, risk, validation, and permission engine to the next active slice.

### Security

- Runtime execution remains disabled in editor-owned workspace, pack, source/evidence, and catalog workflows.
- Source intake rejects missing or mismatched profile and fingerprint bindings.
- Structured imports are size-limited and fail closed on malformed schemas.
- Catalog promotion cannot grant allowed usages and adds `no_unvalidated_runtime_use`.
- Catalog persistence publishes in-memory changes only after a successful workspace-contained document write.
- Public contribution rules require design review, pre-commit self-review, required CI, resolved review threads, and maintainer approval before merge.

### Known limitations

- The expanded maturity, risk, validation, permission, and prohibition authoring workflow is not complete.
- Relationship and validation-event authoring surfaces are limited; the current catalog inspector reads them from the canonical document and service APIs.
- Domain authoring tools and FoA runtime adapters are not implemented.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
