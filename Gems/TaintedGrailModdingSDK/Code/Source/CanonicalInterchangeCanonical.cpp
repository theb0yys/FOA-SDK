/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeCanonical.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>

#include <charconv>
#include <cmath>
#include <system_error>

namespace TaintedGrailModdingSDK::Interchange
{
    namespace
    {
        bool IsContinuation(unsigned char value)
        {
            return (value & 0xc0) == 0x80;
        }

        void AppendControlEscape(AZStd::string& output, unsigned char value)
        {
            constexpr char Hex[] = "0123456789abcdef";
            output += "\\u00";
            output.push_back(Hex[(value >> 4) & 0x0f]);
            output.push_back(Hex[value & 0x0f]);
        }

        bool AppendCanonicalStringWithLimit(
            AZStd::string& output,
            AZStd::string_view value,
            size_t byteLimit)
        {
            if (value.size() > byteLimit || !IsValidUtf8V1(value))
            {
                return false;
            }

            output.push_back('"');
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                switch (character)
                {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (byte < 0x20)
                    {
                        AppendControlEscape(output, byte);
                    }
                    else
                    {
                        output.push_back(character);
                    }
                    break;
                }
            }
            output.push_back('"');
            return true;
        }

        void NormalizeExponent(AZStd::string& value)
        {
            const size_t exponent = value.find_first_of("eE");
            if (exponent == AZStd::string::npos)
            {
                return;
            }
            value[exponent] = 'e';
            size_t position = exponent + 1;
            if (position < value.size() && value[position] == '+')
            {
                value.erase(position, 1);
            }
            if (position < value.size() && value[position] == '-')
            {
                ++position;
            }
            while (position + 1 < value.size() && value[position] == '0')
            {
                value.erase(position, 1);
            }
        }

        bool AppendStringValue(AZStd::string& output, AZStd::string_view value)
        {
            return AppendCanonicalStringWithLimit(
                output, value, MaxPresentationStringBytesV1);
        }

        bool AppendExtensionStringValue(AZStd::string& output, AZStd::string_view value)
        {
            return AppendCanonicalStringWithLimit(
                output, value, MaxExtensionCanonicalBytesV1);
        }

        bool AppendUnsignedValue(AZStd::string& output, AZ::u64 value)
        {
            output += DeterministicContractJson::UnsignedString(value);
            return true;
        }

        bool AppendSignedValue(AZStd::string& output, AZ::s64 value)
        {
            output += DeterministicContractJson::SignedString(value);
            return true;
        }

        bool AppendNumberValue(AZStd::string& output, double value)
        {
            AZStd::string formatted;
            if (!FormatCanonicalFiniteNumberV1(value, formatted))
            {
                return false;
            }
            output += formatted;
            return true;
        }

        bool AppendBoolValue(AZStd::string& output, bool value)
        {
            output += value ? "true" : "false";
            return true;
        }

        class JsonObjectWriter
        {
        public:
            explicit JsonObjectWriter(AZStd::string& output)
                : m_output(output)
            {
                m_output.push_back('{');
            }

            bool String(const char* name, AZStd::string_view value)
            {
                return Name(name) && AppendStringValue(m_output, value);
            }

            bool ExtensionString(const char* name, AZStd::string_view value)
            {
                return Name(name) && AppendExtensionStringValue(m_output, value);
            }

            bool Unsigned(const char* name, AZ::u64 value)
            {
                return Name(name) && AppendUnsignedValue(m_output, value);
            }

            bool Signed(const char* name, AZ::s64 value)
            {
                return Name(name) && AppendSignedValue(m_output, value);
            }

            bool Number(const char* name, double value)
            {
                return Name(name) && AppendNumberValue(m_output, value);
            }

            bool Bool(const char* name, bool value)
            {
                return Name(name) && AppendBoolValue(m_output, value);
            }

            template<class Enum>
            bool Token(const char* name, Enum value)
            {
                return Name(name) && AppendStringValue(m_output, ToToken(value));
            }

            template<class Writer>
            bool Object(const char* name, Writer writer)
            {
                return Name(name) && writer(m_output);
            }

            template<class Range, class Writer>
            bool Array(const char* name, const Range& values, Writer writer)
            {
                if (!Name(name))
                {
                    return false;
                }
                m_output.push_back('[');
                bool first = true;
                for (const auto& value : values)
                {
                    if (!first)
                    {
                        m_output.push_back(',');
                    }
                    first = false;
                    if (!writer(m_output, value))
                    {
                        m_valid = false;
                        return false;
                    }
                }
                m_output.push_back(']');
                return true;
            }

            bool End()
            {
                m_output.push_back('}');
                return m_valid;
            }

        private:
            bool Name(const char* name)
            {
                if (!m_valid)
                {
                    return false;
                }
                if (!m_first)
                {
                    m_output.push_back(',');
                }
                m_first = false;
                if (!AppendStringValue(m_output, name))
                {
                    m_valid = false;
                    return false;
                }
                m_output.push_back(':');
                return true;
            }

            AZStd::string& m_output;
            bool m_first = true;
            bool m_valid = true;
        };

        template<class T, class Less>
        AZStd::vector<T> SortedCopy(const AZStd::vector<T>& values, Less less)
        {
            AZStd::vector<T> result = values;
            AZStd::sort(result.begin(), result.end(), less);
            return result;
        }

        template<class T, class Less>
        AZStd::vector<T> SortedUniqueCopy(const AZStd::vector<T>& values, Less less)
        {
            AZStd::vector<T> result = SortedCopy(values, less);
            result.erase(AZStd::unique(result.begin(), result.end()), result.end());
            return result;
        }

        template<class T>
        AZStd::vector<T> SortedUniqueCopy(const AZStd::vector<T>& values)
        {
            return SortedUniqueCopy(values, [](const T& left, const T& right)
            {
                return left < right;
            });
        }

        bool WriteString(AZStd::string& output, const AZStd::string& value)
        {
            return AppendStringValue(output, value);
        }

        template<class Wrapper>
        bool WriteValueWrapper(AZStd::string& output, const Wrapper& value)
        {
            return AppendStringValue(output, value.m_value);
        }

        bool WriteRevision(AZStd::string& output, const RevisionFingerprintV1& value)
        {
            return AppendStringValue(output, value.m_sha256);
        }

        bool WriteDigest(AZStd::string& output, const Sha256DigestV1& value)
        {
            return AppendStringValue(output, value.m_hex);
        }

        bool WriteSpatialBasis(AZStd::string& output, const CanonicalSpatialBasisV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("handedness", value.m_handedness) &&
                object.String("right_axis", value.m_rightAxis) &&
                object.String("forward_axis", value.m_forwardAxis) &&
                object.String("up_axis", value.m_upAxis) &&
                object.String("linear_unit", value.m_linearUnit) &&
                object.String("angular_unit", value.m_angularUnit) &&
                object.String("vector_convention", value.m_vectorConvention) &&
                object.String("multiplication_convention", value.m_multiplicationConvention) &&
                object.String("storage_order", value.m_storageOrder) &&
                object.End();
        }

        bool WriteTemporalBasis(AZStd::string& output, const CanonicalTemporalBasisV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("time_unit", value.m_timeUnit) && object.End();
        }

        bool WriteMatrix(AZStd::string& output, const Matrix4x4V1& value)
        {
            JsonObjectWriter object(output);
            return object.Array("column_major_values", value.m_columnMajorValues,
                       [](AZStd::string& target, double item)
                       {
                           return AppendNumberValue(target, item);
                       }) &&
                object.String("mapping_direction", value.m_mappingDirection) &&
                object.End();
        }

        bool WriteRate(AZStd::string& output, const RationalRateV1& value)
        {
            JsonObjectWriter object(output);
            return object.Unsigned("numerator", value.m_numerator) &&
                object.Unsigned("denominator", value.m_denominator) &&
                object.End();
        }

        bool WriteTolerance(AZStd::string& output, const NumericToleranceV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("subject_property_path", value.m_subjectPropertyPath) &&
                object.Number("absolute", value.m_absolute) &&
                object.Number("relative", value.m_relative) &&
                object.Number("angular_radians", value.m_angularRadians) &&
                object.String("comparison_norm", value.m_comparisonNorm) &&
                object.String("unit", value.m_unit) &&
                object.String("scope", value.m_scope) &&
                object.End();
        }

        AZStd::vector<NumericToleranceV1> SortedTolerances(
            const AZStd::vector<NumericToleranceV1>& values)
        {
            return SortedCopy(values, [](const auto& left, const auto& right)
            {
                if (left.m_subjectPropertyPath != right.m_subjectPropertyPath)
                {
                    return left.m_subjectPropertyPath < right.m_subjectPropertyPath;
                }
                if (left.m_comparisonNorm != right.m_comparisonNorm)
                {
                    return left.m_comparisonNorm < right.m_comparisonNorm;
                }
                if (left.m_unit != right.m_unit)
                {
                    return left.m_unit < right.m_unit;
                }
                return left.m_scope < right.m_scope;
            });
        }

        bool WriteProducer(AZStd::string& output, const ProducerV1& value)
        {
            JsonObjectWriter object(output);
            return object.Token("host_kind", value.m_hostKind) &&
                object.String("application_id", value.m_applicationId) &&
                object.String("application_version", value.m_applicationVersion) &&
                object.String("provider_id", value.m_providerId) &&
                object.String("provider_version", value.m_providerVersion) &&
                object.String("extension_id", value.m_extensionId) &&
                object.String("extension_version", value.m_extensionVersion) &&
                object.String("extension_revision", value.m_extensionRevision) &&
                object.String("extension_digest", value.m_extensionDigest.m_hex) &&
                object.String("configuration_fingerprint", value.m_configurationFingerprint.m_hex) &&
                object.String("workspace_id_observation", value.m_workspaceIdObservation) &&
                object.String("profile_id_observation", value.m_profileIdObservation) &&
                object.End();
        }

        bool WriteConsumer(AZStd::string& output, const ConsumerConstraintV1& value)
        {
            JsonObjectWriter object(output);
            return object.Token("host_kind", value.m_hostKind) &&
                object.String("consumer_id", value.m_consumerId) &&
                object.String("consumer_version", value.m_consumerVersion) &&
                object.String("profile_id", value.m_profileId) &&
                object.Bool("required", value.m_required) &&
                object.End();
        }

        bool WriteToolchainComponent(AZStd::string& output, const ToolchainComponentV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("component_kind", value.m_componentKind) &&
                object.String("component_id", value.m_componentId) &&
                object.String("version", value.m_version) &&
                object.String("revision", value.m_revision) &&
                object.String("digest", value.m_digest.m_hex) &&
                object.End();
        }

        bool WriteToolchainLock(AZStd::string& output, const ToolchainLockV1& value)
        {
            const auto components = SortedCopy(value.m_components, [](const auto& left, const auto& right)
            {
                if (left.m_componentKind != right.m_componentKind)
                {
                    return left.m_componentKind < right.m_componentKind;
                }
                if (left.m_componentId != right.m_componentId)
                {
                    return left.m_componentId < right.m_componentId;
                }
                if (left.m_version != right.m_version)
                {
                    return left.m_version < right.m_version;
                }
                if (left.m_revision != right.m_revision)
                {
                    return left.m_revision < right.m_revision;
                }
                return left.m_digest.m_hex < right.m_digest.m_hex;
            });

            JsonObjectWriter object(output);
            return object.String("foa_sdk_commit", value.m_foaSdkCommit) &&
                object.String("schema_profile", value.m_schemaProfile) &&
                object.String("configuration_fingerprint", value.m_configurationFingerprint.m_hex) &&
                object.String("fixture_revision", value.m_fixtureRevision) &&
                object.Array("components", components, WriteToolchainComponent) &&
                object.End();
        }

        bool WriteDocument(AZStd::string& output, const DomainDocumentRecordV1& value)
        {
            const auto subjects = SortedUniqueCopy(value.m_subjectReferences);
            const auto assets = SortedUniqueCopy(value.m_assetIds);
            const auto provenance = SortedUniqueCopy(value.m_provenanceIds);
            const auto licensing = SortedUniqueCopy(value.m_licensingIds);
            JsonObjectWriter object(output);
            return object.String("document_id", value.m_documentId.m_value) &&
                object.String("document_kind", value.m_documentKind) &&
                object.String("document_schema_id", value.m_documentSchemaId) &&
                object.Unsigned("document_schema_version", value.m_documentSchemaVersion) &&
                object.String("payload_id", value.m_payloadId.m_value) &&
                object.String("revision_fingerprint", value.m_revisionFingerprint.m_sha256) &&
                object.Array("subject_refs", subjects, WriteString) &&
                object.Array("asset_ids", assets, WriteValueWrapper<AssetIdV1>) &&
                object.Array("provenance_ids", provenance, WriteValueWrapper<ProvenanceIdV1>) &&
                object.Array("licensing_ids", licensing, WriteValueWrapper<LicensingIdV1>) &&
                object.End();
        }

        bool WriteAsset(AZStd::string& output, const AssetRecordV1& value)
        {
            const auto payloads = SortedUniqueCopy(value.m_payloadIds);
            const auto documents = SortedUniqueCopy(value.m_documentIds);
            const auto subjects = SortedUniqueCopy(value.m_subjectReferences);
            const auto provenance = SortedUniqueCopy(value.m_provenanceIds);
            const auto licensing = SortedUniqueCopy(value.m_licensingIds);
            JsonObjectWriter object(output);
            return object.String("asset_id", value.m_assetId.m_value) &&
                object.String("asset_kind", value.m_assetKind) &&
                object.String("revision_fingerprint", value.m_revisionFingerprint.m_sha256) &&
                object.Array("payload_ids", payloads, WriteValueWrapper<PayloadIdV1>) &&
                object.Array("document_ids", documents, WriteValueWrapper<DocumentIdV1>) &&
                object.Array("subject_refs", subjects, WriteString) &&
                object.Array("provenance_ids", provenance, WriteValueWrapper<ProvenanceIdV1>) &&
                object.Array("licensing_ids", licensing, WriteValueWrapper<LicensingIdV1>) &&
                object.End();
        }

        bool WriteIdentityMapping(AZStd::string& output, const IdentityMappingRecordV1& value)
        {
            const auto sourceAssets = SortedUniqueCopy(value.m_sourceAssetIds);
            const auto targetAssets = SortedUniqueCopy(value.m_targetAssetIds);
            const auto sourceRevisions = SortedUniqueCopy(value.m_sourceRevisions);
            const auto targetRevisions = SortedUniqueCopy(value.m_targetRevisions);
            const auto transformations = SortedUniqueCopy(value.m_transformationIds);
            const auto losses = SortedUniqueCopy(value.m_lossIds);
            const auto provenance = SortedUniqueCopy(value.m_provenanceIds);
            const auto evidence = SortedUniqueCopy(value.m_evidenceReferenceIds);
            JsonObjectWriter object(output);
            return object.String("mapping_id", value.m_mappingId.m_value) &&
                object.Token("operation", value.m_operation) &&
                object.Array("source_asset_ids", sourceAssets, WriteValueWrapper<AssetIdV1>) &&
                object.Array("target_asset_ids", targetAssets, WriteValueWrapper<AssetIdV1>) &&
                object.Array("source_revisions", sourceRevisions, WriteRevision) &&
                object.Array("target_revisions", targetRevisions, WriteRevision) &&
                object.Array("transformation_ids", transformations, WriteValueWrapper<TransformationIdV1>) &&
                object.Array("loss_ids", losses, WriteValueWrapper<LossIdV1>) &&
                object.Array("provenance_ids", provenance, WriteValueWrapper<ProvenanceIdV1>) &&
                object.Array("evidence_ref_ids", evidence, WriteValueWrapper<EvidenceReferenceIdV1>) &&
                object.Token("completeness", value.m_completeness) &&
                object.End();
        }

        bool WriteBinding(AZStd::string& output, const NativeBindingRecordV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("binding_id", value.m_bindingId.m_value) &&
                object.String("subject_id", value.m_subjectId) &&
                object.Token("host_kind", value.m_hostKind) &&
                object.String("consumer_profile", value.m_consumerProfile) &&
                object.String("native_id_kind", value.m_nativeIdKind) &&
                object.String("native_id_value", value.m_nativeIdValue) &&
                object.String("native_path_observation", value.m_nativePathObservation) &&
                object.Token("state", value.m_state) &&
                object.String("supersedes_binding_id", value.m_supersedesBindingId.m_value) &&
                object.Bool("required", value.m_required) &&
                object.End();
        }

        bool WritePayload(AZStd::string& output, const PayloadRecordV1& value)
        {
            const auto dependencies = SortedUniqueCopy(value.m_dependencyIds);
            const auto subjects = SortedUniqueCopy(value.m_subjectIds);
            const auto provenance = SortedUniqueCopy(value.m_provenanceIds);
            const auto licensing = SortedUniqueCopy(value.m_licensingIds);
            JsonObjectWriter object(output);
            return object.String("payload_id", value.m_payloadId.m_value) &&
                object.String("relative_path", value.m_relativePath) &&
                object.Token("role", value.m_role) &&
                object.String("media_type", value.m_mediaType) &&
                object.Unsigned("byte_size", value.m_byteSize) &&
                object.String("digest", value.m_digest.m_hex) &&
                object.Array("dependency_ids", dependencies, WriteValueWrapper<PayloadIdV1>) &&
                object.Array("subject_ids", subjects, WriteString) &&
                object.Array("provenance_ids", provenance, WriteValueWrapper<ProvenanceIdV1>) &&
                object.Array("licensing_ids", licensing, WriteValueWrapper<LicensingIdV1>) &&
                object.End();
        }

        bool WriteParameter(AZStd::string& output, const CanonicalParameterV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("name", value.m_name) &&
                object.Token("kind", value.m_kind) &&
                object.Bool("boolean_value", value.m_booleanValue) &&
                object.Signed("signed_value", value.m_signedValue) &&
                object.Unsigned("unsigned_value", value.m_unsignedValue) &&
                object.Number("number_value", value.m_numberValue) &&
                object.String("string_value", value.m_stringValue) &&
                object.Object("matrix_value", [&value](AZStd::string& target)
                {
                    return WriteMatrix(target, value.m_matrixValue);
                }) &&
                object.Object("rate_value", [&value](AZStd::string& target)
                {
                    return WriteRate(target, value.m_rateValue);
                }) &&
                object.End();
        }

        AZStd::vector<CanonicalParameterV1> SortedParameters(
            const AZStd::vector<CanonicalParameterV1>& values)
        {
            return SortedCopy(values, [](const auto& left, const auto& right)
            {
                return left.m_name < right.m_name;
            });
        }

        bool WriteTransformationInternal(
            AZStd::string& output,
            const TransformationRecordV1& value,
            bool includeEvidence)
        {
            const auto subjects = SortedUniqueCopy(value.m_subjectIds);
            const auto properties = SortedUniqueCopy(value.m_propertyPaths);
            const auto parameters = SortedParameters(value.m_parameters);
            const auto tolerances = SortedTolerances(value.m_tolerances);
            const auto evidence = SortedUniqueCopy(value.m_evidenceReferenceIds);
            JsonObjectWriter object(output);
            const bool base = object.String("transformation_id", value.m_transformationId.m_value) &&
                object.Unsigned("sequence", value.m_sequence) &&
                object.Token("phase", value.m_phase) &&
                object.String("operation_code", value.m_operationCode) &&
                object.Array("subject_ids", subjects, WriteString) &&
                object.Array("property_paths", properties, WriteString) &&
                object.String("source_basis_or_profile", value.m_sourceBasisOrProfile) &&
                object.String("destination_basis_or_profile", value.m_destinationBasisOrProfile) &&
                object.Array("parameters", parameters, WriteParameter) &&
                object.Token("reversibility", value.m_reversibility) &&
                object.Array("tolerances", tolerances, WriteTolerance);
            if (!base)
            {
                return false;
            }
            if (includeEvidence &&
                !object.Array("evidence_ref_ids", evidence, WriteValueWrapper<EvidenceReferenceIdV1>))
            {
                return false;
            }
            return object.End();
        }

        bool WriteTransformation(AZStd::string& output, const TransformationRecordV1& value)
        {
            return WriteTransformationInternal(output, value, true);
        }

        bool WriteTransformationSemantic(AZStd::string& output, const TransformationRecordV1& value)
        {
            return WriteTransformationInternal(output, value, false);
        }

        bool WriteLossInternal(
            AZStd::string& output,
            const LossRecordV1& value,
            bool includeEvidence)
        {
            const auto tolerances = SortedTolerances(value.m_tolerances);
            const auto evidence = SortedUniqueCopy(value.m_evidenceReferenceIds);
            JsonObjectWriter object(output);
            const bool base = object.String("loss_id", value.m_lossId.m_value) &&
                object.Unsigned("sequence", value.m_sequence) &&
                object.String("subject_id", value.m_subjectId) &&
                object.String("property_path", value.m_propertyPath) &&
                object.String("source_profile", value.m_sourceProfile) &&
                object.String("destination_profile", value.m_destinationProfile) &&
                object.Token("phase", value.m_phase) &&
                object.String("reason_code", value.m_reasonCode) &&
                object.Token("severity", value.m_severity) &&
                object.String("source_feature", value.m_sourceFeature) &&
                object.String("result_representation", value.m_resultRepresentation) &&
                object.Token("reversibility", value.m_reversibility) &&
                object.Array("tolerances", tolerances, WriteTolerance);
            if (!base)
            {
                return false;
            }
            if (includeEvidence &&
                !object.Array("evidence_ref_ids", evidence, WriteValueWrapper<EvidenceReferenceIdV1>))
            {
                return false;
            }
            return object.End();
        }

        bool WriteLoss(AZStd::string& output, const LossRecordV1& value)
        {
            return WriteLossInternal(output, value, true);
        }

        bool WriteLossSemantic(AZStd::string& output, const LossRecordV1& value)
        {
            return WriteLossInternal(output, value, false);
        }

        bool WriteProvenance(AZStd::string& output, const ProvenanceRecordV1& value)
        {
            const auto evidence = SortedUniqueCopy(value.m_evidenceReferenceIds);
            JsonObjectWriter object(output);
            return object.String("provenance_id", value.m_provenanceId.m_value) &&
                object.String("subject_id", value.m_subjectId) &&
                object.Token("source_kind", value.m_sourceKind) &&
                object.String("source_identity", value.m_sourceIdentity) &&
                object.String("source_revision", value.m_sourceRevision) &&
                object.String("source_digest", value.m_sourceDigest.m_hex) &&
                object.String("authorship", value.m_authorship) &&
                object.String("modification_state", value.m_modificationState) &&
                object.String("attribution", value.m_attribution) &&
                object.String("permitted_use", value.m_permittedUse) &&
                object.Array("evidence_ref_ids", evidence, WriteValueWrapper<EvidenceReferenceIdV1>) &&
                object.End();
        }

        bool WriteLicensing(AZStd::string& output, const LicensingRecordV1& value)
        {
            const auto notices = SortedUniqueCopy(value.m_requiredNotices);
            const auto evidence = SortedUniqueCopy(value.m_evidenceReferenceIds);
            JsonObjectWriter object(output);
            return object.String("licensing_id", value.m_licensingId.m_value) &&
                object.String("subject_id", value.m_subjectId) &&
                object.String("spdx_expression_or_license_ref", value.m_spdxExpressionOrLicenseRef) &&
                object.Token("redistribution_state", value.m_redistributionState) &&
                object.Array("required_notices", notices, WriteString) &&
                object.String("reviewer", value.m_reviewer) &&
                object.String("decision_at_utc", value.m_decisionAtUtc) &&
                object.Array("evidence_ref_ids", evidence, WriteValueWrapper<EvidenceReferenceIdV1>) &&
                object.End();
        }

        bool WriteEvidence(AZStd::string& output, const EvidenceReferenceV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("evidence_ref_id", value.m_evidenceReferenceId.m_value) &&
                object.String("evidence_kind", value.m_evidenceKind) &&
                object.String("subject_id", value.m_subjectId) &&
                object.String("external_evidence_id", value.m_externalEvidenceId) &&
                object.String("external_evidence_fingerprint", value.m_externalEvidenceFingerprint.m_hex) &&
                object.End();
        }

        bool WriteExtension(AZStd::string& output, const ExtensionRecordV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("namespace", value.m_namespace) &&
                object.Unsigned("schema_version", value.m_schemaVersion) &&
                object.ExtensionString("canonical_json", value.m_canonicalJson) &&
                object.String("digest", value.m_digest.m_hex) &&
                object.End();
        }

        bool WriteSemanticPayload(AZStd::string& output, const PayloadRecordV1& value)
        {
            const auto dependencies = SortedUniqueCopy(value.m_dependencyIds);
            const auto subjects = SortedUniqueCopy(value.m_subjectIds);
            JsonObjectWriter object(output);
            return object.String("payload_id", value.m_payloadId.m_value) &&
                object.Token("role", value.m_role) &&
                object.String("media_type", value.m_mediaType) &&
                object.Unsigned("byte_size", value.m_byteSize) &&
                object.String("digest", value.m_digest.m_hex) &&
                object.Array("dependency_ids", dependencies, WriteValueWrapper<PayloadIdV1>) &&
                object.Array("subject_ids", subjects, WriteString) &&
                object.End();
        }

        bool WriteDeclaredPayloadTuple(AZStd::string& output, const PayloadRecordV1& value)
        {
            JsonObjectWriter object(output);
            return object.String("relative_path", value.m_relativePath) &&
                object.Unsigned("byte_size", value.m_byteSize) &&
                object.String("media_type", value.m_mediaType) &&
                object.String("digest", value.m_digest.m_hex) &&
                object.End();
        }

        const PayloadRecordV1* FindPayload(
            const CanonicalInterchangeManifestV1& manifest,
            const PayloadIdV1& id)
        {
            for (const auto& payload : manifest.m_payloads)
            {
                if (payload.m_payloadId == id)
                {
                    return &payload;
                }
            }
            return nullptr;
        }

        const DomainDocumentRecordV1* FindDocument(
            const CanonicalInterchangeManifestV1& manifest,
            const DocumentIdV1& id)
        {
            for (const auto& document : manifest.m_documents)
            {
                if (document.m_documentId == id)
                {
                    return &document;
                }
            }
            return nullptr;
        }

        bool ContainsSubject(
            const AZStd::vector<AZStd::string>& subjects,
            AZStd::string_view subject)
        {
            return AZStd::find(subjects.begin(), subjects.end(), subject) != subjects.end();
        }

        AZStd::string SerializeDocumentProjection(
            const CanonicalInterchangeManifestV1& manifest,
            const DomainDocumentRecordV1& document)
        {
            const PayloadRecordV1* payload = FindPayload(manifest, document.m_payloadId);
            if (payload == nullptr)
            {
                return {};
            }
            const auto subjects = SortedUniqueCopy(document.m_subjectReferences);
            const auto assets = SortedUniqueCopy(document.m_assetIds);
            AZStd::string output;
            JsonObjectWriter object(output);
            if (!object.String("schema_profile", manifest.m_schemaProfile) ||
                !object.String("document_id", document.m_documentId.m_value) ||
                !object.String("document_kind", document.m_documentKind) ||
                !object.String("document_schema_id", document.m_documentSchemaId) ||
                !object.Unsigned("document_schema_version", document.m_documentSchemaVersion) ||
                !object.Object("payload", [payload](AZStd::string& target)
                {
                    return WriteSemanticPayload(target, *payload);
                }) ||
                !object.Array("subject_refs", subjects, WriteString) ||
                !object.Array("asset_ids", assets, WriteValueWrapper<AssetIdV1>) ||
                !object.End())
            {
                return {};
            }
            return output;
        }

        struct DocumentRevisionTuple
        {
            DocumentIdV1 m_documentId;
            RevisionFingerprintV1 m_revision;
        };

        bool WriteDocumentRevisionTuple(
            AZStd::string& output,
            const DocumentRevisionTuple& value)
        {
            JsonObjectWriter object(output);
            return object.String("document_id", value.m_documentId.m_value) &&
                object.String("revision_fingerprint", value.m_revision.m_sha256) &&
                object.End();
        }

        AZStd::string SerializeAssetProjection(
            const CanonicalInterchangeManifestV1& manifest,
            const AssetRecordV1& asset)
        {
            // Schema 1 has no admitted extension namespace. A non-empty envelope
            // is therefore ambiguous for a semantic asset projection and fails
            // closed until a later reviewed profile defines subject linkage.
            if (!manifest.m_extensions.empty())
            {
                return {};
            }

            AZStd::vector<PayloadRecordV1> payloads;
            for (const auto& payloadId : asset.m_payloadIds)
            {
                const PayloadRecordV1* payload = FindPayload(manifest, payloadId);
                if (payload == nullptr)
                {
                    return {};
                }
                payloads.push_back(*payload);
            }
            AZStd::sort(payloads.begin(), payloads.end(), [](const auto& left, const auto& right)
            {
                return left.m_payloadId < right.m_payloadId;
            });

            AZStd::vector<DocumentRevisionTuple> documents;
            for (const auto& documentId : asset.m_documentIds)
            {
                const DomainDocumentRecordV1* document = FindDocument(manifest, documentId);
                if (document == nullptr)
                {
                    return {};
                }
                const RevisionFingerprintV1 revision =
                    CalculateDocumentRevisionFingerprintV1(manifest, *document);
                if (!IsSha256HexV1(revision.m_sha256))
                {
                    return {};
                }
                documents.push_back({ documentId, revision });
            }
            AZStd::sort(documents.begin(), documents.end(), [](const auto& left, const auto& right)
            {
                return left.m_documentId < right.m_documentId;
            });

            AZStd::vector<TransformationRecordV1> transformations;
            for (const auto& transformation : manifest.m_transformations)
            {
                if (ContainsSubject(transformation.m_subjectIds, asset.m_assetId.m_value))
                {
                    transformations.push_back(transformation);
                }
            }

            AZStd::vector<LossRecordV1> losses;
            for (const auto& loss : manifest.m_losses)
            {
                if (loss.m_subjectId == asset.m_assetId.m_value)
                {
                    losses.push_back(loss);
                }
            }

            const auto subjects = SortedUniqueCopy(asset.m_subjectReferences);
            const AZStd::vector<ExtensionRecordV1> admittedExtensions;
            AZStd::string output;
            JsonObjectWriter object(output);
            if (!object.String("schema_profile", manifest.m_schemaProfile) ||
                !object.String("asset_id", asset.m_assetId.m_value) ||
                !object.String("asset_kind", asset.m_assetKind) ||
                !object.Array("payloads", payloads, WriteSemanticPayload) ||
                !object.Array("documents", documents, WriteDocumentRevisionTuple) ||
                !object.Array("subject_refs", subjects, WriteString) ||
                !object.Array("transformations", transformations, WriteTransformationSemantic) ||
                !object.Array("losses", losses, WriteLossSemantic) ||
                !object.Array("extensions", admittedExtensions, WriteExtension) ||
                !object.End())
            {
                return {};
            }
            return output;
        }
    } // namespace

    bool IsValidUtf8V1(AZStd::string_view value)
    {
        size_t index = 0;
        while (index < value.size())
        {
            const unsigned char lead = static_cast<unsigned char>(value[index]);
            if (lead <= 0x7f)
            {
                ++index;
                continue;
            }

            size_t continuationCount = 0;
            unsigned int codePoint = 0;
            if (lead >= 0xc2 && lead <= 0xdf)
            {
                continuationCount = 1;
                codePoint = lead & 0x1f;
            }
            else if (lead >= 0xe0 && lead <= 0xef)
            {
                continuationCount = 2;
                codePoint = lead & 0x0f;
            }
            else if (lead >= 0xf0 && lead <= 0xf4)
            {
                continuationCount = 3;
                codePoint = lead & 0x07;
            }
            else
            {
                return false;
            }

            if (index + continuationCount >= value.size())
            {
                return false;
            }
            for (size_t offset = 1; offset <= continuationCount; ++offset)
            {
                const unsigned char continuation =
                    static_cast<unsigned char>(value[index + offset]);
                if (!IsContinuation(continuation))
                {
                    return false;
                }
                codePoint = (codePoint << 6) | (continuation & 0x3f);
            }

            if ((continuationCount == 2 && codePoint < 0x800) ||
                (continuationCount == 3 && codePoint < 0x10000) ||
                codePoint > 0x10ffff ||
                (codePoint >= 0xd800 && codePoint <= 0xdfff))
            {
                return false;
            }
            index += continuationCount + 1;
        }
        return true;
    }

    bool AppendCanonicalPresentationStringV1(
        AZStd::string& output,
        AZStd::string_view value)
    {
        return AppendCanonicalStringWithLimit(
            output, value, MaxPresentationStringBytesV1);
    }

    bool FormatCanonicalFiniteNumberV1(
        double value,
        AZStd::string& output)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
        if (value == 0.0)
        {
            output = "0";
            return true;
        }

        char buffer[128] = {};
        const auto converted = std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            value,
            std::chars_format::general);
        if (converted.ec != std::errc{})
        {
            return false;
        }
        output.assign(buffer, static_cast<size_t>(converted.ptr - buffer));
        NormalizeExponent(output);
        return true;
    }

    Sha256DigestV1 CalculateCanonicalBytesDigestV1(AZStd::string_view bytes)
    {
        const AZStd::string prefixed =
            TaintedGrailModdingSDK::CalculateCanonicalSha256(AZStd::string(bytes));
        constexpr AZStd::string_view Prefix = "sha256:";
        Sha256DigestV1 result;
        if (prefixed.starts_with(Prefix))
        {
            result.m_hex = prefixed.substr(Prefix.size());
        }
        return result;
    }

    bool CanonicalBytesDigestMatchesV1(
        AZStd::string_view bytes,
        const Sha256DigestV1& digest)
    {
        return IsSha256HexV1(digest.m_hex) &&
            CalculateCanonicalBytesDigestV1(bytes) == digest;
    }

    AZStd::string SerializeCanonicalManifestV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const auto consumers = SortedCopy(manifest.m_intendedConsumers, [](const auto& left, const auto& right)
        {
            const AZStd::string_view leftHost = ToToken(left.m_hostKind);
            const AZStd::string_view rightHost = ToToken(right.m_hostKind);
            if (leftHost != rightHost)
            {
                return leftHost < rightHost;
            }
            if (left.m_consumerId != right.m_consumerId)
            {
                return left.m_consumerId < right.m_consumerId;
            }
            if (left.m_consumerVersion != right.m_consumerVersion)
            {
                return left.m_consumerVersion < right.m_consumerVersion;
            }
            return left.m_profileId < right.m_profileId;
        });
        const auto documents = SortedCopy(manifest.m_documents, [](const auto& left, const auto& right)
        {
            return left.m_documentId < right.m_documentId;
        });
        const auto assets = SortedCopy(manifest.m_assets, [](const auto& left, const auto& right)
        {
            return left.m_assetId < right.m_assetId;
        });
        const auto bindings = SortedCopy(manifest.m_bindings, [](const auto& left, const auto& right)
        {
            return left.m_bindingId < right.m_bindingId;
        });
        const auto payloads = SortedCopy(manifest.m_payloads, [](const auto& left, const auto& right)
        {
            return left.m_payloadId < right.m_payloadId;
        });
        const auto provenance = SortedCopy(manifest.m_provenance, [](const auto& left, const auto& right)
        {
            return left.m_provenanceId < right.m_provenanceId;
        });
        const auto licensing = SortedCopy(manifest.m_licensing, [](const auto& left, const auto& right)
        {
            return left.m_licensingId < right.m_licensingId;
        });
        const auto evidence = SortedCopy(manifest.m_evidenceReferences, [](const auto& left, const auto& right)
        {
            return left.m_evidenceReferenceId < right.m_evidenceReferenceId;
        });
        const auto extensions = SortedCopy(manifest.m_extensions, [](const auto& left, const auto& right)
        {
            if (left.m_namespace != right.m_namespace)
            {
                return left.m_namespace < right.m_namespace;
            }
            return left.m_schemaVersion < right.m_schemaVersion;
        });

        AZStd::string output;
        JsonObjectWriter object(output);
        if (!object.Unsigned("schema_version", manifest.m_schemaVersion) ||
            !object.String("schema_profile", manifest.m_schemaProfile) ||
            !object.String("canonical_profile", manifest.m_canonicalProfile) ||
            !object.Token("package_kind", manifest.m_packageKind) ||
            !object.String("package_id", manifest.m_packageId.m_value) ||
            !object.String("pack_id", manifest.m_packId) ||
            !object.String("display_name", manifest.m_displayName) ||
            !object.Object("producer", [&manifest](AZStd::string& target)
            {
                return WriteProducer(target, manifest.m_producer);
            }) ||
            !object.Array("intended_consumers", consumers, WriteConsumer) ||
            !object.Object("toolchain_lock", [&manifest](AZStd::string& target)
            {
                return WriteToolchainLock(target, manifest.m_toolchainLock);
            }) ||
            !object.Object("spatial_basis", [&manifest](AZStd::string& target)
            {
                return WriteSpatialBasis(target, manifest.m_spatialBasis);
            }) ||
            !object.Object("temporal_basis", [&manifest](AZStd::string& target)
            {
                return WriteTemporalBasis(target, manifest.m_temporalBasis);
            }) ||
            !object.Array("documents", documents, WriteDocument) ||
            !object.Array("assets", assets, WriteAsset) ||
            !object.Array("identity_mappings", manifest.m_identityMappings, WriteIdentityMapping) ||
            !object.Array("bindings", bindings, WriteBinding) ||
            !object.Array("payloads", payloads, WritePayload) ||
            !object.Array("transformations", manifest.m_transformations, WriteTransformation) ||
            !object.Array("losses", manifest.m_losses, WriteLoss) ||
            !object.Array("provenance", provenance, WriteProvenance) ||
            !object.Array("licensing", licensing, WriteLicensing) ||
            !object.Array("evidence_refs", evidence, WriteEvidence) ||
            !object.Array("extensions", extensions, WriteExtension) ||
            !object.End() ||
            output.size() > MaxCanonicalManifestBytesV1)
        {
            return {};
        }
        return output;
    }

    AZStd::string SerializeDeclaredPackageFingerprintInputV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string manifestBytes = SerializeCanonicalManifestV1(manifest);
        if (manifestBytes.empty())
        {
            return {};
        }
        const Sha256DigestV1 manifestDigest =
            CalculateCanonicalBytesDigestV1(manifestBytes);
        const auto payloads = SortedCopy(manifest.m_payloads, [](const auto& left, const auto& right)
        {
            if (left.m_relativePath != right.m_relativePath)
            {
                return left.m_relativePath < right.m_relativePath;
            }
            return left.m_payloadId < right.m_payloadId;
        });

        AZStd::string output;
        JsonObjectWriter object(output);
        if (!object.String("manifest_digest", manifestDigest.m_hex) ||
            !object.Array("payloads", payloads, WriteDeclaredPayloadTuple) ||
            !object.End())
        {
            return {};
        }
        return output;
    }

    Sha256DigestV1 CalculateCanonicalManifestDigestV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string bytes = SerializeCanonicalManifestV1(manifest);
        return bytes.empty() ? Sha256DigestV1{} : CalculateCanonicalBytesDigestV1(bytes);
    }

    Sha256DigestV1 CalculateDeclaredPackageFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string input = SerializeDeclaredPackageFingerprintInputV1(manifest);
        return input.empty() ? Sha256DigestV1{} : CalculateCanonicalBytesDigestV1(input);
    }

    RevisionFingerprintV1 CalculateDocumentRevisionFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest,
        const DomainDocumentRecordV1& document)
    {
        RevisionFingerprintV1 result;
        const AZStd::string projection = SerializeDocumentProjection(manifest, document);
        if (!projection.empty())
        {
            result.m_sha256 = CalculateCanonicalBytesDigestV1(projection).m_hex;
        }
        return result;
    }

    RevisionFingerprintV1 CalculateAssetRevisionFingerprintV1(
        const CanonicalInterchangeManifestV1& manifest,
        const AssetRecordV1& asset)
    {
        RevisionFingerprintV1 result;
        const AZStd::string projection = SerializeAssetProjection(manifest, asset);
        if (!projection.empty())
        {
            result.m_sha256 = CalculateCanonicalBytesDigestV1(projection).m_hex;
        }
        return result;
    }

    bool CanonicalManifestBytesMatchV1(
        AZStd::string_view bytes,
        const CanonicalInterchangeManifestV1& manifest)
    {
        const AZStd::string expected = SerializeCanonicalManifestV1(manifest);
        return !expected.empty() && bytes == expected;
    }
} // namespace TaintedGrailModdingSDK::Interchange
