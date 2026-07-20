/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

// Keep the established catalog implementation byte-for-byte while extending the
// three document-bound methods below with population collections. The renamed
// methods remain private implementation details and are not alternate APIs.
#define ReplaceFromDocument ReplaceFromDocumentWithoutPopulation
#define Clear ClearWithoutPopulation
#define BuildDocument BuildDocumentWithoutPopulation
#include "CatalogDatabaseBase.inl"
#undef BuildDocument
#undef Clear
#undef ReplaceFromDocument

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class Value, class Identity>
        bool HasDuplicatePopulationIdentity(
            const AZStd::vector<Value>& values,
            Identity identity,
            AZStd::string& duplicateIdentity)
        {
            AZStd::vector<AZStd::string> identities;
            identities.reserve(values.size());
            for (const Value& value : values)
            {
                identities.push_back(identity(value));
            }
            AZStd::sort(identities.begin(), identities.end());
            const auto duplicate = AZStd::adjacent_find(
                identities.begin(),
                identities.end());
            if (duplicate == identities.end())
            {
                return false;
            }
            duplicateIdentity = *duplicate;
            return true;
        }

        bool ValidatePopulationDocumentIdentities(
            const CatalogDocument& document,
            AZStd::string* error)
        {
            AZStd::string duplicateIdentity;
            if (HasDuplicatePopulationIdentity(
                    document.m_actorProfiles,
                    [](const PopulationActorProfile& profile)
                    {
                        return profile.m_recordId;
                    },
                    duplicateIdentity))
            {
                if (error)
                {
                    *error = "Catalog document contains duplicate population actor "
                        "profile identity: " + duplicateIdentity;
                }
                return false;
            }
            if (HasDuplicatePopulationIdentity(
                    document.m_troopProfiles,
                    [](const PopulationTroopProfile& profile)
                    {
                        return profile.m_recordId;
                    },
                    duplicateIdentity))
            {
                if (error)
                {
                    *error = "Catalog document contains duplicate population troop "
                        "profile identity: " + duplicateIdentity;
                }
                return false;
            }
            if (HasDuplicatePopulationIdentity(
                    document.m_troopMembers,
                    [](const PopulationTroopMember& member)
                    {
                        return member.m_linkId;
                    },
                    duplicateIdentity))
            {
                if (error)
                {
                    *error = "Catalog document contains duplicate population troop "
                        "membership identity: " + duplicateIdentity;
                }
                return false;
            }
            return true;
        }

        template<class Value, class Less>
        void SortPopulationDocumentValues(
            AZStd::vector<Value>& values,
            Less less)
        {
            AZStd::sort(values.begin(), values.end(), less);
        }
    } // namespace

    bool CatalogDatabase::ReplaceFromDocument(
        const CatalogDocument& document,
        AZStd::string* error)
    {
        const bool isLegacySchema =
            document.m_schemaVersion == LegacyCatalogSchemaVersion;
        const bool isPopulationSchema =
            document.m_schemaVersion == PopulationCatalogSchemaVersion;
        if (!isLegacySchema && !isPopulationSchema)
        {
            if (error)
            {
                *error = "Unsupported canonical catalog schema version.";
            }
            return false;
        }
        if (isLegacySchema
            && (!document.m_actorProfiles.empty()
                || !document.m_troopProfiles.empty()
                || !document.m_troopMembers.empty()))
        {
            if (error)
            {
                *error = "Catalog schema 1 cannot contain population collections.";
            }
            return false;
        }
        if (!ValidatePopulationDocumentIdentities(document, error))
        {
            return false;
        }

        CatalogDocument legacyDocument = document;
        legacyDocument.m_schemaVersion = LegacyCatalogSchemaVersion;
        legacyDocument.m_actorProfiles.clear();
        legacyDocument.m_troopProfiles.clear();
        legacyDocument.m_troopMembers.clear();

        CatalogDatabase candidate;
        if (!candidate.ReplaceFromDocumentWithoutPopulation(
                legacyDocument,
                error))
        {
            return false;
        }
        for (const PopulationActorProfile& profile : document.m_actorProfiles)
        {
            if (!candidate.UpsertPopulationActorProfile(profile, error))
            {
                return false;
            }
        }
        for (const PopulationTroopProfile& profile : document.m_troopProfiles)
        {
            if (!candidate.UpsertPopulationTroopProfile(profile, error))
            {
                return false;
            }
        }
        for (const PopulationTroopMember& member : document.m_troopMembers)
        {
            if (!candidate.UpsertPopulationTroopMember(member, error))
            {
                return false;
            }
        }

        *this = AZStd::move(candidate);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void CatalogDatabase::Clear()
    {
        ClearWithoutPopulation();
        m_populationActorProfiles.clear();
        m_populationTroopProfiles.clear();
        m_populationTroopMembers.clear();
    }

    CatalogDocument CatalogDatabase::BuildDocument(
        const WorkspaceModel& workspace,
        const GameProfile& profile) const
    {
        CatalogDocument document = BuildDocumentWithoutPopulation(
            workspace,
            profile);
        document.m_actorProfiles = m_populationActorProfiles;
        document.m_troopProfiles = m_populationTroopProfiles;
        document.m_troopMembers = m_populationTroopMembers;

        SortPopulationDocumentValues(
            document.m_actorProfiles,
            [](const PopulationActorProfile& left,
               const PopulationActorProfile& right)
            {
                return left.m_recordId < right.m_recordId;
            });
        SortPopulationDocumentValues(
            document.m_troopProfiles,
            [](const PopulationTroopProfile& left,
               const PopulationTroopProfile& right)
            {
                return left.m_recordId < right.m_recordId;
            });
        SortPopulationDocumentValues(
            document.m_troopMembers,
            [](const PopulationTroopMember& left,
               const PopulationTroopMember& right)
            {
                return left.m_linkId < right.m_linkId;
            });

        const bool hasPopulationData = !document.m_actorProfiles.empty()
            || !document.m_troopProfiles.empty()
            || !document.m_troopMembers.empty();
        document.m_schemaVersion = hasPopulationData
            ? PopulationCatalogSchemaVersion
            : LegacyCatalogSchemaVersion;
        return document;
    }
} // namespace TaintedGrailModdingSDK
