# CI, Runner, and Local Validation Policy

## Purpose

This document is the single validation matrix for FOA-SDK. Validation requirements are selected by changed surface and risk, not by a universal checklist.

Automated validation is read-only with respect to repository state and external protected systems. A bounded installer smoke may mutate only its disposable hosted-runner installation/temp state; it must not touch game files, saves, user machines, releases, or repository state.

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
- focused CTest with `--no-tests=error`;
- exact-head compilation of the Windows installer front door.

A missing executable or zero matching tests is a failure.

### L3 — Editor/UI/manual host interaction

Proves behavior that requires an actual Editor or platform interaction: pane lifecycle, visual rendering, keyboard/accessibility behavior, saved UI state, or a manual workflow.

Screenshots and logs must exclude private paths and protected/proprietary content.

### L4 — Operational/runtime evidence

Proves an operation that leaves the authoring host or can mutate external state: installer lifecycle, deployment, rollback, save behavior, runtime adapter behavior, Fall of Avalon launch/verification, signing, or publication.

L4 evidence is operation-specific. One L4 lane does not prove another. A disposable-runner installer fixture can prove the installer control path and guided finish actions while still not proving the complete packaged O3DE payload, manual visual-quality inspection, signing, or release publication.

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
- classifies whether the changed paths can affect the O3DE host/build, Windows prerequisite, or Windows installer surfaces.

### Conditional host jobs

The host/build jobs are conditional:

- `tool-execution-operational` runs for the selected compiled host surface, builds both M2 test targets plus the native fixture, requires that fixture to exist, and runs nonzero matching CTest cases;
- `canonical-interchange-compiled` runs only when C++/CMake/Gem/project/O3DE-lock or validation-workflow paths can affect the compiled host surface;
- `windows-prerequisites` runs only when Developer Preview, project, O3DE-lock, or validation-workflow paths can affect Windows prerequisites;
- `windows-installer-smoke` runs only when Windows installer launcher, packaging, installer-test, or validation-workflow paths can affect the installer front door.

Documentation-only, governance-only, `.codex` helper-only, PR-template-only, and unrelated Python-policy changes do not wait on O3DE configure/build or installer jobs merely for ceremony.

When selected, the canonical-interchange job uses pinned `windows-2022`, checks out the exact pinned O3DE commit, builds with bounded `--parallel 2`, and executes CTest with `--no-tests=error`.

When selected, the Windows-prerequisite job checks the pinned O3DE policy surface and Developer Preview prerequisites. It does not claim a full Editor build.

When selected, `windows-installer-smoke` uses a disposable `windows-2022` runner and `Installer/Tests/WindowsInstallerLauncher/Invoke-FoaInstallerWindowsSmoke.ps1`. The smoke:

- builds a bounded self-contained `FOA-SDK.exe` fixture whose `--self-test` is deterministic;
- creates an MSI fixture with `INSTALL_MANIFEST.json`, `SHA256SUMS`, and the launcher;
- builds the exact event-head self-contained `FOA-SDK-Installer.exe` with that MSI embedded;
- constructs the normal WinForms wizard through `--smoke-test`;
- performs a real Windows Installer clean install;
- requires installed-file SHA-256 validation followed by startup self-test validation;
- deliberately damages the installed launcher and proves Repair restores the reviewed bytes;
- builds a bad-integrity MSI fixture and proves the installer refuses to report success;
- uses Windows UI Automation to open the real `FOA-SDK Setup` window, select **Install**, and wait for the **FOA-SDK is ready** finish screen;
- verifies **Open FOA-SDK** and **Create desktop shortcut** are selected by default, selects **Finish**, proves the installed entry point was launched, and verifies the desktop shortcut targets that installed `FOA-SDK.exe`;
- uninstalls every fixture installation and proves an external workspace sentinel survives;
- preserves a machine-readable smoke summary and installer logs as hosted-runner evidence.

This automatic smoke proves the exact installer executable/control path and the guided finish actions on Windows against the bounded fixture payload. It does **not** prove the complete multi-GB O3DE product payload, the canonical MSI's Start Menu registration, manual visual quality, code signing, distribution provenance, or public release behavior.

A skipped conditional job is `NOT_APPLICABLE` for that reviewed path set, not a pass or failure.

The canonical installer workflow builds the compiled Catalog test target, the O3DE
`INSTALL` target, and the installed `FOA-SDK.exe` launcher with `--parallel 2`.
It copies that launcher into the install layout, hash-compares it with the
reviewed build output, and inventories only the O3DE install root. Inventory
mode produces an exact fingerprint and notices for review. Package mode accepts
review metadata only from the human operator invoking the canonical workflow;
no repository automation may synthesize reviewer identity, review time,
evidence, or approval. Package mode stages the complete payload, verifies the
staged manifest, runs `bin\Windows\profile\Default\FOA-SDK.exe --self-test`
from that staged self-contained layout with root `engine.json` and writable
materialized LocalAppData `External` Gem roots plus project, and only then
creates the portable ZIP, MSI, embedded installer wizard, and
functional-readiness smoke artifacts.

## Full local validation

When the changed surface requires the broad existing host suite, use a complete exact pinned O3DE checkout and configured external build directory:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --engine-root ../o3de \
  --ctest-build-dir release/revisions/tg-sdk-developer-preview-0-windows-profile
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

Host-heavy Editor, repository-hygiene, foundation, and the **canonical full-product installer/package workflow** remain manual/read-only where currently configured. They must not write repository state.

The bounded automatic `windows-installer-smoke` is not a release/package workflow and cannot supply redistribution review metadata, sign, publish, deploy, or operate on protected game data.

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

## Capability execution M1 validation

M1 follows its merged [implementation authority](CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md).
The automatic read-only `capability-execution-compiled` Windows job checks out the exact PR
head and pinned O3DE, builds `TaintedGrailModdingSDK.CapabilityExecution.Tests`, and runs its
CTest registration with `--no-tests=error`. This Core/AzTest target uses NO_UNITY and its own
three-file manifest. No production source is recompiled inside the test target.

`run_local_validation.py` includes `validate_capability_execution_contracts.py` in static
mode. Full mode runs an independent M1 CTest command after legacy Catalog/CanonicalInterchange,
so a missing M1 test cannot be hidden by successful legacy tests. Python adversarial fixtures
exercise missing files, altered types, dependency/ownership changes and CI permission drift.

Required local evidence includes prerequisites, pinned configure, Core and dedicated tests,
legacy compiled regression and Editor/AssetProcessorBatch build compatibility. Reused host
build inputs must be from the same pin and configuration, with product targets rebuilt from
the reviewed checkout. M1 has no operational consumers; Editor/UI, provider processes,
deployment/rollback execution and game runtime proof are NOT_APPLICABLE. Compilation and
synthetic receipt tests must not be reported as those forms of operational evidence.

## M2 Tool Execution Service validation

The accepted M2 design requires L0-L4. Static validation includes both
ExternalToolchain validators and their negative unit tests. The separate pure
execution Core and host libraries must build against the locked O3DE revision;
run both `ExternalToolchain.Execution.Tests` and
`ExternalToolchain.Execution.Operational.Tests`, plus existing discovery, M1
and canonical-interchange regressions. Build the fixture and affected Editor
modules before running their tests. A reused dependency build is permissible
only at the same pin/configuration; it does not replace changed-target builds.

The operational CTest command supplies `FOA_M2_FIXTURE` with the built native
fixture path. A missing fixture, unavailable LPAC, zero matching tests or failed
isolation probe is not a pass. Network initialization failure cannot substitute
for observing access denial on a functioning socket API. A child that cannot
start cannot prove descendant cleanup. No test may silently grant additional
capabilities or launch an unsandboxed fixture to obtain a green result.

L3 starts and normally closes the Editor with M2 connected but default-denied,
and observes the service's activation/deactivation markers. A startup exception,
forced termination or successful link alone does not satisfy that lifecycle row.
L4 uses disposable repository-owned native fixtures and private resource roots;
it grants no game, deployment or release authority. Preserve exact command,
source snapshot, build configuration, actual test counts, timing, failures and
skips in private evidence. An incomplete operational or lifecycle row leaves M2
PARTIAL even when all contract tests pass.
