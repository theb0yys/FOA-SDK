# Semantic Repair Package

`semantic_repair` is a project-owned, engine-neutral package for deterministic synthetic repair work.

It provides:

- explicit transaction phases and validated transitions;
- typed mapping and synthetic mount transactions;
- hash-chained crash-recovery journals;
- deterministic Cartesian failure matrices;
- diagnostic path, redaction, byte-limit and atomic-publication utilities;
- API v2 canonical JSON serialization and version negotiation.

The package is not a game runtime adapter. It has no game or loader assembly references, dynamic import, reflection, network, subprocess, installation discovery, deployment, evidence admission, or promotion capability.

`repair_primitives.py` remains as the Batch 006 compatibility façade. New project-owned code should import from `semantic_repair` directly.
