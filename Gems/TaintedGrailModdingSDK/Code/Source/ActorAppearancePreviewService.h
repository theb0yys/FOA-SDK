/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;

    enum class ActorAppearancePreviewState
    {
        Ready,
        Partial,
        Blocked,
    };

    struct ActorEquipmentPreviewEntry
    {
        AZStd::string m_slot;
        AZStd::string m_relationshipId;
        AZStd::string m_itemRecordId;
        AZStd::string m_itemSubjectRef;
        AZStd::string m_itemAssetRef;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_blockers;
        bool m_current = false;
    };

    struct ActorAppearancePreviewView
    {
        AZStd::string m_actorRecordId;
        AZStd::string m_actorSubjectRef;
        AZStd::string m_portraitAssetRef;
        AZStd::string m_modelAssetRef;
        AZStd::vector<ActorEquipmentPreviewEntry> m_equipment;
        AZStd::vector<AZStd::string> m_blockers;
        ActorAppearancePreviewState m_state = ActorAppearancePreviewState::Blocked;
        size_t m_relationshipsExamined = 0;
    };

    //! Engine-neutral Stage 8 projection for actor appearance and equipment preview.
    //! It never loads assets, mutates catalog state, grants permission, or invokes runtime code.
    class ActorAppearancePreviewService final
    {
    public:
        static ActorAppearancePreviewView BuildView(
            const CatalogDatabase& catalog,
            const AZStd::string& actorRecordId);

        static bool IsEquipmentRelationshipKind(const AZStd::string& relationshipKind);
        static AZStd::string SlotForRelationshipKind(const AZStd::string& relationshipKind);
        static const AZStd::vector<AZStd::string>& OrderedSlots();
    };
} // namespace TaintedGrailModdingSDK
