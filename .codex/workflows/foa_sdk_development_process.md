# FOA-SDK Development Process

This workflow is mandatory for repository work.

## Gate 0: Skill Activation

- Identify the exact request and target paths.
- Inspect repository status and treat unrelated changes as user-owned.
- Run or reproduce `.codex/scripts/Get-AgentSkillPlan.ps1`.
- Activate the universal sentinel and every narrower selected skill.
- Read `CURRENT_TASK.md` and `DECISIONS.md` when present.

## Gate 1: Research Lock

- Read root governing documents, protected-files policy, system index, matching workflows, local folder governance, relevant research, existing implementation, and tests.
- Identify controlling authority versus supporting context.
- Record contradictions, stale sources, uncertainty, stop conditions, owner, protected paths, and the controlling research path.
- For capability, adapter, provider execution, build-manifest, package, deployment, launch, verification, rollback, execution-result, release-assembly/signing, or artifact-ownership work, read and follow:
  - `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`;
  - `.codex/workflows/foa_capability_execution_contract.md`.
- If authority is missing, unclear, contradictory, outdated, or unproven, stop and produce a Deep Research Brief.

## Gate 2: Mandatory Deep Review

Complete `.codex/checklists/deep_review.md` before editing. Complex work uses `.codex/checklists/review_record_template.md` internally.

Capability-execution work must explicitly classify the existing service as an owner, preview adapter, compatibility facade, or retirement candidate; identify the immutable preview/execute binding; separate support, qualification, environment, policy, authorisation, outcome, assessment, and promotion; and map the shared Build -> Package -> Deploy -> Launch -> Verify spine.

## Gate 3: Scoped Implementation

- Make the smallest owner-scoped change authorised by the request and research.
- Do not expand scope, clean unrelated work, invent architecture, weaken gates, or change public contracts, persistence, build behaviour, runtime behaviour, or dependencies without authority.
- Preserve O3DE authoring-host, external Unity-runtime, neutral interchange, permission, evidence, and adapter boundaries.
- Preserve inert V1 contracts. Future execution versions forward and may not be activated by flipping existing `*Allowed` flags or relaxing validators.
- Domain systems may own native materialisation and domain verification semantics, but must use the shared production spine for build, package, deployment, launch, rollback, receipts, and evidence handoff.
- No heavy invasive unnecessary code or shortcut implementation.

## Gate 4: Validation

- Run the focused commands printed by the test, performance, artifact/deployment, and capability-execution preflights.
- Keep foundation, plug-in, UI, interchange, conversion, installer, runtime-adapter, migration, and harness proof distinct.
- Separate local runnable checks, static package assertions, host-heavy/manual rows, exact-install runtime rows, and non-runnable governed rows.
- For capability execution, validate immutable plan binding, deterministic provider resolution, artifact ownership, idempotency, rollback, receipt binding, and evidence-promotion separation in the applicable lanes.
- Missing required proof is partial or blocked validation, never a pass.

## Gate 5: Post-Change Deep Review

Repeat the checklist after edits. Confirm research compliance, scope, protected-file safety, compatibility, performance, evidence completeness, capability-execution invariants, and honest proof status.

## Gate 6: Handoff

Report:

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

Use a non-`main` branch, DCO-compliant commit, and maintainer-audited pull request. Do not merge, approve, or claim acceptance.
