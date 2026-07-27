# Mandatory GitHub Agent Policy

This policy is binding for every automated agent, assistant, bot, workflow, and tool operating on this repository.

## Designated workflow

- The reviewed integration branch is `main`.
- Agent-authored repository changes must be made on a non-`main` working branch.
- Completed agent work must be submitted to `main` by pull request for maintainer audit.
- The repository owner/maintainer must perform the final audit and merge decision.
- A different branch or direct-main path may be used only when the repository owner explicitly names that exception for the current task.

## Mandatory repository-reading gate

Before any repository write, branch/ref change, workflow/job action, pull-request action, release action, deployment action, validation claim, or completion claim, the agent must read the governing documents for the exact scope being touched in the current task.

At minimum, the agent must read:

1. `AGENTS.md`;
2. `README.md`;
3. `GOVERNANCE.md`;
4. `CONTRIBUTING.md`;
5. the governing `README.md`, policy document, design document, or process document in every folder being touched;
6. any linked architecture, release, installer, plug-in, validation, research, or design document that controls the requested change.

A file, folder, workflow, validator, test, process record, release path, or artifact path is not eligible for modification until every governing document for that scope has been read for the current task.

Before writing, the agent must state:

- the exact files it intends to touch;
- the governing documents read;
- the controlling requirements from those documents;
- what the change is not allowed to claim, grant, weaken, bypass, or promote;
- what remains unclear, if anything.

If any required governing document is missing, unread, contradictory, or unclear for the requested change, the agent must stop before writing. The agent must not infer, simplify, replace, weaken, or reinterpret the governing documents to make implementation easier.

The repository documents are authority. Agent judgement, convenience, memory, generic practice, tool defaults, and passing checks are not authority.

## Research and document authority

Agents must not create, update, delete, move, rename, rewrite, summarise into replacement form, reclassify, or structurally reorganise research records, governing documents, process documents, policy documents, contribution rules, release gates, validation gates, architecture records, design records, or folder-governing `README.md` files unless the repository owner explicitly authorises that exact document change for the current task.

An owner exception for research or document changes must identify:

- the repository owner or maintainer granting the exception;
- the exact document or research path to be changed;
- the exact adjustment authorised;
- the target branch or explicitly authorised direct-main path;
- the current task for which the exception applies.

General approval to work on a feature, fix a build, clean the repository, improve documentation, repair tests, restore workflows, or make the project usable does not authorise research, governance, process, validation, release-gate, or folder-governing document changes.

When the requested work conflicts with existing research or governing documents, the agent must stop and report the conflict. It must not resolve the conflict by editing the research, editing the governance, weakening validation, or treating the implementation target as more authoritative than the documents.

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
- modify tests, validators, workflows, process documents, governance documents, contribution rules, release gates, research records, architecture records, design records, or folder-governing documents unless the user explicitly requested that exact governance, validation, research, or document change;
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
