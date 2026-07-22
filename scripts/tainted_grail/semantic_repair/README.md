# Semantic Repair Package

`semantic_repair` is a project-owned, engine-neutral package for deterministic synthetic repair work.

It provides:

- explicit transaction, cancellation and rollback phases with validated transitions;
- typed mapping and synthetic mount transactions;
- owner/generation resource leases and locked journal writes;
- hash-chained crash-recovery journals with deterministic recovery plans;
- lease-owned terminal-journal compaction with canonical checkpoint proofs;
- interrupted-compaction rollback/finalization matrices, rotated checkpoint chains and archive manifests;
- cumulative backup quotas across repeated compaction operations;
- manual lock-abandonment reviews, quorum decisions, revocations and supersessions with no automatic lock theft;
- recoverable multi-file publication intent journals with checkpoint integration, bounded before-image backups and dependency graphs;
- deterministic Cartesian and cancellation/recovery composition matrices;
- diagnostic path, redaction, byte-limit, generation and atomic-publication utilities;
- canonical API v2 JSON, bounded generation gaps, replay-window receipts and replay sequence proofs;
- negative workflow policy validation, reproducible receipts, Python 3.11–3.13 cross-version fixtures and v1-to-v2 migration fixtures.

The package is not a game runtime adapter. It has no game or loader assembly references, dynamic import, reflection, network, subprocess, installation discovery, deployment, evidence admission, or promotion capability.

`repair_primitives.py` remains as the Batch 006 compatibility facade. New project-owned code should import from `semantic_repair` directly.
