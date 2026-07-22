# Semantic Repair Package

`semantic_repair` is a project-owned, engine-neutral package for deterministic synthetic repair work.

It provides:

- explicit transaction, cancellation and rollback phases with validated transitions;
- typed mapping and synthetic mount transactions;
- owner/generation resource leases and locked journal writes;
- hash-chained crash-recovery journals with deterministic recovery plans;
- deterministic Cartesian failure matrices;
- diagnostic path, redaction, byte-limit, generation and atomic-publication utilities;
- canonical API v2 JSON, version negotiation and stale-generation snapshot rejection;
- negative policy validation for the read-only offline workflow.

The package is not a game runtime adapter. It has no game or loader assembly references, dynamic import, reflection, network, subprocess, installation discovery, deployment, evidence admission, or promotion capability.

`repair_primitives.py` remains as the Batch 006 compatibility facade. New project-owned code should import from `semantic_repair` directly.
