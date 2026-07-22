# Semantic Hook Batch 010 — Checkpoint Rotation and Replay Receipts

Batch 010 extends the project-owned offline repair package with deterministic crash-window proofs, bounded recovery material and cross-version receipt fixtures. It does not inspect or load game or mod assemblies, execute game code, admit runtime evidence, or promote hooks.

## Base

- Batch 009 merge commit: `7ebdae69f9f167af826658d59fbd1e336b2b911e`.
- Package authority: engine-neutral synthetic validation only.
- Runtime authority: none.
- Promotion authority: none.

## Interrupted compaction matrix

`semantic_repair.interrupted_compaction` adds a lease-owned four-stage protocol:

```text
prepared
→ journal-replaced
→ proof-published
→ chain-published
```

Before mutation it durably stores:

- the complete source journal backup;
- the prior proof bytes when present;
- the prior checkpoint-chain bytes when present;
- exact source, compacted, proof and chain hashes;
- the owner and lock generation performing compaction.

Synthetic interruption can be injected after every stage. Recovery is deterministic:

- `prepared` → discard the unused compaction and retain the source journal;
- `journal-replaced` → restore the source journal and prior proof/chain;
- `proof-published` → restore the source journal and prior proof/chain because the proof is not yet chain-anchored;
- `chain-published` → verify the exact compacted journal, proof and chain hashes, then finalize cleanup.

Unknown journal, proof, chain or backup bytes fail closed for manual review. Recovery requires the exact original active lease. It does not steal, replace or expire a lock.

The committed eight-case golden covers all four interruption stages with and without an existing proof.

## Checkpoint-chain rotation

`semantic_repair.checkpoint_chain` records canonical JSONL entries containing:

- monotonic entry index;
- entry kind;
- previous-entry SHA-256;
- proof or rotation payload;
- current entry SHA-256.

A proof entry binds the compaction proof SHA, source journal SHA, compacted journal SHA and compacted record hash.

When the configured entry limit is exceeded, rotation replaces the active chain with one `rotation-anchor` entry binding:

- the complete rotated-chain SHA-256;
- the rotated entry count;
- the rotated final entry SHA-256.

Future proof entries link from that anchor. Rotation removes no authority boundary and does not imply runtime continuity.

## Publication-intent checkpoint integration

`semantic_repair.intent_checkpoint` requires every publication intent in a journal to be terminal before checkpointing:

```text
intent-committed | intent-rolled-back
```

The integration receipt binds:

- source intent-journal SHA-256;
- checkpoint proof SHA-256;
- active checkpoint-chain SHA-256;
- sorted intent IDs;
- each terminal intent status;
- each intent SHA-256;
- each final intent record hash.

An incomplete intent, missing prepared record, intent hash mismatch or omitted proof state fails closed.

## Reviewer-quorum abandonment decisions

`semantic_repair.abandonment_quorum` builds a decision only when:

- at least two distinct reviewers are present;
- the required quorum is met;
- every review describes the exact same resource, owner, generation and lock hashes;
- review references are sorted deterministically.

The decision is:

```text
quorum-met-retain-lock
```

Its recommended action remains:

```text
manual-review-no-lock-theft
```

Verification re-reads the active lock and generation state. The decision never deletes, expires, replaces or steals the lock.

## Bounded publication backups

`semantic_repair.publication_limits` wraps the existing multi-file publisher with explicit limits:

- maximum bytes captured for one target before-image;
- maximum total bytes captured across all target before-images;
- maximum target count.

Defaults are:

```text
per target: 65,536 bytes
total:      262,144 bytes
targets:    32
```

Limits must be positive and the per-target limit cannot exceed the total limit. Oversized backups fail before the intent is published.

These are project-owned synthetic recovery bounds. They do not authorize capture of runtime diagnostics, game saves or private support bundles.

## API v2 replay-window receipts

`semantic_repair.replay_window` produces canonical, non-mutating receipts for:

- accepted forward snapshots;
- already-current snapshots;
- same-generation conflicts;
- stale replayed snapshots;
- snapshots beyond the configured generation window.

Each receipt binds:

- current and incoming generations;
- signed generation gap;
- configured maximum gap;
- current snapshot SHA-256;
- incoming snapshot SHA-256;
- deterministic disposition.

The committed fixtures cover accepted, stale and replay-window-exceeded cases. All identities remain synthetic and provide no Avalon Companions runtime evidence.

## Cross-version CI receipt fixtures

`semantic_repair.ci_matrix` adds canonical fixtures for Python:

```text
3.11
3.12
3.13
```

Every fixture must bind the exact same:

- workflow SHA-256;
- workflow policy receipt SHA-256;
- valid or invalid status.

The hosted matrix verifies the fixture matching its Python version and verifies the complete fixture set. The receipts do not upload artifacts or include environment paths, timestamps, runner IDs, secrets or machine identity.

## Validation

The focused Batch 010 suite passed:

```text
12 tests passed
0 failed
0 external Python dependencies
```

Coverage includes:

- eight interrupted-compaction compositions;
- checkpoint chain rotation and invalid limits;
- two-review abandonment quorum and duplicate rejection;
- per-target, total and count backup limits;
- replay-window receipt goldens;
- Python 3.11–3.13 cross-version policy fixtures;
- terminal intent-journal checkpoint integration.

Hosted workflow results are recorded separately from local synthetic validation.

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
- runtime replay authority: 0.

Passing Batch 010 proves only deterministic behavior of project-owned offline models.

## Scope

Allowed changes:

- offline Python package code;
- synthetic fixtures, goldens and tests;
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

Semantic Hook Batch 011 should add checkpoint-chain archive manifests, compaction backup quotas across repeated operations, intent dependency graphs, quorum revocation and supersession records, replay-window sequence proofs, and workflow-receipt migration fixtures. It must retain the same no-runtime and no-promotion boundary.
