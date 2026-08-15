# Current Task

This file records the active FOA-SDK task state so work can continue without treating conversation history as authority.

## Goal

- Make the reviewed capability-execution architecture durable in repository documentation and mandatory agent process.
- Preserve the existing planning, preview, evidence, reconciliation, and release-contract owners.
- Establish one shared Build -> Package -> Deploy -> Launch -> Verify spine without implementing it in this task.

## Files Currently Involved

- `CURRENT_TASK.md`
- `DECISIONS.md`
- `docs/systems/SYSTEM_INDEX.md`
- `docs/tainted-grail-sdk/ARCHITECTURE.md`
- `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`
- `.codex/README.md`
- `.codex/workflows/foa_capability_execution_contract.md`
- `.codex/workflows/foa_sdk_development_process.md`
- `.codex/workflows/foa_research_first_process_stack.md`
- `.codex/scripts/Get-AgentSkillPlan.ps1`
- `.codex/checklists/deep_review.md`
- `.codex/skills/tests/Validate-AgentSkills.ps1`

## Constraints

- Documentation and process only; no product implementation source changes.
- Existing inert V1 adapter, deployment, release, and external-tool-interchange contracts remain inert.
- No `*Allowed` flag may be reinterpreted as execution authority.
- Build, package, deployment, launch, verification, rollback, signing, publication, save mutation, catalog mutation, and evidence promotion remain unimplemented and unauthorised by this task.
- Future implementation must use immutable preview/execute pairs, deterministic per-phase provider resolution, exact artifact ownership, idempotency, rollback planning, and result-evidence separation.
- Work remains on `architecture/capability-execution-contract` and enters `main` only through maintainer-audited pull request.

## Completed Work

- Current capability, adapter, build-manifest, package-preview, staging-preview, deployment-work-order, runtime-result, verification, release, and external-tool-interchange services were decomposed read-only at exact `main` commit `1f4f6176e7a0b06d31600447ed70a107946645ea`.
- The canonical Capability Execution Contract and shared production spine were defined.
- The documented process was updated to route future affected work through the contract.
- The stale prior continuation claim naming `governance/foa-sdk-context-port` was removed after that branch could not be found on GitHub.

## Remaining Work

- Run the agent-skill pack validator in a PowerShell-capable checkout.
- Complete maintainer audit of the focused documentation/process pull request.
- Authorise implementation batches separately; this documentation task grants no implementation transition.

## Do Not Change

- Product implementation source.
- Existing V1 canonical bytes or validators.
- Current runtime, deployment, save, signing, publication, or evidence-promotion authority.
- Unrelated repository files.

## Next Concrete Step

- Maintainer review of the capability-execution documentation/process pull request, followed by a separately authorised M0 implementation-authority decision before any source change.
