# Semantic Repair Package

`semantic_repair` is a project-owned, engine-neutral package for deterministic synthetic repair work.

It provides:

- explicit transaction, cancellation and rollback phases with validated transitions;
- typed mapping and synthetic mount transactions;
- owner/generation resource leases and locked journal writes;
- hash-chained crash-recovery journals with deterministic recovery plans;
- lease-owned terminal-journal compaction with canonical checkpoint proofs;
- interrupted-compaction recovery, rotated checkpoint chains, archive manifests, retention plans and metadata-only tombstones;
- cumulative repeated-compaction quotas with immutable release and ledger-compaction records;
- manual lock-abandonment reviews, quorum decisions, revocations, supersessions and appeals with no automatic lock theft;
- recoverable multi-file publication intent journals with checkpoint integration, bounded before-image backups, dependency graphs and read-only partial-rollback plans;
- deterministic Cartesian and cancellation/recovery composition matrices;
- diagnostic path, redaction, byte-limit, generation and atomic-publication utilities;
- canonical API v2 JSON, bounded generation gaps, replay-window receipts, replay sequences and sequence-compaction proofs;
- negative workflow policy validation, reproducible receipts, Python 3.11–3.13 fixtures, deterministic migrations and chained migration histories.

The package is not a game runtime adapter. It has no game or loader assembly references, dynamic import, reflection, network, subprocess, installation discovery, deployment, evidence admission, or promotion capability.

`repair_primitives.py` remains as the Batch 006 compatibility facade. New project-owned code should import from `semantic_repair` directly.
