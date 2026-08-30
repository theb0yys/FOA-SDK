# FOA-SDK Engineering Process

## Purpose

This is the single engineering workflow for FOA-SDK. Other process documents link here instead of restating competing gate stacks.

The objective is simple: apply enough rigor to make a change trustworthy without requiring irrelevant proof.

## Authority

For a task, use:

1. repository owner's current explicit instruction;
2. legal, licence, security, and protected-file restrictions;
3. `AGENTS.md` for automated-agent execution rules;
4. this engineering process;
5. `CURRENT_TASK.md` for the active milestone;
6. `DECISIONS.md` for accepted durable decisions;
7. the architecture/design/schema/local policy owning the changed surface;
8. `CI_AND_LOCAL_VALIDATION.md` for validation;
9. development/user guides and historical context.

When two lower-level documents disagree, resolve the contradiction in the owning source rather than creating another parallel rule.

## Change classes

### Routine

Use Routine when the change stays inside accepted architecture and does not create a new consequential contract.

Typical examples:

- bug fixes;
- implementation of an already accepted design;
- focused tests;
- internal refactors with unchanged behavior/contracts;
- build or tooling repairs;
- ordinary documentation corrections;
- validator/test hardening for an existing requirement.

Routine flow:

```text
scope -> inspect owner surface -> implement -> focused validation -> review
```

No separate design proposal, research stack, performance plan, evidence pack, or deep-review ceremony is required unless the specific change introduces that need.

### Significant

Use Significant when the change establishes or materially changes an engineering contract.

Typical examples:

- new public API or subsystem;
- persistence/schema/migration change;
- new dependency;
- architecture or ownership boundary change;
- substantial build-graph/project integration;
- compatibility contract change;
- process/governance/validation-policy change.

Significant flow:

```text
scope -> short design/decision -> implement -> expanded relevant validation -> review
```

The design should be proportional: problem, owner, contract/boundary, compatibility or migration impact, failure behavior, and validation plan.

### Critical/Runtime

Use Critical/Runtime when incorrect behavior can affect external machines, user data, game state, security, distribution, or authoritative runtime claims.

Typical examples:

- external process execution;
- deployment or game-install mutation;
- save mutation;
- runtime adapters/injection;
- signing or publication;
- release execution;
- permission/security boundary changes;
- claims about live Fall of Avalon runtime behavior.

Critical/Runtime flow:

```text
scope -> explicit design/threat boundary -> implementation
-> exact operational validation -> maintainer decision
```

These changes require negative/failure coverage, rollback or recovery design where applicable, and evidence from the layer that actually performs the operation.

## Standard engineering cycle

### 1. Define

State:

- desired outcome;
- owner system;
- intended files;
- classification;
- out-of-scope behavior;
- acceptance criteria.

`CURRENT_TASK.md` should describe the active milestone, not preserve completed historical gate state.

### 2. Inspect

Read only the documents and implementation needed to understand the owner surface. Inspect existing tests before changing behavior.

Escalate to research when consequential facts are genuinely unknown. Do not make research a default precondition.

### 3. Implement

Make the smallest coherent change that satisfies the requested result. Avoid unrelated cleanup, opportunistic redesign, and automatic progression to another milestone.

### 4. Validate

Use `CI_AND_LOCAL_VALIDATION.md`.

Validation is **applicability-based**:

- run the strongest evidence needed for the surface you changed;
- do not require unrelated layers;
- do not substitute one evidence layer for another;
- report exact results.

### 5. Review and integrate

Use a focused non-`main` branch and pull request. Record classification, scope, actual validation, compatibility/migration/rollback impact where applicable, and documentation changes.

Maintainer review determines integration into `main`.

## Research escalation

Research is required when implementation depends on unresolved consequential facts outside the trusted repository state, especially game/runtime facts, native identities, proprietary formats, third-party compatibility/licensing, deployment/save behavior, or security claims.

The conditional research workflow is `.codex/workflows/foa_research_first_process_stack.md`.

Research does not itself authorize execution, mutation, release, or evidence promotion.

## Current-task format

`CURRENT_TASK.md` should remain short and use:

```text
Status
Goal
Classification
In scope
Out of scope
Acceptance criteria
Current branch
Next action
```

Completed historical context belongs in commits, pull requests, changelog entries when notable, architecture/design records, or durable decisions—not in the active-task file.

## Definition of done

A change is done when:

- requested scope is implemented;
- applicable validation has actually passed;
- compatibility/migration/rollback is addressed where applicable;
- affected documentation is current;
- no unrelated work is present;
- the requested repository transition has occurred.

A change is not made "more complete" by adding irrelevant gates or evidence.

## Evidence truth

Evidence layers remain distinct:

- **L0** repository/static;
- **L1** unit/contract;
- **L2** configure/build/compiled host;
- **L3** Editor/UI/manual host interaction;
- **L4** installer/deployment/runtime/signing/release operation.

The validation policy defines when each layer applies.

Pending is not passing. `NOT_RUN` and `NOT_APPLICABLE` are valid states and should be used explicitly.
