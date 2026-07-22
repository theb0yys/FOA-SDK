# Semantic Hook Batch 012 — Retention, Rollback and History Proofs

Batch 012 extends the project-owned offline repair package with deterministic retention metadata, quota lifecycle proofs, dependency-aware rollback planning, immutable appeals, compacted replay summaries and chained workflow-receipt migrations. It does not inspect or load game or mod assemblies, execute game code, admit runtime evidence, or promote hooks.

## Base

- Batch 011 merge commit: `34b40f6387cbb07fbf8c87855f36012277e5fec7`.
- Package authority: engine-neutral synthetic validation only.
- Runtime authority: none.
- Promotion authority: none.

## Archive retention and tombstone manifests

`semantic_repair.archive_retention` adds a deterministic retention policy over the Batch 011 archive-manifest chain.

A retention plan validates contiguous archive indices and previous-manifest SHA-256 links, retains only the newest configured count, marks older manifests for metadata tombstones, binds the final source-manifest SHA-256, and deletes no archive or checkpoint chain.

The default retained count is four and must be positive.

A tombstone binds its contiguous index, archive identity and hash, source manifest hash, previous tombstone hash, reason, and effect `archive-metadata-tombstoned-no-delete-authority`. Tombstones cannot delete, truncate, move, overwrite or restore archive bytes.

## Quota release and ledger-compaction records

`semantic_repair.quota_lifecycle` adds immutable release records above the repeated-compaction quota ledger. Each record binds the source ledger, original reservation, released bytes, previous release, reason, and effect `quota-reservation-released-metadata-only`.

Ledger compaction validates every release against the exact source reservation, rejects duplicate releases, preserves unreleased hashes, reindexes remaining reservations, and recalculates cumulative byte counts. Its record declares `ledger-compacted-provenance-retained`. No backup file is deleted and no compaction operation is executed.

## Dependency-graph partial-rollback plans

`semantic_repair.dependency_rollback` converts a validated intent dependency graph into a read-only partial-rollback plan. It computes the transitive dependent closure, reverse-topological rollback order, source graph hash, per-intent status and one of:

```text
rollback-committed
cancel-and-rollback-prepared
none-already-rolled-back
```

The plan declares `execution_authority: none` and does not publish, restore, mutate or cancel anything.

## Quorum appeal records

`semantic_repair.abandonment_appeal` adds immutable appeals for a quorum decision, revocation or supersession. Appeals bind the exact target, resource, generation, at least two distinct reviewers, reason, requested outcome `manual-reconsideration-no-lock-theft`, and effect `appeal-recorded-lock-retained`.

Appeals cannot expire, remove, replace or steal a lock.

## Replay-sequence compaction proofs

`semantic_repair.replay_sequence_compaction` verifies sequence indices, entry links, generation continuity and entry hashes before producing a canonical summary. The proof binds source sequence hash/count, initial/final generation, first/final entry hashes, status counts, prior compaction proof and summary hash.

A later proof can chain to an earlier compaction proof. No runtime replay or snapshot-application authority is granted.

## Chained workflow-receipt migration histories

`semantic_repair.ci_migration_history` extends the version-1-to-version-2 migration with a deterministic version-2-to-version-3 receipt and a two-entry hash chain:

```text
policy v1 → policy v2 → policy v3
```

The history preserves workflow SHA-256, valid/invalid status and the ordered-violation-set hash. The valid fixture binds the exact Batch 012 workflow; the invalid fixture preserves both violations produced by changing `contents: read` to `contents: write`.

The workflow remains read-only, secret-free, dependency-free, network-free and artifact-free.

## Validation

```text
14 tests passed
0 failed
0 external Python dependencies
```

Coverage includes retention/tombstone chains, quota release and compaction, dependency rollback ordering, appeals, replay compaction chains, and valid/invalid v1→v2→v3 migration histories.

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
- archive deletion authority: 0;
- rollback execution authority: 0.

Passing Batch 012 proves only deterministic behavior of project-owned offline models.

## Scope

Allowed changes are limited to offline Python package code, synthetic fixtures/tests, the read-only workflow, and catalogue documentation. No Batch 004 manifest or decision, hook record, runtime profile, upstream source, build graph, installer, deployment, signing, publication or evidence-status change is included.

## Next unit

Semantic Hook Batch 013 should add tombstone supersession and restoration-review records, quota reconciliation journals, rollback-plan acknowledgement receipts, appeal withdrawal and resolution histories, replay-compaction retention policies, and multi-version workflow migration compatibility matrices. It must retain the same no-runtime and no-promotion boundary.
