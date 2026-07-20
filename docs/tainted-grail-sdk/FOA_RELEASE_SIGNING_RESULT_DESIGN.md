# FoA Release-Signing Result Envelope Design

Status: proposed for design review

Target: Phase 8 release-signing result evidence slice

## Decision requested

Approve a focused read-only contract that validates a release-signing result supplied by a separately reviewed external signer and binds it to one exact accepted release-assembly/checksum result.

Approval authorises contract, validation, transient-registry, read-only Editor-pane, test, validator, and documentation work on `foa-development` only.

It does not authorise the SDK to:

- open or modify an archive;
- load a private key, certificate, credential, token, or PIN;
- resolve or authenticate a signing identity;
- perform cryptographic signing or signature verification;
- copy or write signature artifacts;
- upload or publish a release;
- launch FoA or call a runtime adapter;
- mutate a deployment or save.

## Why this slice comes next

The current Phase 8 chain can preserve one structurally valid external release-assembly/checksum result, including the exact archive identity, safe reference, byte size, and reported fingerprint.

It does not yet provide a typed handoff for the next externally performed operation. A later reviewer therefore cannot distinguish:

- the exact accepted assembly result presented to a signer;
- the approved signing intent and identity;
- the separately reviewed signer and tool;
- whether signing was attempted, succeeded, failed, or skipped;
- which signature artifacts were reported and how they were fingerprinted;
- failures and safe diagnostics returned by the signer;
- structurally valid supplied evidence from SDK-performed signing authority.

The roadmap names the release-signing result envelope as the next ordered Phase 8 slice. This design keeps it evidence-only and preserves the existing editor/runtime and operational boundaries.

## Architecture ownership

### Core

Core owns:

- versioned signing-result contracts;
- enum parsing and deterministic stringification;
- stable-identity, fingerprint, timestamp, and safe-reference validation;
- exact upstream-binding validation;
- deterministic issue construction and ordering;
- candidate source/evidence construction by value;
- a transient signing-result registry.

### Editor

Editor owns:

- one non-editable **Tainted Grail Release Signing Results** pane;
- one pane-lifecycle system component;
- presentation of exact bindings, outcomes, artifacts, failures, diagnostics, and safety state.

### Framework and runtime

Framework and runtime gain no filesystem, key-store, cryptographic, process, network, upload, publication, launch, adapter, deployment, or save implementation.

## Exact inputs

The evidence service should consume:

1. the exact ready `AdapterReleaseArtifactEnvelope` containing the reviewed signing intent;
2. the exact `AdapterReleaseAssemblyResultEnvelope`;
3. the exact `AdapterReleaseAssemblyEvidenceReturn`, which must be contract-valid and accepted for that result;
4. one separately supplied `AdapterReleaseSigningResultEnvelope`.

The signing result must bind exactly to:

- release-artifact identity and fingerprint;
- assembly-result identity and fingerprint;
- archive identity;
- archive safe relative path;
- archive format;
- archive byte size;
- archive lowercase SHA-256 fingerprint;
- reconciliation identity;
- package-preview identity;
- manifest identity and fingerprint;
- pack identity and version;
- profile identity;
- game version;
- branch;
- runtime target;
- signing-intent identity;
- signing-intent decision;
- signing identity kind;
- signer identity;
- identity locator;
- identity fingerprint.

Canonical-byte drift, fingerprint drift, archive drift, release-context drift, or signing-intent drift must fail closed.

## Signing-intent behavior

This slice applies only when the exact ready release-artifact envelope declares `SignExternally`.

An artifact whose reviewed decision is `Unsigned` must not be converted into signing evidence. It should produce a distinct `signing_not_requested` status and no candidate signing evidence.

The supplied result cannot select a different signer, identity kind, locator, or identity fingerprint from the approved signing intent.

Identity locators and fingerprints remain reviewed metadata. The SDK must not resolve the locator, load a key or certificate, contact a transparency or identity service, or authenticate the signer.

## Proposed contract surface

### Reviewed external signer

Preserve:

- stable review identity;
- stable signer-tool identity;
- strict semantic signer-tool version;
- lowercase SHA-256 signer-tool fingerprint;
- `unknown`, `accepted`, or `rejected` review decision;
- named reviewer;
- evidence identities;
- UTC review timestamp;
- notes;
- required capabilities for archive signing and signature-artifact fingerprint reporting.

An accepted, evidence-backed, capability-complete review is required. The SDK does not discover, install, load, invoke, or authenticate the signer or its identity.

### Signing outcome

Preserve a typed outcome:

- `not_attempted`;
- `succeeded`;
- `failed`;
- `skipped`.

The result also preserves:

- attempted state;
- UTC completion timestamp;
- exact bound archive identity and fingerprint;
- reported signature-artifact identities;
- failure identities;
- diagnostic-reference identities.

A successful result must:

- report that signing was attempted;
- include a valid completion timestamp;
- contain at least one valid signature artifact;
- contain no failure that contradicts success.

A failed result must:

- report that signing was attempted;
- include a valid completion timestamp;
- contain at least one bound failure.

A not-attempted or skipped result cannot claim successful signing or completed signature artifacts.

Contract acceptance remains separate from operational success. A structurally valid failed, skipped, or not-attempted result remains representable as adverse supplied evidence.

### Signature artifacts

Each reported signature artifact should preserve:

- stable artifact identity;
- typed artifact kind;
- safe relative reference;
- media type;
- reported byte size;
- lowercase SHA-256 fingerprint;
- UTC creation timestamp;
- exact archive identity and fingerprint binding;
- optional safe diagnostic-reference identities.

The contract should support detached signatures and other reviewed signature evidence without assuming one vendor, identity technology, archive format, or publication target.

No signature artifact is opened, hashed, parsed, verified, copied, written, registered, or persisted by this slice.

### Failures

Failures should preserve:

- stable failure identity;
- typed failure kind;
- bounded code;
- bounded safe message;
- retryable state;
- bound diagnostic-reference identities.

Failure kinds should cover contract/input binding, signer unavailability, identity unavailability, access denial, unsupported identity or format, archive read, signing, signature-artifact creation, internal failure, and unknown failure.

### Diagnostics

Diagnostic references should preserve:

- stable diagnostic identity;
- typed diagnostic kind;
- safe relative reference;
- lowercase SHA-256 fingerprint;
- optional signature-artifact bindings.

Reject:

- absolute or drive-qualified paths;
- path traversal;
- backslashes;
- control bytes;
- empty path components;
- malformed fingerprints or timestamps;
- duplicate identities or references;
- orphan failure, diagnostic, or artifact references;
- contradictory reciprocal bindings.

Referenced diagnostic content is never opened or inspected.

## Deterministic fail-closed status

Proposed precedence:

1. `assembly_result_not_accepted`;
2. `signing_not_requested`;
3. `signer_unreviewed`;
4. `assembly_binding_mismatch`;
5. `signing_intent_binding_mismatch`;
6. `envelope_invalid`;
7. `signing_outcome_mismatch`;
8. `signature_artifact_binding_mismatch`;
9. `failure_diagnostic_binding_mismatch`;
10. `accepted`.

`accepted` means only that supplied metadata is structurally valid and exactly bound. It must not mean that the SDK:

- performed signing;
- verified a signature;
- trusted or authenticated the signer;
- approved a release;
- uploaded or published an artifact.

Issues must use deterministic precedence and ordering. Candidate evidence must also have deterministic identity and ordering.

## Candidate evidence

A contract-valid result may return candidate source/evidence documents by value for:

- exact release-artifact binding;
- exact assembly-result and archive binding;
- accepted signer review;
- approved signing-intent binding;
- signing attempt and outcome;
- every signature artifact;
- every failure;
- every safe diagnostic reference.

No candidate document is automatically:

- registered;
- persisted;
- promoted;
- validated;
- permitted;
- uploaded;
- published.

## Transient registry

Add an `AdapterReleaseSigningResultRegistry` keyed by stable signing-result identity.

The registry must:

- reject malformed identities and fingerprints;
- reject duplicate result identities;
- preserve stable insertion order;
- remain in memory only;
- clear when the dedicated Editor pane component deactivates.

## Read-only Editor pane

Add a **Tainted Grail Release Signing Results** pane showing:

- signing-result identity and status;
- exact release-artifact and assembly-result bindings;
- exact archive identity, path, size, format, and fingerprint;
- reviewed signer/tool identity and decision;
- approved signing identity kind, signer, locator, and fingerprint;
- attempted state, outcome, and completion time;
- signature artifacts and fingerprints;
- failures and safe diagnostic references;
- manifest, pack, profile, game, branch, and runtime context;
- explicit SDK safety flags.

The pane must expose no edit, registration, review-authoring, file-open, key-load, cryptographic, copy, sign, verify, upload, publish, launch, adapter, deployment, or save action.

The ordinary Developer Preview state should contain zero registered signing-result envelopes. The Windows manual checklist should expand from twenty-one to twenty-two TG SDK panes.

## Persistence and compatibility

- Contract version: `1`.
- Transient in-memory registry only.
- No durable schema change.
- No migration.
- No change to existing release-artifact or release-assembly result contracts.
- No new dependency.
- No platform-specific signing API.

## Security, privacy, and legal boundary

The slice must never accept or retain:

- private-key bytes;
- passwords, tokens, credentials, or PINs;
- unrestricted environment dumps;
- secret-bearing command lines;
- unrestricted absolute paths;
- raw key-store or certificate-store inventories.

The slice must never:

- resolve or probe an identity locator;
- disclose private signing infrastructure;
- log credentials or key-store details;
- claim legal identity verification;
- claim cryptographic trust or signature validity;
- infer success from intent alone;
- erase failed, skipped, partial, or adverse evidence.

Messages and diagnostic metadata must remain bounded and safe for public evidence handling.

## Failure and rollback behavior

Malformed or mismatched input returns deterministic issues and no candidate evidence. Validation is non-mutating.

Rollback is removal or reversion of the new contracts, service, registry, pane, tests, validator, and documentation. Existing durable data requires no migration.

## Test plan

### Pure Core compiled tests

Cover:

- exact accepted assembly-result and canonical/fingerprint binding;
- unsigned-intent refusal;
- signer-review decision, capability, evidence, version, timestamp, and fingerprint validation;
- exact signing-intent identity binding;
- every outcome and attempted/completion-state combination;
- successful results requiring signature artifacts;
- failed results requiring failures;
- safe-reference, size, fingerprint, timestamp, duplicate, orphan, and reciprocal-reference checks;
- deterministic issue and evidence ordering;
- input non-mutation;
- transient-registry duplicate rejection and clear behavior;
- all operational flags remaining false.

### Repository validation

Add a focused validator and negative tests enforcing:

- required contract, service, registry, pane, and test files;
- CMake and compiled-test integration;
- Editor module and pane registration;
- documentation and roadmap status;
- local-validation integration;
- twenty-two-pane Windows manual UI coverage;
- explicit no-operation safety flags and boundary text.

### Host validation

Require:

- repository-owned local validation;
- applicable O3DE configure and host build;
- the focused compiled signing-result test target;
- existing compiled TG SDK tests;
- Windows Editor pane validation for the accepted exact head.

## Implementation sequence

After design approval, proceed in focused reviewable commits:

1. Core contracts, parsing, stable validation helpers, and transient registry;
2. deterministic evidence service and candidate-evidence construction;
3. focused compiled tests and negative coverage;
4. read-only Editor pane and lifecycle registration;
5. repository validator and local-validation integration;
6. public contract documentation, roadmap, release-process, documentation-hub, and twenty-two-pane checklist updates;
7. exact-head validation, host build, compiled tests, and Windows pane evidence.

Each commit must receive pre-commit self-review and relevant local validation. `foa-development` must be synchronized before and after every commit.

## Acceptance criteria

The slice is complete only when:

- only an exact accepted assembly/checksum result for a `SignExternally` artifact can produce accepted signing evidence;
- signer review and required capabilities are complete and exact-bound;
- signing outcomes and signature artifacts are self-consistent without erasing adverse results;
- failures and diagnostics are safe, complete, deterministic, and referentially valid;
- candidate evidence is returned only for a contract-valid envelope;
- the registry remains transient;
- the Editor pane remains non-editable;
- all file, archive, key, cryptographic, upload, publication, launch, adapter, deployment, and save operations remain absent;
- focused validators, compiled tests, repository validation, applicable host build, and Windows pane coverage pass.

## Review boundary

Approval authorises implementation of this evidence-only release-signing result envelope on `foa-development`.

It does not authorise real signing, key handling, identity resolution, signature verification, upload, publication, FoA launch, adapter execution, deployment mutation, save mutation, telemetry, or durable schema changes.
