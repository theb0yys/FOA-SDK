# Code Quality Standard

This standard is mandatory for project-controlled code, tests, schemas, workflows, and documentation.

## Quality goals

A change should be:

- correct;
- safe by default;
- easy to review;
- explicit about identity and ownership;
- testable and observable;
- deterministic where practical;
- portable across supported O3DE host platforms;
- maintainable by someone who did not write it;
- honest about uncertainty and incomplete proof.

## General rules

- Keep changes scoped and cohesive.
- Prefer clear code over clever code.
- Remove dead code, temporary diagnostics, and abandoned flags.
- Do not suppress errors merely to make validation pass.
- Do not introduce speculative abstractions without a real use.
- Avoid unrelated formatting and refactoring in feature pull requests.
- Add comments for constraints and reasoning, not line-by-line narration.
- Keep public names stable and meaningful.

## Architecture boundaries

Code must preserve the architecture invariants in `ARCHITECTURE.md`.

Forbidden in the editor and knowledge layer:

- FoA runtime API calls;
- Harmony patch execution;
- BepInEx plugin loading;
- gameplay spawning, grants, registration, or mutation;
- direct save writes;
- deployment without an explicit adapter and user action;
- automatic runtime permission from import or parsing success.

## Identity quality

- Use stable IDs for durable records.
- Preserve exact native refs and GUIDs as received.
- Store display names separately.
- Never use a display name as a database key or deduplication rule.
- Synthetic IDs must identify the owning pack.
- Aliases must not silently replace canonical identity.
- Source IDs should be deterministic when based on immutable profile and fingerprint inputs.
- Duplicate detection must specify its scope.

## Evidence quality

- A source describes an artifact; evidence describes an observation from that source.
- Parsing success is not validation.
- Evidence does not automatically create a reviewed catalog fact.
- Confidence, maturity, validation, risk, and permission remain independent.
- Unknown or unsupported fields must not be invented.
- Generic importers must not infer evidence.
- Limitations and exact locators must be retained.
- Profile and fingerprint linkage must be verified on load and registration.

## C++ rules

### Language and style

Follow the O3DE C++ style and the formatting conventions of nearby code.

- Use RAII for resources.
- Prefer value semantics when ownership is simple.
- Use references for required borrowed values and pointers for optional or ownership-significant values.
- Avoid raw owning pointers.
- Mark overrides with `override`.
- Use `const` consistently.
- Avoid global mutable state; the foundation singleton is an explicit orchestration boundary and should not become a catch-all.
- Avoid undefined behavior, unchecked indexing, and unchecked integer narrowing.

### Includes

- Include every directly used declaration.
- Do not rely on transitive includes.
- Keep headers self-contained.
- Use forward declarations only when they reduce coupling without obscuring requirements.
- Add Qt includes explicitly in `.cpp` files and only forward-declare Qt classes in headers when valid.

### Strings and encodings

- Treat external and UI text as UTF-8.
- Convert between `QString` and `AZStd::string` explicitly.
- Preserve exact refs byte-for-byte unless the format contract explicitly defines normalisation.
- Do not use locale-sensitive comparison for identifiers.
- Avoid temporary-pointer lifetime bugs during conversion.

### Containers and algorithms

- Reserve capacity when size is known and meaningful.
- Use deterministic ordering for persisted or displayed data.
- Do not expose references or pointers invalidated by routine mutation without documenting the lifetime.
- Keep duplicate detection and equality rules explicit.

### Error handling

- Return `AZ::Outcome` or an explicit success/error result for operations that can fail.
- Do not throw across O3DE/Qt boundaries unless the surrounding subsystem is explicitly exception-safe.
- Copy error strings when the source result's lifetime might end.
- Include actionable context without leaking secrets or private paths unnecessarily.
- Distinguish operation errors from persistent blockers.

## Qt and UI rules

- Widgets collect input and display service state; they do not own canonical domain logic.
- Long-running parsing, hashing, build, or scanning work must not block the UI thread once workloads become significant.
- Use labels associated with inputs and meaningful button text.
- Provide keyboard-accessible controls and reasonable tab order.
- Use word-wrapped, selectable text for long paths and errors.
- Preserve user input on recoverable errors.
- Avoid modal dialogs for routine status; reserve them for decisions or failures requiring immediate attention.
- Do not use color as the only indicator of severity.
- Register stable pane names and save keys.
- Use `Q_OBJECT` only when Qt meta-object features are needed.

## Persistence rules

- Every durable top-level document has a schema version.
- Breaking changes require a migration or an explicit unsupported-version error.
- Reads validate identity, ownership, bindings, and expected document relationships.
- Writes remain inside approved roots.
- Related multi-file writes publish in-memory state only after all required files succeed.
- Use deterministic filenames and layouts.
- Do not write secrets or credentials.
- Avoid absolute personal paths in distributable project files when a workspace-relative path is sufficient.
- File-format examples must be legal to redistribute.

## Importer rules

An importer must:

- have stable ID and version;
- declare supported kinds/extensions;
- enforce bounded file size and resource use;
- fingerprint before registration;
- bind to an exact configured game profile;
- report malformed input as structured issues;
- preserve source registration when manual extraction is appropriate;
- reject duplicate evidence IDs within the applicable document;
- avoid automatic claim promotion or permission.

Parsers should be tested with:

- valid minimal input;
- valid complete input;
- empty input;
- malformed input;
- duplicate IDs;
- missing required fields;
- oversized input;
- unusual UTF-8;
- platform line endings;
- unsupported schema versions.

## Catalog rules

- Exact-reference lookup must be deterministic.
- Search filters must not mutate records.
- Records must not merge by display name.
- Promotion requires explicit owner, identity kind, evidence links, and review state.
- Relationships requiring evidence or validation are first-class data.
- Deleting or superseding a record must preserve auditability.
- Query results should have stable ordering.

## Validation and blocker rules

- Fail closed for missing proof.
- A blocker must name the subject and reason.
- Include affected usages when known.
- Warnings must not be silently treated as permission.
- Validation state must retain enough information to understand what was checked, against which version and evidence.
- Runtime permission must be explicit and usage-specific.

## Security rules

- Treat imported files and workspace documents as untrusted.
- Validate path containment; consider `..`, symlinks, case sensitivity, and platform path rules.
- Limit memory and CPU consumption.
- Do not execute imported content.
- Avoid shell construction from untrusted strings.
- Keep deployment, process launch, and runtime adapter APIs outside generic persistence and parsing services.
- Follow `SECURITY.md` for vulnerability handling.

## Testing requirements

Every behavior change needs evidence of correctness appropriate to its risk.

Expected test layers:

- repository contract validation;
- unit tests for pure rules and queries;
- persistence round-trip and migration tests;
- parser fixture tests;
- integration tests for service orchestration;
- manual UI verification for interaction changes;
- host-platform compilation.

Higher-risk changes require stronger coverage:

- schema changes: old/new fixtures and migration tests;
- security changes: regression and abuse-case tests;
- save/deployment changes: dry run, containment, backup, and rollback tests;
- permission changes: negative tests proving unavailable usages remain blocked.

## Performance

- Avoid repeated full scans in UI refresh paths.
- Index exact refs, IDs, aliases, and common filters when catalog size requires it.
- Hash large files incrementally.
- Bound structured imports.
- Measure before adding caches.
- Document cache invalidation and persistence rules.

## Documentation quality

- Describe current behavior, not intent presented as fact.
- Label planned and experimental features.
- Keep examples internally consistent.
- Update data formats and user guide with behavior changes.
- Explain migrations and compatibility impact.
- Avoid private paths, secrets, and copyrighted game content.

## Review checklist

Reviewers should confirm:

- architecture boundary preserved;
- direct dependencies and includes are explicit;
- ownership and lifetimes are clear;
- IDs and exact refs are correct;
- error and blocker paths are covered;
- persistence and migrations are defined;
- untrusted input is bounded;
- tests cover success and failure;
- docs match behavior;
- no runtime permission is implied without proof;
- no legally restricted material is introduced.

## Definition of done

A change is done only when:

- implementation is complete;
- self-review is complete;
- focused validation passes;
- applicable tests and builds pass;
- review threads are resolved;
- documentation and changelog are updated;
- migration and compatibility effects are handled;
- maintainer approval is recorded;
- the change is merged into `main` and `foa-development` is synchronized.
