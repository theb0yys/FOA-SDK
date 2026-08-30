# Deep Review Checklist

Use this optional checklist for Significant or Critical/Runtime changes, or for a focused review whose compatibility, evidence, security, or operational blast radius is non-trivial.

Do not use it as a universal pre-edit or post-edit gate for Routine changes. Select only the sections applicable to the reviewed surface.

## Review Setup

- **Request:** exact ask, desired outcome, out of scope, and authorized repository transition.
- **Classification:** Routine, Significant, or Critical/Runtime, with the reason for that classification.
- **Owner surface:** owning system, governing architecture/design, implementation, consumers, and existing focused tests.
- **Review scope:** exact files and claims being reviewed; unrelated cleanup and later milestones remain excluded.

## Before Implementation or Review

### Authority and evidence

- Distinguish controlling repository authority from supporting context and historical records.
- Identify contradictions, stale instructions, or missing proof only when they affect the requested claim.
- Research only when consequential external facts are unresolved or research is explicitly requested.
- Keep repository/static, research, decompilation/static, host execution, Editor/UI, and live runtime evidence separate.

### Protected information

- Apply `docs/protected-files-policy.md` when game files, saves, installations, credentials, private paths, proprietary content, or external writes are relevant.
- Record protected paths avoided and any explicit permission required before an external operation.

### Ownership and compatibility

- Map owners, producers, consumers, public APIs, schemas, persistence, configuration, build, UI, runtime, and release impact where applicable.
- For durable/public changes, identify migration, rejection, downgrade, rollback, and degraded-dependency behavior.
- Do not infer permission, runtime compatibility, or promotion authority from a declaration, plan, hash, receipt, build, or research report.

### Tests and validation

- Select evidence from `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`.
- Run focused owner tests first; add broader host or operational lanes only when the changed surface can affect them.
- Record required but unavailable checks as `PARTIAL`, `BLOCKED`, or `NOT_RUN`.
- Record irrelevant evidence layers as `NOT_APPLICABLE`.

### Performance

- Review performance only when a material latency, responsiveness, startup/build-time, memory, throughput, scale, or hot-path risk exists.
- For applicable high-risk paths, record representative cardinality, baseline, threshold, command, configuration, and measured result.

### Artifacts and external operations

- Identify affected artifacts and external destinations only when the change can build, package, install, copy, deploy, sign, or publish output.
- Keep generated output outside source checkouts.
- Require explicit current-task authority, backup/rollback or recovery, and exact evidence before an external mutation.

### Capability execution

- When capability execution is in scope, preserve inert V1 contracts, immutable preview/execute binding, per-phase provider resolution, policy and authorization separation, artifact ownership, idempotency, rollback, receipts, candidate-evidence separation, and the shared `Build -> Package -> Deploy -> Launch -> Verify` spine.

## After Implementation or Review

- Every change traces to the request and owning contract.
- Only intended files changed; unrelated and protected files remain untouched.
- Public contracts, schemas, persistence, configuration, build, UI, runtime, permission, and evidence boundaries remain intact unless explicitly changed by the reviewed design.
- Failure, malformed-input, rollback/recovery, and degraded-dependency behavior is covered where applicable.
- No unbounded scans, repeated hot-path IO/reflection/logging, hidden repair, stale-truth cache, or unnecessary architecture expansion was introduced.
- Actual validation results are reported without upgrading static, compiled, skipped, stale-head, zero-test, or self-declared evidence into a broader pass.
- `CURRENT_TASK.md` and `DECISIONS.md` changed only when active state or a durable decision actually changed.
- The final handoff records scope, files, exact validation states, protected-data impact, remaining uncertainty, and the repository transition that actually occurred.
- Do not invent or execute a follow-on task merely because a checklist, report, or roadmap names one.
