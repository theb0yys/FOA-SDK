# FoA Adapter Deployment Confirmation and Work-Order Contract

## Status

Implemented as Correction Slice 14, the fourth Phase 8 contract slice. It binds one exact ready staging/deployment preview to explicit human confirmation, a typed confirmation scope, expiry, a reviewed UTC maintenance window, typed preflight evidence, deterministic work-order steps, and an operator-facing checklist.

`review_ready` is an operator-review state only. Every execution and mutation flag remains false.

## Purpose

Slice 13 describes additions, replacements, removals, unchanged paths, conflicts, backups, and inverse rollback steps without touching the filesystem. Slice 14 answers the next bounded question:

> Has one named reviewer explicitly confirmed the exact ready staging/deployment preview, within an exact scope and expiry, during a reviewed maintenance window, with every required preflight evidence record present, so a deterministic non-executable operator work order can be reviewed?

The answer does not authorize execution. It adds no executor, deployment command, filesystem access, launch path, or adapter call.

## Exact preview binding

Every request binds to one exact ready staging/deployment preview through:

- preview ID;
- lowercase `sha256:<64 hex>` preview fingerprint;
- pack ID;
- target inventory ID;
- canonical preview JSON;
- additions, replacements, removals, backups, and rollback steps;
- `StagingMutationAllowed: false`;
- `DeploymentMutationAllowed: false`;
- `RollbackExecutionAllowed: false`;
- `LaunchAllowed: false`.

A non-ready preview, changed fingerprint, conflict, blocker, missing canonical JSON, or enabled mutation flag produces `preview_not_ready` before confirmation or preflight checks.

## Explicit confirmation

`AdapterDeploymentConfirmation` records:

- stable confirmation ID;
- exact preview ID and fingerprint;
- `unknown`, `confirmed`, or `rejected` decision;
- typed confirmation scope;
- named reviewer;
- evidence IDs;
- exact UTC issue time;
- exact UTC expiry;
- notes.

The supported confirmation scope values are:

- `additions_only` — valid only when the preview contains no replacements or removals;
- `additions_and_replacements` — valid only when the preview contains no removals;
- `full_preview` — covers all additions, replacements, and removals in the exact preview.

A narrower scope cannot silently omit a reviewed mutation. Scope drift produces `scope_mismatch`.

Confirmation timestamps use exact `YYYY-MM-DDTHH:MM:SSZ` UTC values. The deterministic evaluation time must be no earlier than issue time and no later than expiry. Expired or malformed confirmation time produces `confirmation_expired`.

## Maintenance window

`AdapterDeploymentMaintenanceWindow` records:

- stable window ID;
- exact preview ID and fingerprint;
- UTC start and end;
- named operator group;
- evidence IDs.

The interval must be increasing, evidence-backed, and bound to the exact preview. The deterministic evaluation time must fall inside the window. Invalid windows produce `maintenance_window_invalid`; a valid window evaluated outside its interval produces `outside_maintenance_window`.

The service does not read the host clock. The evaluation timestamp is explicit input so equivalent reviewed inputs produce equivalent results.

## Typed preflight evidence

Every preflight record contains:

- stable preflight ID;
- typed preflight kind;
- `unknown`, `passed`, or `failed` status;
- exact preview ID and fingerprint;
- UTC check time;
- named checker;
- evidence IDs;
- notes.

Required kinds are:

- `package_integrity`;
- `target_inventory`;
- `rollback_readiness`;
- `operator_readiness`;
- `backup_readiness` when the preview contains backup requirements.

Every required kind must appear exactly once. It must be `passed`, exact-preview-bound, evidence-backed, and checked between confirmation issue and work-order evaluation. Missing kinds produce `preflight_missing`; duplicates, failures, malformed identity, time drift, or binding drift produce `preflight_failed`.

Preflight evidence remains supplied metadata. Slice 14 does not scan files, calculate hashes, allocate backup storage, test rollback, or verify an operator session.

## Deterministic work-order steps

The contract derives typed steps for:

- preflight verification;
- maintenance-window confirmation;
- every required backup description;
- every addition;
- every replacement;
- every removal;
- post-change fingerprint verification;
- preservation of the typed rollback description.

Every step records deterministic sequence and identity, target and backup paths where applicable, previous and desired fingerprints, evidence IDs, and description.

Every step retains `ExecutionAllowed: false`. A step is a canonical instruction description, not an invokable action.

The contract verifies exact coverage:

- one backup step per preview backup requirement;
- one add step per addition;
- one replace step per replacement;
- one remove step per removal;
- one inverse rollback row for every hypothetical addition, replacement, or removal.

Missing or extra coverage produces `work_order_incomplete`.

## Operator-facing checklist

The read-only checklist includes:

- exact preview and fingerprint binding;
- confirmation scope;
- maintenance window;
- typed preflight evidence;
- backup destination review when backups exist;
- review of every addition, replacement, removal, unchanged path, and fingerprint;
- stop-on-drift instruction;
- rollback-description retention.

Checklist states are:

- `contract_satisfied`;
- `operator_action_required`;
- `blocked`.

Operator-action items remain pending. `AcknowledgementRecorded` is always false because Slice 14 adds no acknowledgement UI, signature, execution control, or mutation path.

## Status precedence

Every work order receives exactly one deterministic status:

1. `preview_not_ready`;
2. `confirmation_missing`;
3. `confirmation_rejected`;
4. `confirmation_binding_mismatch`;
5. `scope_mismatch`;
6. `confirmation_expired`;
7. `maintenance_window_invalid`;
8. `outside_maintenance_window`;
9. `preflight_missing`;
10. `preflight_failed`;
11. `work_order_incomplete`;
12. `review_ready`.

Earlier identity, decision, time, and evidence failures cannot be hidden by later work-order details.

## Canonical representation

Equivalent reviewed inputs produce byte-identical canonical JSON with:

- fixed property order;
- exact preview, confirmation, expiry, maintenance-window, reviewer, and operator-group binding;
- deterministic step, checklist, and blocker ordering;
- JSON escaping;
- locale-independent integer formatting;
- `ExecutionAllowed: false` on every step and the work order;
- `CopyAllowed: false`;
- `DeleteAllowed: false`;
- `BackupAllowed: false`;
- `RestoreAllowed: false`;
- `DeploymentAllowed: false`;
- `LaunchAllowed: false`.

## Transient registry and Editor view

`AdapterDeploymentWorkOrderRegistry` stores complete requests in transient Core memory. It has no durable schema, loader, file picker, persistence service, or registration UI and is cleared on Editor shutdown.

**Tainted Grail Deployment Confirmation and Work Orders** displays:

- work-order, preview, pack, and target-inventory identities;
- named reviewer, confirmation scope, and expiry;
- maintenance window, evaluation time, and operator group;
- deterministic status;
- typed work-order steps;
- operator-facing checklist;
- blockers;
- all prohibited permission flags;
- canonical JSON.

The ordinary Developer Preview state contains zero registered deployment confirmation/work-order inputs.

## No copy, delete, backup, restore, deployment, launch, or adapter call

Slice 14 adds no filesystem scan, file hashing, copy, move, replacement, deletion, backup, restoration, archive creation, package assembly, deployment mutation, rollback execution, process launch, FoA access, BepInEx/Harmony loading, telemetry, save mutation, or adapter invocation.

A `review_ready` work order does not prove that files exist, preflight checks were independently reproduced, backup capacity exists, rollback was tested, an operator acknowledged the checklist, or deployment is safe to execute.

## Tests and enforcement

Production-linked tests cover strict vocabularies, duplicate request rejection, complete review-ready output, status precedence, confirmation decision and scope, expiry and maintenance-window handling, missing versus failed preflight, exact step/backup/rollback coverage, pending operator checklist items, canonical determinism, and complete input non-mutation.

The focused validator rejects filesystem/process/runtime operations, missing expiry or scope gates, mutable Editor controls, incomplete tests, missing CI/public contracts, and stale fourteen-pane acceptance.

## Next ordered slice

Correction Slice 15 is a typed deployment execution-result and verification envelope for a separately supplied executor: bind one exact reviewed work order to attempted step identities, backup/restore results, deployed fingerprints, rollback outcomes, logs, and evidence candidates without adding an executor, deployment command, launch path, or automatic evidence promotion.

## Rollback

Revert the implementing pull request. Confirmations, maintenance windows, preflight records, work orders, and checklists are transient metadata, so no durable workspace, pack, catalog, source/evidence, adapter, work-order-plan, runtime-result, build-manifest, staging, package, deployment, backup, rollback, or confirmation schema requires migration.
