/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "ExtensionAPI.h"

#include <AzCore/EBus/EBus.h>

namespace TaintedGrailModdingSDK
{
    //! Cross-module access to the bounded ExtensionAPI and host-owned document store.
    //! Optional Tool Gems never receive mutable Foundation objects or workspace paths.
    class ExtensionRequests
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy =
            AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy =
            AZ::EBusAddressPolicy::Single;

        virtual ~ExtensionRequests() = default;

        virtual bool RegisterExtension(
            const ExtensionAPI::ExtensionDeclaration& declaration,
            AZStd::string* error) = 0;
        virtual bool UnregisterExtension(
            const AZStd::string& extensionId,
            AZStd::string* error) = 0;
        virtual bool IsExtensionRegistered(const AZStd::string& extensionId) const = 0;

        virtual bool GetActiveProfile(
            const AZStd::string& extensionId,
            ExtensionAPI::ProfileView& profile,
            AZStd::string* error) const = 0;
        virtual bool QueryCatalog(
            const AZStd::string& extensionId,
            const CatalogQuery& query,
            AZStd::vector<CatalogRecord>& records,
            size_t maximumResults,
            AZStd::string* error) const = 0;
        virtual bool SubmitCandidateEvidence(
            const AZStd::string& extensionId,
            const EvidenceRecord& evidence,
            AZStd::string* error) = 0;

        virtual bool SaveExtensionDocument(
            const AZStd::string& extensionId,
            const AZStd::string& relativePath,
            const AZStd::string& contents,
            AZStd::string* error) = 0;
        virtual bool LoadExtensionDocument(
            const AZStd::string& extensionId,
            const AZStd::string& relativePath,
            AZStd::string& contents,
            AZStd::string* error) const = 0;
    };

    using ExtensionRequestBus = AZ::EBus<ExtensionRequests>;
} // namespace TaintedGrailModdingSDK
