# CI, Runner, and Local Validation Policy

## Binding automation boundary

`AGENTS.md` is the repository-specific authority for GitHub agents and automated
workflows. Agent-authored repository changes must happen on a non-main branch and
be submitted by pull request for maintainer audit. Agents must not commit
directly to main, move refs, post comments, merge pull requests, change
repository settings, or trigger, cancel, rerun, approve, or otherwise control
workflow runs unless the repository owner explicitly authorises that exact action
for the current task.

Automated validation is read-only. Workflow tokens use `contents: read` unless a
separately documented human-operated release design explicitly requires a narrower
write capability. Current validation and installer workflows do not write repository
contents or GitHub discussion metadata.

## Validation layers

FOA-SDK separates hosted policy checks from host-heavy acceptance. GitHub-hosted
Linux and Windows jobs can prove repository contracts, exact source identity,
reviewed-range whitespace, compiled target execution, and Windows prerequisites.
They do not by themselves prove Windows Editor interaction, release signing, game
runtime behavior, or deployment safety.

### Automatic read-only validation

`.github/workflows/tainted-grail-sdk-pr-validation.yml` runs on relevant pull
requests, pushes to `main`, and manual dispatch. Pull requests remain available for
human contributors, but the workflow never modifies their state. It has no
`pull_request_target` trigger, no write permission, and no GitHub API mutation.
This is the automatic pull-request static validation boundary; its additional
compiled and Windows-prerequisite jobs remain read-only.

The path filter covers `.github/**`, `docs/**`, `scripts/**`, product code,
installer code, plug-ins, research, project files, root Markdown,
`.gitattributes`, and `o3de.lock.json`. Documentation-only policy changes therefore
cannot bypass validation.

The Linux job checks out the exact event commit with persisted credentials disabled.
It resolves the event base and runs:

```shell
git diff --check <base-sha> <exact-head-sha>
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going --static-only --skip-source-policy
```

The base, head, event, static log, and reviewed-range log are retained as workflow
evidence. Static-only validation does not claim an O3DE build or compiled test pass.

### Automatic compiled canonical-interchange validation

A separate `windows-2022` job checks out the exact event commit and the pinned O3DE
commit, configures the dedicated product project, builds
`TaintedGrailModdingSDK.CanonicalInterchange.Tests` with bounded parallelism, and
runs CTest with `--no-tests=error`. A missing test executable or a zero-test match is
a failure.

### Automatic Windows prerequisite validation

A read-only job on the pinned `windows-2022` image checks out the exact event
commit, clones only the pinned O3DE policy surface, verifies the exact O3DE
commit, and runs:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites `
  --engine-root <pinned-o3de-policy-root> `
  --build-dir <external-empty-build-root>
```

This proves the hosted machine has the required Windows x64 host, Python, CMake,
Git, Git LFS, Visual Studio C++ tools, exact O3DE identity, product root, and safe
external build location. It does not configure or build the full Editor.

## Manual read-only workflows

The following host-heavy workflows are manual because they require
platform-specific tools, reviewed inputs, or operator decisions:

- `Tainted Grail SDK Foundation`;
- `Tainted Grail Editor Entry`;
- `Tainted Grail Repository Hygiene`;
- `FOA-SDK Windows Installer`.

They retain `workflow_dispatch` only and `contents: read`. They must not push
commits, move refs, create branches, post comments, modify pull requests or issues,
or dispatch other workflows.

The installer workflow builds the compiled Catalog test target and the O3DE
`INSTALL` target with `--parallel 2`. Inventory mode produces an exact fingerprint
and notices for review. Package mode accepts review metadata only from the human
operator invoking the canonical workflow; no repository automation may synthesize
reviewer identity, review time, evidence, or approval.

## Full local validation

A full validation claim requires a complete exact-commit O3DE checkout, an already
configured external build directory, and the compiled Catalog test target:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --engine-root ../o3de \
  --ctest-build-dir ../foa-build/tg-sdk-developer-preview-0-windows-profile
```

This runs the complete static layer, temporary fixtures, exact pinned O3DE source
policy, and compiled Catalog CTest selected by:

```text
TaintedGrailModdingSDK.Catalog.Tests
```

CTest uses `--no-tests=error`. The build directory must contain `CMakeCache.txt` and
`CTestTestfile.cmake` and belong to the exact pinned engine and dedicated project.
Calling the runner without either `--ctest-build-dir` or `--static-only` is a
configuration failure. Full mode rejects `--skip-source-policy`.

`--static-only` is intentionally narrower and must never be described as a full,
compiled, exact-head acceptance pass.

## Required evidence

For each candidate commit, record the evidence that actually ran:

1. exact source commit and reviewed base;
2. reviewed-range `git diff --check` result;
3. static validator and unit-test result;
4. Windows prerequisite result;
5. exact O3DE configure and required target build result;
6. compiled CTest result with non-zero test count;
7. Windows Editor/UI evidence when applicable;
8. installer inventory fingerprint and human review evidence when packaging;
9. rollback or revert path for risky changes.

Pending is not passing. Queued, waiting, skipped, approval-blocked, cancelled,
missing, zero-test, stale-head, wrong-event, or wrong-commit runs are not successful
evidence. Workflow source, comments, account names, and self-declared metadata are
not proof that the repository owner authorized an action.

## Self-hosted runner boundary

A general-purpose self-hosted runner is not connected to public pull requests.
Public contributions can contain untrusted code, so attaching a personal or
privileged machine would create code-execution and secret-exposure risk.

A future self-hosted runner requires a separately reviewed design with a disposable
isolated machine, no personal files or unrelated credentials, no automatic fork
execution, narrow labels and permissions, environment approval, clean teardown,
and explicit operator ownership and incident response.

A runner registration token is a secret. Never place it in chat, issues, pull
requests, screenshots, logs, source files, or retained shell history.

## Exact-head validation receipt

Create receipts outside the repository from a clean exact commit. The tool
derives the 40-character source commit from `git rev-parse HEAD`; callers do not
 supply a claimed commit, status, exit code, or timestamp.

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py init \
  --output ../tg-sdk-receipt \
  --tester-alias maintainer \
  --platform "Windows 11 x64" \
  --configuration profile
```

Record each automated gate through `validation_receipt.py record -- <command>`.
The receipt tool runs that command and derives its exit status, timestamps, and
bounded log evidence rather than trusting caller-supplied results. Reviewed-range
whitespace, local validation, O3DE configure, O3DE build, and compiled Catalog
CTest must all have an executed, zero-exit result.

Only the Windows UI gate can be recorded as not run and then accepted by a named
maintainer with a concrete rationale:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py skip \
  --output ../tg-sdk-receipt --name windows-ui \
  --reason "Exact UI evidence could not be captured on this host"
python Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py accept-risk \
  --output ../tg-sdk-receipt --gate windows-ui \
  --maintainer-alias maintainer \
  --rationale "Concrete commit-bound reason and residual risk"
```

No automated gate can be waived. A Windows UI pass must not be invented; risk
acceptance records only that the UI gate did not run.

Finalize, verify, and summarize against the same exact commit:

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

The non-waivable automated gates are reviewed-range whitespace, local static
validation, O3DE configure, O3DE build, and compiled tests. A Windows UI gate may be
recorded as not run only with an explicit, concrete, commit-bound human risk
acceptance. Receipts and logs must not be committed. A receipt hash detects
later modification but is not a signature or independent authorization.
