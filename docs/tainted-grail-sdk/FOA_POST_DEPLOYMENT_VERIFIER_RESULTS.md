# FoA Independent Post-Deployment Verifier Results

## Status

Slice 17 is implemented as a typed, deterministic, read-only contract for metadata supplied by a separately reviewed independent verifier.

The TG SDK does not discover, install, load, invoke, or trust a verifier automatically. It does not inspect or mutate deployment targets. It validates supplied metadata and returns candidate source/evidence documents by value.

## Purpose

The contract keeps three claims separate:

1. the deployment execution-result envelope records what a separately reviewed executor reported;
2. the post-deployment report derives compatibility and release blockers from that execution metadata;
3. the independent verifier envelope records separately supplied observations of the exact expected final target state.

Contract validity is not compatibility certification, release approval, or authority to execute another action.

## Exact report binding

Every verifier envelope binds to one current post-deployment report through:

- report ID, typed report status, and exact deterministic canonical report JSON;
- deployment execution-result ID and fingerprint;
- deployment work-order ID and fingerprint;
- exact profile ID, game version, branch, and `Mono` or `IL2CPP` runtime target.

The service also requires the exact current `review_ready` work order and matching execution-result envelope. The work order retains every execution and mutation permission as false.

Reports backed by rejected or incomplete execution evidence are ineligible. Reports with explicit verification, compatibility, rollback, or release blockers remain eligible inputs because an independent verifier must preserve adverse state rather than silently discard it.

## Separately reviewed verifier

The verifier review contains:

- stable review and verifier identities;
- strict semantic verifier version;
- lowercase SHA-256 verifier fingerprint;
- `unknown`, `accepted`, or `rejected` review decision;
- named reviewer and stable evidence IDs;
- UTC review time and notes;
- unique typed capabilities: `target_presence`, `target_fingerprint`, and `target_absence`.

The accepted review must cover every capability required by the exact work-order changes. Additions and replacements require presence and fingerprint checks. Removals require absence checks.

No verifier executable, configuration, signature provider, trust store, or timestamp authority is located or called by this slice.

## Exact independent checks

The envelope contains exactly one check for every canonical add, replace, and remove work-order step. Each check preserves:

- stable check ID and exact work-order sequence;
- exact source step and target path;
- expected target presence and fingerprint derived from the work order;
- attempted and observation-recorded state;
- observed target presence and fingerprint;
- typed outcome and UTC check time;
- same-check failure and diagnostic references.

Unknown, duplicate, missing, extra, or drifted checks fail closed.

## Typed outcomes

Independent check outcomes are:

- `not_run` — no attempt, timestamp, observation, failure, or diagnostic claim;
- `matched` — a timestamped observation exactly matches expected presence and fingerprint;
- `mismatched` — a timestamped observation differs from expected presence or fingerprint;
- `failed` — an attempted check reports typed failures;
- `inconclusive` — an attempted check reports typed failures explaining why no conclusion is available.

A complete mismatched envelope is structurally contract-valid but receives `observation_mismatch`, not `accepted`. Candidate evidence is still returned so the mismatch can be reviewed without being mistaken for a successful verification.

## Failures and safe diagnostics

Typed failure kinds distinguish contract, input-binding, target-unavailable, access-denied, unsupported-check, read, hash, internal, and unknown failures.

Typed diagnostic kinds distinguish verifier, filesystem, hash, binding, and general diagnostics. Every diagnostic requires stable identity, a safe relative reference, a lowercase SHA-256 fingerprint, and one or more exact check bindings.

Failed and inconclusive checks require typed failures. Every failure requires at least one diagnostic bound to the same exact check. Duplicate, unknown, cross-check, or orphan references fail closed.

The service does not open, inspect, persist, redact, upload, or interpret diagnostic content.

## Status precedence

Every evidence return receives one deterministic status:

1. `report_not_ready`;
2. `verifier_unreviewed`;
3. `report_binding_mismatch`;
4. `envelope_invalid`;
5. `check_coverage_incomplete`;
6. `failure_diagnostic_binding_mismatch`;
7. `observation_mismatch`;
8. `accepted`.

`accepted` means the supplied contract is valid and every required independent check reports an exact match. It is still not release permission, compatibility certification, evidence promotion, or execution authority.

## Candidate evidence return

Structurally valid `accepted` and `observation_mismatch` envelopes return candidate documents by value for:

- exact post-deployment report binding;
- reviewed verifier identity;
- every independent target-state check;
- typed failures;
- separately fingerprinted diagnostic references.

Candidate confidence remains `unrated`. Nothing is registered, persisted, imported, promoted, validated, permitted, signed, published, released, launched, or dispatched automatically.

## Transient registry and Editor pane

`AdapterPostDeploymentVerifierResultRegistry` is transient and is cleared during Editor shutdown.

The read-only **Tainted Grail Independent Post-Deployment Verifier Results** pane displays:

- verifier-result and post-deployment-report identity;
- structural contract status;
- reviewed verifier and capabilities;
- exact expected and independently observed target states;
- failures and safe diagnostics;
- candidate source/evidence counts;
- exact profile, game, branch, and runtime context;
- contract issues and the permanent safety boundary.

The ordinary Developer Preview state contains zero registered verifier envelopes. The pane exposes no registration, verifier, filesystem, deployment, launch, adapter, promotion, archive, signing, publication, or release control.

## Architecture and safety boundary

Contracts, transient registry, deterministic report serialization, exact validation, and candidate evidence return are Core-owned. Presentation is Editor-owned.

Slice 17 adds no:

- verifier discovery, installation, loading, invocation, or process execution;
- target filesystem scanning, reading, hashing, copying, replacement, deletion, or mutation;
- deployment, backup, restore, rollback execution, or FoA launch;
- BepInEx/Harmony or runtime-adapter call;
- evidence import or promotion;
- archive assembly, checksum publication, signing, release publication, or upload;
- durable verifier-result schema.

The next ordered slice reconciles a structurally valid verifier evidence return with the existing compatibility and release blockers. That reconciliation remains read-only and cannot treat contract validity as certification or release authority.
