# Semantic Hook Batch 005

## Identity

- base: merged Semantic Hook Batch 004 commit `aaba9fe3f2374a8952da2dece0c53bf71a91b41f`
- authority: source-repair design and offline specification validation only
- runtime authority: none
- promotion authority: none

Batch 005 converts the Batch 004 prohibitions into repair contracts and adds a project-owned offline runner for the four exact-profile fixture manifests. It is not a promotion pass and weakens no adverse case.

## Deliverables

### Source repairs

[`repairs/BATCH_005_SOURCE_REPAIR_DESIGNS.md`](repairs/BATCH_005_SOURCE_REPAIR_DESIGNS.md) specifies replacements for generic economy mutation, diagnostic output, wolf mount conversion, and Avalon Companions cleanup. The designs require exact typed adapters, transactions, idempotent rollback, canonical identities, bounded diagnostics, API versioning, owner isolation, and retryable cleanup.

### Offline runner

- `scripts/tainted_grail/semantic_fixture_runner.py`
- `scripts/tainted_grail/test_semantic_fixture_runner.py`
- [`fixtures/OFFLINE_RUNNER.md`](fixtures/OFFLINE_RUNNER.md)

The runner validates specification shape and fail-closed decisions only. It rejects accepted runtime fingerprints and promotion-bearing decisions. Deterministic receipts always state `runtime_authority: none` and `promotion: none`.

## Validation performed

Python standard-library `unittest` result:

- tests: 8;
- passed: 8;
- failed: 0;
- external dependencies: 0.

Coverage includes valid specifications, duplicate cases, runtime fingerprints presented as evidence, promotion attempts, deterministic receipts, sorted discovery, invalid manifests, and non-JSON input.

## Decisions

- all Batch 004 current-source prohibitions remain unchanged;
- no hook, helper, controller, writer, adapter, or profile is promoted;
- a runner-valid manifest is not an executed fixture receipt;
- a repair design is not an implementation;
- runtime verification and governance promotion remain separate future authorities.

## Scope boundary

No game inspection, assembly loading, dependency download, upstream-mod build, deployment, game launch, runtime reflection, Harmony registration, game mutation, diagnostics capture, mount conversion, dialogue registration, save access, publication, catalog mutation, or evidence promotion occurs.

The runner reads only explicit JSON files or immediate JSON children of an explicit directory. It can write one named receipt and has no network or subprocess behavior.

## Next documented unit

Semantic Hook Batch 006 should implement the first project-owned repaired components behind inert interfaces: typed transaction primitives, hardened diagnostic path/publication utilities, a synthetic wolf conversion transaction model, Avalon Companions API v2 contract types and retryable cleanup state, plus runner golden receipts. It must still grant no game-runtime authority.
