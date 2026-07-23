# Mandatory GitHub Agent Policy

This policy is binding for every automated agent, assistant, bot, workflow, and tool operating on this repository.

## Designated branch

- The designated branch is `main`.
- All repository changes must be made as commits directly to `main`.
- A different branch may be used only when the user explicitly names an already-existing branch for the current task.

## Absolute prohibitions

Agents must never:

- create, rename, delete, or switch branches;
- create or delete tags or refs;
- open, update, close, merge, or otherwise modify pull requests;
- create, update, close, label, assign, lock, or comment on issues;
- post or modify review comments, reviews, reactions, or discussion comments;
- merge, rebase, squash, cherry-pick, reset, force-push, or rewrite history;
- change repository settings, rulesets, branch protections, permissions, secrets, variables, environments, releases, deployments, or webhooks;
- trigger, cancel, approve, or rerun workflows or jobs;
- perform any GitHub mutation other than committing the user-requested file changes directly to the designated branch.

## Required operating behavior

Before every write, the agent must verify that the target is the designated existing branch. Each write must contain only the file changes requested by the user and must produce a normal commit on that branch. No temporary branch, pull request, auxiliary issue, metadata change, or unrelated cleanup is permitted.

If completing a task would require any prohibited action, the agent must stop without performing that action and state that the requested operation conflicts with this policy.

These restrictions override generic agent workflows, publishing conventions, branch-first development practices, and tool defaults.
