/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "WorldModels.h"
#include <AzCore/Serialization/SerializeContext.h>

namespace TaintedGrailModdingSDK
{
    void WorldPlaceProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<WorldPlaceProfile>()->Version(1)
                ->Field("RecordId", &WorldPlaceProfile::m_recordId)
                ->Field("ParentRecordId", &WorldPlaceProfile::m_parentRecordId)
                ->Field("Description", &WorldPlaceProfile::m_description)
                ->Field("HasPosition", &WorldPlaceProfile::m_hasPosition)
                ->Field("X", &WorldPlaceProfile::m_x)
                ->Field("Z", &WorldPlaceProfile::m_z)
                ->Field("EvidenceIds", &WorldPlaceProfile::m_evidenceIds);
        }
    }
    void WorldPathProfile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<WorldPathProfile>()->Version(1)
                ->Field("RecordId", &WorldPathProfile::m_recordId)
                ->Field("SceneRecordId", &WorldPathProfile::m_sceneRecordId)
                ->Field("RoadRecordId", &WorldPathProfile::m_roadRecordId)
                ->Field("Description", &WorldPathProfile::m_description)
                ->Field("TravelConstraints", &WorldPathProfile::m_travelConstraints)
                ->Field("EvidenceIds", &WorldPathProfile::m_evidenceIds);
        }
    }
    void WorldPathNode::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<WorldPathNode>()->Version(1)
                ->Field("NodeId", &WorldPathNode::m_nodeId)
                ->Field("PathRecordId", &WorldPathNode::m_pathRecordId)
                ->Field("LocationRecordId", &WorldPathNode::m_locationRecordId)
                ->Field("Notes", &WorldPathNode::m_notes)
                ->Field("EvidenceIds", &WorldPathNode::m_evidenceIds);
        }
    }
    void WorldPathEdge::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<WorldPathEdge>()->Version(1)
                ->Field("EdgeId", &WorldPathEdge::m_edgeId)
                ->Field("PathRecordId", &WorldPathEdge::m_pathRecordId)
                ->Field("FromNodeId", &WorldPathEdge::m_fromNodeId)
                ->Field("ToNodeId", &WorldPathEdge::m_toNodeId)
                ->Field("TravelMode", &WorldPathEdge::m_travelMode)
                ->Field("Bidirectional", &WorldPathEdge::m_bidirectional)
                ->Field("TravelCost", &WorldPathEdge::m_travelCost)
                ->Field("Notes", &WorldPathEdge::m_notes)
                ->Field("EvidenceIds", &WorldPathEdge::m_evidenceIds);
        }
    }
}
