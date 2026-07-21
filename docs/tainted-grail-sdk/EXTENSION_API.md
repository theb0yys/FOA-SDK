# TaintedGrailModdingSDK.ExtensionAPI

## Status

Implemented as a public Framework service. The API is an editor-side authoring and evidence boundary; it does not grant runtime, deployment, signing, save-mutation, or mutable-catalog authority.

## Access

Extensions obtain the persistent service from:

```cpp
auto& api = TaintedGrailModdingSDK::FoundationService::Get().GetExtensionAPI();
```

The service owns no game runtime hooks and exposes no `CatalogDatabase&`, `SourceEvidenceRegistry&`, mutable `GameProfile`, workspace path, install path, plugin path, diagnostics path, or extracted-data path.

## Deterministic registration

An extension registers one `ExtensionDeclaration` containing:

- a stable namespaced extension ID;
- a strict semantic version;
- explicit supported FoA game versions;
- explicit supported branches;
- a non-empty capability set.

Registrations are canonicalized and returned in extension-ID order. Compatibility lists and capabilities are sorted. Duplicate identities, duplicate declarations, unknown capability values, oversized declarations, and registry overflow fail closed.

Supported capabilities are:

- `ReadActiveProfile`;
- `QueryCatalog`;
- `SubmitCandidateEvidence`.

Every operation requires prior registration, the matching capability, and exact support for the active game version and branch.

## Read-only profile view

`GetActiveProfile()` returns a copy containing only:

- profile ID and display name;
- exact game version and branch;
- runtime target;
- Unity and BepInEx versions;
- sorted DLC scopes.

Filesystem and installation locations are deliberately excluded.

## Read-only catalog query

`QueryCatalog()` accepts the existing `CatalogQuery` contract and returns copied `CatalogRecord` values. Results retain the catalog's deterministic record-ID ordering and are capped at 512 records per call.

The API does not expose catalog mutation, persistence, governance, validation, permission changes, pack mutation, or direct catalog references.

## Governed candidate-evidence submission

`SubmitCandidateEvidence()` is the sole write lane. It registers an `EvidenceRecord` only when:

- the extension declared `SubmitCandidateEvidence`;
- the extension supports the exact active game version and branch;
- the referenced source already exists and has a usable import status;
- source fingerprint, profile, game version, branch, and runtime target match exactly;
- evidence identity, provenance, subject, claim, locator, record path, and UTC extraction time are complete and bounded;
- the underlying source/evidence registry accepts chronology, uniqueness, and capacity.

Submission creates candidate evidence only. It does not promote evidence into the catalog. Promotion, governance, validation, permissions, persistence, and publication remain owned by the existing Foundation services.

## Explicit non-authority

The ExtensionAPI cannot:

- mutate catalog records or relationships;
- bypass governance or validation;
- register source documents;
- access private workspace/game paths through profile queries;
- deploy files or launch FoA;
- execute BepInEx or Harmony code;
- alter saves;
- sign, upload, or publish releases.

## Verification

Production-linked compiled tests cover deterministic registration, duplicate refusal, capability gating, sanitized profiles, exact branch/version compatibility, bounded copy-only catalog queries, exact-source candidate evidence, and unknown enum rejection.
