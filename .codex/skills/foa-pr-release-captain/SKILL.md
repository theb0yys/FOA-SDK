---
name: foa-pr-release-captain
description: Use for every FOA-SDK GitHub handoff, pull request, commit, release, branch, issue, review, CI plan, or local-to-GitHub process update. It preserves researched scope, DCO, local gate authority, evidence-pack integrity, protected-file audit, CODEOWNERS status, and maintainer-only approval and merge authority.
---

# FOA-SDK PR and Release Captain

Use after implementation and evidence audit whenever work is committed, submitted, reviewed, or prepared for release.

## First Actions

1. Check repository, branch, and worktree state.
2. Exclude unrelated dirty files and generated output.
3. Confirm the PR or release scope matches the researched owner scope.
4. Check `.github/PULL_REQUEST_TEMPLATE.md` and `.github/CODEOWNERS`.
5. Require the evidence pack and final deep review.
6. Confirm every commit is focused and DCO-signed.
7. Confirm the current PR head and any exact-head marker or receipt belong to the same commit.

## Research

Read:

- `AGENTS.md`
- `GOVERNANCE.md`
- `CONTRIBUTING.md`
- review and merge policy
- release process when applicable
- research-first process stack
- evidence-pack template
- applicable owner, compatibility, migration, artifact, installer, and runtime-adapter gates

GitHub automation does not replace repository-local authority or evidence.

## Scope Control

The handoff must identify:

- user request and intended scope
- files intentionally changed
- unrelated files explicitly excluded
- protected files audited and avoided or explicitly authorised
- controlling research and next process
- systems, surfaces, consumers, and blast radius
- compatibility and migration status
- tests, performance, artifacts, and runtime proof

Do not create a PR from “everything currently dirty.”

## Pull Request Requirements

A PR must include:

- exact summary and scope
- out-of-scope statement
- architecture and runtime-boundary review
- identity, ownership, evidence, and permission review
- schema and migration status
- security, privacy, legal, save, deployment, and rollback impact
- exact commands and validation results
- exact-head receipt status where required
- UI or manual evidence when required
- performance impact
- documentation status
- author self-review
- mandatory merge obligations
- current 40-character head marker where the template requires it

Unchecked, failed, skipped, or blocked obligations must not be described as passing.

## CODEOWNERS and Review

- Check whether exact GitHub users or teams own the changed paths.
- Do not invent owners.
- If CODEOWNERS is incomplete, report that enforcement gap.
- Do not approve, merge, auto-merge, close, or dismiss required review for your own PR.
- Leave final audit, approval, and merge to the maintainer.

## Commit and Branch Rules

- Work only on the authorised non-`main` branch.
- Keep commits focused and DCO-signed.
- Do not rewrite protected integration branches or force-push unless explicitly authorised.
- Do not mix generated artifacts, private paths, credentials, or unrelated changes.
- A new commit invalidates exact-head evidence and requires the PR marker and receipts to be refreshed.

## Release Gate

Before a release claim, require applicable proof for:

- research and design authority
- compatibility and migration
- security and legal review
- required local and hosted checks
- O3DE configure/build and compiled tests
- Editor and UI evidence
- Unity conversion evidence
- installer build and lifecycle evidence
- Mono and IL2CPP adapter evidence
- external artifact identity and provenance
- exact-install Fall of Avalon runtime status
- known defects, rollback, and documentation

A local build that “probably passed” is not release evidence.

## Hard Stops

Stop or report blocked when:

- unrelated changes are mixed into the handoff
- research or evidence is incomplete
- unclear authority lacks a Deep Research Brief
- protected files lack permission
- required local or hosted gates are skipped without reason
- CODEOWNERS cannot be enforced and that status is hidden
- DCO sign-off is absent
- exact-head marker or receipt is stale
- release claims lack compatibility, build, artifact, manual, adapter, or runtime evidence
- the requested action requires self-approval, self-merge, branch-protection bypass, workflow manipulation, or another maintainer-only action

## Validation

Report:

- repository, branch, commit, PR, and release scope
- controlling research and evidence-pack status
- files included and excluded
- protected-file audit
- DCO status
- PR-template and exact-head-marker status
- CODEOWNERS status
- local and hosted checks
- compatibility, migration, performance, artifact, manual, and runtime evidence
- complete, partial, or blocked merge/release state

## Runtime Proof

A GitHub handoff creates no runtime proof. State `runtime sign-off not performed` unless exact-install Fall of Avalon evidence was independently captured and belongs to the current head and artifacts.
