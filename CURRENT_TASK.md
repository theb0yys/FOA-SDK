# Current Task

## Status

`P0 — Repository Process Reset` — review candidate.

## Goal

Make FOA-SDK engineering explicit, proportional, and executable by replacing duplicated mandatory process stacks with one clear operating model.

## Classification

**Significant — process/governance only.**

## In scope

- establish one engineering-process authority;
- define Routine, Significant, and Critical/Runtime change classes;
- make validation proportional to the changed surface;
- simplify automated-agent rules to repository execution governance;
- make research escalation conditional on an actual research dependency;
- remove universal PR-body merge-obligation checkboxes;
- correct stale repository identity, checkout, and branch guidance;
- record that the Capability Execution M0 decision is already merged and effective.

## Out of scope

- SDK product implementation;
- M1 capability-execution source work;
- C++ contracts or behavior;
- O3DE engine changes;
- runtime adapters;
- deployment, saves, installers, signing, publication, or release behavior;
- protected Fall of Avalon files or installations.

## Acceptance criteria

P0 is ready for maintainer review when:

- `AGENTS.md` contains agent execution rules without a universal research/deep-review/preflight stack;
- `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md` is the single engineering workflow;
- `GOVERNANCE.md`, `CONTRIBUTING.md`, and review policy use the same three change classes;
- `CI_AND_LOCAL_VALIDATION.md` is the single validation matrix;
- the PR template records classification and actual validation without requiring irrelevant host/UI/runtime evidence;
- PR/CI policy validators enforce read-only automation and the new template contract;
- `DEVELOPMENT_GUIDE.md` uses the separate pinned upstream O3DE checkout;
- `DECISIONS.md` marks the imported context-only process as superseded and records P0 progressive rigor;
- no product/runtime behavior changes.

## Current branch

`governance/p0-process-reset`

## Next action

Maintainer audit of the focused P0 pull request. After P0 is accepted, define the first SDK recovery milestone in this file before resuming product implementation.
