/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExtensionAPI.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::ExtensionAPI
{
    namespace
    {
        bool RequireBoundClient(
            const Service* service,
            const AZStd::string& extensionId,
            AZStd::string* error)
        {
            if (service && !extensionId.empty())
            {
                return true;
            }
            if (error)
            {
                *error = "Extension operation requires an identity-bound client created by Foundation.";
            }
            return false;
        }
    } // namespace

    Client::Client(Service& service, AZStd::string extensionId)
        : m_service(&service)
        , m_extensionId(AZStd::move(extensionId))
    {
    }

    bool Client::IsBound() const
    {
        return m_service != nullptr && !m_extensionId.empty();
    }

    const AZStd::string& Client::GetExtensionId() const
    {
        return m_extensionId;
    }

    bool Client::GetActiveProfile(
        ProfileView& profile,
        AZStd::string* error) const
    {
        if (!RequireBoundClient(m_service, m_extensionId, error))
        {
            return false;
        }
        return m_service->GetActiveProfile(m_extensionId, profile, error);
    }

    bool Client::QueryCatalog(
        const CatalogQuery& query,
        AZStd::vector<CatalogRecord>& records,
        size_t maximumResults,
        AZStd::string* error) const
    {
        if (!RequireBoundClient(m_service, m_extensionId, error))
        {
            return false;
        }
        return m_service->QueryCatalog(
            m_extensionId,
            query,
            records,
            maximumResults,
            error);
    }

    bool Client::SubmitCandidateEvidence(
        const EvidenceRecord& evidence,
        AZStd::string* error)
    {
        if (!RequireBoundClient(m_service, m_extensionId, error))
        {
            return false;
        }
        return m_service->SubmitCandidateEvidence(
            m_extensionId,
            evidence,
            error);
    }
} // namespace TaintedGrailModdingSDK::ExtensionAPI
