# Mandatory GitHub Agent Policy

This policy is binding for every automated agent, assistant, bot, workflow, and tool operating on this repository.

## Designated workflow

- The reviewed integration branch is `main`.
- Agent-authored repository changes must be made on a non-`main` working branch.
- Completed agent work must be submitted to `main` by pull request for maintainer audit.
- The repository owner/maintainer must perform the final audit and merge decision.
- A different branch or direct-main path may be used only when the repository owner explicitly names that exception for the current task.

## Absolute prohibitions

Agents must never, unless explicitly authorised for the current task by the repository owner:

- commit directly to `main`;
- merge, approve, auto-merge, or close their own pull request;
- bypass a required pull-request audit;
- create, rename, delete, switch, reset, force-push, or rewrite protected integration branches;
- create or delete tags or refs;
- create, update, close, label, assign, lock, or comment on issues;
- post or modify review comments, reviews, reactions, or discussion comments except inside the pull request created for the current requested work;
- change repository settings, rulesets, branch protections, permissions, secrets, variables, environments, releases, deployments, or webhooks;
- trigger, cancel, approve, or rerun workflows or jobs;
- modify tests, validators, workflows, process documents, governance documents, contribution rules, or release gates unless the user explicitly requested that exact governance or validation change;
- claim validation, review, approval, authorization, provenance, signing, or completion that was not directly performed and verified.

## Required operating behavior

Before every write, the agent must verify the target branch and repository. Each write must contain only the file changes requested by the user. Broad cleanup, unrelated refactors, generated status churn, hidden process rewrites, or validation changes that make the requested work easier to pass are forbidden.

For normal work, the agent must:

1. create or select the requested non-`main` working branch;
2. make focused commits on that branch only;
3. preserve DCO sign-off when committing through the repository workflow;
4. run or report only validation that actually executed;
5. open a pull request to `main` for maintainer audit;
6. leave merge, approval, and final acceptance to the maintainer.

If completing a task would require any prohibited action, the agent must stop before performing that action and state that the requested operation conflicts with this policy.

These restrictions override generic agent workflows, publishing conventions, direct-to-main defaults, branchless editing patterns, and tool defaults.
