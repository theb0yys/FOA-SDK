/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    bool FoundationService::PromoteEvidenceToCatalog(
        const CatalogPromotionRequest& request,
        AZStd::string* error)
    {
        AZ::Outcome<CatalogRecord, AZStd::string> promotion = m_catalogPromotion.BuildReviewedRecord(
            request,
            m_workspace,
            m_packs,
            m_sourceRegistry);
        if (!promotion.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(promotion.GetError());
            }
            return false;
        }

        CatalogDatabase candidate = m_catalog;
        AZStd::string catalogError;
        if (!candidate.InsertNew(promotion.GetValue(), &catalogError))
        {
            if (error)
            {
                *error = catalogError;
            }
            return false;
        }
        return PersistCatalogCandidate(candidate, error);
    }

    bool FoundationService::UpsertCatalogRelationship(
        const CatalogRelationship& relationship,
        AZStd::string* error)
    {
        CatalogDatabase candidate = m_catalog;
        AZStd::string catalogError;
        if (!candidate.UpsertRelationship(relationship, &catalogError))
        {
            if (error)
            {
                *error = catalogError;
            }
            return false;
        }
        return PersistCatalogCandidate(candidate, error);
    }

    bool FoundationService::AddCatalogValidationEvent(
        const CatalogValidationEvent& validation,
        AZStd::string* error)
    {
        CatalogDatabase candidate = m_catalog;
        AZStd::string catalogError;
        if (!candidate.AddValidationEvent(validation, &catalogError))
        {
            if (error)
            {
                *error = catalogError;
            }
            return false;
        }
        return PersistCatalogCandidate(candidate, error);
    }

    bool FoundationService::SaveCatalog(AZStd::string* error)
    {
        return PersistCatalogCandidate(m_catalog, error);
    }

    bool FoundationService::ReloadCatalog(AZStd::string* error)
    {
        if (m_workspace.m_rootPath.empty())
        {
            m_catalog.Clear();
            m_catalogFilePath.clear();
            RefreshSnapshot();
            return true;
        }
        if (!m_catalogPersistence.Exists(m_workspace.m_rootPath))
        {
            m_catalog.Clear();
            m_catalogFilePath.clear();
            RefreshSnapshot();
            return true;
        }

        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            if (error)
            {
                *error = "An exact active game profile is required before the canonical catalog can be loaded.";
            }
            return false;
        }

        AZ::Outcome<CatalogDocument, AZStd::string> loadResult = m_catalogPersistence.Load(m_workspace.m_rootPath);
        if (!loadResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(loadResult.GetError());
            }
            return false;
        }

        CatalogDocument document = loadResult.TakeValue();
        if (document.m_workspaceId != m_workspace.m_workspaceId
            || document.m_profileId != profile->m_profileId
            || document.m_gameVersion != profile->m_gameVersion
            || document.m_branch != profile->m_branch)
        {
            if (error)
            {
                *error = "The canonical catalog document is bound to a different workspace or game profile.";
            }
            return false;
        }

        CatalogDatabase candidate;
        AZStd::string catalogError;
        if (!candidate.ReplaceFromDocument(document, &catalogError))
        {
            if (error)
            {
                *error = catalogError;
            }
            return false;
        }

        m_catalog = AZStd::move(candidate);
        m_catalogFilePath = m_catalogPersistence.GetCatalogPath(m_workspace.m_rootPath);
        RefreshSnapshot();
        return true;
    }

    const AZStd::string& FoundationService::GetCatalogFilePath() const
    {
        return m_catalogFilePath;
    }

    bool FoundationService::PersistCatalogCandidate(
        const CatalogDatabase& candidate,
        AZStd::string* error)
    {
        const GameProfile* profile = m_workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            if (error)
            {
                *error = "An exact active game profile is required before the canonical catalog can be saved.";
            }
            return false;
        }
        if (m_workspace.m_rootPath.empty() || m_workspace.m_workspaceId.empty())
        {
            if (error)
            {
                *error = "A configured workspace root and workspace ID are required before the catalog can be saved.";
            }
            return false;
        }

        const CatalogDocument document = candidate.BuildDocument(m_workspace, *profile);
        AZ::Outcome<AZStd::string, AZStd::string> saveResult = m_catalogPersistence.Save(
            document,
            m_workspace.m_rootPath);
        if (!saveResult.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(saveResult.GetError());
            }
            return false;
        }

        m_catalog = candidate;
        m_catalogFilePath = saveResult.TakeValue();
        RefreshSnapshot();
        return true;
    }
} // namespace TaintedGrailModdingSDK
