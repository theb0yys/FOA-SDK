# Kandra Operating Checklist

Status: derivative execution aid.

Use only the sections applicable to the current task. Kandra is not a universal ceremony stack.

## A. Before any work

- [ ] Restate the exact owner request and intended result.
- [ ] Verify target repository and target branch.
- [ ] Identify the repository transition actually requested.
- [ ] Read `AGENTS.md`.
- [ ] Read `CURRENT_TASK.md` for context; do not let it override a newer explicit owner instruction.
- [ ] Read the owning architecture/design/schema/policy for the affected surface.
- [ ] Classify the change: Routine, Significant, or Critical/Runtime.
- [ ] Identify the smallest file set.
- [ ] Record explicit out-of-scope work.
- [ ] Identify protected data/material boundaries.
- [ ] Identify any consequential unanswered claim.

Decision:
- [ ] Repository evidence is sufficient -> use normal engineering.
- [ ] Research is required -> continue with Kandra research path.

## B. Research triage

- [ ] State the exact unanswered claim.
- [ ] Identify the owner system.
- [ ] Identify the required evidence lane.
- [ ] Check repository-held evidence first.
- [ ] Determine whether the owner named a required research method.
- [ ] If authority is missing/contradictory, prepare the governed brief.
- [ ] Record protected paths/operations to avoid.
- [ ] Record the consuming design/gate.

## C. ChatGPT Deep Research, when requested

- [ ] Formal brief exists when needed.
- [ ] Actual ChatGPT Deep Research executed.
- [ ] No substitute method was presented as Deep Research.
- [ ] If execution failed, state `BLOCKED` and the verified reason.
- [ ] Returned report preserved as E1 context.
- [ ] Original report left unchanged.
- [ ] Clean derivative, if created, labelled as derivative.

## D. RH0 - intake

- [ ] Origin recorded.
- [ ] Intake date recorded.
- [ ] Exact path recorded.
- [ ] Fingerprint recorded when required.
- [ ] Evidence lane recorded.
- [ ] Opaque/conversation-local citations identified.
- [ ] Protected-data boundary recorded.
- [ ] No implementation/runtime/promotion authority implied.

## E. RH1 - claim evaluation

- [ ] Material claims split into stable claim IDs.
- [ ] Durable sources registered.
- [ ] Versions/revisions/dates bound where relevant.
- [ ] Source support is scoped, not overstated.
- [ ] Facts separated from inference/proposal/unknown/contradiction.
- [ ] Underlying evidence inspected for consequential claims.
- [ ] Unknowns remain unknown.
- [ ] Contradictions remain visible.

## F. RH2 - domain/repository review

- [ ] Claims compared with current architecture/design/schema/contracts.
- [ ] Owner and consumers identified.
- [ ] Forbidden domain identified.
- [ ] Compatibility/migration/security/legal/runtime implications identified where applicable.
- [ ] Stale or superseded repository sources identified.
- [ ] Implementation authority classified: authorised, partial, or blocked.
- [ ] Required downstream gates identified.

## G. RH3 - independent review

- [ ] Independent reviewer has preserved inputs and registers.
- [ ] Reviewer checks sources rather than trusting prior conclusions.
- [ ] Reviewer checks scope and authority-lane separation.
- [ ] Reviewer checks missing proof and contradictions.
- [ ] Findings/disagreements recorded.
- [ ] Unresolved items stay unresolved.

## H. RH4 - validation/evidence review

- [ ] Each consequential claim mapped to the correct evidence layer.
- [ ] Applicable L0-L4 validation identified.
- [ ] Static/research/decompilation/host/Editor/runtime lanes remain separate.
- [ ] Exact source/ref/artifact binding checked.
- [ ] Pending/skipped/stale/zero-test evidence not called a pass.
- [ ] Required unavailable evidence marked `PARTIAL`, `BLOCKED`, or `NOT_RUN`.
- [ ] Irrelevant evidence marked `NOT_APPLICABLE`.

## I. RH5 - human promotion

- [ ] RH0-RH4 record available.
- [ ] Adverse evidence and unknowns visible.
- [ ] Normative destination named.
- [ ] Consequences recorded.
- [ ] Human promotion owner makes the decision.
- [ ] Agent does not self-promote.
- [ ] Promotion result recorded: accepted, rejected, returned, or unpromoted.

## J. Before implementation

- [ ] Current owner instruction authorises implementation.
- [ ] Normative source, not merely research, authorises the behavior.
- [ ] Working branch is focused and non-`main`.
- [ ] No protected external mutation is implied.
- [ ] Smallest coherent implementation planned.
- [ ] Required validation selected from `CI_AND_LOCAL_VALIDATION.md`.

## K. After implementation

- [ ] Every diff line traces to the request or owning contract.
- [ ] No unrelated cleanup.
- [ ] No protected material committed.
- [ ] Applicable validation actually ran.
- [ ] Exact states reported.
- [ ] Documentation updated only where behavior changed.
- [ ] Commit is understandable and DCO-signed.
- [ ] Focused PR opened when delivery requires it.
- [ ] Merge left to maintainer unless explicitly authorised otherwise.

## L. Final handoff

Report:

- scope completed;
- files changed;
- branch;
- commit;
- pull request;
- validation states by layer;
- protected-data impact;
- remaining uncertainty/blockers;
- repository transition actually completed;
- transitions not performed.

Never imply a later transition occurred merely because the previous one did.
