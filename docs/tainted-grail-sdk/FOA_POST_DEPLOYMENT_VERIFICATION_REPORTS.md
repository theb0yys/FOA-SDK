# FoA Post-Deployment Verification and Release-Blocker Reports

Status: implemented as a deterministic, read-only Phase 8 contract and Editor view.

## Purpose

Slice 16 turns one already accepted deployment execution-result evidence return into an explicit operator-review report. It aggregates the supplied work-order binding, candidate source/evidence documents, step outcomes, backup results, target-verification states, rollback completeness, failures, and safe referenced diagnostics.

The report does not independently verify FoA or the filesystem. It does not convert executor-supplied metadata into promoted truth. A `review_ready` report only means that the supplied, contract-valid metadata contains no blocker recognised by this aggregation contract; human review remains required.

## Exact inputs

`AdapterPostDeploymentVerificationService::BuildReport` receives:

1. the exact current `review_ready` deployment work order;
2. one deployment execution-result envelope bound to that work order; and
3. the candidate evidence return produced for that exact pair.

The service fails closed when the current work order, canonical JSON, preview, pack, target inventory, result/work-order fingerprints, typed counts, source documents, evidence documents, or exact profile/game/branch/runtime context drift.

The current work order must keep every execution and mutation permission false.

## Report states

The deterministic report status is one of:

- `evidence_rejected` — the upstream execution-result evidence envelope was not accepted;
- `evidence_incomplete` — accepted metadata or candidate evidence does not preserve the exact required binding;
- `verification_incomplete` — at least one declared target verification remains `not_checked`;
- `compatibility_blocked` — supplied target-verification metadata reports a mismatch;
- `rollback_incomplete` — a required rollback or restore did not complete successfully;
- `release_blocked` — other release blockers remain, including failed/skipped steps, incomplete backups, successful rollback of the attempted candidate, reported failures, or missing diagnostics;
- `review_ready` — no explicit compatibility or release blocker remains in the supplied metadata.

Status precedence is stable and independent of input container order.

## Blockers

Every blocker has a stable result-scoped ID, typed kind, code, subject, message, optional step and rollback identities, sorted evidence IDs, sorted log-reference IDs, and independent compatibility/release effects.

Typed blocker kinds are:

- `execution_evidence_rejected`;
- `evidence_binding_mismatch`;
- `candidate_evidence_missing`;
- `step_not_completed`;
- `step_failed`;
- `backup_incomplete`;
- `target_not_checked`;
- `target_mismatch`;
- `rollback_incomplete`;
- `deployment_rolled_back`;
- `failure_reported`;
- `diagnostic_missing`.

A target mismatch or unchecked target blocks both compatibility review and release review. Operational failures, incomplete backups, rollback problems, and missing diagnostics block release review. A successful rollback is still a release blocker because the attempted deployed state is no longer the current release candidate.

## Candidate evidence and diagnostics

Candidate source/evidence documents remain returned-by-value metadata. The report records their stable IDs and counts but does not register, persist, import, promote, validate, permit, or publish them.

Log references remain safe, fingerprinted references from the accepted execution-result envelope. The report records log IDs and blocker associations but does not open or inspect log content.

## Editor view

Open **Tools → Tainted Grail SDK → Tainted Grail Post-Deployment Verification and Release Blockers**.

The seventeenth TG SDK pane is read-only and shows:

- result, report, and exact current work-order identity;
- profile, game version, branch, and runtime target;
- candidate source/evidence counts and IDs;
- completed, failed, and incomplete work-order step counts;
- matched, mismatched, and unchecked target-verification counts;
- rollback-required, rollback-succeeded, rollback-incomplete, and backup-incomplete counts;
- failures and referenced diagnostics;
- explicit compatibility blockers; and
- explicit release blockers.

With the ordinary Developer Preview fixture, no deployment execution-result envelope is registered, so the pane shows zero reports.

## Determinism and non-mutation

Equivalent exact inputs produce the same report ID, status, counts, sorted IDs, blocker IDs, blocker ordering, and blocker associations. Report construction does not mutate the work order, execution-result envelope, evidence return, or any registry.

## Prohibited actions

The report contract and pane expose no verifier execution, executor invocation, adapter call, FoA launch, file inspection, copy, replace, delete, backup, restore, rollback execution, deployment, evidence promotion, release archive creation, signing, or release publication action.

The report fields keep these facts explicit:

- `HumanReviewRequired: true`;
- `VerifierExecuted: false`;
- `EvidencePromoted: false`;
- `ReleasePublished: false`;
- `LaunchPerformed: false`;
- `AdapterCalled: false`.
