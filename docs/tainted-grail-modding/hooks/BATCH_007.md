# Semantic Hook Batch 007 — Reusable Offline Repair Package

Batch 007 hardens the Batch 006 synthetic primitives into a reusable project-owned package, adds explicit state and recovery contracts, broadens deterministic failure matrices, versions the API v2 interchange fixtures, and wires the complete offline suite into CI.

It grants no game-runtime or promotion authority.

## Base identity

```text
Batch 006 merge commit: 8acc1e79ae69f7063d2c4c30993e92a81b0836c7
Batch 007 branch: agent/tg-semantic-hook-batch-007
runtime authority: none
promotion authority: none
```

## Package boundary

The new `scripts/tainted_grail/semantic_repair/` package contains:

- `state_machine.py` — explicit transaction phases and legal transitions;
- `journal.py` — append-only hash-chained synthetic recovery records;
- `transactions.py` — typed mapping and synthetic mount transactions;
- `diagnostics.py` — path, redaction, byte-limit and atomic-publication utilities;
- `dialogue_v2.py` — canonical API v2 documents, negotiation, registry and retryable cleanup;
- `failure_matrix.py` — deterministic Cartesian case generation and result receipts;
- `errors.py` — shared fail-closed exceptions.

`repair_primitives.py` is retained as a compatibility façade. It adds no capability.

## Explicit transaction states

The shared state machine uses:

```text
new
→ preparing
→ prepared
→ committing
→ committed
```

Failure and cleanup use:

```text
preparing|committing
→ failed
→ rolling-back
→ rolled-back
```

A committed transaction may also enter `rolling-back`. Illegal transitions fail closed. Rollback is idempotent, and original `null` values are represented explicitly rather than treated as an absent snapshot.

## Synthetic crash-recovery journal

Each newline-delimited record contains:

- schema version;
- monotonic sequence;
- transaction ID;
- phase;
- deterministic JSON payload;
- previous-record SHA-256;
- current-record SHA-256.

The reader verifies the full sequence and hash chain. Interior corruption is rejected. A torn final line is recoverable: it is removed before a later append, and no record is admitted behind crash debris.

The journal records synthetic project state only. It is not a game save, mod log, runtime evidence receipt, or diagnostics-export format.

## Broader failure matrices

The package adds deterministic Cartesian matrices without a third-party property-test dependency.

The mount matrix covers:

- four injected stages: object creation, field write, movement change and ownership change;
- two prior ownership states: explicit `null` and an existing mount;
- exact restoration of native actions, fields, movement flags and created-object identities.

All eight injected combinations must fail and restore the complete pre-state.

## API v2 serialization and negotiation fixtures

Canonical JSON fixtures cover:

- a local consumer supporting API versions 2 and 1;
- a remote provider supporting versions 3 and 2;
- a no-overlap provider supporting versions 4 and 3;
- an API v2 command-registration round trip.

Negotiation selects the highest mutual version and fails closed when no overlap exists. Unknown fields, unsupported schema versions and unsupported registration API versions are rejected.

These fixtures use synthetic assembly names and versions. They do not establish an Avalon Companions binary fingerprint, load result, runtime behavior, cleanup behavior, or combined-mod compatibility.

## Offline authority guard

AST and source-boundary tests prohibit the package from importing network, subprocess, dynamic-loading or native-interop modules. They also reject game, loader and DLL identity markers inside the reusable package.

This is a project boundary test, not malware scanning or runtime sandboxing.

## CI wiring

`.github/workflows/semantic-hook-offline.yml` runs only when the offline scripts, Batch 007 documentation, catalogue entry or workflow changes.

The workflow:

- has read-only repository permission;
- uses no secrets;
- compiles `scripts/tainted_grail`;
- runs standard-library `unittest` discovery;
- tests Python 3.11, 3.12 and 3.13;
- uploads no binaries, logs, saves, evidence or artifacts.

## Local validation

Focused Batch 006/007 package validation:

```text
24 tests passed
0 failed
0 external Python dependencies
```

The CI job is responsible for running the complete repository offline suite, including the Batch 005 manifest runner and Batch 006 golden receipts.

## Governance result

- reusable project-owned package: added;
- explicit transaction state machines: added;
- synthetic recovery journal: added;
- deterministic failure matrices: added;
- API v2 serialization fixtures: added;
- offline CI workflow: added;
- hook promotions: 0;
- evidence promotions: 0;
- runtime adapters: 0;
- game/loader assembly inspection: 0;
- Batch 004 prohibitions changed: 0.

Passing this unit proves only synthetic package behavior and deterministic offline validation.

## Next documented unit

Semantic Hook Batch 008 should add concurrency and crash-window hardening to the project-owned package: journal locking and ownership, two-writer publication fixtures, deterministic recovery planning, transaction cancellation semantics, API v2 stale-generation serialization, and CI negative tests for workflow permissions and artifact publication. It must retain the no-runtime and no-promotion boundary.
