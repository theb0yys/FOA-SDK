# CI, Runner, and Local Validation Policy

## Current status

GitHub Actions automatic triggers are temporarily suspended for this fork.
Multiple exact-head pull-request runs remained queued without acquiring either
`ubuntu-latest` or `windows-latest` and exposed no executable job steps.

The following repository-owned workflows are therefore **manual-only**:

- `Tainted Grail SDK Foundation`;
- `Tainted Grail Editor Entry`;
- `Tainted Grail Repository Hygiene`.

The inherited full-engine `AR` workflow and generic upstream `Validation`
workflow are removed from this fork. They produced broad or unavailable checks
that did not prove the TG SDK slice had been tested.

**No automated per-commit test result is claimed.** A queued, waiting, skipped,
or absent workflow is not a passing test.

## Authoritative local command

From the repository root, run:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going
```

This command runs:

- all Python unit tests beneath `Gems/TaintedGrailModdingSDK/Tools/tests`;
- the CI/runner policy validator;
- Core/Framework ownership validation;
- economy, adapter, planning, runtime-result, build, package, deployment,
  confirmation/work-order, and execution-result contract validators;
- Developer Preview, path, workspace atomicity, fixture, diagnostics, governance,
  catalog-test, and tracked-path validators;
- temporary Developer Preview fixture generation and verification;
- temporary redacted diagnostics collection and verification;
- the O3DE source-policy validator for the TG SDK Gem.

The command exits non-zero when any included check fails. `--keep-going` reports
all failures rather than stopping at the first one.

To add a compiled test run from an already configured O3DE build directory:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --ctest-build-dir build/tg-sdk-developer-preview-0-windows-profile
```

Without `--ctest-build-dir`, no compiled O3DE test result is claimed. The real
Windows pane and screenshot pass also remains a separate manual acceptance gate.

## Self-hosted runner boundary

A general-purpose self-hosted runner is not connected to pull requests in this
public repository. Public pull requests can contain untrusted code, so attaching
a personal workstation or privileged machine would create an avoidable code
execution and secret-exposure risk.

A future self-hosted runner requires a separate reviewed design with, at minimum:

- a disposable, isolated machine or virtual machine;
- no personal files, game saves, credentials, signing keys, or unrelated secrets;
- no automatic execution for fork pull requests;
- narrow runner labels and narrowly scoped workflows;
- restricted repository permissions and environment approval where appropriate;
- clean teardown or re-imaging after jobs;
- explicit operator ownership, patching, monitoring, and incident response.

Until that design exists, workflows must not use the `self-hosted runner` label.

## Registration-token handling

A runner registration token is a secret even when it is short-lived. Never paste
it into chat, issues, pull requests, screenshots, logs, source files, or shell
history that will be retained.

Any registration token exposed in a screenshot or message must be treated as
compromised: abandon it and generate a new token from the repository runner page
before configuring a runner. Do not reuse the exposed token.

## Restoring automatic CI

Automatic pull-request and push triggers may return only after a reviewed change
proves all of the following:

1. repository GitHub Actions are enabled and permitted by account policy;
2. a GitHub-hosted manual workflow starts, acquires a runner, and completes;
3. the local validation command passes on the same exact commit;
4. required checks and branch protection name only checks that can actually run;
5. queued, skipped, or approval-waiting states are not represented as success;
6. the workflow change documents its security and cost boundary.

Restoration should prefer GitHub-hosted runners. A self-hosted runner must not be
used as a shortcut around an unresolved repository or account Actions setting.

## Review evidence

Pull requests should record:

- the exact commit tested;
- the complete local command used;
- the exit result;
- whether compiled O3DE tests ran and from which build directory;
- whether the Windows manual UI pass ran;
- any skipped checks and the reason.

Local results are evidence supplied by the tester. They are not independently
verified by GitHub until automatic CI is safely restored.
