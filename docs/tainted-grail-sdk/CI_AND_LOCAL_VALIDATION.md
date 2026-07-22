# CI, Runner, and Local Validation Policy

## Validation layers

FOA-SDK uses separate validation layers because a GitHub-hosted Linux job cannot
prove a Windows O3DE build or editor pass.

### Automatic pull-request obligation enforcement

The governed workflow has a narrow `pull_request_target` job for ready pull
requests. It checks out only the trusted base commit identified by
`github.event.pull_request.base.sha`; it never checks out or executes pull-request
head code. The job reads the GitHub event payload and validates the pull-request
body with the base branch copy of `validate_pr_obligations.py`.

A ready pull request must contain every mandatory obligation exactly once, mark
each one complete, and record the current 40-character PR head in its
`merge-head` marker. A new commit makes that marker stale. Missing, duplicate,
unchecked, malformed, or stale obligations cause the governor to use GitHub's
API to return the pull request to draft and fail the check.

The privileged job has only `contents: read` and `pull-requests: write`. It does
not run the static suite, install dependencies, invoke build tools, use LFS, or
execute repository code from the proposed head. Repository rules must require
both `Enforce ready-PR obligations` and the static validation job as required
status checks on `main`; workflow source alone cannot prevent an administrator
from bypassing repository rules.

### Automatic pull-request static validation

`.github/workflows/tainted-grail-sdk-pr-validation.yml` runs the read-only static
job on relevant pull requests, pushes to `main`, and manual dispatch. It executes:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going --static-only --skip-source-policy
```

This automatic pull-request static validation covers:

- every Python unit test beneath `Gems/TaintedGrailModdingSDK/Tools/tests`;
- every focused repository validator in the local runner inventory;
- deterministic Tainted Framework and Developer Preview fixture generation and
  verification;
- redacted diagnostics fixture collection and verification;

The workflow uses a GitHub-hosted `ubuntu-latest` runner with read-only repository
permissions. It does not use secrets or a self-hosted runner. Because O3DE is a
separate pinned checkout, this job explicitly uses `--skip-source-policy`; it does
not claim pinned O3DE source-policy coverage. A passing static job does not claim an O3DE build,
compiled C++ execution, Windows editor startup, pane coverage, packaging,
deployment, signing, or FoA runtime behavior.

### Manual host workflows

The following host-heavy workflows remain manual-only because they require
platform-specific tools, reviewed inputs, or release/operator decisions:

- `Tainted Grail SDK Foundation`;
- `Tainted Grail Editor Entry`;
- `Tainted Grail Repository Hygiene`;
- `Tainted Grail SDK Windows Installer`.

Their source files retain `workflow_dispatch` only. They must not run untrusted
pull-request code on a personal or privileged machine.

The inherited full-engine `AR` workflow and generic upstream `Validation`
workflow remain removed. They did not prove the FOA-SDK product slice.

## Full local validation

A full validation claim requires an already configured O3DE build and the
compiled Catalog test target:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --engine-root ../o3de \
  --ctest-build-dir ../foa-build/tg-sdk-developer-preview-0-windows-profile
```

This executes the complete static layer, all temporary fixtures, source policy
from the exact checkout pinned by `o3de.lock.json`, and the compiled Catalog CTest
selected by:

```text
TaintedGrailModdingSDK.Catalog.Tests
```

CTest is invoked with `--no-tests=error`; a missing or misnamed test target is a
failure rather than a green zero-test run. The build directory must exist and
contain both `CMakeCache.txt` and `CTestTestfile.cmake`.

Calling `run_local_validation.py` without either `--ctest-build-dir` or
`--static-only` is a configuration failure. Full mode also rejects
`--skip-source-policy`. This prevents an operator from accidentally reporting a
non-compiled or non-source-policy run as full validation.

`--keep-going` continues across all three stages—static checks, fixtures, and
compiled CTest—so one invocation reports the complete failure inventory. Without
`--keep-going`, later stages stop after the first failed stage.

`--static-only` is intentionally narrower. GitHub uses it together with
`--skip-source-policy`; a local exact-head run may omit that skip and therefore
include pinned O3DE source policy. Static-only mode is never sufficient for
exact-head merge acceptance.

## Merge evidence

Every pull request must distinguish these results:

1. automatic PR static-validation workflow result;
2. exact reviewed commit;
3. full local or exact-head host configure/build result;
4. compiled Catalog CTest result and build directory;
5. Windows editor/UI evidence or the one permitted explicit risk acceptance.

Pending is not passing. Queued, waiting, skipped, approval-blocked, cancelled,
missing, zero-test, or stale-head runs are not successful evidence.

## Self-hosted runner boundary

A general-purpose self-hosted runner is not connected to public pull requests.
Public pull requests can contain untrusted code, so attaching a personal
workstation or privileged machine would create avoidable code-execution and
secret-exposure risk.

A future self-hosted runner requires a separate reviewed design with, at minimum:

- a disposable, isolated machine or virtual machine;
- no personal files, game saves, credentials, signing keys, or unrelated secrets;
- no automatic execution for fork pull requests;
- narrow runner labels and narrowly scoped workflows;
- restricted repository permissions and environment approval where appropriate;
- clean teardown or re-imaging after jobs;
- explicit operator ownership, patching, monitoring, and incident response.

A self-hosted runner must not be used as a shortcut around an unavailable
GitHub-hosted job.

## Registration-token handling

A runner registration token is a secret even when it is short-lived. Never paste
it into chat, issues, pull requests, screenshots, logs, source files, or retained
shell history.

Any registration token exposed in a screenshot or message must be treated as
compromised: abandon it and generate a new token before configuring a runner.

## Exact-head validation receipt

After committing the candidate head, create the receipt **outside the
repository** from a clean working tree. The tool derives the 40-character source
commit from `git rev-parse HEAD`; it does not accept a caller-supplied commit,
status, exit code, or execution timestamp.

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py init \
  --output ../tg-sdk-receipt \
  --tester-alias maintainer \
  --platform "Windows 11 x64" \
  --configuration profile
```

The recommended coordinator is:

```shell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py plan \
  --review-base <40-character-reviewed-base> \
  --tester-alias maintainer \
  --windows-version "Windows 11"
```

The coordinator records five non-waivable automated gates:

- reviewed-range `git diff --check <base> HEAD`;
- `run_local_validation.py --keep-going --static-only`;
- O3DE configure;
- O3DE build;
- compiled Catalog CTest with `--no-tests=error`.

The static runner is deliberately recorded separately from compiled CTest so the
receipt proves both layers and cannot hide a missing test executable behind one
combined command.

To record a command manually, place the real executable and arguments after `--`.
The tool runs that command from the exact clean repository head, captures stdout
and stderr into exclusive bounded files, records observed exit status and UTC
times, and rejects a dirty tree or changed head before and after execution.

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py record \
  --output ../tg-sdk-receipt \
  --name local-validation \
  -- python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
     --keep-going --static-only
```

`git-diff-check`, `local-validation`, `o3de-configure`, `o3de-build`, and
`compiled-tests` must all have an executed, zero-exit result. They cannot be
waived. A Windows UI pass may be recorded with `skip` only when it genuinely was
not run, followed by a local maintainer declaration with `accept-risk`.

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py skip \
  --output ../tg-sdk-receipt --name windows-ui --reason "Exact reason"
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py accept-risk \
  --output ../tg-sdk-receipt --gate windows-ui \
  --maintainer-alias maintainer --rationale "Concrete local risk decision"
```

Finalize, verify, and summarize against the same clean exact head:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py finalize \
  --output ../tg-sdk-receipt --expected-commit <40-character-head>
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py verify \
  --output ../tg-sdk-receipt --expected-commit <40-character-head> \
  --require-merge-ready
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py summarize \
  --output ../tg-sdk-receipt --expected-commit <40-character-head> \
  --require-merge-ready
```

`developer_preview_verification.py status` is a gating command: it returns
non-zero unless the authoritative receipt, UI evidence, and exact reviewed range
all verify.

The receipt and captured logs must not be committed. Their hashes detect later
modification but are not a signature or independently authenticated CI
attestation; reviewers must treat them as exact-head, tester-supplied evidence.
