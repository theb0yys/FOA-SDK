# Deep Review Checklist

Use this checklist before and after every agent edit. It is internal unless requested.

## Pre-Edit Review

- **Request:** exact ask, out of scope, active repository state, concise objective.
- **Skill activation:** skill, test, performance, and artifact/deployment preflights run where applicable; universal and narrower skills loaded.
- **Research:** root governance, protected policy, system index, current task, decisions, local governance, domain research, existing implementation, tests, and controlling research path identified.
- **Authority:** controlling versus supporting sources, contradictions, stale or missing proof, stop conditions, Deep Research Brief requirement, next researched process.
- **Protected files:** nearby protected paths, avoided paths, permission requirements.
- **Ownership and impact:** owner system, changed surfaces, consumers, cross-system chains, blast radius, public API/schema/persistence/config/build/runtime/UI impact.
- **Compatibility:** producers, consumers, readers, writers, serializers, interchange readers, loaders, migration policy, compatibility status.
- **Capability execution:** when applicable, read the canonical contract and execution workflow; classify every current service as owner, preview adapter, compatibility facade, or retirement candidate; keep inert V1 contracts inert; bind preview and execute to one immutable fingerprinted plan; separate support, qualification, environment, policy, authorisation, outcome, assessment, and promotion; map per-phase provider resolution, artifact ownership, idempotency, rollback, receipts, evidence projection, and the shared Build -> Package -> Deploy -> Launch -> Verify spine.
- **Tests:** system-specific commands, O3DE gates, static assertions, manual Editor/Unity/installer rows, runtime rows, non-runnable rows, missing lanes.
- **Performance:** risk, hot paths, data cardinality, forbidden shortcuts, guard, baseline, threshold, command, configuration, context.
- **Artifacts/deployment:** affected products, build commands, external output root, destinations, approvals, backups, hashes, timestamps.
- **Evidence and GitHub:** evidence-pack fields, PR template, CODEOWNERS, branch scope, unrelated dirty files.
- **Plan:** smallest safe change, exact files, validation, no unresearched next action.

## Post-Edit Review

- Every change traces to the request and controlling research.
- Only intended files changed; unrelated and protected files remain untouched.
- Ownership, public contracts, schemas, persistence, configuration, build, runtime, permission, and evidence boundaries remain intact unless explicitly authorised.
- Capability-execution changes preserve inert V1 contracts, immutable preview/execute binding, deterministic per-phase provider resolution, policy/authorisation separation, artifact ownership, idempotency, planned rollback, receipt binding, candidate-evidence separation, and the single shared production spine.
- No domain-specific private build, deployment, launch, rollback, receipt, or evidence-promotion path was introduced.
- No invasive unnecessary code, shortcuts, unbounded scans, repeated IO/reflection/logging, hidden repair, or stale-truth cache was added.
- Required checks ran; missing checks are reported partial/blocked.
- Artifacts are current, externally stored, and verified where applicable.
- Runtime proof is not claimed from static or local evidence.
- `CURRENT_TASK.md` and `DECISIONS.md` are updated only when continuation or durable decisions require it.
- Final handoff lists research, files, protected paths, validation, uncertainty, and next researched stop/process.
