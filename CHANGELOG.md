# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows the principles of Keep a Changelog. Version numbers follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe merged capability slices before a tagged release exists.

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
- Public project README, user and contributor documentation, governance, security, support, review, quality, and release policies.
- GitHub issue forms, pull-request template, and CODEOWNERS rules.

### Security

- Runtime execution remains disabled in editor-owned workspace and pack formats.
- Source intake rejects missing or mismatched profile and fingerprint bindings.
- Structured imports are size-limited and fail closed on malformed schemas.

### Known limitations

- The canonical catalog browser and record inspector are under development.
- Domain authoring tools and FoA runtime adapters are not implemented.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
