# FOA-SDK Agent Execution Policy

This file defines repository-specific rules for automated agents working in FOA-SDK. It is intentionally limited to execution governance. Product architecture, task scope, and validation details live in their owning documents.

## Authority order

For a repository task, use the following order:

1. the repository owner's current explicit instruction for the task;
2. applicable legal, licence, security, and protected-file restrictions;
3. this agent policy;
4. [Engineering Process](docs/tainted-grail-sdk/ENGINEERING_PROCESS.md);
5. [Current Task](CURRENT_TASK.md);
6. accepted durable decisions in [DECISIONS.md](DECISIONS.md);
7. the architecture, design, schema, or folder policy that owns the files being changed;
8. [CI, Runner, and Local Validation Policy](docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md);
9. guides, examples, historical records, and issue discussion.

A current explicit owner instruction may change task scope or authorize a repository transition. It never turns an unexecuted test into a pass, a static result into runtime proof, or an unlawful/protected operation into a valid claim.

## Before changing the repository

For the exact requested scope:

- verify the repository and target branch;
- read this file, `CURRENT_TASK.md`, and the governing document for the files being changed;
- read `README.md` when repository identity, checkout layout, or build setup matters;
- read `docs/protected-files-policy.md` when external game data, saves, installations, credentials, or protected inputs are relevant;
- classify the change as Routine, Significant, or Critical/Runtime using `ENGINEERING_PROCESS.md`;
- identify the smallest file set required for the requested result.

Do not perform broad cleanup, unrelated refactors, architecture redesign, or follow-on tasks unless the owner explicitly requests them.

## Research escalation

Research is a tool, not a universal precondition.

Escalate to the repository research process when the requested change depends on consequential facts that are not established by the repository or directly inspected evidence, including:

- Fall of Avalon runtime behavior or native identities;
- proprietary/external file formats or executable behavior;
- third-party compatibility, licence, or version claims;
- uncertain deployment, save, signing, publication, or security behavior;
- a material contradiction in the architecture or evidence needed for the requested implementation;
- an explicit owner request for research or Deep Research.

Routine implementation inside accepted architecture does not require a research-sentinel stack, Deep Research brief, skill-plan script, performance plan, evidence pack, or pre/post deep-review checklist unless that specific task actually needs one.

Research findings are context until their consequential claims are verified through the evidence appropriate to the claim.

## Repository writes

Normal agent-authored changes:

- use a focused non-`main` working branch;
- change only files required by the current task;
- keep commits understandable and DCO-signed;
- submit completed work to `main` through a pull request for maintainer audit;
- leave approval and merge to the maintainer unless the owner explicitly authorizes a different transition for the current task.

Before each write, confirm that the target branch is not `main` unless the owner explicitly authorized direct-main work.

## Actions requiring explicit owner authorization

Do not independently:

- commit directly to `main`;
- merge, approve, auto-merge, or close a pull request;
- force-push, reset, delete, or rewrite shared/protected branches or tags;
- create a release, deployment, publication, or signing action;
- change repository settings, branch protection, rulesets, secrets, variables, environments, or webhooks;
- trigger, cancel, approve, or rerun workflows when that action changes repository/CI state;
- modify protected game files, saves, installations, credentials, or external proprietary material;
- broaden a task into another milestone or execute a documented "next task" without a new owner instruction.

Creating a focused working branch, commit, or pull request is permitted when it is the normal repository transition needed to deliver the task the owner requested.

## Validation and claims

Run the validation required by the change classification and affected surface in `CI_AND_LOCAL_VALIDATION.md`.

Always distinguish:

- static validation;
- unit/contract tests;
- configure/build/compiled tests;
- Editor/UI evidence;
- installer/deployment/runtime/release evidence.

Use exact states: `PASSED`, `FAILED`, `PARTIAL`, `BLOCKED`, `NOT_RUN`, or `NOT_APPLICABLE`.

Never claim:

- a command ran when it did not;
- a pending or skipped job passed;
- zero matching tests are a pass;
- compilation proves runtime behavior;
- a receipt/hash proves authorization or authenticity;
- repository evidence proves the state of a user's installed game;
- research or decompilation proves live runtime behavior.

## Protected information

Never commit secrets, credentials, personal machine paths, private user data, proprietary game source/assets without redistribution rights, saves, signing material, or generated build output that belongs outside the source checkout.

When protected material is needed only as read-only evidence, keep it outside the repository and report that boundary.

## Completion

A task is complete when:

- the requested scope is implemented;
- required applicable validation has actually run and its result is reported;
- documentation affected by the behavior is current;
- the diff contains no unrelated work;
- the repository transition requested by the owner has been completed, but no further transition has been inferred.

If work is incomplete, report the exact remaining state rather than inventing a procedural substitute.
