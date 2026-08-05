/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct GameProfile;
    class SourceEvidenceRegistry;

    enum class ActorAppearanceBindingRole
    {
        Portrait,
        Model,
    };

    struct ActorAppearanceBindingRequest
    {
        AZStd::string m_actorRecordId;
        ActorAppearanceBindingRole m_role = ActorAppearanceBindingRole::Model;
        AZStd::string m_productAssetId;
        AZStd::string m_sourceAssetSubjectRef;
        AZStd::vector<AZStd::string> m_productEvidenceIds;
    };

    struct ActorAppearanceBindingResult
    {
        CatalogDatabase m_catalog;
        CatalogRelationship m_provenanceRelationship;
    };

    //! Builds one complete candidate containing both the actor-profile reference
    //! and its provenance relationship. It performs no persistence or publication.
    class ActorAppearanceBindingService final
    {
    public:
        static AZ::Outcome<ActorAppearanceBindingResult, AZStd::string> BuildCandidate(
            const ActorAppearanceBindingRequest& request,
            const GameProfile& activeProfile,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& currentCatalog);
    };
} // namespace TaintedGrailModdingSDK
