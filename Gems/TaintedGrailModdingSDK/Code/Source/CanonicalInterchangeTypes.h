/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::Interchange
{
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

#define TG_INTERCHANGE_ID_WRAPPER(TypeName, MemberName) \
    struct TypeName { AZStd::string MemberName; bool operator==(const TypeName&) const = default; bool operator<(const TypeName& rhs) const { return MemberName < rhs.MemberName; } }
    TG_INTERCHANGE_ID_WRAPPER(PackageIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(AssetIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(RevisionFingerprintV1, m_sha256);
    TG_INTERCHANGE_ID_WRAPPER(BindingIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(MappingIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(Sha256DigestV1, m_hex);
    TG_INTERCHANGE_ID_WRAPPER(DocumentIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(PayloadIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(TransformationIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(LossIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(ProvenanceIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(LicensingIdV1, m_value);
    TG_INTERCHANGE_ID_WRAPPER(EvidenceReferenceIdV1, m_value);
#undef TG_INTERCHANGE_ID_WRAPPER

    bool IsSemanticIdV1(AZStd::string_view value);
    bool IsExactVersionTokenV1(AZStd::string_view value);
    bool IsSha256HexV1(AZStd::string_view value);
    bool IsRelativePackagePathV1(AZStd::string_view value);

    enum class PackageKindV1 : AZ::u8 { AuthoringInterchange };
    enum class HostKindV1 : AZ::u8 { O3de, Blender, UnityEditor, HostNeutral };
    enum class BindingStateV1 : AZ::u8 { Unresolved, Active, Stale, Conflicted, Superseded, Retired };
    enum class MappingOperationV1 : AZ::u8 { Rename, Move, Duplicate, Fork, Replace, Merge, Split, Aggregate, Tombstone };
    enum class MappingCompletenessV1 : AZ::u8 { Incomplete, Complete };
    enum class PayloadRoleV1 : AZ::u8 { CanonicalDocument, SceneSource, MeshSource, TextureSource, MaterialMap, SkeletonSource, AnimationSource, ColliderSource, Metadata, Preview, LicenceNotice };
    enum class TransformationPhaseV1 : AZ::u8 { Authoring, Export, Import, Normalization, ReverseExport, Migration };
    enum class ReversibilityV1 : AZ::u8 { Reversible, ConditionallyReversible, Irreversible, Unknown };
    enum class LossSeverityV1 : AZ::u8 { Information, Warning, Error, Blocker };
    enum class SourceKindV1 : AZ::u8 { ProjectOwned, Synthetic, UserSuppliedReviewed, UserSuppliedUnreviewed, LocalReferenceOnly, UpstreamReference, Prohibited };
    enum class RedistributionStateV1 : AZ::u8 { Allowed, LocalOnly, ReviewRequired, Blocked };
    enum class IssueSeverityV1 : AZ::u8 { Information, Warning, Error, Blocker };
    enum class MigrationStatusV1 : AZ::u8 { AlreadyCurrent, SourceInvalid, UnsupportedSourceVersion, UnsupportedTargetVersion, DowngradeForbidden, MigratorUnavailable, SemanticDrift, Succeeded };
    enum class ParameterKindV1 : AZ::u8 { Boolean, SignedInteger, UnsignedInteger, Number, String, Matrix4x4, RationalRate };

    AZStd::string_view ToToken(PackageKindV1 value); AZStd::string_view ToToken(HostKindV1 value); AZStd::string_view ToToken(BindingStateV1 value); AZStd::string_view ToToken(MappingOperationV1 value); AZStd::string_view ToToken(MappingCompletenessV1 value); AZStd::string_view ToToken(PayloadRoleV1 value); AZStd::string_view ToToken(TransformationPhaseV1 value); AZStd::string_view ToToken(ReversibilityV1 value); AZStd::string_view ToToken(LossSeverityV1 value); AZStd::string_view ToToken(SourceKindV1 value); AZStd::string_view ToToken(RedistributionStateV1 value); AZStd::string_view ToToken(IssueSeverityV1 value); AZStd::string_view ToToken(MigrationStatusV1 value); AZStd::string_view ToToken(ParameterKindV1 value);
    bool TryParseToken(AZStd::string_view token, PackageKindV1& value); bool TryParseToken(AZStd::string_view token, HostKindV1& value); bool TryParseToken(AZStd::string_view token, BindingStateV1& value); bool TryParseToken(AZStd::string_view token, MappingOperationV1& value); bool TryParseToken(AZStd::string_view token, MappingCompletenessV1& value); bool TryParseToken(AZStd::string_view token, PayloadRoleV1& value); bool TryParseToken(AZStd::string_view token, TransformationPhaseV1& value); bool TryParseToken(AZStd::string_view token, ReversibilityV1& value); bool TryParseToken(AZStd::string_view token, LossSeverityV1& value); bool TryParseToken(AZStd::string_view token, SourceKindV1& value); bool TryParseToken(AZStd::string_view token, RedistributionStateV1& value); bool TryParseToken(AZStd::string_view token, IssueSeverityV1& value); bool TryParseToken(AZStd::string_view token, MigrationStatusV1& value); bool TryParseToken(AZStd::string_view token, ParameterKindV1& value);

    struct CanonicalSpatialBasisV1 { AZStd::string m_handedness = "right-handed"; AZStd::string m_rightAxis = "+x"; AZStd::string m_forwardAxis = "+y"; AZStd::string m_upAxis = "+z"; AZStd::string m_linearUnit = "metre"; AZStd::string m_angularUnit = "radian"; AZStd::string m_vectorConvention = "column-vector"; AZStd::string m_multiplicationConvention = "matrix-on-left"; AZStd::string m_storageOrder = "column-major"; };
    struct CanonicalTemporalBasisV1 { AZStd::string m_timeUnit = "second"; };
    struct Matrix4x4V1 { AZStd::array<double, 16> m_columnMajorValues{}; AZStd::string m_mappingDirection; };
    struct RationalRateV1 { AZ::u64 m_numerator = 0; AZ::u64 m_denominator = 1; };
    struct NumericToleranceV1 { AZStd::string m_subjectPropertyPath; double m_absolute = 0.0; double m_relative = 0.0; double m_angularRadians = 0.0; AZStd::string m_comparisonNorm; AZStd::string m_unit; AZStd::string m_scope; };
    struct ProducerV1 { HostKindV1 m_hostKind = HostKindV1::HostNeutral; AZStd::string m_applicationId; AZStd::string m_applicationVersion; AZStd::string m_providerId; AZStd::string m_providerVersion; AZStd::string m_extensionId; AZStd::string m_extensionVersion; AZStd::string m_extensionRevision; Sha256DigestV1 m_extensionDigest; Sha256DigestV1 m_configurationFingerprint; AZStd::string m_workspaceIdObservation; AZStd::string m_profileIdObservation; };
    struct ConsumerConstraintV1 { HostKindV1 m_hostKind = HostKindV1::HostNeutral; AZStd::string m_consumerId; AZStd::string m_consumerVersion; AZStd::string m_profileId; bool m_required = false; };
    struct ToolchainComponentV1 { AZStd::string m_componentKind; AZStd::string m_componentId; AZStd::string m_version; AZStd::string m_revision; Sha256DigestV1 m_digest; };
    struct ToolchainLockV1 { AZStd::string m_foaSdkCommit; AZStd::string m_schemaProfile = AZStd::string(SchemaProfileV1); Sha256DigestV1 m_configurationFingerprint; AZStd::string m_fixtureRevision; AZStd::vector<ToolchainComponentV1> m_components; };
    struct DomainDocumentRecordV1 { DocumentIdV1 m_documentId; AZStd::string m_documentKind; AZStd::string m_documentSchemaId; AZ::u32 m_documentSchemaVersion = 1; PayloadIdV1 m_payloadId; RevisionFingerprintV1 m_revisionFingerprint; AZStd::vector<AZStd::string> m_subjectReferences; AZStd::vector<AssetIdV1> m_assetIds; AZStd::vector<ProvenanceIdV1> m_provenanceIds; AZStd::vector<LicensingIdV1> m_licensingIds; };
    struct AssetRecordV1 { AssetIdV1 m_assetId; AZStd::string m_assetKind; RevisionFingerprintV1 m_revisionFingerprint; AZStd::vector<PayloadIdV1> m_payloadIds; AZStd::vector<DocumentIdV1> m_documentIds; AZStd::vector<AZStd::string> m_subjectReferences; AZStd::vector<ProvenanceIdV1> m_provenanceIds; AZStd::vector<LicensingIdV1> m_licensingIds; };
    struct IdentityMappingRecordV1 { MappingIdV1 m_mappingId; MappingOperationV1 m_operation = MappingOperationV1::Rename; AZStd::vector<AssetIdV1> m_sourceAssetIds; AZStd::vector<AssetIdV1> m_targetAssetIds; AZStd::vector<RevisionFingerprintV1> m_sourceRevisions; AZStd::vector<RevisionFingerprintV1> m_targetRevisions; AZStd::vector<TransformationIdV1> m_transformationIds; AZStd::vector<LossIdV1> m_lossIds; AZStd::vector<ProvenanceIdV1> m_provenanceIds; AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds; MappingCompletenessV1 m_completeness = MappingCompletenessV1::Incomplete; };
    struct NativeBindingRecordV1 { BindingIdV1 m_bindingId; AZStd::string m_subjectId; HostKindV1 m_hostKind = HostKindV1::HostNeutral; AZStd::string m_consumerProfile; AZStd::string m_nativeIdKind; AZStd::string m_nativeIdValue; AZStd::string m_nativePathObservation; BindingStateV1 m_state = BindingStateV1::Unresolved; BindingIdV1 m_supersedesBindingId; bool m_required = false; };
    struct PayloadRecordV1 { PayloadIdV1 m_payloadId; AZStd::string m_relativePath; PayloadRoleV1 m_role = PayloadRoleV1::Metadata; AZStd::string m_mediaType; AZ::u64 m_byteSize = 0; Sha256DigestV1 m_digest; AZStd::vector<PayloadIdV1> m_dependencyIds; AZStd::vector<AZStd::string> m_subjectIds; AZStd::vector<ProvenanceIdV1> m_provenanceIds; AZStd::vector<LicensingIdV1> m_licensingIds; };
    struct CanonicalParameterV1 { AZStd::string m_name; ParameterKindV1 m_kind = ParameterKindV1::String; bool m_booleanValue = false; AZ::s64 m_signedValue = 0; AZ::u64 m_unsignedValue = 0; double m_numberValue = 0.0; AZStd::string m_stringValue; Matrix4x4V1 m_matrixValue; RationalRateV1 m_rateValue; };
    struct TransformationRecordV1 { TransformationIdV1 m_transformationId; AZ::u32 m_sequence = 0; TransformationPhaseV1 m_phase = TransformationPhaseV1::Authoring; AZStd::string m_operationCode; AZStd::vector<AZStd::string> m_subjectIds; AZStd::vector<AZStd::string> m_propertyPaths; AZStd::string m_sourceBasisOrProfile; AZStd::string m_destinationBasisOrProfile; AZStd::vector<CanonicalParameterV1> m_parameters; ReversibilityV1 m_reversibility = ReversibilityV1::Unknown; AZStd::vector<NumericToleranceV1> m_tolerances; AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds; };
    struct LossRecordV1 { LossIdV1 m_lossId; AZ::u32 m_sequence = 0; AZStd::string m_subjectId; AZStd::string m_propertyPath; AZStd::string m_sourceProfile; AZStd::string m_destinationProfile; TransformationPhaseV1 m_phase = TransformationPhaseV1::Authoring; AZStd::string m_reasonCode; LossSeverityV1 m_severity = LossSeverityV1::Information; AZStd::string m_sourceFeature; AZStd::string m_resultRepresentation; ReversibilityV1 m_reversibility = ReversibilityV1::Unknown; AZStd::vector<NumericToleranceV1> m_tolerances; AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds; };
    struct ProvenanceRecordV1 { ProvenanceIdV1 m_provenanceId; AZStd::string m_subjectId; SourceKindV1 m_sourceKind = SourceKindV1::Synthetic; AZStd::string m_sourceIdentity; AZStd::string m_sourceRevision; Sha256DigestV1 m_sourceDigest; AZStd::string m_authorship; AZStd::string m_modificationState; AZStd::string m_attribution; AZStd::string m_permittedUse; AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds; };
    struct LicensingRecordV1 { LicensingIdV1 m_licensingId; AZStd::string m_subjectId; AZStd::string m_spdxExpressionOrLicenseRef; RedistributionStateV1 m_redistributionState = RedistributionStateV1::ReviewRequired; AZStd::vector<AZStd::string> m_requiredNotices; AZStd::string m_reviewer; AZStd::string m_decisionAtUtc; AZStd::vector<EvidenceReferenceIdV1> m_evidenceReferenceIds; };
    struct EvidenceReferenceV1 { EvidenceReferenceIdV1 m_evidenceReferenceId; AZStd::string m_evidenceKind; AZStd::string m_subjectId; AZStd::string m_externalEvidenceId; Sha256DigestV1 m_externalEvidenceFingerprint; };
    struct ExtensionRecordV1 { AZStd::string m_namespace; AZ::u32 m_schemaVersion = 1; AZStd::string m_canonicalJson; Sha256DigestV1 m_digest; };
    struct CanonicalInterchangeManifestV1 { AZ::u32 m_schemaVersion = SchemaVersionV1; AZStd::string m_schemaProfile = AZStd::string(SchemaProfileV1); AZStd::string m_canonicalProfile = AZStd::string(CanonicalProfileV1); PackageKindV1 m_packageKind = PackageKindV1::AuthoringInterchange; PackageIdV1 m_packageId; AZStd::string m_packId; AZStd::string m_displayName; ProducerV1 m_producer; AZStd::vector<ConsumerConstraintV1> m_intendedConsumers; ToolchainLockV1 m_toolchainLock; CanonicalSpatialBasisV1 m_spatialBasis; CanonicalTemporalBasisV1 m_temporalBasis; AZStd::vector<DomainDocumentRecordV1> m_documents; AZStd::vector<AssetRecordV1> m_assets; AZStd::vector<IdentityMappingRecordV1> m_identityMappings; AZStd::vector<NativeBindingRecordV1> m_bindings; AZStd::vector<PayloadRecordV1> m_payloads; AZStd::vector<TransformationRecordV1> m_transformations; AZStd::vector<LossRecordV1> m_losses; AZStd::vector<ProvenanceRecordV1> m_provenance; AZStd::vector<LicensingRecordV1> m_licensing; AZStd::vector<EvidenceReferenceV1> m_evidenceReferences; AZStd::vector<ExtensionRecordV1> m_extensions; };
} // namespace TaintedGrailModdingSDK::Interchange
