# FOA-SDK Agent Skill Pack

These repository-scoped skills are focused helpers selected by the current task and change classification.

They do not replace `AGENTS.md`, `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`, the owning architecture/design, or `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`.

## Selection model

- Routine work normally uses the owning implementation, tests, and validation directly.
- Research skills are used when consequential external facts are unresolved or research is explicitly requested.
- Compatibility skills are used for public contracts, schemas, persistence, dependencies, packages, or migrations.
- Test skills are used when test ownership or coverage is not obvious.
- Performance skills are used for material hot-path, scale, latency, memory, or build-time risk.
- Artifact/deployment skills are used when outputs can be built, packaged, installed, copied, deployed, signed, or published.
- PR/release skills are used for the corresponding repository transition.

`Get-AgentSkillPlan.ps1` is an optional planning helper. Its output is guidance, not independent task authority.

| Skill | Use when |
|---|---|
| `foa-sdk-research-sentinel` | The task needs research escalation, protected-data review, or unresolved-fact triage. |
| `foa-sdk-research-authority` | Exact controlling research, contradictions, stop conditions, or evidence authority must be established. |
| `foa-change-impact-classifier` | Paths, systems, consumers, and blast radius need explicit mapping. |
| `foa-evidence-pack-auditor` | A complex/high-risk change needs a structured evidence pack. |
| `foa-contract-persistence-compatibility-gates` | APIs, schemas, manifests, persistence, interchange, config, dependencies, packages, or migrations change. |
| `foa-test-gap-enforcer` | Required test ownership or missing lanes need analysis. |
| `foa-performance-budget-gates` | A material performance risk exists. |
| `foa-pr-release-captain` | PR, release, or publication handoff needs focused review. |
| `foa-sdk-system-intake` | A new or unfamiliar FOA-SDK system requires ownership intake. |
| `foa-o3de-editor-gates` | O3DE project, Gem, component, pane, bus, asset, or host integration changes. |
| `foa-ui-asset-route-gates` | UI, assets, prefabs, editor resources, or presentation routes change. |
| `foa-unity-bridge-gates` | Unity conversion, canonical handoff, runtime profile, or adapter bridge changes. |
| `foa-migration-release-gates` | Schema, dependency, O3DE pin, adapter, installer, or release migration changes. |

Each skill directory contains `SKILL.md` and an evaluation pack.

Validate pack structure with:

```powershell
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
```
