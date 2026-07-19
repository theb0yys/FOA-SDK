# Review and Merge Policy

## Purpose

Review is a safety, quality, and governance control—not a ceremonial step. This
policy defines the required reviews before implementation, before commit, and
before merge.

The current CI and runner state is governed by
[CI, Runner, and Local Validation Policy](CI_AND_LOCAL_VALIDATION.md). Automatic
GitHub Actions triggers are suspended until hosted jobs can actually start and
complete. No queued or absent workflow is treated as test evidence.

## Three review gates

### Gate 1 — Design review before implementation

Required for significant changes, including:

- new editor tools, services, or domain systems;
- schema, identifier, or persistence changes;
- adapter contracts or runtime-facing work;
- build, package, deployment, save, or rollback behavior;
- new dependencies;
- security-sensitive changes;
- breaking public APIs or data formats.

The proposal must define:

- user problem and acceptance criteria;
- owning architecture layer;
- identity and ownership effects;
- evidence and validation requirements;
- persisted data and schema version;
- failure and rollback behavior;
- security, privacy, legal, and compatibility impact;
- test plan.

A maintainer records approval to proceed. Approval means the direction is
acceptable; it does not pre-approve the code.

### Gate 2 — Pre-commit self-review

Before every commit, the author reviews the complete staged diff.

Required checks:

- change is scoped and understandable;
- no secrets, private paths, runner tokens, or restricted content;
- no debug code or accidental generated files;
- explicit includes and dependencies;
- error paths and fail-closed behavior;
- stable IDs, exact refs, schema versions, and ownership;
- documentation and tests included where required;
- repository-owned local validation run for TG SDK changes;
- DCO sign-off used.

Recommended commands:

```shell
git diff --cached --check
git diff --cached
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going
git commit -s -m "Describe the change"
```

When an O3DE build is available, add compiled tests with `--ctest-build-dir` and
record the exact build directory and configuration.

### Gate 3 — Pull-request review before merge

Every change to `main` uses a pull request from `foa-development`.

The pull request must include:

- linked issue or design review where required;
- summary and user-visible behavior;
- architecture and boundary impact;
- security and privacy impact;
- compatibility, schema, and migration impact;
- save and deployment impact;
- exact test and build evidence;
- documentation changes;
- rollback or revert plan.

## Required merge conditions

A pull request may merge only when:

- it is marked ready for review;
- all commits satisfy DCO requirements;
- the exact reviewed head was tested with the local validation command;
- the command, exit result, skipped checks, and compiled-test status are recorded;
- applicable host builds and compiled tests pass, or their absence is explicitly
  recorded as a limitation accepted by the maintainer;
- automatic CI checks, when enabled and expected for that head, complete
  successfully;
- requested changes are addressed;
- all review threads are resolved;
- documentation and changelog are current;
- migrations or unsupported-version behavior are defined;
- at least one maintainer approval is recorded;
- no unresolved security, legal, data-loss, or architectural concern remains.

Pending is not passing. Queued, waiting, skipped, approval-blocked, missing, and
cancelled runs are not successful test evidence.

While automatic Actions remain suspended:

- unavailable workflow names must not be configured as required branch checks;
- pull requests must not claim exact-head CI success;
- local evidence is mandatory but is identified as tester-supplied evidence;
- a maintainer makes the explicit risk decision for any missing host build,
  compiled test, or Windows UI pass.

## Review depth by risk

### Low risk

Examples:

- typo correction;
- documentation clarification;
- test-only improvement;
- internal refactor with no behavior or schema change.

Expected review:

- correctness;
- scope;
- documentation consistency;
- focused checks.

### Medium risk

Examples:

- user-interface workflow;
- new query or validation rule;
- importer support without schema break;
- new persistence field with backward-compatible defaults.

Expected review additionally covers:

- failure states;
- accessibility;
- serialization round trip;
- malformed input;
- compatibility and migration.

### High risk

Examples:

- security boundary;
- deployment, process launch, save, or rollback;
- permission changes;
- breaking schema or identity change;
- new dependency;
- runtime adapter behavior.

Expected review additionally requires:

- reviewed design proposal;
- threat/failure analysis;
- negative tests;
- migration and rollback proof;
- independent reviewer when available;
- explicit maintainer merge decision.

## Reviewer responsibilities

Reviewers must:

- read the issue/design context;
- inspect the complete diff, not only highlighted lines;
- test or reason through failure paths;
- distinguish blocking findings from preferences;
- explain why a change is required;
- check code, tests, schemas, and documentation together;
- verify that claimed test evidence belongs to the exact head;
- avoid approving code they do not understand;
- disclose conflicts of interest;
- revisit approval if the pull request changes materially.

## Author responsibilities

Authors must:

- keep the change reviewable;
- respond to every blocking comment;
- push fixes as clear commits;
- avoid hiding unresolved problems with suppression or broad exceptions;
- request re-review after material changes;
- update the PR description as scope or validation evidence changes;
- never resolve a review thread without addressing or documenting the decision;
- never present a queued workflow, syntax-only check, or isolated fixture as a
  successful full repository build.

## Review outcomes

### Approve

The change meets requirements and may merge after all other gates pass.

### Comment

Non-blocking questions or suggestions remain. The reviewer must state when a
comment is optional.

### Request changes

The pull request must not merge until the finding is addressed and the reviewer
or maintainer accepts the resolution.

## Self-authored maintainer changes

Maintainers follow the same gates. Independent review is preferred. When no
second qualified reviewer is available, the maintainer must:

- document the limitation;
- provide stronger local and manual validation;
- avoid merging high-risk work without external review unless an urgent security
  or data-loss issue requires action;
- schedule post-merge review when an emergency exception is used.

## Emergency exceptions

A maintainer may bypass normal public timing only to address an active security
vulnerability, data-loss defect, or unsafe release.

Even then:

- the change must be scoped to mitigation;
- review and tests must occur privately or immediately after disclosure as
  appropriate;
- the exception and follow-up actions must be recorded;
- normal governance resumes after the emergency.

## Merge method

Use a normal merge commit unless a maintainer chooses squash or rebase for a
clearly justified history reason. The selected method must preserve DCO and
useful attribution.

The merge commit or PR title should describe the capability or fix, not merely a
ticket number.

## After merge

1. Confirm `main` points to the accepted merge commit.
2. Synchronize `foa-development` to that commit.
3. Record post-merge local validation or automatic workflows when they are
   actually available.
4. Update roadmap or release tracking when applicable.
5. Revert promptly if the merged change introduces unacceptable risk.

## Prohibited merge behavior

- direct push to `main` for normal development;
- merge while an enabled required check is pending or failing;
- configure unavailable workflows as required checks;
- describe queued, absent, skipped, or manual-only workflows as passing;
- merge with unresolved requested changes;
- merge by deleting tests or weakening validation solely to obtain a green state;
- merge undocumented breaking schema changes;
- merge runtime permission based only on imported or decompiled information;
- merge proprietary or legally restricted material.
