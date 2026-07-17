# Contributing to the Tainted Grail Modding Editor and SDK

Thank you for helping build a public, evidence-governed modding toolchain for **Tainted Grail: The Fall of Avalon**.

This repository is an O3DE fork. Contributions must satisfy both this project's rules and the applicable O3DE licence, source, build, and Developer Certificate of Origin requirements.

## Read before contributing

Required reading:

1. [Code of Conduct](CODE_OF_CONDUCT.md)
2. [Architecture](docs/tainted-grail-sdk/ARCHITECTURE.md)
3. [Code quality standard](docs/tainted-grail-sdk/CODE_QUALITY.md)
4. [Review and merge policy](docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md)
5. [Development guide](docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md)
6. [Security policy](SECURITY.md)

Contributions that bypass the product boundary, evidence rules, ownership model, or review requirements will not be merged.

## What contributions are welcome

- editor models, services, and user interfaces;
- importers and durable data formats;
- catalog, validation, maturity, risk, and permission logic;
- domain authoring tools;
- documentation, examples, and tests;
- diagnostic and evidence-acquisition improvements;
- separately scoped adapter contracts and non-executing handoff formats;
- accessibility, performance, build, and reliability improvements.

## Contributions that are not accepted

- copyrighted game assets or proprietary source material without redistribution rights;
- cheats, malware, credential theft, telemetry without consent, or destructive payloads;
- code that silently edits saves, deploys files, or mutates the game without explicit user action and rollback design;
- runtime actions inside the editor or Avalon Core knowledge layer;
- records created from display-name matching alone;
- invented native references, GUIDs, game facts, validation results, or runtime permissions;
- unbounded scanning of a user's system or game installation;
- dependencies with unclear licences or unnecessary supply-chain risk.

## Branch model

The repository uses only two long-lived branches:

- `main` — reviewed integration state;
- `foa-development` — active development.

Do not create feature branches in the public repository unless a maintainer explicitly authorises an exception. Do not commit directly to `main`.

## Contribution lifecycle

### 1. Open or select an issue

Use the appropriate issue form. Explain the problem, expected outcome, affected architecture, and validation plan.

A substantial change requires an issue or design proposal before implementation. Substantial changes include:

- new editor tools or services;
- schema or identifier changes;
- persistence changes;
- runtime-adapter contracts;
- build, deployment, or save-impact changes;
- new dependencies;
- changes to the security or permission model.

### 2. Receive design review

A maintainer must confirm the proposed boundary and direction before substantial implementation begins. Design approval is not approval of the final code.

The design review must answer:

- Which layer owns the change?
- What facts, evidence, validation, and permissions are involved?
- What is persisted, versioned, or migrated?
- What fails closed?
- What user data, game data, saves, or deployments could be affected?
- What tests prove the intended behavior?

### 3. Implement on `foa-development`

Keep changes focused. Do not combine unrelated refactors, generated files, formatting churn, and new behavior in one review unit.

Every commit must:

- build toward one understandable outcome;
- include DCO sign-off;
- avoid secrets and local machine paths;
- include tests or a documented validation method;
- update documentation when behavior or formats change.

### 4. Perform pre-commit self-review

Before each commit:

- review the complete diff;
- remove debug output and dead code;
- verify explicit includes and ownership;
- verify error paths and fail-closed behavior;
- check that no runtime execution entered the editor layer;
- check identifiers, schemas, and migration implications;
- run the focused validator for affected TG SDK changes.

### 5. Open a pull request to `main`

Use the repository pull-request template. Link the issue or design discussion. Explain behavior, risk, testing, documentation, compatibility, data migration, and rollback.

### 6. Address review and CI

A pull request must not be merged while required review conversations are unresolved or required checks are failing or pending.

Reviewers may request:

- code changes;
- additional tests;
- schema migration notes;
- evidence or source references;
- screenshots of editor UI;
- threat or save-impact analysis;
- a smaller or clearer change set.

### 7. Merge and synchronize

A maintainer merges approved work into `main`. After merging, `foa-development` is synchronized to the merge commit before new work begins.

## Developer Certificate of Origin

All commits require DCO sign-off:

```shell
git commit -s -m "Describe the change"
```

The sign-off certifies that you have the right to submit the contribution under the repository's licence terms.

## Commit messages

Use an imperative summary of about 72 characters or fewer when practical.

Good examples:

```text
Add exact-reference catalog search
Reject mismatched evidence profile bindings
Document pack manifest migration rules
```

Explain motivation and important constraints in the commit body when the summary is insufficient.

## Code and data requirements

All changes must follow [CODE_QUALITY.md](docs/tainted-grail-sdk/CODE_QUALITY.md). In particular:

- public models require stable type IDs and serialization versions;
- persistence formats require explicit schema versions;
- exact native references must be preserved without normalisation;
- synthetic identities must be pack-owned;
- display names are never identity keys;
- errors and blockers must be actionable;
- file writes belong in persistence services;
- UI classes should delegate domain logic to services;
- new dependencies require licence and maintenance review.

## Testing

At minimum, run:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
```

Depending on the change, also provide:

- unit tests;
- serialization round-trip tests;
- malformed-input tests;
- migration tests;
- UI screenshots or interaction steps;
- O3DE configure and host build results;
- performance measurements;
- diagnostic or evidence fixtures that are legally distributable.

Never use private game files or copyrighted assets as committed test fixtures.

## Documentation

Update the relevant public documentation in the same pull request. New data fields must be documented in [DATA_FORMATS.md](docs/tainted-grail-sdk/DATA_FORMATS.md). User-facing behavior must be reflected in [USER_GUIDE.md](docs/tainted-grail-sdk/USER_GUIDE.md).

## Security and privacy

Do not place vulnerability details, secrets, personal information, private paths, or proprietary game data in public issues or pull requests. Follow [SECURITY.md](SECURITY.md).

## Review authority

Maintainers may decline a contribution that is unsafe, out of scope, insufficiently evidenced, legally unclear, unmaintainable, or inconsistent with the project's architecture—even when the code works technically.
