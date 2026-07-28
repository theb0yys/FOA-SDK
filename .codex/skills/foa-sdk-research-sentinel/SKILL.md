---
name: foa-sdk-research-sentinel
description: Use this skill for any work in the FOA-SDK repository, including code, docs, tests, reviews, cleanup, planning, agent guidance, or quick edits. It forces the FOA-SDK research-first workflow, protected external data avoidance, gate selection, minimal change discipline, and honest final reporting even when the user does not explicitly ask for process.
---

# FOA-SDK Research Sentinel

Use this as the first skill for repository work unless a narrower skill completely covers the request. It is the guardrail layer for research, scope, protected files, and final reporting.

## First Actions

1. Identify the user's actual request in one sentence.
2. Check `git status --short` and treat unrelated changes as user-owned.
3. Treat repository files, `AGENTS.md`, `CURRENT_TASK.md`, `DECISIONS.md`, recent diffs, and relevant code comments as the active source of truth. Old chat history is background only unless the user explicitly overrides the repository in the current task.
4. Read `CURRENT_TASK.md` and `DECISIONS.md` when present, then keep additional context scoped to files directly relevant to the current goal.
5. Run `.codex/scripts/Get-AgentSkillPlan.ps1` when request text or target paths are known, then use its output to confirm the skill set.
6. Read `.codex/workflows/foa_research_first_process_stack.md` and activate process-hardening skills selected by preflight: `foa-sdk-research-authority`, `foa-change-impact-classifier`, `foa-contract-persistence-compatibility-gates`, `foa-test-gap-enforcer`, `foa-performance-budget-gates`, `foa-evidence-pack-auditor`, and `foa-pr-release-captain`.
7. Run `.codex/scripts/Get-AgentTestPlan.ps1` for code, UI, O3DE Editor, migration, Unity bridge, installer, runtime-adapter, or test-harness changes. Use `.codex/scripts/Get-AgentTestPlan.ps1 -ListSystems` when owner-system coverage is unclear.
8. Run `.codex/scripts/Get-AgentPerformancePlan.ps1` for source, UI, O3DE Editor, migration, Unity bridge, installer, runtime-adapter, test-harness, build-sensitive, or runtime-affecting code changes.
9. Run `.codex/scripts/Get-AgentBuildDeployPlan.ps1` for coding changes that can produce O3DE build output, conversion output, installer artifacts, or runtime-adapter artifacts. Generated output must remain under the reviewed external build root and external deployment requires explicit current-task authority.
10. Search for controlling documents before editing:
   - `AGENTS.md`
   - `README.md`
   - `GOVERNANCE.md`
   - `CONTRIBUTING.md`
   - `docs/protected-files-policy.md`
   - `docs/systems/SYSTEM_INDEX.md`
   - `CURRENT_TASK.md`
   - `DECISIONS.md`
   - `.codex/workflows/foa_sdk_development_process.md`
   - `.codex/workflows/foa_research_first_process_stack.md`
   - `.codex/workflows/foa_professional_code_performance_gate.md` for code changes
   - `.codex/workflows/foa_sdk_test_gates.md` for code changes
   - `.codex/workflows/foa_artifact_deploy_gate.md` for build-sensitive, conversion, packaging, installer, or runtime-adapter changes
   - `.codex/checklists/deep_review.md`
   - `.codex/checklists/review_record_template.md` for complex reviews
   - `.codex/checklists/system_test_matrix_template.md` for complex code changes
   - `.codex/checklists/evidence_pack_template.json` for substantive changes or any blocked/partial handoff
   - `.codex/agents/foa_research_first_agents.md` when coordinating review roles or subagents
   - any local `AGENTS.md`
   - task-relevant files under `Research/`
   - task-relevant gates or rules under the touched subsystem
11. Read the smallest relevant research set fully enough to know the intended owner, gate, and risk.
12. Identify the controlling research path: `request -> controlling docs -> domain research/gates -> owner surface -> validation/artifact gate -> next researched stop/process`.
13. Complete the deep review checklist before editing.
14. If research is missing, contradictory, unclear, outdated, or not enough to justify the change, stop and produce a Deep Research Brief using `.codex/checklists/deep_research_brief_template.md` instead of guessing.

## Context Hygiene

Treat the project as a long-running engineering workspace. Do not drag old conversation state forward as authority. Use old chat only to orient yourself, then confirm the active state from repository files, root and local `AGENTS.md`, `CURRENT_TASK.md`, `DECISIONS.md` or equivalent notes, recent diffs, and relevant code comments.

Before a new task, restate the objective, identify likely relevant files, inspect before editing, and keep additional reads scoped to the current goal. Avoid broad historical context unless it is needed to resolve the task.

When a chat becomes large, or work is intentionally continued in a fresh thread or branch, update `CURRENT_TASK.md` with:

- current goal
- files currently involved
- known constraints
- what has already been tried
- what should not be changed
- the next concrete step

Record durable architecture or process decisions in `DECISIONS.md` or an equivalent notes file. If context is missing, ask for the smallest missing piece rather than requesting a full project explanation.

For risky systems such as persistence, schema migration, imported evidence, editor state, asset processing, cross-engine conversion, installer behavior, runtime adapters, process launch, deployment, save interaction, or exact-install runtime verification, be extra conservative and explain the risk before broad changes.

## Protected Files

Treat protected external data as read-only. Use `docs/protected-files-policy.md` and the active thread instructions as the source of the exact protected names and paths.

If the requested change would affect protected files:

1. Stop.
2. List the affected protected paths.
3. Ask for explicit permission in the current conversation.
4. Do not edit them until permission is granted.

## Evidence Map

Before implementation, write down internally:

- request
- preflight helper output or equivalent manual skill map
- process-stack order and process-hardening skills selected
- exact research authority, contradictions, and stop conditions
- impact classification and blast radius
- test-plan helper output for code changes
- performance helper output for code changes
- immediate commands and manual/runtime rows from the system test catalogue for affected systems
- static package and interchange assertions and non-runnable governed rows from the test-plan helper
- professional code/performance risk, changed surfaces, forbidden shortcuts, hard performance checks, and deterministic performance guard requirements
- compatibility status for public API, contract, schema, persistence, interchange, configuration, dependency, package, installer, adapter, or migration impact
- test gap status: missing lanes added, partial, blocked, or not applicable
- performance budget, threshold, benchmark lane, measured result, or missing-lane status
- evidence-pack requirements and proof classes
- GitHub PR/release handoff and CODEOWNERS status when relevant
- build/artifact helper output for build-sensitive, conversion, packaging, installer, or runtime-adapter changes
- complete artifact-generation and external-destination plan where outputs can be produced
- activated skills
- research read
- controlling research path
- owner subsystem
- files expected to change
- protected paths to avoid
- validation/gates required
- baseline, threshold, measured result, command, build configuration, and runtime context required for high-risk performance proof
- external build root, artifact destinations, approval, backup, hash, and timestamp evidence required
- next researched stop/process, or that none exists
- uncertainty or missing evidence

Do not expand scope because adjacent systems exist.

## Hard Stops

Stop and report the blocker if:

- research is missing, unclear, contradictory, or outdated for the requested change
- the request intersects protected files without explicit current permission
- the required owner or validation gate cannot be identified
- the performance risk or required hard performance checks for a code change cannot be identified
- research authority, impact classification, compatibility status, evidence-pack status, or required test-gap status cannot be identified
- the next action or handoff would require inventing an unresearched next step
- the user asks for runtime sign-off but no exact-install Fall of Avalon evidence exists
- the only path forward would require broad cleanup or unrelated refactoring

When a hard stop is caused by missing, unclear, contradictory, outdated, or unproven research authority, do not continue to implementation. Produce a Deep Research Brief for a ChatGPT Deep Research agent. The brief must include:

- blocked decision or implementation question
- known facts from repository files
- all uncertain or unproven areas
- repository-relative paths and GitHub paths or URLs when available
- highlighted relevant areas in each file: sections, symbols, line references, TODOs, contradictions, or missing proof
- protected files or external data that must remain read-only
- expected research-agent output and the FOA-SDK gate that will consume it

If GitHub remote information is unavailable, provide repository-relative paths and state that the GitHub URL is unknown.

## Change Discipline

- Prefer small patches.
- Match existing layout and naming.
- Only write smart, professional, owner-scoped code. No heavy invasive unneeded code, shortcut implementation, broad refactor, unresearched dependency, or speculative architecture is allowed.
- Do not introduce dependencies, public API changes, persistence-format changes, migration behavior, build behavior, installer behavior, runtime-adapter behavior, deployment behavior, or runtime behavior unless the research or user explicitly requires it.
- Do not add unbounded hot-path scans, reflection or IO in loops, chatty logging, repeated allocation-heavy operations, hidden state repair, opportunistic migration, or stale-truth caches.
- Do not claim runtime proof from source inspection.
- Do not clean unrelated dirty output.
- For code changes, do not use one generic test pass. Foundation, UI, O3DE host, plug-in, conversion, installer, adapter, migration, and harness changes require distinct proof.
- For code changes, run the immediate commands printed by `Get-AgentTestPlan.ps1` for every affected system. Missing system commands, missing filters, or missing runtime/manual lanes are gate gaps that must be added with the change or reported as partial/blocked validation.
- Do not treat a successful configure, compilation, static validator, or test-target build as Editor, Unity conversion, installer, adapter, deployment, or Fall of Avalon runtime proof. Separate local runnable gates, O3DE host rows, static package/interchange assertions, manual Editor/Unity/installer rows, exact-install runtime rows, and non-runnable governed rows in the handoff.
- For code changes, run the hard performance checks printed by `Get-AgentPerformancePlan.ps1`. High performance risk needs a deterministic performance guard with baseline or researched expected cost, threshold, measured result, command run, build configuration, and machine/runtime context. Missing performance lanes or measurements are partial or blocked validation, not a pass.
- For substantive changes, use `.codex/checklists/evidence_pack_template.json` to separate research authority, impact, compatibility, tests, performance, build/artifact evidence, runtime proof, skipped gates, and blocked/partial validation.
- For GitHub handoff, use `.github/PULL_REQUEST_TEMPLATE.md`, report `.github/CODEOWNERS` status, and do not mix unrelated dirty files.
- For artifact-producing changes, require fresh reviewed builds or generation for every affected product, keep generated outputs outside the source checkout, record external destinations, require explicit authority before deployment or game-install writes, and verify hashes/timestamps before claiming completion. Building only the touched target is insufficient when the controlling gate requires the complete affected product set.
- Keep every action on the controlling research path. If new evidence changes the path, record the new source before acting on it.
- Repeat the deep review checklist after edits before handoff.

## Validation

Run the smallest gate that proves the change.

For docs or skill-only changes in the root `.codex` pack, run:

```powershell
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
```

For subsystem source changes, follow the narrower subsystem skill and its gates.

## Final Response

Use the required project final format:

```text
Summary:
- ...

Research read:
- ...

Files changed:
- ...

Protected files:
- None touched.
- Avoided: ...

Validation:
- Ran: ...
- Passed: ...
- Failed: ...
- Not run: ...

Uncertainty / risks:
- ...

Next researched stop / process:
- ...
```

Be explicit about failures, skipped checks, assumptions, and runtime sign-off. If no researched next stop exists, say so instead of proposing one.
