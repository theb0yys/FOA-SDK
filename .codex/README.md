# Root Codex Pack

This directory holds repository-scoped Codex workflow assets for FOA-SDK.

Skills are mandatory process gates, not optional notes. They package the research-first instructions, protected-files policy, ownership model, compatibility gates, validation, performance, artifact/deployment review, evidence, and handoff rules.

Process entry points:

- `AGENTS.md`
- `.codex/workflows/foa_sdk_development_process.md`
- `.codex/workflows/foa_research_first_process_stack.md`
- `.codex/workflows/foa_capability_execution_contract.md`
- `.codex/checklists/deep_review.md`
- `.codex/checklists/review_record_template.md`
- `.codex/checklists/evidence_pack_template.json`
- `.codex/checklists/deep_research_brief_template.md`
- `CURRENT_TASK.md`
- `DECISIONS.md`
- `.codex/agents/foa_research_first_agents.md`
- `.codex/scripts/Get-AgentSkillPlan.ps1`
- `.codex/scripts/Get-AgentTestPlan.ps1`
- `.codex/scripts/Get-AgentPerformancePlan.ps1`
- `.codex/scripts/Get-AgentBuildDeployPlan.ps1`
- `.codex/workflows/foa_professional_code_performance_gate.md`
- `.codex/workflows/foa_sdk_test_gates.md`
- `.codex/workflows/foa_artifact_deploy_gate.md`
- `.codex/checklists/system_test_matrix_template.md`

Before any edit, activate `foa-sdk-research-sentinel`, add narrower skills selected by preflight, follow the process stack, and complete deep review. Code changes require test and performance preflight. Build-sensitive, conversion, packaging, installer, or runtime-adapter work requires the artifact/deployment preflight.

Capability, adapter, build-manifest, package, staging, deployment, launch, verification, rollback, execution-result, release-assembly, signing, provider-execution, or artifact-ownership work must also follow `.codex/workflows/foa_capability_execution_contract.md` and the canonical public contract at `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`. Those documents do not grant implementation or runtime authority.

Every action must remain on the controlling research path. If authority is missing, unclear, contradictory, outdated, or unproven, implementation stops and a Deep Research Brief is produced. Handoff ends with the next researched stop/process or states that none exists.

Validation:

```powershell
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
```
