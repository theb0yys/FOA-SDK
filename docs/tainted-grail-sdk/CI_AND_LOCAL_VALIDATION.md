# CI, Runner, and Local Validation Policy

## Purpose

This document is the single validation matrix for FOA-SDK. Validation requirements are selected by changed surface and risk, not by a universal checklist.

Automated validation is read-only.

## Evidence layers

### L0 — Repository and static validation

Proves repository structure, policy contracts, reviewed-range whitespace, Python/static validators, and other non-compiled checks.

Typical command:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going --static-only --skip-source-policy
```

`--static-only` does not prove compilation, Editor behavior, runtime behavior, or source-policy validation.

### L1 — Focused unit and contract tests

Proves executable behavior in the focused test layer: Python unit tests, deterministic contract tests, malformed-input tests, migration tests, or other owned unit suites.

L1 should target the changed subsystem rather than requiring every unrelated test surface.

### L2 — Configure, build, and compiled host tests

Proves integration with the pinned O3DE checkout and compiled targets.

Examples include:

- Developer Preview prerequisites/configure;
- affected O3DE build targets;
- focused CTest with `--no-tests=error`.

A missing executable or zero matching tests is a failure.

### L3 — Editor/UI/manual host interaction

Proves behavior that requires an actual Editor or platform interaction: pane lifecycle, visual rendering, keyboard/accessibility behavior, saved UI state, or a manual workflow.

Screenshots and logs must exclude private paths and protected/proprietary content.

### L4 — Operational/runtime evidence

Proves an operation that leaves the authoring host or can mutate external state: installer lifecycle, deployment, rollback, save behavior, runtime adapter behavior, Fall of Avalon launch/verification, signing, or publication.

L4 evidence is operation-specific. One L4 lane does not prove another.

## Validation matrix

| Change surface | Classification default | Required evidence |
| --- | --- | --- |
| Documentation text only | Routine | L0 targeted documentation/static validation |
| Process/governance/validation policy | Significant | L0 targeted policy/static validation and policy-validator tests |
| Python tooling/validators | Routine | L0 + focused L1 |
| C++ logic inside an existing target | Routine | L0 + focused L2 compiled tests; L1 where separate contract tests exist |
| CMake/Gem/project/build graph | Significant when a contract changes | L0 + relevant L2 configure/build/tests |
| Public API/contract | Significant | L0 + relevant L1/L2 + compatibility/negative coverage |
| Persistence/schema/migration | Significant | L0 + migration/malformed/round-trip L1/L2 as owned by the implementation |
| Editor/UI behavior | Routine or Significant | L0 + relevant L2 + L3 |
| External process/provider execution | Critical/Runtime | L0-L2 as applicable + exact operational L4; L3 when the operator workflow matters |
| Deployment/save/runtime adapter | Critical/Runtime | L0-L2 as applicable + exact deployment/runtime L4 and rollback/recovery proof |
| Installer/release/signing/publication | Critical/Runtime | L0-L2 as applicable + the exact installer/release/signing L4 lane |

If a change spans rows, use the union of applicable evidence. Do not require a row that the change cannot affect.

## Automatic read-only validation

`.github/workflows/tainted-grail-sdk-pr-validation.yml` runs on relevant pull requests, pushes to `main`, and manual dispatch.

It uses:

```yaml
permissions:
  contents: read
```

There is no `pull_request_target` trigger. Validation must not push commits, move refs, post comments, merge pull requests, or mutate repository state.

### Static job

`static-validation` runs for every matched change. It:

- checks out the exact event head with persisted credentials disabled;
- records the reviewed base/head;
- runs `git diff --check`;
- validates the pull-request policy contract;
- runs the non-compiled repository validation layer;
- classifies whether the changed paths can affect the O3DE host/build or Windows prerequisite surfaces.

### Conditional host jobs

The host/build jobs are conditional:

- `canonical-interchange-compiled` runs only when C++/CMake/Gem/project/O3DE-lock or validation-workflow paths can affect the compiled host surface;
- `windows-prerequisites` runs only when Developer Preview, project, O3DE-lock, or validation-workflow paths can affect Windows prerequisites.

Documentation-only, governance-only, `.codex` helper-only, PR-template-only, and unrelated Python-policy changes do not wait on O3DE configure/build jobs merely for ceremony.

When selected, the canonical-interchange job uses pinned `windows-2022`, checks out the exact pinned O3DE commit, builds with bounded `--parallel 2`, and executes CTest with `--no-tests=error`.

When selected, the Windows-prerequisite job checks the pinned O3DE policy surface and Developer Preview prerequisites. It does not claim a full Editor build.

A skipped conditional job is `NOT_APPLICABLE` for that reviewed path set, not a pass or failure.

## Full local validation

When the changed surface requires the broad existing host suite, use a complete exact pinned O3DE checkout and configured external build directory:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --engine-root ../o3de \
  --ctest-build-dir ../foa-build/tg-sdk-developer-preview-0-windows-profile
```

Full local validation includes the repository static layer, pinned O3DE source policy, fixtures, and configured compiled test selection. It is not mandatory for a documentation/process-only or otherwise L0-only change.

## Exact-head validation receipts

`validation_receipt.py` remains available to bind executed commands to an exact source commit.

Use a merge-ready receipt when:

- the change is Critical/Runtime; or
- a Significant owning design explicitly requires a receipt for the affected host/operational boundary.

Routine changes do not require a receipt merely to complete a pull request.

Receipts must be stored outside the repository. A receipt hash detects modification; it does not prove signer identity or authorization.

## Manual workflows

Host-heavy Editor, repository-hygiene, foundation, and installer workflows remain manual/read-only where currently configured. They must not write repository state.

A general-purpose self-hosted runner must not execute public pull-request code. Any future self-hosted design requires isolated disposable infrastructure, no personal data or unrelated credentials, restricted triggers, and explicit operator ownership.

A runner registration token is a secret.

## Evidence reporting

For every reported check, record enough information to identify what actually ran:

- exact source/ref when relevant;
- command or hosted job;
- test target/pattern when relevant;
- result;
- any intentionally `NOT_RUN` or `NOT_APPLICABLE` layer.

Pending is not passing. Queued, skipped, absent, stale-head, wrong-commit, or zero-test results are not passes. Self-declared metadata are not proof that the repository owner authorized an action.

Use `PASSED`, `FAILED`, `PARTIAL`, `BLOCKED`, `NOT_RUN`, or `NOT_APPLICABLE`.
