# Review and Merge Policy

## Purpose

Review is a safety, quality, and governance control—not a ceremonial step. This
policy defines what must be proven before implementation, commit, review, and
merge.

The validation boundary is defined by
[CI, Runner, and Local Validation Policy](CI_AND_LOCAL_VALIDATION.md): GitHub
provides an automatic PR static-validation check, while exact-head Windows/O3DE
configure, build, compiled tests, and UI evidence remain separately recorded host
gates.

## Gate 1 — Design review before implementation

Significant changes require an approved design before code is written, including:

- new editor tools, services, public APIs, or domain systems;
- schema, identifier, migration, or persistence changes;
- adapter contracts or runtime-facing work;
- build, packaging, deployment, save, or rollback behavior;
- security-sensitive changes or new dependencies;
- imported upstream systems or licensed assets.

The design must define the user problem, owning layer, identity and evidence
rules, persisted data, failure behavior, rollback, security/legal impact, and the
test plan. Approval authorizes the direction, not the final implementation.

## Gate 2 — Pre-commit self-review

Before every commit, review the complete staged diff and confirm:

- the change is scoped and understandable;
- no secrets, private paths, tokens, proprietary data, or accidental generated
  output are present;
- includes and build ownership are explicit;
- failure paths are fail-closed;
- stable IDs, exact references, schema versions, and ownership remain correct;
- documentation and regression tests are included;
- the applicable validation layer has been run;
- DCO sign-off is present.

For a fast pre-commit/static pass:

```shell
git diff --cached --check
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going --static-only
git commit -s -m "Describe the change"
```

A full validation claim requires:

```shell
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py \
  --keep-going \
  --ctest-build-dir ../foa-build/tg-sdk-developer-preview-0-windows-profile
```

The static-only command must never be described as a compiled or exact-head pass.

## Gate 3 — Pull-request review before merge

Every normal change to `main` uses a pull request from
`FOA-plug-in-development` or another focused development branch based on the
current `main` head.

The pull request must include:

- linked issue or approved design where required;
- summary, scope, and explicit out-of-scope behavior;
- architecture and runtime-boundary impact;
- identity, evidence, schema, persistence, security, privacy, and legal impact;
- exact commands and results for every relevant test layer;
- rollback or revert plan;
- updated documentation.

## Required merge conditions

A pull request may merge only when all of the following are true:

- it is marked ready for review;
- all commits satisfy DCO requirements;
- the automatic PR static-validation check completed successfully for the exact
  reviewed head;
- the reviewed-range whitespace gate passed;
- applicable host prerequisites, O3DE configure, and O3DE build passed;
- the compiled `TaintedGrailModdingSDK.Catalog.Tests` target executed and passed
  with zero-test matching treated as failure;
- a merge-ready exact-head validation receipt is verified with
  `validation_receipt.py verify --require-merge-ready`, its receipt source commit matches the reviewed head,
  and the verified summary is included in the pull request;
- the Windows UI gate passed, or the one permitted explicit local maintainer risk
  acceptance is recorded with a concrete rationale;
- requested changes and blocking review threads are resolved;
- documentation, migration behavior, and rollback are current;
- at least one maintainer approval is recorded;
- no unresolved security, legal, data-loss, or architectural concern remains.

The automatic static check complements the receipt; it does not replace host
proof. Host configure, host build, and compiled-test gates remain mandatory.

Pending is not passing. Queued, waiting, skipped, approval-blocked, cancelled,
missing, stale-head, and zero-test runs are not successful evidence.

## Review depth by risk

### Low risk

Examples include documentation corrections, test-only hardening, and internal
refactors without behavior or schema changes. Review correctness, scope,
validation, and documentation consistency.

### Medium risk

Examples include UI workflows, new validation rules, importers, and
backward-compatible persistence fields. Also review malformed input, failure
states, accessibility, serialization round trips, and compatibility.

### High risk

Examples include permissions, deployment, process launch, save mutation, runtime
adapters, breaking schemas, security boundaries, and new dependencies. These
require an approved design, threat/failure analysis, negative tests, migration
and rollback proof, and an explicit maintainer merge decision.

## Reviewer responsibilities

Reviewers must:

- inspect the complete diff and its design context;
- check code, tests, schemas, build ownership, and documentation together;
- reason through failure paths and adversarial input;
- distinguish blockers from optional preferences;
- verify that every claimed result belongs to the exact reviewed head;
- confirm the automatic static check and exact-head receipt prove different,
  complementary layers;
- revisit approval after material changes;
- avoid approving code they do not understand.

## Author responsibilities

Authors must:

- keep changes reviewable and focused;
- respond to every blocking comment;
- add regression tests for repaired defects;
- avoid suppressions or broad exceptions used only to obtain a green result;
- request re-review after material changes;
- update the PR description when scope or evidence changes;
- never present a syntax-only, static-only, queued, absent, or zero-test result as
  a successful full repository build.

## Self-authored maintainer changes

Maintainers follow the same gates. Independent review is preferred. When no
second qualified reviewer is available, document that limitation and provide
stronger exact-head evidence. High-risk changes must not be merged without
external review unless an active security or data-loss emergency requires a
scoped mitigation.

## Emergency exceptions

An emergency exception is limited to an active vulnerability, data-loss defect,
or unsafe release. The mitigation must be minimal, the exception recorded, and
normal review and validation completed immediately afterward.

## Merge method

Use a normal merge commit unless squash or rebase has a documented reason. The
selected method must preserve DCO and useful attribution.

## After merge

1. Confirm `main` points to the accepted merge commit.
2. Synchronize `FOA-plug-in-development` to that commit.
3. Confirm the push-to-main static workflow ran for the merge commit.
4. Record post-merge exact-head validation only when it actually ran.
5. Revert promptly if the merged result violates an accepted gate.

## Prohibited merge behavior

- direct push to `main` for normal development;
- merge while an enabled required check is pending or failing;
- merge without an executed compiled test target;
- merge with a missing or stale exact-head receipt;
- describe static-only, queued, skipped, absent, or zero-test runs as full passes;
- merge by deleting tests or weakening validation solely to obtain green status;
- merge unresolved requested changes, undocumented schema breaks, or legally
  restricted material;
- grant runtime permission based only on imported or decompiled information.
