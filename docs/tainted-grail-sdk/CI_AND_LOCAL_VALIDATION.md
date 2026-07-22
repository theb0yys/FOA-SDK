# CI, Runner, and Local Validation Policy

## Validation layers

FOA-SDK separates hosted policy checks from host-heavy acceptance. GitHub-hosted
Linux and Windows jobs can prove repository contracts, an exact reviewed range,
and Windows tool prerequisites, but they do not prove a full O3DE build or Editor
pass.

### Automatic pull-request obligation enforcement

The governed workflow has a narrow `pull_request_target` job for ready pull
requests. It checks out only the trusted base commit identified by
`github.event.pull_request.base.sha`; it never checks out or executes pull-request
head code. The job reads the GitHub event payload and validates the pull-request
body with the base-branch copy of `validate_pr_obligations.py`.

A ready pull request must contain every mandatory obligation exactly once, mark
each one complete, and record the current 40-character PR head in its
`merge-head` marker. A new commit makes that marker stale. Missing, duplicate,
unchecked, malformed, or stale obligations cause the governor to use GitHub's API
to return the pull request to draft and fail the check.

The privileged job has only `contents: read` and `pull-requests: write`. It does
not run validation, install dependencies, invoke build tools, use LFS, or execute
repository code from the proposed head.

### Required `main` repository rules

Workflow source cannot stop an administrator or an allowed bypass actor from
merging around status checks. The `main` ruleset must therefore require:

- changes through a pull request;
- the branch to be up to date before merge;
- at least one approving review from someone other than the author;
- all review conversations resolved;
- required checks named `Enforce ready-PR obligations`,
  `Python, validators, and fixtures`, and `Windows O3DE prerequisites`;
- force pushes and branch deletion blocked;
- no general bypass actor. A documented emergency-only bypass identity may be
  retained only with an auditable incident rationale.

The ruleset is a repository setting, not a source file. A pull request must not
claim this layer complete until an administrator verifies the live setting.

### Automatic pull-request static validation

`.github/workflows/tainted-grail-sdk-pr-validation.yml` runs on relevant pull
requests, pushes to `main`, and manual dispatch. Its path filter covers the full
`.github/**`, `docs/**`, and `scripts/**` trees in addition to product code,
installer, plug-ins, research, project files, root Markdown, `.gitattributes`, and
`o3de.lock.json`. Documentation-only changes therefore cannot silently bypass
validation.

The read-only Linux job checks out the exact pull-request head rather than the
synthetic merge ref. With full history available, it resolves the event base and
runs the reviewed-range command:

```shell
git diff --check <base-sha> <exact-head-sha>
```

The base/head/event tuple is retained in `tg-sdk-reviewed-range.txt`, together
with the `git diff --check` log. The job then executes:

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
- Suite Wizard graphical smoke under a virtual display;
- exact reviewed-range whitespace validation.

The Linux job uses `ubuntu-latest`, read-only repository permissions, no secrets,
and no self-hosted runner. Because O3DE is a separate pinned checkout, it uses
`--skip-source-policy`; it does not claim pinned O3DE source-policy coverage. A
passing static job does not claim an O3DE build, compiled C++ execution, Windows
Editor startup, pane coverage, packaging, deployment, signing, or FoA runtime
behavior.

### Automatic Windows prerequisite validation

A separate read-only `windows-latest` job checks out the same exact event head,
clones only the pinned O3DE policy surface, verifies the exact O3DE commit, and
runs:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --engine-root <pinned-o3de-policy-root> `
  --build-dir <external-empty-build-root>
```

This Windows prerequisite gate proves the hosted machine has the required
Windows x64 host, Python, CMake, Git, Git LFS, Visual Studio C++ tools, valid
FOA-SDK product root, exact O3DE identity and commit, and a safe external build
location. Its log is retained as `tg-sdk-windows-prerequisites.log`.

The prerequisite job deliberately uses a sparse O3DE policy checkout. It does not
configure O3DE, build targets, run compiled Catalog tests, launch the Editor, or
produce UI evidence. Those remain host-heavy gates.

### Manual host workflows

The following host-heavy workflows remain manual-only because they require
platform-specific tools, reviewed inputs, or release/operator decisions:

- `Tainted Grail SDK Foundation`;
- `Tainted Grail Editor Entry`;
- `Tainted Grail Repository Hygiene`;
- `Tainted Grail SDK Windows Installer`.

Their source files retain `workflow_dispatch` only. They must not run untrusted
pull-request code on a personal or privileged machine. The inherited full-engine
`AR` workflow and generic upstream `Validation` workflow remain removed because
they did not prove the FOA-SDK product slice.

## Full local validation

A full validation claim requires a complete exact-commit O3DE checkout, an
already configured external build directory, and the compiled Catalog test
target:

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
failure rather than a green zero-test run. The build directory must contain both
`CMakeCache.txt` and `CTestTestfile.cmake` and must belong to the exact pinned
engine and dedicated product project.

Calling `run_local_validation.py` without either `--ctest-build-dir` or
`--static-only` is a configuration failure. Full mode rejects
`--skip-source-policy`. `--keep-going` continues across static checks, fixtures,
and compiled CTest so one invocation reports the complete failure inventory.

`--static-only` is intentionally narrower. Static-only mode is never sufficient
for exact-head merge acceptance.

## Merge evidence

Every pull request must distinguish these results:

1. automatic exact-head static-validation result and artifact identity;
2. hosted reviewed-range base/head evidence;
3. hosted Windows prerequisite result and artifact identity;
4. full exact-head O3DE configure and required target build result;
5. compiled Catalog CTest result and configured build directory;
6. Windows Editor/UI evidence or the one permitted explicit risk acceptance;
7. live repository rules, resolved threads, and independent maintainer approval.

Pending is not passing. Queued, waiting, skipped, approval-blocked, cancelled,
missing, zero-test, stale-head, or wrong-event runs are not successful evidence.

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
- restricted repository permissions and environment approval;
- clean teardown or re-imaging after jobs;
- explicit operator ownership, patching, monitoring, and incident response.

A self-hosted runner must not be used as a shortcut around an unavailable
GitHub-hosted job.

## Registration-token handling

A runner registration token is a secret even when it is short-lived. Never paste
it into chat, issues, pull requests, screenshots, logs, source files, or retained
shell history. Any exposed token must be abandoned and regenerated.

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

Record each automated gate through the receipt tool. The
`validation_receipt.py record` subcommand runs that command from the clean exact
head and derives its observed exit code, timestamps, bounded logs, and hashes:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py record \
  --output ../tg-sdk-receipt \
  --name <gate-id> \
  -- <executable> <arguments...>
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

The static runner is recorded separately from compiled CTest so a receipt cannot
hide a missing test executable behind one combined command. Commands are run from
the clean exact head, with bounded exclusive logs, observed exit status and UTC
times, and before/after head and cleanliness checks.

`git-diff-check`, `local-validation`, `o3de-configure`, `o3de-build`, and
`compiled-tests` must all have an executed, zero-exit result. They cannot be
waived. A Windows UI pass may be recorded with `skip` only when it genuinely was
not run. The permitted local declaration must then use
`validation_receipt.py accept-risk` with a concrete maintainer rationale:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py skip \
  --output ../tg-sdk-receipt \
  --name windows-ui \
  --reason "Windows UI validation was not run"
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py accept-risk \
  --output ../tg-sdk-receipt \
  --gate windows-ui \
  --maintainer-alias maintainer \
  --rationale "Concrete, reviewable reason for accepting this exact-head UI risk"
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

`developer_preview_verification.py status` returns non-zero unless the
authoritative receipt, UI evidence, and exact reviewed range all verify. Receipt
and captured logs must not be committed. Their hashes detect later modification
but are not a signature or independently authenticated CI attestation.
