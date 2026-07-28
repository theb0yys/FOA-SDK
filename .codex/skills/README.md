# FOA-SDK Agent Skill Pack

These repository-scoped skills are mandatory research-first gates.

Every task starts with `foa-sdk-research-sentinel`. `Get-AgentSkillPlan.ps1` selects narrower skills from the request and target paths. The mandatory order is defined by `foa_research_first_process_stack.md`.

Code changes use `Get-AgentTestPlan.ps1` and `Get-AgentPerformancePlan.ps1`. Build-sensitive, conversion, packaging, installer, and runtime-adapter changes use `Get-AgentBuildDeployPlan.ps1`. Missing authority or proof is partial/blocked, not success.

| Skill | Use when |
|---|---|
| `foa-sdk-research-sentinel` | Every repository task. |
| `foa-sdk-research-authority` | Exact controlling research, contradictions, stop conditions, and next process are required. |
| `foa-change-impact-classifier` | Paths, systems, surfaces, consumers, and blast radius must be classified. |
| `foa-evidence-pack-auditor` | Any change, review, validation, PR, release, or blocked task. |
| `foa-contract-persistence-compatibility-gates` | APIs, schemas, manifests, persistence, interchange, config, dependencies, packages, or migrations. |
| `foa-test-gap-enforcer` | Code, UI, plug-in, toolchain, conversion, installer, adapter, migration, or harness work. |
| `foa-performance-budget-gates` | Any performance-relevant path. |
| `foa-pr-release-captain` | GitHub, PR, commit, branch, release, or handoff work. |
| `foa-sdk-system-intake` | Foundation, authoring, toolchain, integration, runtime-adapter, or support systems. |
| `foa-o3de-editor-gates` | O3DE Editor project, Gems, components, panes, buses, assets, or host integration. |
| `foa-ui-asset-route-gates` | UI, assets, prefabs, editor resources, or presentation routes. |
| `foa-unity-bridge-gates` | Unity provider, conversion project, canonical handoff, runtime profile, or adapter bridge. |
| `foa-migration-release-gates` | Schema, dependency, O3DE pin, Unity profile, adapter, installer, or release migration. |

Every skill directory contains its full `SKILL.md` operating gate and an `evals/evals.json` behavioural evaluation pack with at least three cases. JSON formatting does not determine evaluation completeness.

Validate the pack with:

```powershell
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
```
