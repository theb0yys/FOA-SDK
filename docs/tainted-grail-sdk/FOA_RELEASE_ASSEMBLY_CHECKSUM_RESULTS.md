# FoA Release Assembly and Checksum Results

## Status

This Phase 8 slice is a read-only contract for results supplied by a separately reviewed external assembler/checksummer. It does not implement a package assembler, filesystem inventory, file reader, hasher, signer, uploader, publisher, launcher, runtime adapter, or deployment executor.

The contract is evidence intake, not operational authority. An `accepted` envelope means that the supplied metadata is structurally valid and exactly bound to one ready release-artifact envelope. It does not mean that the TG SDK performed, trusted, approved, signed, uploaded, or published the reported archive.

## Inputs and ownership

`AdapterReleaseAssemblyEvidenceService::BuildEvidenceReturn(...)` consumes:

- one `AdapterReleaseArtifactEnvelope` whose status is `ready`, whose `MetadataReady` flag is true, and whose canonical JSON is present;
- one separately supplied `AdapterReleaseAssemblyResultEnvelope`.

Core owns the contracts, validation service, candidate-evidence construction, and transient result registry. Editor owns the non-editable pane and its pane-lifecycle component. Framework does not gain a filesystem or process implementation from this slice.

## Exact release-artifact binding

The result envelope carries the complete release-artifact canonical JSON and a lowercase `sha256:<64 hex>` fingerprint over those exact UTF-8 bytes. Validation also requires exact equality for:

- artifact identity;
- reconciliation identity;
- package-preview identity;
- manifest identity and fingerprint;
- pack identity and version;
- profile, game version, branch, and runtime target.

Reformatting or replacing the canonical JSON, changing any release context, or presenting a fingerprint for different bytes yields `artifact_binding_mismatch`.

## Reviewed external assembler/checksummer

The supplied review contains:

- stable review and assembler identities;
- assembler version and binary/tool fingerprint;
- `unknown`, `accepted`, or `rejected` decision;
- named reviewer, evidence identities, review timestamp, and notes;
- declared `content_checksum`, `archive_assembly`, and `archive_checksum` capabilities.

All three capabilities and an accepted evidence-backed review are required. The SDK does not discover, install, load, invoke, or authenticate the tool.

## Per-content checksum observations

Every content row declared by the ready release-artifact envelope has exactly one checksum observation. Each observation binds to the exact content identity, package path, and expected lowercase SHA-256 declaration.

Observation outcomes are:

- `not_observed` — no checksum attempt or observation is claimed;
- `matched` — an observed SHA-256 equals the exact expected declaration;
- `mismatched` — a valid observed SHA-256 differs from the expected declaration;
- `failed` — an attempt is reported with at least one bound failure;
- `inconclusive` — an attempt is reported with failure or diagnostic context but no conclusive match decision.

The outcome, attempted/recorded flags, observed checksum, timestamp, failures, and diagnostics must agree. `accepted` is contract acceptance, so a valid envelope can preserve mismatched, failed, inconclusive, or not-observed results without hiding them.

## Archive result

One archive result preserves:

- stable archive identity;
- safe relative archive reference and declared format;
- `not_attempted`, `succeeded`, `failed`, or `skipped` outcome;
- whether assembly was attempted and whether an archive was reported present;
- reported byte size, lowercase SHA-256 archive fingerprint, and completion timestamp;
- bound failure and diagnostic identities.

A successful result requires an attempted, present, non-empty archive with a valid fingerprint and UTC completion time. A failed result requires an attempted operation, completion time, and at least one failure; a reported partial archive must still have a non-zero size and valid fingerprint. Not-attempted and skipped results cannot claim an archive, size, fingerprint, or completion time.

These are externally supplied observations. The TG SDK does not open the archive or independently verify the reported identity, size, or fingerprint.

## Failures and safe diagnostics

Failures carry stable identity, typed kind, code, message, optional content binding, diagnostic references, and retryable metadata. Failure kinds cover contract/input binding, unavailable content, access denial, unsupported archive formats, reads, hashing, archive creation, internal failures, and unknown failures.

Diagnostics carry stable identity, typed kind, a safe relative reference, lowercase SHA-256 fingerprint, and optional declared-content bindings. Absolute paths, drive-qualified paths, traversal components, backslashes, control bytes, empty components, and duplicate identities or references are rejected. Referenced diagnostic content is never opened, persisted, or inspected by this slice.

## Deterministic fail-closed status

Status precedence is:

1. `artifact_not_ready`;
2. `assembler_unreviewed`;
3. `artifact_binding_mismatch`;
4. `envelope_invalid`;
5. `content_coverage_incomplete`;
6. `checksum_observation_mismatch`;
7. `archive_binding_mismatch`;
8. `failure_diagnostic_binding_mismatch`;
9. `accepted`.

Issues are sorted by code, content identity, observation identity, failure identity, diagnostic identity, and message. Candidate evidence is returned only when the contract is valid.

## Candidate evidence

A valid envelope yields candidate source/evidence documents for:

- exact release-artifact binding;
- accepted assembler/checksummer review;
- the reported archive outcome and fingerprint;
- each per-content checksum observation;
- every supplied failure;
- each safe diagnostic reference.

The service returns these documents by value. It does not register or promote them in `SourceEvidenceRegistry`, approve release metadata, or author a later signing/publication decision.

## Transient registry and Editor pane

`AdapterReleaseAssemblyResultRegistry` accepts envelopes with stable result/artifact identities and valid artifact/result fingerprint shapes. Duplicate result identities are rejected. The registry is transient and is cleared when the dedicated pane component deactivates.

The read-only **Tainted Grail Release Assembly and Checksum Results** pane displays:

- result and exact release-artifact binding;
- reviewed assembler/checksummer identity and decision;
- reported archive attempt, outcome, path, size, and fingerprint;
- per-content checksum outcome, expected checksum, and observed checksum;
- failure identities and safe diagnostic references;
- manifest, pack, profile, game, branch, and runtime context;
- explicit SDK safety state.

The pane has no edit, registration, review-authoring, file-open, hash, copy, archive, sign, upload, publish, launch, adapter, or deployment action. The ordinary Developer Preview state contains zero registered release assembly-result envelopes.

## Operational boundary

The result service keeps all operational flags false. It does not read or hash package files, generate a checksum, copy files, or assemble an archive. No signature is produced. No upload or publication occurs. No FoA launch, adapter call, or deployment mutation occurs.

Real assembly, hashing, identity/time trust, signing, upload, and publication remain separately reviewed future work.

## Acceptance

The slice is acceptable when:

- only one exact ready release-artifact envelope can be bound;
- assembler/checksummer review and required capabilities are complete;
- every declared content has exactly one exact-bound observation;
- checksum outcomes and archive outcomes are self-consistent without erasing adverse results;
- failures and diagnostics have complete safe references;
- candidate evidence is built only for a contract-valid envelope;
- the registry remains transient and the pane remains non-editable;
- all operational flags stay false;
- the focused repository validator and local validation gate pass;
- the Windows checklist covers all twenty-one TG SDK panes and the zero-result default state.
