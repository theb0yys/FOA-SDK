---
name: foa-sdk-research-sentinel
description: Use when an FOA-SDK task depends on unresolved consequential facts, protected external data, uncertain evidence authority, or an explicit research request. Do not use as a universal gate for routine implementation inside accepted architecture.
---

# FOA-SDK Research Sentinel

This skill triages whether the current task needs research escalation. The default engineering workflow is `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`.

## Trigger

The task needs research escalation only when at least one of the following is true:

- the repository owner explicitly requests research or Deep Research;
- implementation depends on unknown Fall of Avalon runtime behavior, native identity, save/install state, or proprietary format behavior;
- a material compatibility, licence, dependency, deployment, signing, publication, permission, or security fact is unproven;
- protected external data may be involved;
- controlling evidence is materially contradictory or insufficient for the requested consequential claim.

Do not use this skill merely because the task touches code, tests, documentation, a pull request, or an existing architecture.

## Triage

1. State the exact unanswered claim.
2. Identify the owner system and requested repository transition.
3. Check `AGENTS.md`, `CURRENT_TASK.md`, the owning design/architecture, and directly inspected evidence.
4. Identify the required evidence lane: repository/static, research, decompilation/static, host execution, or live runtime.
5. Apply `docs/protected-files-policy.md` when external game data, saves, installations, credentials, or proprietary material are relevant.
6. Select narrower research/compatibility/evidence skills only when they fit the claim.

## Outcomes

### Repository evidence is sufficient

Return to the normal Routine, Significant, or Critical/Runtime workflow. Do not add research ceremony.

### Focused research is needed

Use `.codex/workflows/foa_research_first_process_stack.md` and record the exact question, sources, contradictions, and resulting confidence.

### ChatGPT Deep Research is explicitly requested

Execute actual ChatGPT Deep Research. A brief, web search, repository inspection, or ordinary synthesis is not a substitute for a returned Deep Research report.

### Required evidence remains unavailable

Report the affected claim as `PARTIAL`, `BLOCKED`, or `NOT_RUN`. Do not invent a substitute or broaden the task.

## Evidence discipline

Never convert:

- research context into verified runtime behavior;
- decompilation/static evidence into live execution proof;
- configure/build success into Editor interaction;
- adapter compilation into Fall of Avalon compatibility;
- a receipt/hash into authorization;
- candidate evidence into permission or promotion.

## Protected material

Do not modify or commit protected external material without explicit current-task authorization. Keep read-only evidence outside the repository and redact private paths, credentials, saves, and proprietary payloads.

## Handoff

Report the exact question, evidence inspected, conclusion, remaining uncertainty, protected-data boundary, and whether implementation may proceed under the normal engineering process.
