# FoA Adapter Deployment Execution-Result and Verification Contract

## Status

Implemented as Correction Slice 15, the fifth bounded Phase 8 contract after build manifests, package
previews, staging/deployment previews, and deployment confirmation/work-order review.

This slice validates metadata supplied by a **separately reviewed executor**. It does not implement, invoke,
locate, install, dispatch, or trust an executor.

## Purpose

Slice 14 creates one exact `review_ready` deployment work order with every execution and filesystem-mutation
permission false. Slice 15 answers the next observation-only question:

> Given metadata supplied after a separately reviewed executor attempted that exact work order, do the
> attempted step identities, backup and restore outcomes, deployed fingerprints, rollback results, failures,
> and safe log references form a complete contract-valid result envelope that can be returned as candidate
> evidence?

Acceptance means only that the metadata conforms to the contract. It does not mean the deployment succeeded,
was safe, was independently reproduced, or should be promoted into validation or permission.

## Exact reviewed deployment work order

Every result envelope binds to one exact `AdapterDeploymentWorkOrder` through:

- work-order ID;
- exact canonical work-order JSON;
- lowercase `sha256:<64 hex>` work-order fingerprint;
- preview ID and preview fingerprint;
- pack ID;
- target-inventory ID.

The referenced work order must be `review_ready`, contain canonical JSON, contain no blockers, retain every
execution/copy/delete/backup/restore/deployment/launch flag as false, and retain `ExecutionAllowed: false` on
every work-order step.

A changed, incomplete, executable, or non-review-ready work order produces `work_order_not_ready` or
`work_order_binding_mismatch` before step, backup, verification, or rollback checks.

## Separately reviewed executor

`AdapterDeploymentExecutorReview` records:

- stable review ID;
- stable namespaced executor ID;
- strict semantic executor version;
- lowercase SHA-256 executor fingerprint;
- `unknown`, `accepted`, or `rejected` review decision;
- named reviewer;
- one or more evidence IDs;
- exact UTC review time;
- notes.

Only an accepted, evidence-backed, named review can reach an accepted envelope. The review time cannot be
later than the declared result capture time.

The review describes supplied executor metadata only. This slice has no executor registry, binary loader,
process launcher, signature verifier, identity provider, trust store, or command dispatch.

## Typed execution outcomes

Attempted steps, backups, and rollback results use:

- `not_attempted`;
- `succeeded`;
- `failed`;
- `skipped`.

Outcome shape is fail-closed:

- `not_attempted` is not attempted and has no failure IDs;
- `succeeded` is attempted and has no failure IDs;
- `failed` is attempted and has one or more failure IDs;
- `skipped` is not attempted and has one or more failure IDs explaining the skip.

A failed execution may still produce an **accepted contract-valid evidence envelope**. Contract acceptance and
operational success remain separate axes.

## Exact attempted step identities

The envelope contains exactly one `AdapterDeploymentExecutionStepResult` for every canonical deployment work-
order step.

Each result preserves:

- step ID;
- one-based sequence;
- typed work-order step kind;
- target path;
- backup path;
- previous fingerprint;
- desired fingerprint;
- typed outcome;
- attempted state;
- observed fingerprint;
- target-presence state;
- failure IDs;
- log-reference IDs.

Unknown, missing, duplicate, reordered-by-identity, or changed steps fail closed as `step_binding_mismatch`.

For successful mutation steps:

- `add` and `replace` report the target present with the exact desired fingerprint;
- `remove` reports the target absent with no observed fingerprint.

The service never opens or hashes a target file to prove those claims.

## Backup outcomes

Every canonical `backup` work-order step requires exactly one `AdapterDeploymentBackupResult`, including when
no backup was attempted.

The result preserves:

- exact backup step ID;
- exact target path;
- exact backup path;
- exact source fingerprint;
- reported backup fingerprint;
- typed outcome;
- attempted state;
- failure and log references.

A successful backup must report a backup fingerprint equal to the exact pre-change source fingerprint.
Missing, duplicate, changed, or mismatched backup results produce `backup_binding_mismatch`.

No backup is created, checked, restored, deleted, or retained by this contract.

## Deployed fingerprints and target verification

Every `add`, `replace`, and `remove` step requires exactly one `AdapterDeploymentTargetVerification`.

Typed verification states are:

- `not_checked`;
- `matched`;
- `mismatched`.

Each verification preserves:

- exact source step ID;
- exact target path;
- expected presence and fingerprint;
- observed presence and fingerprint;
- UTC check time when checked;
- log references.

`matched` requires exact expected state. `mismatched` requires an observed state that actually differs.
`not_checked` cannot claim an observation or check time.

Missing, duplicate, changed, or internally contradictory verification metadata produces
`verification_binding_mismatch`.

The contract validates reported values only. It does not read, stat, compare, or hash deployment targets.

## Typed rollback and restore results

Every `add`, `replace`, and `remove` step requires exactly one rollback result, including `not_attempted`.

Typed rollback actions are:

- `remove_added`;
- `restore_replaced`;
- `restore_removed`.

Every result preserves:

- stable rollback-result ID;
- exact source work-order step ID;
- typed inverse action;
- target path;
- backup path where required;
- expected deployed fingerprint;
- exact restore fingerprint;
- typed outcome;
- final fingerprint and target-presence state;
- failure and log references.

A successful rollback must report the exact inverse final state:

- `remove_added` leaves the target absent;
- `restore_replaced` leaves the target present with the exact pre-replacement fingerprint;
- `restore_removed` leaves the target present with the exact removed fingerprint.

Missing, duplicate, changed, or contradictory rollback metadata produces `rollback_binding_mismatch`.

No rollback or restore operation is implemented or authorised.

## Failures and safe log references

Typed failure kinds are:

- `contract`;
- `executor`;
- `preflight`;
- `backup`;
- `copy`;
- `replace`;
- `delete`;
- `verification`;
- `rollback`;
- `restore`;
- `unknown`.

Failures preserve stable identity, kind, code, message, exact step or rollback binding, referenced logs, and
retryability.

Typed log kinds are:

- `executor`;
- `filesystem`;
- `backup`;
- `verification`;
- `rollback`;
- `diagnostic`.

Every log reference must be a safe relative locator with a lowercase SHA-256 fingerprint. Logs bind back to
known work-order step IDs and/or rollback-result IDs.

The service does not open, parse, upload, persist, redact, or inspect log content. Invalid failure or log
bindings produce `failure_log_binding_mismatch`.

## Envelope status precedence

Every validation return receives one deterministic status:

1. `work_order_not_ready`;
2. `executor_unreviewed`;
3. `work_order_binding_mismatch`;
4. `envelope_invalid`;
5. `step_binding_mismatch`;
6. `backup_binding_mismatch`;
7. `verification_binding_mismatch`;
8. `rollback_binding_mismatch`;
9. `failure_log_binding_mismatch`;
10. `accepted`.

Earlier identity and trust failures cannot be hidden by a later outcome or log detail.

`accepted` means structurally valid metadata only. It is not an execution approval, success determination,
validation event, permission grant, deployment authorisation, or release approval.

## Candidate evidence return

An accepted envelope returns new candidate documents by value:

- one primary deployment execution-result source;
- exact work-order binding evidence;
- executor-review evidence;
- one evidence record per attempted work-order step;
- one evidence record per backup outcome;
- one evidence record per target verification;
- one evidence record per rollback/restore result;
- typed failure evidence;
- one separate source and evidence document per referenced log fingerprint.

Candidate evidence preserves the supplied profile, game version, branch, runtime target, executor
identity/version, result capture time, and source fingerprints.

Nothing is automatically:

- registered in the source/evidence registry;
- written to the workspace;
- imported;
- promoted into the catalog;
- marked valid;
- granted permission;
- attached to a release;
- dispatched to another executor.

The candidate confidence remains `unrated`.

## Transient registry

`AdapterDeploymentExecutionResultRegistry` stores explicit envelopes in transient Core memory.

It has no file picker, loader, durable schema, persistence service, transport, network client, process
integration, or automatic registration path. Duplicate result IDs fail closed. Editor shutdown clears the
registry.

## Read-only Editor pane

**Tainted Grail Deployment Execution Result Evidence** displays:

- result and work-order identities;
- contract status;
- reviewed executor identity, version, fingerprint, and reviewer;
- every attempted step and typed outcome;
- observed fingerprints and target presence;
- backup outcomes;
- target verification;
- rollback and restore outcomes;
- failures and safe log references;
- candidate source/evidence counts;
- actionable contract issues.

The pane is non-editable. The ordinary Developer Preview state contains **zero registered deployment
execution-result envelopes**.

## No executor, deployment command, launch path, adapter call, or automatic evidence promotion

Slice 15 adds no:

- executor implementation or executor invocation;
- command dispatch;
- filesystem scan or hashing;
- file copy, move, replacement, or deletion;
- backup or restore;
- package assembly or archive creation;
- deployment-directory mutation;
- rollback execution;
- FoA launch or process access;
- BepInEx/Harmony loading;
- adapter call;
- telemetry or save mutation;
- automatic source/evidence registration;
- catalog promotion, validation, permission, or release publication.

## Tests and enforcement

Production-linked tests cover:

- strict typed vocabularies, fingerprints, timestamps, and safe references;
- duplicate result registration;
- accepted envelopes returning candidate evidence only;
- work-order status precedence;
- accepted executor review and exact work-order binding;
- exact attempted step identity and outcome shape;
- exact backup fingerprints;
- matched and mismatched deployed-target verification;
- rollback and restore final-state binding;
- same-step failure and log references;
- failed execution remaining distinct from contract validity;
- deterministic evidence ordering;
- complete input non-mutation.

The focused validator rejects executor/process/file mutation, automatic evidence promotion, missing rollback
gates, mutable Editor controls, incomplete tests, missing CI/public contracts, and stale fifteen-pane Windows
acceptance.

## Next ordered slice

Correction Slice 16 is a deterministic post-deployment verification and release-blocker report. It should
aggregate one accepted deployment execution-result envelope, candidate evidence, target-verification states,
rollback completeness, and referenced diagnostics into explicit compatibility and release blockers without
executing a verifier, launching FoA, promoting evidence, or publishing a release.

## Rollback

Revert the implementing pull request. Execution-result envelopes and returned candidate documents are
transient metadata, so no durable workspace, pack, catalog, source/evidence, adapter, work-order, runtime-
result, build-manifest, package, deployment, backup, rollback, confirmation, or execution-result schema
requires migration.
