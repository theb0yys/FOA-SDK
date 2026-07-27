---
name: foa-sdk-research-sentinel
description: Use this skill for every FOA-SDK repository task, including code, docs, tests, reviews, cleanup, planning, agent guidance, or quick edits. It enforces research-first authority, protected-data avoidance, gate selection, minimal change discipline, and honest final reporting.
---

# FOA-SDK Research Sentinel

Use this first unless a narrower skill completely covers the request.

## First Actions

1. State the actual request in one sentence.
2. Inspect repository status; unrelated changes are user-owned.
3. Treat repository files, `AGENTS.md`, `CURRENT_TASK.md`, `DECISIONS.md`, recent diffs, and relevant comments as source of truth. Old chat is background only.
4. Run `Get-AgentSkillPlan.ps1`, and for code run the test, performance, and artifact/deployment preflights where applicable.
5. Read the process stack, root governance, protected policy, system index, local governance, domain research, implementation, and tests.
6. Identify `request -> controlling docs -> domain research/gates -> owner surface -> validation/artifact gate -> next researched stop/process`.
7. Complete deep review before editing.
8. If authority is missing, unclear, contradictory, outdated, or unproven, stop and produce a Deep Research Brief.

## Context Hygiene

Keep context scoped to the current goal. Update `CURRENT_TASK.md` for continuation and `DECISIONS.md` for durable decisions. Ask only for the smallest missing fact.

## Protected Files

Protected external O3DE, Unity, game, save, credential, signing, private-path, and proprietary data is read-only. Stop and request explicit current-task permission before any protected write.

## Hard Stops

Stop when research, owner, compatibility, required proof, protected status, or next researched action cannot be established; runtime sign-off is requested without exact-install evidence; or progress would require unrelated cleanup or invented architecture.

## Change Discipline

Prefer small owner-scoped patches. Preserve O3DE/Unity/runtime boundaries. Do not introduce unresearched dependencies, public contract or persistence changes, runtime mutation, silent deployment, unbounded scans, hot-path IO/reflection/logging, hidden repair, stale-truth caches, or generic false-green validation.

## Validation

Run the smallest gate that proves the change. For `.codex`-only work run `Validate-AgentSkills.ps1`.

## Final Response

Use the project format: Summary, Research read, Files changed, Protected files, Validation, Uncertainty/risks, Next researched stop/process. State failures, skipped checks, assumptions, and `runtime sign-off not performed` when applicable.
