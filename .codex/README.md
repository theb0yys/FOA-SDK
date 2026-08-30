# Root Codex Pack

This directory contains optional repository-scoped helpers for FOA-SDK agents and maintainers.

The authoritative engineering workflow is [`docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`](../docs/tainted-grail-sdk/ENGINEERING_PROCESS.md). Agent repository transitions are governed by [`AGENTS.md`](../AGENTS.md). Validation applicability is governed by [`docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`](../docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md).

## How this pack is used

The files under `.codex/` are selected when they fit the current change; they are not a universal pre-edit gate.

- Research skills and workflows apply when consequential external facts are unresolved or research is explicitly requested.
- Test helpers apply when a change needs system-specific test mapping.
- Performance helpers apply when the changed path has a material performance risk.
- Artifact/deployment helpers apply when a change can produce or move artifacts.
- Capability-execution helpers apply to the accepted Build -> Package -> Deploy -> Launch -> Verify architecture.
- Checklists and evidence templates are available for complex or high-risk work.

Routine implementation inside accepted architecture does not require the complete skill pack, every preflight script, a Deep Research brief, an evidence pack, or a deep-review ceremony.

## Main resources

- `.codex/workflows/foa_sdk_development_process.md` — agent adapter for the public engineering process.
- `.codex/workflows/foa_research_first_process_stack.md` — conditional research escalation workflow.
- `.codex/workflows/foa_capability_execution_contract.md` — capability-execution architecture helper.
- `.codex/workflows/foa_sdk_test_gates.md` — system-specific test evidence guidance.
- `.codex/workflows/foa_professional_code_performance_gate.md` — performance-risk guidance.
- `.codex/workflows/foa_artifact_deploy_gate.md` — artifact and deployment guidance.
- `.codex/scripts/` — optional planning helpers.
- `.codex/checklists/` — optional complex-review and evidence templates.
- `.codex/skills/` — focused conditional skills.

## Validation

The pack structure can be checked with:

```powershell
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
```

That command validates the helper pack. It does not make every helper mandatory for every repository task.
