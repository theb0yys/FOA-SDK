# Current Task

This file is the active handoff surface for long-running FOA-SDK Codex work. Update it when a chat becomes large, work moves to a fresh thread or branch, or the user asks for a handoff summary.

## Active State

- Current goal: remove process incoherency that lets agents route around the FOA research-first process, keep the wrong separate local Codex workflow removed, and add explicit skills for Unity authoring and Tainted Grail modding gates.
- Required outcome: root policy, `.codex` entry points, the research sentinel, the skill preflight helper, and structural validators require the research-first process stack, no separate local workflow of that kind, and domain gates for Unity authoring plus Tainted Grail modding work.
- Files currently involved: `AGENTS.md`, `CURRENT_TASK.md`, `DECISIONS.md`, `.codex/README.md`, `.codex/skills/README.md`, `.codex/scripts/Get-AgentSkillPlan.ps1`, `.codex/skills/foa-sdk-research-sentinel/SKILL.md`, `.codex/skills/foa-unity-authoring-gates/SKILL.md`, `.codex/skills/foa-tainted-grail-modding-gates/SKILL.md`, `.codex/skills/tests/Validate-AgentSkills.ps1`, `.codex/skills/tests/Validate-AgentSkills-v2.ps1`, `Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_repository_structure.py`, and the local workflow file being removed.
- Known constraints: no process weakening beyond removing the wrong workflow, no redesign, no public mod-development handbook edits, no product source edits, no installer edits, and no unrelated cleanup; existing FOA-SDK architecture and authority boundaries remain controlling; runtime mutation, silent deployment, save modification, signing, publication, catalog mutation, and evidence promotion remain prohibited.
- Branch and worktree state observed on 2026-07-29: this checkout is on non-`main` branch `codex/designed-installer-wizard`; no local branch named `governance/foa-sdk-context-port` was found; unrelated installer changes were already present and must remain excluded from this process slice.
- Local-control caution: `.agents/skills/verified-slice-github-flow/SKILL.md` treats `AGENTS.md`, `.agents/`, and `.codex/` as local control files that must not be staged or pushed. If this process pack is meant to be published through GitHub, that policy conflict needs explicit repository-owner resolution before staging or PR handoff.
- What has already been tried: the plugin skill guidance was read, root FOA-SDK governing documents were read, the FOA skill preflight selected the research, impact, evidence, Unity bridge, and PR handoff skills, exact-reference search found no remaining required reference to the wrong local workflow in the main process pack, and domain research confirmed Unity authoring/interchange and Tainted Grail modding need separate no-authority escape gates.
- Current blocker found: the process pack had no dedicated Unity authoring gate and no dedicated Tainted Grail modding gate, so a request could select the bridge gate without forcing Unity Editor package/test-project boundaries, exact modding profile evidence, source-port provenance, and Mono/IL2CPP route separation.
- What should not be changed: public mod-development docs, product implementation source, installer files, protected external data, source-repository files, root product README, GitHub workflow behavior, or unrelated dirty worktree files.
- Next concrete step: rerun the structural validators, skill preflight checks, and exact-reference scans after adding the new domain skills and routing.

## Continuation Template

- Current goal:
- Required outcome:
- Files currently involved:
- Known constraints:
- Branch and worktree state:
- What has already been tried:
- Current blocker:
- What should not be changed:
- Next concrete step:
