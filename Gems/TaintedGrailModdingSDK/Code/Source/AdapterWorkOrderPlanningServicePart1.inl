/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct ReadySubject
        {
            const CatalogRecord* m_record = nullptr;
            AZStd::vector<const CatalogRelationship*> m_relationships;
            AZStd::vector<AZStd::string> m_permissionEventIds;
            AZStd::vector<AZStd::string> m_permissionEvidenceIds;
            AZStd::vector<AZStd::string> m_validationEvidenceIds;
            AZStd::vector<AZStd::string> m_validationIds;
        };

        struct MatrixGroup
        {
            const PackManifest* m_pack = nullptr;
            const AdapterDeclaration* m_declaration = nullptr;
            AZStd::string m_adapterId;
            AZStd::vector<const AdapterCapabilityMatrixRow*> m_rows;
        };

        const AdapterCapability AllCapabilities[] = {
            AdapterCapability::ItemGrant,
            AdapterCapability::RecipeLearn,
            AdapterCapability::RecipeAppend,
            AdapterCapability::CustomItemRegistration,
            AdapterCapability::CustomRecipeRegistration,
            AdapterCapability::VendorMutation,
            AdapterCapability::LootMutation,
            AdapterCapability::RewardMutation,
            AdapterCapability::Persistence,
            AdapterCapability::Cleanup,
            AdapterCapability::Rollback,
        };

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        void AddAllUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::vector<AZStd::string>& additions)
        {
            for (const AZStd::string& value : additions)
            {
                AddUnique(values, value);
            }
        }

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        void AddArgument(
            AZStd::vector<AdapterWorkOrderArgument>& arguments,
            const AZStd::string& key,
            const AZStd::string& value)
        {
            if (key.empty() || value.empty())
            {
                return;
            }
            AdapterWorkOrderArgument argument;
            argument.m_key = key;
            argument.m_value = value;
            arguments.push_back(AZStd::move(argument));
        }

        void AddArguments(
            AZStd::vector<AdapterWorkOrderArgument>& arguments,
            const AZStd::string& key,
            const AZStd::vector<AZStd::string>& values)
        {
            for (const AZStd::string& value : values)
            {
                AddArgument(arguments, key, value);
            }
        }

        AZStd::string BoolString(bool value)
        {
            return value ? AZStd::string("true") : AZStd::string("false");
        }

        AZStd::string UnsignedString(AZ::u64 value)
        {
            char buffer[32];
            size_t position = sizeof(buffer);
            do
            {
                buffer[--position] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(buffer + position, sizeof(buffer) - position);
        }

        AZStd::string DoubleString(double value)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(17) << value;
            const std::string text = stream.str();
            return AZStd::string(text.data(), text.size());
        }

        void SortArguments(AZStd::vector<AdapterWorkOrderArgument>& arguments)
        {
            AZStd::sort(
                arguments.begin(),
                arguments.end(),
                [](const AdapterWorkOrderArgument& left,
                   const AdapterWorkOrderArgument& right)
                {
                    if (left.m_key != right.m_key)
                    {
                        return left.m_key < right.m_key;
                    }
                    return left.m_value < right.m_value;
                });
            arguments.erase(
                AZStd::unique(
                    arguments.begin(),
                    arguments.end(),
                    [](const AdapterWorkOrderArgument& left,
                       const AdapterWorkOrderArgument& right)
                    {
                        return left.m_key == right.m_key
                            && left.m_value == right.m_value;
                    }),
                arguments.end());
        }

        const PackManifest* FindPack(
            const AZStd::vector<PackManifest>& packs,
            const AZStd::string& packId)
        {
            for (const PackManifest& pack : packs)
            {
                if (pack.m_packId == packId)
                {
                    return &pack;
                }
            }
            return nullptr;
        }

        AZ::u64 CapabilityRank(const AZStd::string& capability)
        {
            for (AZ::u64 index = 0;
                 index < sizeof(AllCapabilities) / sizeof(AllCapabilities[0]);
                 ++index)
            {
                if (ToString(AllCapabilities[index]) == capability)
                {
                    return index;
                }
            }
            return sizeof(AllCapabilities) / sizeof(AllCapabilities[0]);
        }

        bool IsCurrentValidated(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
        }

        bool IsCurrentValidated(const CatalogRelationship& relationship)
        {
            return relationship.m_validationState == "validated"
                && relationship.m_stalenessState == "current"
                && relationship.m_missingRefs.empty()
                && relationship.m_conflictRefs.empty()
                && relationship.m_supersededByRelationshipId.empty();
        }

        bool EvidenceSetIsValidForSubjects(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::vector<AZStd::string>& allowedSubjectRefs,
            const GameProfile& profile,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            if (evidenceIds.empty() || allowedSubjectRefs.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> uniqueIds = evidenceIds;
            SortUnique(uniqueIds);
            if (uniqueIds.size() != evidenceIds.size())
            {
                return false;
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence
                    || !Contains(allowedSubjectRefs, evidence->m_subjectRef)
                    || evidence->m_claim.empty()
                    || evidence->m_evidenceKind.empty()
                    || evidence->m_locator.empty()
                    || evidence->m_recordPath.empty()
                    || !IsStrictUtcTimestamp(evidence->m_extractedAt)
                    || !IsSha256Fingerprint(evidence->m_sourceFingerprint))
                {
                    return false;
                }
                const SourceRecord* source =
                    sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source
                    || !IsUsableImportStatus(source->m_importStatus)
                    || evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != source->m_profileId
                    || evidence->m_gameVersion != source->m_gameVersion
                    || evidence->m_branch != source->m_branch
                    || source->m_profileId != profile.m_profileId
                    || source->m_gameVersion != profile.m_gameVersion
                    || source->m_branch != profile.m_branch
                    || source->m_runtimeTarget != profile.m_runtimeTarget)
                {
                    return false;
                }
            }
            return true;
        }
