# Changelog

All notable changes to the Tainted Grail Modding Editor and SDK are documented here.

The project follows the principles of Keep a Changelog. Version numbers follow Semantic Versioning once public releases begin. During pre-alpha development, entries may describe capability slices before a tagged release exists.

## Unreleased

### Added

- Internal `TaintedGrailModdingSDK.Core.Static` and `TaintedGrailModdingSDK.Framework.Static` targets with explicit Core → Framework → Editor dependency direction.
- Core/Framework build-graph validation and unit tests for unique production-source ownership, test-only manifests, target dependencies, internal aliases, and Core isolation from Qt, AzToolsFramework, and Framework headers.
- Durable workspace schema 1 with explicit legacy schema-0 migration, unknown-version rejection, stable workspace/profile identities, unique profile bindings, and project-owned fixture round-trip tests.
- `FoundationWorkspaceLoadService` and `FoundationWorkspaceLoadCandidate` for all-or-nothing workspace, source/evidence, catalog, canonical-path, and active-profile transitions.
- Direct `FoundationService` integration tests for workspace document, active-profile, path, source, import-issue, registry, catalog-load, catalog-binding, and catalog-validation failures.
- O3DE `TaintedGrailModdingSDK` editor Gem and host-tool registration.
- Foundation contract validator and focused GitHub Actions workflow.
- Approved Developer Preview 0 design for a source-built Windows x64 usability milestone with deterministic fixtures, smoke verification, diagnostics, and a legally reviewed artifact path.
- Developer Preview 0 prerequisite, configure, build, and ordered validation command layer for Windows x64 Profile.
- Non-installing checks for Python, CMake, Git, Git LFS, Visual Studio C++ tools, repository identity, build-directory ownership, and expected Editor output.
- Dry-run CMake wrappers for the approved `windows-vs-unity` x64 configure path and Profile `Editor` plus `TaintedGrailModdingSDK.Catalog.Tests` build targets.
- Machine-readable Developer Preview prerequisite and validation results with original child-process exit-code propagation.
- Developer Preview command contract validator and unit coverage for argument construction, path safety, dry runs, deterministic validation order, JSON output, unsupported hosts, and failure propagation.
- Dedicated repository-owned `TaintedGrailModdingEditor` O3DE project with `TaintedGrailModdingSDK` enabled, deterministic sibling-engine resolution, and project-owned preview PNG and Windows shortcut ICO.
- `developer_preview_open.py` command-line Editor opener that selects the dedicated project and delegates to the restricted launch wrapper.
- Low-level `developer_preview_shortcut.py` Windows `.lnk` generator with a project-owned icon, fixed `--project-path`, explicit replacement, SHA-256 manifest output, and verification.
- Supported `developer_preview_entry.py` clickable-entry command with semantic Windows shortcut inspection and verified-before-replace behavior.
- Dedicated-entry project, opener, shortcut, and hardened-entry contract validation with twenty-one focused unit tests and a Windows workflow.
- Deterministic Developer Preview 0 fixture generator containing only project-owned synthetic `preview.*` identities, portable paths, runtime-disabled pack data, source/evidence, catalog, governance, and economy documents.
- Canonical `preview-fixture.manifest.json` output with relative paths, byte sizes, and SHA-256 digests for all fixture payloads.
- Fixture verification for canonical JSON, exact file sets, hashes, path traversal, symlinks, private paths, source/evidence binding, catalog identity and ownership, relationship targets, governance proof links, and economy joins.
- Seventeen fixture tests covering byte-for-byte determinism, safe replacement, tamper detection, manifest safety, bindings, reserved namespaces, and allowed, forbidden, blocked, stale, and unresolved example states.
- Shared persistence compatibility loading for documented plain schema-1 documents and existing O3DE `JsonSerialization` envelopes.
- Service-level Developer Preview persistence smoke coverage using the real workspace, pack, source/evidence, catalog, registry, database, and economy evidence services.
- Canonical-state comparison across load, save, close-equivalent state disposal, and reopen, including governance history, economy profiles/joins, and station/learnability evidence rows.
- Compatibility tests proving reviewed proof-backed allowances survive catalog load while unproven allowances are cleared fail-closed.
- Controlled Windows x64 `Editor.exe` launch wrapper with explicit executable/build selection, O3DE `--project-path` support, dry-run output, wrapper-owned logs, pane-registration guidance, and exact process exit-code propagation.
- Redacted local diagnostics collector and verifier with tool/build summaries, supplied validation and launch summaries, tail-limited Editor log excerpts, workspace-relative durable-document hashes, an allow-listed file set, SHA-256 manifest verification, and review-before-sharing guidance.
- Twenty-five launch and diagnostics unit tests covering command restrictions, host checks, project validation, exit propagation, path/secret redaction, inventory limits, symlinks, overwrite protection, tamper detection, traversal rejection, and collect→verify behavior.
- Developer Preview troubleshooting documentation for missing Editor output, clickable-entry failures, absent TG SDK panes, native log locations, diagnostics failures, verification, and disclosure review.
- Windows manual UI checklist covering all TG SDK panes, normal scaling, keyboard traversal, synthetic data display, Item and Recipe Editor state, station/learnability evidence, save-close-reopen behavior, actionable failure messages, and the runtime boundary.
- Screenshot-evidence initializer, recorder, PNG attachment helper, final attestation, and verifier bound to an exact source commit.
- Screenshot evidence checks for PNG integrity, dimensions, size limits, SHA-256 hashes, required checklist coverage, path traversal, symlinks, unexpected files, textual private paths, secret-like material, privacy review, and no-runtime attestations.
- Sixteen manual UI evidence unit tests covering pending initialization, output containment, note privacy, PNG validation, evidence completion, tampering, missing coverage, symlinks, unexpected files, and commit mismatch.
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
- Read-only recipe station visibility and learnability evidence rows combining exact station IDs, profile declarations, `crafted_at` and `learned_from` relationships, evidence health, governance state, and blockers.
- Economy blockers for missing profiles/evidence/stations/outputs, unresolved joins, quest/unique risks, and identity-incompatible lanes.
- Economy authoring unit tests covering profile identity, station references, joins, quantity/chance constraints, canonical round trips, acquisition defaults, lane status, deterministic evidence rows, fail-closed evidence/governance, and non-mutation.
- Item and Recipe Editor user guide, recipe station evidence guide, and typed economy data-format documentation.
- Full public project README, user, catalog, governance-engine, architecture, development, data-format, quality, review, governance, security, support, conduct, legal/content, privacy, accessibility, release, roadmap, glossary, changelog, and maintainer documentation.
- GitHub issue forms, pull-request template, TG SDK CODEOWNERS rules, and CI-enforced documentation contracts.

### Changed

- Production implementation files are compiled exactly once by Core, Framework, or Editor; catalog and path-policy tests now link `Framework.Static` instead of recompiling private copies of production `.cpp` files.
- The Editor Gem target now owns only Qt views, the Editor system component, and module composition; persistence, path policy, loading, and Foundation orchestration are owned by `Framework.Static`.
- The focused TG SDK workflow now runs the Core/Framework validator and its focused unit tests before the existing Developer Preview and foundation contracts.
- Workspace loading now builds and validates a complete candidate before publishing any workspace, canonical path, registry, import issue, catalog, catalog path, or snapshot state.
- Workspace persistence now emits explicit schema-1 JSON; unversioned schema-0 documents are migrated only when every current invariant can be validated safely.
- The focused TG SDK workflow runs the atomic workspace/schema contract validator in addition to Developer Preview, path-policy, governance, catalog, and source-policy checks.
- The focused TG SDK workflow runs Developer Preview command, project-integration, opener, fixture, launch, diagnostics, and manual UI evidence tests; command/project/fixture/persistence/launch-diagnostics/manual-evidence contract validators; clean fixture generation/verification; and a clean diagnostics collect→verify cycle before existing foundation, governance, catalog, and source-policy checks.
- A dedicated Windows workflow validates the product-host project, icon assets, command-line opener, low-level shortcut generator, hardened clickable entry, dry-run plan, and a real COM-created `.lnk`.
- `AutomatedTesting` is restored to its upstream engine-testing role and no longer enables or hosts the TG SDK preview product.
- The catalog test target links the real persistence services and receives the reviewed fixture-template path for service-level smoke coverage.
- Catalog compatibility loading preserves current validated allowances only when the latest reviewed permission event has valid proof for the same subject; unproven legacy allowances still fail closed.
- Release and maintainer procedures require exact-commit Windows manual UI evidence, screenshot hashes, privacy review, verifier success, and non-commit handling before a Developer Preview claim.
- Catalog records carry an explicit staleness state.
- Catalog relationships use the same independent maturity, confidence, risk, validation, staleness, permission, prohibition, conflict, missing-reference, and supersession model as records.
- Record and relationship governance transitions execute through one typed state machine instead of duplicated branches.
- Governance and validation APIs accept immutable catalog inputs and return complete state-plus-history candidates only after all in-memory checks succeed.
- Catalog publication occurs only from a successful `CatalogTransactionService` result after persistence completes.
- Validation events can target records or relationships while retaining the legacy record-only binding for schema-1 compatibility.
- Canonical catalog schema 1 accepts optional governance-history, typed economy arrays, and extended state fields without reinterpreting existing identity data.
- Foundation status snapshots count relationships, validation events, governance decisions, typed item/recipe profiles, recipe joins, stale subjects, allowed usages, and prohibitions.
- Catalog blockers cover missing evidence, missing pack ownership, conflicts, missing refs, unvalidated state, permission-before-validation, invalid supersession, relationship evidence, validation-profile mismatches, governance proof gaps, and economy authoring gaps.
- The roadmap advances Phase 6 with the Item and Recipe Editor and its first station/learnability evidence coverage view.

### Security

- The Core/Framework split remains host-tool-only and exposes no internal static target through Tool or Builder aliases; it adds no runtime adapter, deployment, launch, injection, telemetry, or save behavior.
- Failed workspace candidates cannot publish partial objects, change the live canonical workspace path, redirect pack containment, or replace the previous Foundation snapshot.
- Workspace root, output, staging, deployment, diagnostics, extracted-data, managed-assembly, and Mono plugin paths are validated before publication and save.
- Developer Preview fixture generation performs no network access or process launch and refuses silent overwrite of unrelated, partial, modified, or symlink-containing output directories.
- Fixture verification rejects absolute/private paths, secret-like material, path traversal, borrowed native identities, ownership mismatches, unknown evidence, malformed relationships, and binding drift.
- The persistence smoke uses temporary workspace roots only, performs no runtime or deployment action, and verifies proof-backed permission preservation separately from legacy fail-closed cleanup.
- The Editor launch wrapper accepts only `Editor.exe`, constructs only the documented optional `--project-path` argument, uses no shell command strings or arbitrary passthrough arguments, and never launches FoA or runtime tooling.
- The project opener selects only `TaintedGrailModdingEditor`, validates the dedicated project contract before launch, and delegates to the existing restricted launch wrapper.
- The low-level shortcut generator uses a fixed PowerShell COM script with explicit environment values, creates no desktop entry silently, exposes no arbitrary Editor arguments, and records the generated `.lnk` size and SHA-256.
- The supported clickable-entry command inspects the real Windows shortcut target, project arguments, working directory, icon, and description, and verifies an existing generated entry before `--replace` can remove it.
- Diagnostics collection is explicit and local-only, excludes source artifact contents, game files, saves, environment variables, credentials, binary logs, and unrestricted filesystem listings, and performs no automatic upload.
- Diagnostics verification enforces relative allow-listed paths, size limits, UTF-8 text, SHA-256 hashes, symlink/traversal rejection, and path/secret redaction before a bundle can be replaced or shared.
- Manual UI evidence tooling does not capture screenshots, automate UI coordinates, inspect screenshot pixels, perform OCR, access the network, or upload evidence.
- Screenshot attachment requires explicit privacy and project-owned-content review; final verification requires exact commit binding, successful launch, activation-log confirmation, all checklist items passing, required screenshot coverage, and disclosure/runtime attestations.
- Generated screenshots must remain beneath `build/` or outside the repository and are not release package contents.
- Developer Preview commands do not install dependencies silently, use shell command strings, launch FoA, invoke runtime adapters, deploy files, modify saves, or collect telemetry.
- Preview build-directory validation rejects the repository root, directories containing the checkout, `.git` paths, unrelated non-empty directories, and CMake caches bound to another source tree.
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
- The Item and Recipe Editor exposes governance lanes and recipe station/learnability research as read-only status and cannot grant permission.
- Missing, mismatched, unresolved, stale, blocked, conflicted, missing-reference, and superseded station/learnability inputs fail closed.
- Public contribution rules require design review, pre-commit self-review, required CI, resolved review threads, and maintainer approval before merge.

### Known limitations

- The Windows manual UI checklist and evidence verifier are implemented, but the actual Windows screenshot pass remains pending.
- Developer Preview 0 does not include a prebuilt or verified preview archive; the Editor must be built from source before the clickable `.lnk` is generated.
- The service-level smoke and controlled Editor process path do not themselves prove every pane visually on a real Windows desktop or prove FoA runtime compatibility.
- The UI evidence verifier checks metadata and file integrity but cannot inspect screenshot pixels; human privacy review remains mandatory.
- The Item and Recipe Editor does not yet generate adapter work orders.
- The evidence view reports station visibility and recipe learnability research but does not append, learn, register, persist, clean up, roll back, or prove runtime behavior.
- Runtime append, custom registration, vendor/loot mutation, reward mutation, persistence, cleanup, and rollback remain adapter-side research or implementation work.
- Permission usage names remain free-form pending typed adapter-capability contracts; the decision operation itself is strongly typed.
- Remaining actor, spawn, world, faction, quest/state, asset, and localisation authoring tools are not implemented.
- FoA runtime adapters and production deployment are not implemented.
- The project has not published a supported binary release.

## Release history

No tagged public releases have been published yet.
