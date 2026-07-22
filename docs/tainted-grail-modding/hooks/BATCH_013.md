# Semantic Hook Batch 013 — Lifecycle Reconciliation and Compatibility Proofs

Batch 013 extends the project-owned offline repair package with deterministic metadata lifecycle, reconciliation, acknowledgement, appeal-history, retention and compatibility proofs. It does not inspect or load game or mod assemblies, execute game code, mutate runtime state, admit runtime evidence, or promote hooks.

## Base

- Batch 012 merge commit: `e6881746c13607d314708e8a223ba4a477237463`.
- Package authority: engine-neutral synthetic validation only.
- Runtime authority: none.
- Promotion authority: none.

## Tombstone supersession and restoration review

`semantic_repair.tombstone_lifecycle` adds immutable records above Batch 012 archive tombstones.

A supersession record requires:

- one previous and one replacement tombstone for the exact same archive identity;
- replacement chaining directly from the previous tombstone;
- a strictly later tombstone index;
- at least two distinct reviewers;
- a non-empty reason.

Its effect is:

```text
tombstone-superseded-no-archive-mutation
```

A restoration-review record may target either a tombstone or supersession. It binds the target hash, archive identity, archive and manifest hashes, distinct reviewers, reason, and requested outcome:

```text
manual-restoration-review-no-file-mutation
```

Its effect is `restoration-review-recorded-no-archive-mutation`. Neither record deletes, moves, truncates, restores, rewrites or republishes archive bytes.

## Quota reconciliation journals

`semantic_repair.quota_reconciliation` adds a canonical hash-chained journal over a verified Batch 012 quota release and ledger-compaction operation.

The journal contains:

- the exact source quota-ledger SHA-256;
- source reserved bytes;
- every release record in release-index order;
- signed byte deltas and running balances;
- the exact compacted-ledger SHA-256;
- the exact compaction-record SHA-256;
- released and remaining byte totals.

The source total must equal released plus remaining bytes. Every entry index, previous hash, object hash, delta and running balance is verified. The journal declares `mutation_authority: none`; it does not reserve, release, compact or delete backup data.

## Rollback-plan acknowledgement receipts

`semantic_repair.rollback_acknowledgement` adds a canonical acknowledgement chain for Batch 012 partial-rollback plans.

Each receipt binds:

- contiguous acknowledgement index and previous receipt hash;
- acknowledgement ID;
- exact rollback-plan SHA-256 and source-graph SHA-256;
- reviewer identity;
- action count;
- reason;
- one of:

```text
acknowledged-for-manual-execution-review
declined-for-manual-execution-review
```

The resulting effect is either `rollback-plan-acknowledged-no-execution` or `rollback-plan-declined-no-execution`. Every receipt declares `execution_authority: none`; it cannot invoke a publisher, transaction, cancellation token, journal mutation or file restoration.

## Appeal withdrawal and resolution histories

`semantic_repair.appeal_history` adds immutable withdrawal and resolution records above Batch 012 abandonment appeals.

Withdrawal records bind the exact appeal, resource, generation, distinct reviewers and reason. Their effect is:

```text
appeal-withdrawn-lock-retained
```

Resolution records support:

```text
upheld-manual-review
denied-manual-review
superseded-manual-review
closed-withdrawn
```

A withdrawn appeal must resolve as `closed-withdrawn`, and that outcome requires the exact withdrawal record. Resolution effects remain `appeal-resolved-lock-retained`.

Withdrawal and resolution records may be assembled into a hash-chained lifecycle history. The history permits withdrawal only, resolution only, or withdrawal followed by resolution. It declares `lock_mutation_authority: none` and cannot expire, remove, replace or steal a lock.

## Replay-compaction retention policies

`semantic_repair.replay_compaction_retention` applies a deterministic retention policy to a complete chain of Batch 012 replay-sequence compaction proofs.

The plan verifies:

- the first proof begins at the zero hash;
- every later proof links to the immediately previous proof;
- proof order is preserved;
- the newest configured count is retained;
- older proof metadata is classified `tombstone-metadata`.

The default retained count is four and must be positive. The plan declares `deletion_authority: none`; it does not delete, truncate, restore, apply or replay any proof or snapshot.

## Workflow migration compatibility matrices

`semantic_repair.ci_migration_compatibility` validates the complete policy-receipt chain:

```text
policy v1 → policy v2 → policy v3
```

It then emits a canonical 3×3 compatibility matrix.

For versions 1, 2 and 3:

- identity transitions are `identity-compatible`;
- forward contiguous transitions are `forward-compatible`;
- downgrades are `downgrade-unsupported`.

Every compatible path lists each contiguous policy version. Every downgrade has an empty path. The matrix preserves workflow SHA-256, valid/invalid status and ordered-violation-set SHA-256.

Committed valid and invalid matrices bind the exact Batch 013 workflow. The invalid matrix preserves both violations generated by replacing `contents: read` with `contents: write`.

The matrix declares `migration_execution_authority: none`; it describes compatibility but performs no migration.

## Workflow boundary

The workflow remains:

```text
permissions:
  contents: read
```

It retains:

- checkout credentials disabled;
- no secrets;
- no dependency installation;
- no network or publication commands;
- no artifact upload;
- Python 3.11, 3.12 and 3.13;
- all Batch 009–012 receipt checks;
- the new migration-compatibility verification step.

## Validation

Focused Batch 013 validation passed:

```text
13 tests passed
0 failed
0 external Python dependencies
```

Coverage includes:

- same-archive tombstone supersession and cross-archive rejection;
- restoration-review round trips and reviewer quorum validation;
- quota reconciliation balance and tamper rejection;
- rollback acknowledgement chaining and plan-switch rejection;
- appeal withdrawal, closed-withdrawn resolution and history verification;
- replay-compaction retention and broken-chain rejection;
- valid and invalid migration compatibility matrices plus incomplete-matrix rejection.

Hosted workflow results are recorded separately from focused synthetic validation.

## Governance result

- hook promotions: 0;
- evidence promotions: 0;
- runtime adapters: 0;
- game or loader assembly inspection: 0;
- Batch 004 prohibitions changed: 0;
- runtime diagnostic authority: 0;
- runtime mount-conversion authority: 0;
- Avalon Companions runtime authority: 0;
- automatic lock-theft authority: 0;
- artifact-publication authority: 0;
- runtime replay authority: 0;
- runtime dependency-execution authority: 0;
- archive deletion or restoration authority: 0;
- quota mutation authority: 0;
- rollback execution authority: 0;
- migration execution authority: 0.

Passing Batch 013 proves only deterministic behavior of project-owned offline models.

## Scope

Allowed changes are limited to offline Python package code, synthetic fixtures and tests, the read-only offline workflow, and catalogue documentation.

Not changed:

- Batch 004 fixture manifests or decision register;
- hook records or verified runtime profiles;
- upstream mod source;
- game, loader or framework binaries;
- build graphs, installers, deployment, signing or publication;
- evidence status or promotion decisions.

## Next unit

Semantic Hook Batch 014 should add restoration-review resolution and supersession histories, quota-reconciliation checkpoint summaries, rollback-acknowledgement quorum records, appeal-history compaction proofs, replay-retention tombstone records, and workflow-migration matrix evolution receipts. It must retain the same no-runtime and no-promotion boundary.
