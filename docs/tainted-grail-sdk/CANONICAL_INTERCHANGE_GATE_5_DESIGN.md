# Canonical Interchange Gate 5 Contract Design

Status: proposed focused design; pending explicit maintainer acceptance; no implementation authority

Repository baseline: `eb840862c9d3e239dec91770495c7669c00d10df` (`main`), observed 22 July 2026

Parent research conclusion:
`../../Research/o3de-to-unity-conversion-and-runtime-bridge/areas/02-authoring-interchange-and-assets/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`

Controlling gate map:
`../../Research/o3de-to-unity-conversion-and-runtime-bridge/gates/IMPLEMENTATION_GATE_MAPPING.md`

## Decision

The first Gate 5 implementation unit is one inert Core-only contract package. It contains typed Schema-1 value
objects, a dedicated canonical JSON reader/writer, pure intrinsic validation, pure migration dispatch, public
structural schema documents, and synthetic compiled fixtures.

It introduces no service, registry, persistence, filesystem access, environment access, clock access, network,
process, provider, Blender, Unity, Asset Processor, runtime adapter, deployment, save, signing, publication, or
evidence-promotion path.

Implementation remains blocked until this design and the separate Phase 9/Gate 5 entry decision are explicitly
accepted by the maintainer.

## Exact ownership and dependency graph

```text
Gem::TaintedGrailModdingSDK.Core.Static
    ├── CanonicalInterchangeTypes
    ├── CanonicalInterchangeCanonical
    │     ├── CanonicalInterchangeTypes
    │     ├── CanonicalFingerprint
    │     └── selected primitives from DeterministicContractJson
    ├── CanonicalInterchangeValidation
    │     ├── CanonicalInterchangeTypes
    │     └── CanonicalInterchangeCanonical
    └── CanonicalInterchangeMigration
          ├── CanonicalInterchangeTypes
          ├── CanonicalInterchangeCanonical
          └── CanonicalInterchangeValidation

Gem::TaintedGrailModdingSDK.CanonicalInterchange.Tests
    ├── Gem::TaintedGrailModdingSDK.Core.Static
    └── AZ::AzTest
```

Core remains dependent only on `AZ::AzCore`. The new test target must not link Framework, Editor,
`AZ::AzToolsFramework`, Qt, ExternalToolchain, provider packages, or runtime adapters.

The implementation must not include `FoundationModels.h`, `SourceEvidenceRegistry.h`, `CatalogDatabase.h`,
Framework services, or Editor headers. Existing workspace, pack, source, evidence, permission, and catalogue
systems remain authoritative in their current owners; the interchange contract carries exact references only.

## Source ownership

The first implementation unit owns exactly four Core translation-unit pairs:

```text
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeCanonical.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeCanonical.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeValidation.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeValidation.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeMigration.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeMigration.cpp
```

All public declarations use:

```cpp
namespace TaintedGrailModdingSDK::Interchange
```

The headers are public only through the existing `Core.Static` `Source` include directory. No BehaviorContext,
SerializeContext, EBus, interface singleton, service locator, registry, component, or Editor reflection is added in
the first slice. Reflection is deliberately absent because no service consumes these values yet.

## Core constants and limits

Schema 1 is closed and bounded. These constants are public contract values:

```cpp
inline constexpr AZ::u32 SchemaVersionV1 = 1;
inline constexpr AZStd::string_view SchemaProfileV1 = "foa-interchange-schema-v1";
inline constexpr AZStd::string_view CanonicalProfileV1 = "foa-interchange-canonical-json-v1";
inline constexpr size_t MaxCanonicalManifestBytesV1 = 8 * 1024 * 1024;
inline constexpr size_t MaxCanonicalDepthV1 = 32;
inline constexpr size_t MaxSemanticTokenBytesV1 = 128;
inline constexpr size_t MaxRelativePathBytesV1 = 512;
inline constexpr size_t MaxPresentationStringBytesV1 = 4096;
inline constexpr size_t MaxSourceIdentityBytesV1 = 2048;
inline constexpr size_t MaxExtensionCanonicalBytesV1 = 1024 * 1024;
inline constexpr size_t MaxConsumersV1 = 64;
inline constexpr size_t MaxToolchainComponentsV1 = 128;
inline constexpr size_t MaxDocumentsV1 = 4096;
inline constexpr size_t MaxAssetsV1 = 4096;
inline constexpr size_t MaxIdentityMappingsV1 = 8192;
inline constexpr size_t MaxBindingsV1 = 16384;
inline constexpr size_t MaxPayloadsV1 = 8192;
inline constexpr size_t MaxTransformationsV1 = 16384;
inline constexpr size_t MaxLossesV1 = 16384;
inline constexpr size_t MaxProvenanceRecordsV1 = 8192;
inline constexpr size_t MaxLicensingRecordsV1 = 8192;
inline constexpr size_t MaxEvidenceReferencesV1 = 8192;
inline constexpr size_t MaxExtensionNamespacesV1 = 64;
inline constexpr size_t MaxReferencesPerRecordV1 = 256;
inline constexpr AZ::u64 MaxPayloadBytesV1 = 16ULL * 1024 * 1024 * 1024;
inline constexpr AZ::u64 MaxDeclaredPackageBytesV1 = 256ULL * 1024 * 1024 * 1024;
```

These are memory, validation, and review limits for Schema 1, not claims about Blender, Unity, O3DE, or FoA
capacity. A later profile must version forward rather than silently raising them.

## Canonical token and path grammar

Every fingerprint-participating ID uses the same basic grammar:

- 3 to 128 ASCII bytes;
- first byte `a` through `z`;
- subsequent bytes `a-z`, `0-9`, `.`, `_`, or `-`;
- at least one `.` namespace separator;
- no consecutive separators;
- no trailing separator;
- no whitespace, control character, slash, backslash, colon, or non-ASCII byte.

Exact-version tokens may omit the namespace separator but use the same ASCII and length restrictions.

Relative package paths:

- are ASCII and use `/` only;
- are 1 to 512 bytes;
- are not absolute and have no drive, UNC, URI, or device prefix;
- contain no empty, `.` or `..` segment;
- contain no control byte, colon, backslash, trailing dot, or trailing space;
- reject Windows reserved device names case-insensitively;
- are unique by exact bytes and by a lowercase Windows-equivalence key.

## Primary value types

The five primary identity wrappers are explicit structs, not aliases:

```cpp
struct PackageIdV1 { AZStd::string m_value; };
struct AssetIdV1 { AZStd::string m_value; };
struct RevisionFingerprintV1 { AZStd::string m_sha256; };
struct BindingIdV1 { AZStd::string m_value; };
struct MappingIdV1 { AZStd::string m_value; };
```

`RevisionFingerprintV1::m_sha256` is exactly 64 lowercase hexadecimal characters. The schema records the
algorithm separately as `sha256`; the value never contains a `sha256:` prefix.

Additional exact wrappers:

```cpp
struct Sha256DigestV1 { AZStd::string m_hex; };
struct DocumentIdV1 { AZStd::string m_value; };
struct PayloadIdV1 { AZStd::string m_value; };
struct TransformationIdV1 { AZStd::string m_value; };
struct LossIdV1 { AZStd::string m_value; };
struct ProvenanceIdV1 { AZStd::string m_value; };
struct LicensingIdV1 { AZStd::string m_value; };
struct EvidenceReferenceIdV1 { AZStd::string m_value; };
```

Every wrapper has equality and ordering operators plus a pure `Is...` grammar function. IDs are caller supplied;
Core never creates random IDs, consults a clock, or derives identity from a path, display name, native ID, or host
order.

## Enumerations

The first slice defines closed `enum class` values with exact lowercase token conversion and fail-closed parsing:

```text
PackageKindV1: authoring_interchange
HostKindV1: o3de, blender, unity_editor, host_neutral
BindingStateV1: unresolved, active, stale, conflicted, superseded, retired
MappingOperationV1: rename, move, duplicate, fork, replace, merge, split, aggregate, tombstone
MappingCompletenessV1: incomplete, complete
PayloadRoleV1: canonical_document, scene_source, mesh_source, texture_source, material_map,
               skeleton_source, animation_source, collider_source, metadata, preview, licence_notice
TransformationPhaseV1: authoring, export, import, normalization, reverse_export, migration
ReversibilityV1: reversible, conditionally_reversible, irreversible, unknown
LossSeverityV1: information, warning, error, blocker
SourceKindV1: project_owned, synthetic, user_supplied_reviewed, user_supplied_unreviewed,
              local_reference_only, upstream_reference, prohibited
RedistributionStateV1: allowed, local_only, review_required, blocked
IssueSeverityV1: information, warning, error, blocker
MigrationStatusV1: already_current, source_invalid, unsupported_source_version,
                   unsupported_target_version, downgrade_forbidden, migrator_unavailable,
                   semantic_drift, succeeded
```

Unknown enum tokens fail; they are never mapped to an `Unknown` value.

## Complete C++ contract inventory

### Fixed basis records

```cpp
struct CanonicalSpatialBasisV1
{
    AZStd::string m_handedness = "right_handed";
    AZStd::string m_rightAxis = "+x";
    AZStd::string m_forwardAxis = "+y";
    AZStd::string m_upAxis = "+z";
    AZStd::string m_linearUnit = "metre";
    AZStd::string m_angularUnit = "radian";
    AZStd::string m_vectorConvention = "column_vector";
    AZStd::string m_multiplicationConvention = "matrix_on_left";
    AZStd::string m_storageOrder = "column_major";
};

struct CanonicalTemporalBasisV1
{
    AZStd::string m_timeUnit = "second";
};

struct Matrix4x4V1
{
    AZStd::array<double, 16> m_columnMajorValues{};
    AZStd::string m_mappingDirection;
};

struct RationalRateV1
{
    AZ::u64 m_numerator = 0;
    AZ::u64 m_denominator = 1;
};

struct NumericToleranceV1
{
    AZStd::string m_subjectPropertyPath;
    double m_absolute = 0.0;
    double m_relative = 0.0;
    double m_angularRadians = 0.0;
    AZStd::string m_comparisonNorm;
    AZStd::string m_unit;
    AZStd::string m_scope;
};
```

All 16 matrix values and all tolerance values must be finite. Negative zero is normalized before serialization.
The fixed canonical basis fields must contain exactly the defaults above.

### Producer, consumer, and toolchain records

```cpp
struct ProducerV1
{
    HostKindV1 m_hostKind = HostKindV1::HostNeutral;
    AZStd::string m_applicationId;
    AZStd::string m_applicationVersion;
    AZStd::string m_providerId;
    AZStd::string m_providerVersion;
    AZStd::string m_extensionId;
    AZStd::string m_extensionVersion;
    AZStd::string m_extensionRevision;
    Sha256DigestV1 m_extensionDigest;
    Sha256DigestV1 m_configurationFingerprint;
    AZStd::string m_workspaceIdObservation;
    AZStd::string m_profileIdObservation;
};

struct ConsumerConstraintV1
{
    HostKindV1 m_hostKind = HostKindV1::HostNeutral;
    AZStd::string m_consumerId;
    AZStd::string m_consumerVersion;
    AZStd::string m_profileId;
    bool m_required = false;
};

struct ToolchainComponentV1
{
    AZStd::string m_componentKind;
    AZStd::string m_componentId;
    AZStd::string m_version;
    AZStd::string m_revision;
    Sha256DigestV1 m_digest;
};

struct ToolchainLockV1
{
    AZStd::string m_foaSdkCommit;
    AZStd::string m_schemaProfile = AZStd::string(SchemaProfileV1);
    Sha256DigestV1 m_configurationFingerprint;
    AZStd::string m_fixtureRevision;
    AZStd::vector<ToolchainComponentV1> m_components;
};
```

Empty optional provider/extension fields are permitted only when `host_kind` is `host_neutral` or the selected
producer profile states no provider or extension participated. Workspace and profile values are observations,
not portable identity.

### Documents and assets

```cpp
struct DomainDocumentRecordV1
{
    DocumentIdV1 m_documentId;
    AZStd::string m_documentKind;
    AZStd::string m_documentSchemaId;
    AZ::u32 m_documentSchemaVersion = 1;
    PayloadIdV1 m_payloadId;
    RevisionFingerprintV1 m_revisionFingerprint;
    AZStd::vector<AZStd::string> m_subjectReferences;
    AZStd::vector<AssetIdV1> m_assetIds;
    AZStd::vector<ProvenanceIdV1> m_provenanceIds;
    AZStd::vector<LicensingIdV1> m_licensingIds;
};

struct AssetRecordV1
{
    AssetIdV1 m_assetId;
    AZStd::string m_assetKind;
    RevisionFingerprintV1 m_revisionFingerprint;
    AZStd::vector<PayloadIdV1> m_payloadIds;
    AZStd::vector<DocumentIdV1> m_documentIds;
    AZStd::vector<AZStd::string> m_subjectReferences;
    AZStd::vector<ProvenanceIdV1> m_provenanceIds;
    AZStd::vector<LicensingIdV1> m_licensingIds;
};
```

The manifest inventories domain records but does not duplicate Road Atlas, Avalon AI, actor, troop,
relationship, governance, or future document fields.

### Identity mapping and native binding

```cpp
struct IdentityMappingRecordV1
{
    MappingIdV1 m_mappingId;
    MappingOperationV1 m_operation = MappingOperationV1::Rename;
    AZStd::vector<AssetIdV1> m_sourceAssetIds;
    AZStd::vector<AssetIdV1> m_targetAssetIds;
    AZStd::vector<RevisionFingerprintV1> m_sourceRevisions;
    AZStd::vector<RevisionFingerprintV1> m_targetRevisions;
    AZStd::vector<TransformationIdV1> m_transformationIds;
    AZStd::vector<LossIdV1> m_lossIds;
    AZStd::vector<ProvenanceIdV1> m_provenanceIds;
    AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds;
    MappingCompletenessV1 m_completeness = MappingCompletenessV1::Incomplete;
};

struct NativeBindingRecordV1
{
    BindingIdV1 m_bindingId;
    AZStd::string m_subjectId;
    HostKindV1 m_hostKind = HostKindV1::HostNeutral;
    AZStd::string m_consumerProfile;
    AZStd::string m_nativeIdKind;
    AZStd::string m_nativeIdValue;
    AZStd::string m_nativePathObservation;
    BindingStateV1 m_state = BindingStateV1::Unresolved;
    BindingIdV1 m_supersedesBindingId;
    bool m_required = false;
};
```

A binding never supplies canonical identity. Active bindings for the same subject, host, consumer profile, and
native-ID kind must be unique. Supersession chains must be acyclic and reference an existing older binding.

### Payload inventory

```cpp
struct PayloadRecordV1
{
    PayloadIdV1 m_payloadId;
    AZStd::string m_relativePath;
    PayloadRoleV1 m_role = PayloadRoleV1::Metadata;
    AZStd::string m_mediaType;
    AZ::u64 m_byteSize = 0;
    Sha256DigestV1 m_digest;
    AZStd::vector<PayloadIdV1> m_dependencyIds;
    AZStd::vector<AZStd::string> m_subjectIds;
    AZStd::vector<ProvenanceIdV1> m_provenanceIds;
    AZStd::vector<LicensingIdV1> m_licensingIds;
};
```

Executable payload media types and roles are forbidden. Dependency cycles and references to absent payload IDs
fail. Core validates declared paths, sizes, digests, references, and cycles; it does not open or hash files.

### Typed canonical transformation parameters

```cpp
enum class ParameterKindV1 : AZ::u8
{
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Number,
    String,
    Matrix4x4,
    RationalRate,
};

struct CanonicalParameterV1
{
    AZStd::string m_name;
    ParameterKindV1 m_kind = ParameterKindV1::String;
    bool m_booleanValue = false;
    AZ::s64 m_signedValue = 0;
    AZ::u64 m_unsignedValue = 0;
    double m_numberValue = 0.0;
    AZStd::string m_stringValue;
    Matrix4x4V1 m_matrixValue;
    RationalRateV1 m_rateValue;
};

struct TransformationRecordV1
{
    TransformationIdV1 m_transformationId;
    AZ::u32 m_sequence = 0;
    TransformationPhaseV1 m_phase = TransformationPhaseV1::Authoring;
    AZStd::string m_operationCode;
    AZStd::vector<AZStd::string> m_subjectIds;
    AZStd::vector<AZStd::string> m_propertyPaths;
    AZStd::string m_sourceBasisOrProfile;
    AZStd::string m_destinationBasisOrProfile;
    AZStd::vector<CanonicalParameterV1> m_parameters;
    ReversibilityV1 m_reversibility = ReversibilityV1::Unknown;
    AZStd::vector<NumericToleranceV1> m_tolerances;
    AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds;
};
```

The validator requires unused members of `CanonicalParameterV1` to remain at their defaults. Parameter names are
unique and serialized in sorted name order. Transformations remain in exact sequence order.

### Loss, provenance, licensing, and evidence references

```cpp
struct LossRecordV1
{
    LossIdV1 m_lossId;
    AZ::u32 m_sequence = 0;
    AZStd::string m_subjectId;
    AZStd::string m_propertyPath;
    AZStd::string m_sourceProfile;
    AZStd::string m_destinationProfile;
    TransformationPhaseV1 m_phase = TransformationPhaseV1::Authoring;
    AZStd::string m_reasonCode;
    LossSeverityV1 m_severity = LossSeverityV1::Information;
    AZStd::string m_sourceFeature;
    AZStd::string m_resultRepresentation;
    ReversibilityV1 m_reversibility = ReversibilityV1::Unknown;
    AZStd::vector<NumericToleranceV1> m_tolerances;
    AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds;
};

struct ProvenanceRecordV1
{
    ProvenanceIdV1 m_provenanceId;
    AZStd::string m_subjectId;
    SourceKindV1 m_sourceKind = SourceKindV1::Synthetic;
    AZStd::string m_sourceIdentity;
    AZStd::string m_sourceRevision;
    Sha256DigestV1 m_sourceDigest;
    AZStd::string m_authorship;
    AZStd::string m_modificationState;
    AZStd::string m_attribution;
    AZStd::string m_permittedUse;
    AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds;
};

struct LicensingRecordV1
{
    LicensingIdV1 m_licensingId;
    AZStd::string m_subjectId;
    AZStd::string m_spdxExpressionOrLicenseRef;
    RedistributionStateV1 m_redistributionState = RedistributionStateV1::ReviewRequired;
    AZStd::vector<AZStd::string> m_requiredNotices;
    AZStd::string m_reviewer;
    AZStd::string m_decisionAtUtc;
    AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds;
};

struct EvidenceReferenceV1
{
    EvidenceReferenceIdV1 m_evidenceReferenceId;
    AZStd::string m_evidenceKind;
    AZStd::string m_subjectId;
    AZStd::string m_externalEvidenceId;
    Sha256DigestV1 m_externalEvidenceFingerprint;
};
```

Evidence references identify Framework-owned evidence; they do not contain mutable evidence bodies and cannot
promote evidence.

### Extension envelope

```cpp
struct ExtensionRecordV1
{
    AZStd::string m_namespace;
    AZ::u32 m_schemaVersion = 1;
    AZStd::string m_canonicalJson;
    Sha256DigestV1 m_digest;
};
```

The initial admitted namespace set is empty. Therefore every non-empty extension collection fails with
`interchange.extension.unapproved`. The envelope is defined now so a later reviewed namespace can version
forward without inventing a second extension mechanism.

### Manifest

```cpp
struct CanonicalInterchangeManifestV1
{
    AZ::u32 m_schemaVersion = SchemaVersionV1;
    AZStd::string m_schemaProfile = AZStd::string(SchemaProfileV1);
    AZStd::string m_canonicalProfile = AZStd::string(CanonicalProfileV1);
    PackageKindV1 m_packageKind = PackageKindV1::AuthoringInterchange;
    PackageIdV1 m_packageId;
    AZStd::string m_packId;
    AZStd::string m_displayName;
    ProducerV1 m_producer;
    AZStd::vector<ConsumerConstraintV1> m_intendedConsumers;
    ToolchainLockV1 m_toolchainLock;
    CanonicalSpatialBasisV1 m_spatialBasis;
    CanonicalTemporalBasisV1 m_temporalBasis;
    AZStd::vector<DomainDocumentRecordV1> m_documents;
    AZStd::vector<AssetRecordV1> m_assets;
    AZStd::vector<IdentityMappingRecordV1> m_identityMappings;
    AZStd::vector<NativeBindingRecordV1> m_bindings;
    AZStd::vector<PayloadRecordV1> m_payloads;
    AZStd::vector<TransformationRecordV1> m_transformations;
    AZStd::vector<LossRecordV1> m_losses;
    AZStd::vector<ProvenanceRecordV1> m_provenance;
    AZStd::vector<LicensingRecordV1> m_licensing;
    AZStd::vector<EvidenceReferenceV1> m_evidenceReferences;
    AZStd::vector<ExtensionRecordV1> m_extensions;
};
```

No package fingerprint, permission, authority, performed, path-to-game, command, environment, credential, or
runtime field exists in this struct.

## Canonical JSON API

`CanonicalInterchangeCanonical.h` exposes only pure functions:

```cpp
CanonicalInterchangeValidationResultV1 ParseCanonicalManifestV1(
    AZStd::string_view bytes,
    CanonicalInterchangeManifestV1& manifest);

AZStd::string SerializeCanonicalManifestV1(
    const CanonicalInterchangeManifestV1& manifest);

AZStd::string SerializeDeclaredPackageFingerprintInputV1(
    const CanonicalInterchangeManifestV1& manifest);

Sha256DigestV1 CalculateCanonicalManifestDigestV1(
    const CanonicalInterchangeManifestV1& manifest);

Sha256DigestV1 CalculateDeclaredPackageFingerprintV1(
    const CanonicalInterchangeManifestV1& manifest);

RevisionFingerprintV1 CalculateDocumentRevisionFingerprintV1(
    const CanonicalInterchangeManifestV1& manifest,
    const DomainDocumentRecordV1& document);

RevisionFingerprintV1 CalculateAssetRevisionFingerprintV1(
    const CanonicalInterchangeManifestV1& manifest,
    const AssetRecordV1& asset);

bool CanonicalManifestBytesMatchV1(
    AZStd::string_view bytes,
    const CanonicalInterchangeManifestV1& manifest);
```

The parser is pure and operates only on supplied bytes. It rejects BOMs, malformed UTF-8, duplicate keys,
unknown or forbidden fields, trailing content, excessive depth, excessive bytes, and unsupported profiles. It
must use the AzCore-provided RapidJSON surface rather than add a new JSON dependency.

The writer may reuse `DeterministicContractJson` for minimal integers, booleans, names, commas, and ASCII-only
semantic strings. It must provide dedicated implementations for:

- valid UTF-8 presentation strings, emitted without normalization and with only required JSON escaping;
- shortest round-trip finite doubles;
- negative-zero normalization;
- typed nested objects and exact property order;
- stable-key array sorting without mutating caller inputs.

`CalculateCanonicalSha256` remains the SHA-256 primitive. The interchange wrapper strips and validates the
existing `sha256:` prefix before constructing `Sha256DigestV1`.

### Exact top-level property order

```text
schema_version
schema_profile
canonical_profile
package_kind
package_id
pack_id
display_name
producer
intended_consumers
toolchain_lock
spatial_basis
temporal_basis
documents
assets
identity_mappings
bindings
payloads
transformations
losses
provenance
licensing
evidence_refs
extensions
```

Optional `display_name` is always emitted as a string; absence is represented by an empty string rather than
property omission. Required arrays are always emitted, including when empty. This keeps one byte shape.

Stable-key sorting uses temporary copies and never changes caller-owned vectors:

```text
intended_consumers: host_kind, consumer_id, consumer_version, profile_id
components: component_kind, component_id, version, revision, digest
documents: document_id
assets: asset_id
bindings: binding_id
payloads: payload_id
provenance: provenance_id
licensing: licensing_id
evidence_refs: evidence_reference_id
extensions: namespace, schema_version
```

`identity_mappings`, `transformations`, and `losses` retain their validated sequence order. Subject, dependency,
and reference vectors are sorted and deduplicated unless the record explicitly defines semantic order.

## Fingerprint rules

- Manifest digest: SHA-256 of exact canonical manifest bytes.
- Declared package fingerprint: SHA-256 of a canonical object containing the manifest digest plus payload tuples
  ordered by canonical relative path, each tuple containing path, byte size, media type, and digest.
- Document revision: SHA-256 of the document semantic projection, excluding presentation, evidence, native
  bindings, timestamps, and qualification results.
- Asset revision: SHA-256 of the asset semantic projection, including semantic payload digests, document revisions,
  transformations, losses, and admitted semantic extensions affecting that asset.
- Qualification fingerprint is not implemented in Core; Framework owns it later.

A fingerprint proves only deterministic identity/integrity of declared values. It grants no authenticity,
licensing, permission, compatibility, execution, or runtime authority.

## Pure validation API

```cpp
struct CanonicalInterchangeIssueV1
{
    IssueSeverityV1 m_severity = IssueSeverityV1::Error;
    AZStd::string m_code;
    AZStd::string m_subjectId;
    AZStd::string m_propertyPath;
    AZStd::vector<AZStd::string> m_relatedIds;
};

struct CanonicalInterchangeValidationResultV1
{
    AZStd::vector<CanonicalInterchangeIssueV1> m_issues;

    bool IsValid() const;
    bool IsBlocked() const;
};

CanonicalInterchangeValidationResultV1 ValidateCanonicalManifestV1(
    const CanonicalInterchangeManifestV1& manifest);
```

Validation does not mutate `manifest`. Equivalent invalid inputs produce byte-identical ordered issue records.
Human-readable explanations belong in documentation or UI lookup tables, not in the canonical issue identity.

### Issue precedence

Issues are ordered first by group, then code, subject ID, property path, and related IDs:

| Group | Precedence | Classes |
| --- | ---: | --- |
| schema and forbidden authority | 0 | unsupported version/profile, unknown/forbidden field |
| canonical representation | 1 | duplicate key, invalid UTF-8/token, non-finite, negative zero, order |
| identity | 2 | invalid/duplicate ID and revision |
| paths | 3 | absolute, traversal, case collision, Windows alias |
| references and dependencies | 4 | unresolved references, missing dependency, dependency cycle |
| mappings and bindings | 5 | mapping shape, required unresolved binding, conflict, supersession |
| transforms and losses | 6 | sequence, matrix, parameter, tolerance, incomplete/blocker loss |
| provenance, licensing, content | 7 | missing provenance, `NOASSERTION`, prohibited content |
| toolchain and extensions | 8 | incomplete lock, unapproved namespace, extension digest |
| declared fingerprints | 9 | revision mismatch and package-declaration mismatch |

The public issue codes are the accepted research codes. The Core validator emits only intrinsic codes. Framework-
owned codes such as symlink escape, actual file missing, observed size/digest mismatch, partial publication,
active tool-lock mismatch, and evidence-binding mismatch remain reserved but are not emitted by Core.

### Required blocker classification

These intrinsic codes are always blocker severity:

```text
interchange.schema.unsupported-version
interchange.schema.unsupported-profile
interchange.schema.forbidden-field
interchange.identity.mapping-invalid
interchange.binding.unresolved-required
interchange.binding.conflict
interchange.transform.matrix-invalid
interchange.loss.blocker
interchange.provenance.incomplete
interchange.licensing.noassertion-blocked
interchange.content.prohibited
interchange.lock.incomplete
interchange.extension.unapproved
interchange.migration.unavailable
interchange.migration.semantic-drift
```

Every other intrinsic invalidity is at least error severity and prevents `IsValid()`.

## Migration interface

`CanonicalInterchangeMigration.h` exposes a pure byte-to-byte API so future adjacent versions do not need a
mutable registry or filesystem service:

```cpp
struct MigrationStepV1
{
    AZ::u32 m_sourceVersion = 0;
    AZ::u32 m_targetVersion = 0;
    Sha256DigestV1 m_sourceDigest;
    Sha256DigestV1 m_candidateDigest;
    AZStd::vector<MappingIdV1> m_mappingIds;
    AZStd::vector<TransformationIdV1> m_transformationIds;
    AZStd::vector<LossIdV1> m_lossIds;
};

struct MigrationResultV1
{
    MigrationStatusV1 m_status = MigrationStatusV1::SourceInvalid;
    AZ::u32 m_sourceVersion = 0;
    AZ::u32 m_targetVersion = 0;
    AZStd::string m_candidateCanonicalJson;
    Sha256DigestV1 m_candidateDigest;
    AZStd::vector<MigrationStepV1> m_steps;
    CanonicalInterchangeValidationResultV1 m_validation;
    bool m_migrationPerformed = false;
};

MigrationResultV1 MigrateCanonicalInterchange(
    AZStd::string_view sourceCanonicalJson,
    AZ::u32 targetSchemaVersion);
```

Schema-1 implementation behavior is exact:

- valid `1 -> 1`: `already_current`, candidate bytes equal source canonical bytes, performed false;
- invalid source: `source_invalid`, no candidate;
- source version above the highest compiled version: `unsupported_source_version`, no candidate;
- target version below source: `downgrade_forbidden`, no candidate;
- target above the highest compiled version: `unsupported_target_version`, no candidate;
- `1 -> 2` before an accepted V2 migrator exists: `migrator_unavailable`, no candidate;
- no code path returns `succeeded` until a separately reviewed adjacent migrator is compiled.

Migration never overwrites or mutates source bytes. No timestamps are generated. A future migrator must return a
new candidate, explicit steps, mappings, transformations, and losses; target validation and semantic-drift checks
must pass before `succeeded` is possible.

## Public schema package

The public structural contract is hand-authored and reviewed under:

```text
docs/tainted-grail-sdk/schemas/interchange/v1/README.md
docs/tainted-grail-sdk/schemas/interchange/v1/manifest.tginterchange.schema.json
docs/tainted-grail-sdk/schemas/interchange/v1/CANONICALIZATION.md
docs/tainted-grail-sdk/schemas/interchange/v1/MIGRATION.md
docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-documents/manifest.tginterchange.json
docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-asset/manifest.tginterchange.json
```

The JSON Schema is normative for structural field names, types, required properties, closed objects, enums,
lengths, and numeric ranges. The C++ design is normative for canonical property order, sorting, semantic
projection, issue precedence, and migration behavior. A discrepancy is a release blocker; neither side silently
wins.

The first implementation adds no schema generator. A focused repository validator checks that the public schema,
examples, C++ constants, and required issue-code fragments remain present and version-aligned. Generated schemas
may be researched later but must not replace reviewed source without a separate design.

Third-party consumers receive documentation and validation contracts only. No ABI stability, provider support,
host compatibility, or runtime support is claimed by publication of Schema 1.

## Compiled fixture programme

A separate Core-only test target is required:

```text
Gem::TaintedGrailModdingSDK.CanonicalInterchange.Tests
```

Planned test files:

```text
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeTypesTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeCanonicalTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeValidationTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeMigrationTests.cpp
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake
```

The target links only Core.Static and AzTest and is registered independently from the existing Catalog.Tests
aggregate.

Positive fixtures cover:

- minimal documents-only package;
- minimal asset package;
- exact canonical golden bytes;
- canonical parse/serialize/parse equivalence;
- caller-vector non-mutation during sorting;
- revision fingerprint stability;
- rename preserving `AssetId`;
- semantic edit producing a new revision;
- duplicate, fork, replace, merge, split, aggregate, and tombstone mappings;
- active, unresolved, stale, conflicted, superseded, and retired bindings;
- exact identity matrix and explicit Unity-to-canonical matrix shape;
- UTF-8 presentation text without normalization;
- `NOASSERTION` local-only structure with publication outside Core remaining blocked;
- `1 -> 1` migration preserving exact canonical bytes.

Negative fixtures cover every accepted intrinsic code, including future schema, unknown fields, duplicate keys,
invalid UTF-8, invalid semantic tokens, negative zero verification, non-finite numbers, duplicate identities,
unsafe and colliding paths, missing/cyclic dependencies, incomplete mappings, required unresolved bindings,
binding conflicts, invalid supersession, transform sequence/matrix/parameter failures, incomplete and blocker losses,
missing provenance, `NOASSERTION` misuse, prohibited content, incomplete locks, unapproved extensions, revision
drift, downgrade, unsupported versions, and unavailable migrators.

Every fixture is synthetic and project-owned. No proprietary game file, Unity project, Blender file, executable,
DLL, runtime bundle, private path, credential, or captured user data is included.

## Repository validator plan

The first implementation adds:

```text
Gems/TaintedGrailModdingSDK/Tools/validate_canonical_interchange_schema.py
Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_canonical_interchange_schema.py
```

The validator uses only the Python standard library. It performs source-tree consistency checks; it does not act
as the runtime parser. It verifies:

- required public schema and example paths;
- schema/profile version alignment;
- closed top-level object and required-property inventory;
- all-false/absent operational authority vocabulary;
- required issue-code inventory;
- CMake registration of sources and tests;
- no Framework, Editor, Qt, process, filesystem, Blender, Unity, runtime, deployment, save, signing, or publication
  include or symbol appears in the new Core files;
- fixture files are below configured size limits and contain no prohibited path fragments.

It is added to local and PR non-compiled validation only after its focused tests pass.

## Exact proposed changed-file list for the implementation PR

New Core files:

```text
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeCanonical.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeCanonical.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeValidation.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeValidation.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeMigration.h
Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeMigration.cpp
```

Build/test registration:

```text
Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake
```

Compiled tests:

```text
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeTypesTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeCanonicalTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeValidationTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CanonicalInterchangeMigrationTests.cpp
```

Public schema and examples:

```text
docs/tainted-grail-sdk/schemas/interchange/v1/README.md
docs/tainted-grail-sdk/schemas/interchange/v1/manifest.tginterchange.schema.json
docs/tainted-grail-sdk/schemas/interchange/v1/CANONICALIZATION.md
docs/tainted-grail-sdk/schemas/interchange/v1/MIGRATION.md
docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-documents/manifest.tginterchange.json
docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-asset/manifest.tginterchange.json
```

Repository validation and documentation links:

```text
Gems/TaintedGrailModdingSDK/Tools/validate_canonical_interchange_schema.py
Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_canonical_interchange_schema.py
Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py
.github/workflows/tainted-grail-sdk-pr-validation.yml
docs/tainted-grail-sdk/README.md
Research/o3de-to-unity-conversion-and-runtime-bridge/areas/02-authoring-interchange-and-assets/README.md
```

No other file is in scope without a new review note in the implementation PR.

## Explicit authority matrix

The first implementation has no operational authority. These capabilities are absent from the public manifest and
permanently false in the unit:

| Capability | Gate 5 first slice |
| --- | --- |
| service registration | false |
| mutable registry | false |
| workspace read or write | false |
| filesystem read or write | false |
| package discovery or publication | false |
| environment or registry query | false |
| clock or random ID generation | false |
| network access | false |
| process launch or supervision | false |
| provider discovery or execution | false |
| Blender discovery, import, export, or execution | false |
| Unity discovery, project mutation, import, build, or execution | false |
| O3DE Asset Processor publication | false |
| game-install inspection | false |
| runtime-adapter build or call | false |
| deployment, backup, restore, or rollback execution | false |
| game launch | false |
| save read or mutation | false |
| evidence registration or promotion | false |
| signing or signature verification | false |
| archive assembly | false |
| upload or publication | false |
| compatibility certification | false |

Parsing supplied bytes, serializing caller-supplied values, computing hashes, validating in-memory values, and
returning migration results by value are pure computations and grant none of the capabilities above.

## Rollback and compatibility consequences

- The implementation adds new files and CMake registrations only; no persisted workspace or catalog schema changes.
- No existing public contract or serializer changes behavior.
- `DeterministicContractJson` remains unchanged unless a separately justified additive helper is proven safe for all
  existing callers; the preferred implementation keeps interchange-specific behavior in its own translation unit.
- Removing the Gate 5 implementation reverts source, tests, schema documents, and validation registration without
  migration or data rollback because no service persists Schema-1 documents.
- A later incompatible field or canonicalization change requires Schema 2. Schema 1 canonical bytes are immutable.
- Public examples are conformance fixtures, not host-compatibility claims.
- No Blender, Unity, O3DE round-trip, or FoA runtime compatibility follows from compiled Core tests.

## Acceptance checklist for the implementation PR

The first implementation PR is acceptable only when:

- [ ] exact base is current and drift is reconciled;
- [ ] explicit Phase 9/Gate 5 authority record is present;
- [ ] this design is explicitly accepted by the maintainer;
- [ ] changed files match the reviewed list or explain every addition;
- [ ] Core.Static retains only the AzCore dependency;
- [ ] the dedicated test target links Core.Static, not Framework.Static;
- [ ] all public types and limits match this design;
- [ ] no reflection, service, registry, persistence, filesystem, clock, network, process, provider, host, runtime, or
      publication surface exists;
- [ ] canonical golden bytes pass on hosted Linux and Windows compiled jobs;
- [ ] unusual UTF-8 presentation strings round-trip byte-for-byte without NFC normalization;
- [ ] non-finite values fail and negative zero normalizes deterministically;
- [ ] parsing rejects BOM, duplicate keys, unknown fields, invalid UTF-8, excess depth, and trailing content;
- [ ] every intrinsic invalid class emits the exact ordered public code;
- [ ] caller inputs remain unchanged after serialization, validation, fingerprinting, and migration;
- [ ] package and semantic fingerprints match golden values;
- [ ] `1 -> 1` migration preserves exact canonical bytes;
- [ ] unsupported, downgrade, and unavailable migration paths fail closed with no candidate;
- [ ] public JSON Schema and examples pass the repository validator;
- [ ] synthetic fixtures contain no proprietary or prohibited content;
- [ ] exact-head non-compiled validation passes;
- [ ] exact-head O3DE configure/build and the dedicated compiled test target pass;
- [ ] validation logs and applicable artifacts are preserved;
- [ ] the PR states that no host, round-trip, compatibility, runtime, deployment, or release claim was tested.

## Maintainer acceptance boundary

Acceptance must be explicit and recorded separately. Merging this design document does not by itself authorize
implementation. An accepted authority record must name:

- the exact repository base;
- Gate 5 as the authorized unit;
- the exact contract-only scope;
- the prohibited operational capabilities;
- the required tests and evidence;
- the fact that Gate 6 and later remain closed.

## Following focused unit

After the contract-only implementation passes and is accepted, the next unit is not Blender execution. It is the
Gate 5 package-byte conformance and fixture-publication review:

```text
compiled Schema-1 contracts
→ exact public schema/example conformance
→ independent canonical-byte verification
→ fixture publication and compatibility-claim boundary
→ Gate 6 Blender qualification design
```

Only after that review may Gate 6 propose an exact Blender version, native extension revision, isolated staging
model, command behavior, and synthetic import/export fixture programme.

## Permanent non-authority statement

This proposed design grants no implementation authority. It creates no service, persistence, filesystem,
provider, process, Blender or Unity operation, Asset Processor mutation, game access, runtime adapter, deployment,
launch, save access, evidence promotion, signing, archive, upload, publication, or compatibility claim.