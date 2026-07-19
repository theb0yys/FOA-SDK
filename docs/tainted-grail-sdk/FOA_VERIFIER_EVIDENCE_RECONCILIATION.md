# FoA Verifier Evidence Reconciliation and Release Decision

## Status

Slice 18 is implemented as a typed, deterministic, read-only reconciliation contract.

It reconciles one exact post-deployment report, one structurally valid independent-verifier evidence return, and one explicit named human release review. It does not execute a verifier, access or mutate deployment targets, launch FoA, call an adapter, promote evidence, assemble or sign an archive, or publish a release.

## Purpose

The contract keeps four claims separate:

1. contract validity — whether every identity, fingerprint, count, finding, review, and disposition is exact and complete;
2. compatibility assessment — `unassessed`, `clear`, `blocked`, or `inconclusive`;
3. release decision — `pending`, `hold`, `rejected`, or `approved`;
4. human-review state — `missing`, `invalid`, `disposition_required`, or `complete`.

An all-matched verifier result does not automatically approve a release. A matched observation also cannot silently clear an existing report blocker.

## Exact reconciliation binding

Every reconciliation request binds to:

- one current `review_ready` deployment work order;
- its exact canonical work-order JSON and fingerprint;
- one exact deployment execution-result ID and fingerprint;
- one exact post-deployment report ID, typed status, and canonical report JSON;
- one exact independent-verifier result ID and fingerprint;
- one structurally valid verifier evidence return whose status is either `accepted` or `observation_mismatch`;
- exact profile ID, game version, branch, and `Mono` or `IL2CPP` runtime target;
- exact pack ID, preview ID and fingerprint, and target-inventory ID;
- exact candidate source and evidence identities returned by the report and verifier contracts;
- one deterministic UTC evaluation time.

Rejected or malformed verifier evidence cannot be reconciled. A structurally valid `observation_mismatch` result remains eligible because adverse evidence cannot be filtered out merely because it is adverse.

## Reconciliation findings

The service produces one typed finding for every existing report blocker and every independent-verifier check.

Finding kinds are:

- `report_blocker`;
- `verifier_matched`;
- `verifier_mismatched`;
- `verifier_failed`;
- `verifier_inconclusive`;
- `verifier_not_run`.

Evidence relationships are:

- `preserved`;
- `supporting`;
- `contradictory`;
- `new_finding`.

Every existing report blocker is preserved with its exact subject, step, evidence, diagnostic references, compatibility effect, and release effect.

A matched verifier observation is supporting when no report blocker exists for the same step. When it conflicts with a same-step report blocker, it is contradictory and requires human disposition, but the existing blocker remains present.

A mismatched observation creates a compatibility and release blocker. Failed, inconclusive, and not-run checks remain explicit adverse findings and block release review. They are not converted into successful observations.

## Human release review

The release review records:

- stable review identity;
- exact report ID and canonical report JSON;
- exact verifier-result ID;
- exact work-order, execution-result, and verifier-result fingerprints;
- typed `hold`, `reject`, or `approve` decision;
- named reviewer;
- evidence IDs;
- UTC review time;
- rationale;
- one typed disposition for every finding that requires human disposition.

Disposition decisions are:

- `accepted`;
- `rejected`;
- `deferred`.

Deferred or missing dispositions keep the reconciliation incomplete. Duplicate, unknown, extra, cross-bound, evidence-free, or rationale-free dispositions fail closed.

Reviewer identity and review time are caller-supplied metadata. Slice 18 does not claim a trusted identity provider, trusted clock, cryptographic signature, or timestamp authority.

## Compatibility assessment

The compatibility axis is derived independently from the release decision:

- `unassessed` — upstream report, verifier evidence, or exact binding is invalid;
- `blocked` — one or more preserved or derived compatibility blockers remain;
- `inconclusive` — no compatibility blocker exists, but at least one required verifier check failed, was inconclusive, or was not run;
- `clear` — the exact inputs are valid, no compatibility blocker remains, and every required verifier check is conclusive.

`clear` is not compatibility certification. It describes only the exact supplied and reconciled metadata.

## Release decision

The release axis is derived from the explicit human review:

- `pending` — review or required dispositions are missing or invalid;
- `hold` — the reviewer selected hold, or an attempted approval conflicts with blockers, adverse evidence, an inconclusive compatibility assessment, or unresolved dispositions;
- `rejected` — the reviewer explicitly rejected the candidate;
- `approved` — the reviewer explicitly approved, compatibility is `clear`, no release blocker remains, the verifier evidence is all-matched `accepted`, the upstream report is compatibility-clear and release-blocker-free, and every required disposition is accepted.

Approval never publishes, signs, uploads, packages, launches, or executes anything.

## Contract status precedence

The envelope receives one deterministic contract status:

1. `report_not_ready`;
2. `verifier_evidence_invalid`;
3. `binding_mismatch`;
4. `review_missing`;
5. `review_invalid`;
6. `disposition_incomplete`;
7. `decision_inconsistent`;
8. `accepted`.

`accepted` means the reconciliation contract is structurally valid. It is separate from compatibility and release-decision values.

## Deterministic output

Equivalent exact inputs produce identical:

- reconciliation identities and bindings;
- finding identities, ordering, relationships, blocker effects, evidence IDs, and diagnostic associations;
- disposition ordering and counts;
- compatibility assessment;
- release decision;
- human-review state;
- canonical JSON;
- candidate evidence ordering.

The service does not mutate the work order, execution-result envelope, post-deployment report, verifier-result envelope, verifier evidence return, review request, or registries.

## Candidate evidence return

An accepted reconciliation returns candidate source/evidence documents by value for:

- exact reconciliation binding;
- compatibility assessment;
- named release review and decision;
- every preserved, supporting, contradictory, or newly derived finding.

Candidate confidence remains `unrated`. Nothing is automatically registered, persisted, imported, promoted, validated, permitted, assembled, signed, published, released, launched, or dispatched.

## Transient registry and Editor pane

`AdapterVerifierEvidenceReconciliationRegistry` stores transient reconciliation requests and is cleared during Editor shutdown.

The read-only **Tainted Grail Verifier Evidence Reconciliation and Release Decision** pane displays:

- reconciliation, report, verifier-result, execution-result, and work-order identities;
- contract status;
- compatibility assessment;
- release decision;
- human review and disposition coverage;
- preserved and derived findings;
- compatibility and release blocker counts;
- candidate evidence counts;
- exact profile, game, branch, runtime, pack, preview, and target-inventory context;
- contract issues and the permanent safety boundary.

The ordinary Developer Preview state contains zero reconciliation requests. The pane exposes no registration, review-authoring, verifier, filesystem, deployment, rollback, launch, adapter, promotion, archive, signing, publication, or release control.

## Architecture and safety boundary

Contracts, vocabularies, transient registry, validation, reconciliation, deterministic serialization, and candidate evidence return are Core-owned. Presentation is Editor-owned.

Slice 18 adds no:

- verifier discovery, installation, loading, invocation, or process execution;
- target filesystem scanning, reading, hashing, copying, replacement, deletion, or mutation;
- backup, restore, rollback execution, deployment, or FoA launch;
- BepInEx/Harmony or runtime-adapter call;
- evidence import or promotion;
- package or archive assembly;
- checksum generation;
- archive signing or trusted attestation;
- release publication or upload;
- durable reconciliation or release-decision schema.

## Next ordered slice

The next slice is a release-artifact provenance and signing-intent contract. It should bind one exact approved reconciliation to declared release contents, checksums, provenance, signing identity intent, and publication targets while still performing no archive assembly, hashing, signing, upload, or publication.
