# FoA Release Artifact Provenance and Signing Intent

## Status

This Phase 8 slice is a typed, deterministic, read-only contract for declared release-artifact metadata. It binds one exact approved verifier-evidence reconciliation to one exact ready package layout and records intended release contents, checksum declarations, provenance, legal dispositions, signing identity intent, and publication targets.

It does not perform release operations.

## Inputs

`AdapterReleaseArtifactProvenanceService` receives:

1. one exact accepted `AdapterVerifierEvidenceReconciliationResult`;
2. one exact `ready` `AdapterPackageAssemblyPreview`;
3. one `AdapterReleaseArtifactRequest` containing declared release metadata.

The reconciliation must have:

- `accepted` contract status;
- `clear` compatibility assessment;
- `approved` human release decision;
- complete human review;
- deterministic canonical JSON;
- all verifier, target-access, mutation, promotion, archive, signing, publication, launch, and adapter flags false.

The package preview must have:

- `ready` status;
- deterministic canonical JSON;
- a non-empty package layout;
- assembly, archive, and deployment permissions false.

## Exact binding

The request binds:

- stable release-artifact identity;
- explicit UTC evaluation time;
- exact reconciliation canonical JSON;
- exact package-preview canonical JSON;
- reconciliation, report, verifier-result, work-order, execution-result, and verifier-result fingerprints;
- package-preview, manifest, inventory, pack, adapter, profile, game, branch, and runtime context.

The release evaluation time cannot precede the approved reconciliation evaluation time.

## Declared contents

Every reviewed package-layout entry requires exactly one release-content declaration. No missing, duplicate, or extra entry is accepted.

Each declaration preserves:

- stable content and package-entry identities;
- exact package path;
- role and media type;
- byte size;
- project ownership and redistribution eligibility inherited from the package layout;
- provenance identities;
- legal-disposition identity;
- declared checksum algorithm and value.

## Checksum declarations

The only supported checksum declaration is lowercase SHA-256.

The declared value must equal the exact reviewed `outputDigest` already present in the package layout. This is metadata binding only:

- no package file is opened;
- no file bytes are read;
- no checksum is generated;
- no checksum is independently verified.

A matching declaration means that two metadata fields agree. It is not proof that a file currently exists or has those bytes.

## Provenance

Each content row requires one or more stable provenance records. A record preserves:

- exact content identity and `release-content:<content-id>` subject;
- source kind and stable source identity;
- lowercase SHA-256 source fingerprint;
- unique evidence identities;
- explicit UTC capture time;
- limitations.

Capture time cannot be later than release evaluation. Every referenced provenance identity must resolve to one record for the same content.

## Legal and redistribution disposition

Every content row requires exactly one legal-disposition record. A metadata-ready envelope requires `approved` disposition with:

- stable disposition and content identities;
- named reviewer;
- unique evidence identities;
- UTC review time;
- rationale.

`unknown`, `restricted`, and `rejected` remain explicit non-ready states. The contract does not change ownership, licence, redistribution rights, or third-party obligations.

## Signing intent

Signing intent is reviewed metadata, not a signature.

Decisions are:

- `unsigned` — no signing identity may be supplied;
- `sign_externally` — requires stable signer identity, typed identity kind, safe identity locator, lowercase SHA-256 identity fingerprint, named reviewer, evidence, UTC review time, and rationale.

Identity kinds are:

- `none`;
- `pgp`;
- `x509`;
- `sigstore`;
- `platform`;
- `other`.

The TG SDK does not load a key, contact a signing service, create a signature, or validate a certificate.

## Publication targets

At least one reviewed publication-target declaration is required. Each target preserves:

- stable target identity;
- typed target kind;
- safe locator;
- release channel;
- named reviewer;
- unique evidence identities;
- UTC review time;
- rationale.

Target kinds are:

- `github_release`;
- `mod_portal`;
- `internal_archive`;
- `other`.

A target declaration does not authenticate, upload, publish, create a release, or contact an external service.

## Deterministic status precedence

Envelope status is resolved in this exact order:

1. `reconciliation_not_approved`;
2. `package_not_ready`;
3. `binding_mismatch`;
4. `content_invalid`;
5. `checksum_declaration_invalid`;
6. `provenance_incomplete`;
7. `legal_disposition_incomplete`;
8. `signing_intent_invalid`;
9. `publication_target_invalid`;
10. `ready`.

`ready` means the declared metadata is structurally complete and exact-bound. It is not permission or evidence that any operation occurred.

## Candidate evidence

A ready result returns candidate evidence by value for:

- exact upstream binding;
- every content/checksum declaration;
- reviewed signing intent;
- every publication-target declaration.

Candidate evidence is not automatically registered, persisted, promoted, validated, permitted, signed, uploaded, or published.

## Transient registry and Editor pane

`AdapterReleaseArtifactRegistry` stores transient envelopes for display and is cleared when the dedicated pane system component deactivates.

The read-only **Tainted Grail Release Artifact Provenance and Signing Intent** pane displays:

- artifact and contract status;
- exact reconciliation and package binding;
- content paths, roles, media types, and sizes;
- declared checksum values;
- provenance and legal disposition;
- signing intent;
- publication targets;
- safety flags and blockers.

The pane has no edit, review-authoring, file-open, hash, copy, archive, sign, upload, publish, launch, adapter, or deployment action.

## Operational boundary

Every envelope keeps these fields false:

- `FilesRead`;
- `FilesHashed`;
- `ChecksumGenerated`;
- `FilesCopied`;
- `ArchiveAssembled`;
- `SigningPerformed`;
- `UploadPerformed`;
- `ReleasePublished`;
- `LaunchPerformed`;
- `AdapterCalled`;
- `DeploymentMutated`.

The contract preserves the distinction between declared intent and performed operations. Later operational slices require separately reviewed implementations and evidence.

## Ordinary Developer Preview state

The ordinary Developer Preview has zero registered release-artifact envelopes. The pane therefore displays zero contents and zero publication targets until an external caller supplies and registers an envelope.
