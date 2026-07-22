# Semantic Hook Batch 008 — Concurrency and Crash-Window Hardening

Batch 008 hardens the project-owned offline package for synthetic concurrent writers, stale state and cancellation. It does not inspect or load game or mod assemblies, execute game code, admit runtime evidence, or promote hooks.

## Base

- Batch 007 merge commit: `109c974144f5cd730515190e53e70eb69ed25f34`.
- Package authority: engine-neutral synthetic validation only.
- Runtime authority: none.
- Promotion authority: none.

## Owner and generation leases

`semantic_repair.ownership` adds an atomic lock-file contract:

- one active owner per resource;
- monotonically increasing acquisition generations;
- exact resource, owner and generation identity;
- stale lease rejection;
- cross-resource lease rejection;
- owner-only release;
- no timeout or automatic lock theft.

The lock carries no process, machine, user, installation or game identity. A stranded lock therefore requires an explicit project-owned recovery decision rather than an unsafe time-based steal.

## Locked journals

`CrashRecoveryJournal` now supports owned append and recovery operations. When a lock is active, legacy unlocked append or recovery is rejected. Transaction state machines can receive the exact journal lease and write every phase through that owner/generation.

A journal transition is appended durably before the in-memory phase advances. Failed or stale journal writes therefore leave the in-memory state unchanged.

Ownership fields are reserved and inserted by the journal:

```text
journal_owner_id
journal_lock_generation
```

## Deterministic recovery planning

`semantic_repair.recovery` converts the validated journal into canonical JSON recovery plans.

Planning is read-only and deterministic:

- a torn tail produces the first `truncate-torn-tail` step;
- incomplete known phases produce `rollback`;
- committed, rolled-back and cancelled transactions produce `none`;
- unknown phases or owner/generation changes within one transaction produce `manual-review`;
- transaction steps are sorted by transaction ID;
- the plan records the complete journal SHA-256;
- every plan declares `runtime_authority: none` and `promotion: none`.

The planner does not execute rollback, inspect game state or infer that a synthetic plan applies to a runtime object.

## Two-writer diagnostic publication

Generation-checked publication adds:

- an exclusive session writer lease;
- durable publication generation and byte accounting;
- explicit expected-generation comparison;
- row, summary and publication-state hashes;
- reserved owner and generation summary fields;
- restoration of row, summary and state when any publication step raises.

The synthetic two-writer fixture proves this sequence:

1. writer A acquires lock generation 1;
2. writer B cannot acquire while A owns the resource;
3. A publishes publication generation 1;
4. B later acquires lock generation 2;
5. B's stale expected publication generation 0 is rejected;
6. B refreshes and publishes generation 2.

This models project concurrency only. It does not authorize diagnostic capture or support-file sharing from the prohibited writer reviewed in Batch 004.

## Cancellation semantics

Transactions add explicit phases:

```text
cancel-requested → cancelling → cancelled
```

A cancellation token is checked before mutation and at declared commit checkpoints. Observed cancellation:

- is not reported as transaction failure;
- restores the complete captured pre-state;
- records cancellation phases in the synthetic journal;
- is idempotent after reaching `cancelled`;
- cannot cancel a committed, rolled-back or already-cancelled transaction.

The synthetic mount transaction exposes checkpoints after create, field, movement and ownership stages.

## API v2 stale-generation documents

API v2 registry snapshots now serialize:

- registry generation;
- owner and command identity;
- registration token value;
- registration generation.

Snapshots are canonical and sorted by owner and command. A consumer rejects:

- a snapshot older than its current generation;
- conflicting content at the same generation;
- duplicate owner/command entries;
- registration generations newer than the registry generation;
- unsupported schemas, kinds or API versions.

The committed fixtures use synthetic identities only. They provide no source, compiled-assembly, load, behavior or combined-mod evidence for Avalon Companions.

## CI negative policy

`semantic_repair.ci_policy` validates the committed workflow using the Python standard library. Tests prove rejection of:

- any permission set other than exactly `contents: read`;
- persisted checkout credentials;
- secret references;
- artifact or pages upload actions;
- attestation, release or container-push actions;
- write permissions;
- network/package-publication commands.

The workflow runs this policy test before the complete offline suite and uploads no artifact.

## Validation

The reconstructed Batch 007 plus Batch 008 package surface passed:

```text
40 tests passed
0 failed
0 external Python dependencies
```

Coverage includes legacy compatibility, authority-boundary scans, lock contention, stale leases, owned state transitions, torn-tail planning, ownership conflicts, two-writer publication, three-file restoration, cancellation checkpoints, stale registry snapshots and negative workflow mutations.

Hosted workflow results are recorded separately from local synthetic validation.

## Governance result

- hook promotions: 0;
- evidence promotions: 0;
- runtime adapters: 0;
- game or loader assembly inspection: 0;
- Batch 004 prohibitions changed: 0;
- runtime diagnostics authority: 0;
- runtime mount-conversion authority: 0;
- Avalon Companions runtime authority: 0.

Passing Batch 008 proves only deterministic behavior of project-owned offline models.

## Scope

Allowed changes:

- offline Python package code;
- synthetic fixtures and tests;
- the read-only offline workflow;
- catalogue documentation.

Not changed:

- Batch 004 fixture manifests or decision register;
- hook records or verified runtime profiles;
- upstream mod source;
- game, loader or framework binaries;
- build graphs, installers, deployment, signing or publication;
- evidence status or promotion decisions.

## Next unit

Semantic Hook Batch 009 should add journal compaction and checkpoint proofs, lock abandonment review records, multi-file publication intent journals, cancellation/recovery composition matrices, API v2 generation-gap limits, and reproducible CI policy receipts. It must retain the same no-runtime and no-promotion boundary.
