/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace AZ { class ReflectContext; }
namespace TaintedGrailModdingSDK
{
    struct WorldPlaceProfile
    {
        AZ_TYPE_INFO(WorldPlaceProfile, "{815681C7-7634-4567-B58B-6138FCAD8CC4}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_recordId;
        AZStd::string m_parentRecordId;
        AZStd::string m_description;
        bool m_hasPosition = false;
        double m_x = 0.0;
        double m_z = 0.0;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct WorldPathProfile
    {
        AZ_TYPE_INFO(WorldPathProfile, "{90E05AFD-55BE-47DB-9D83-87DF36653A3E}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_recordId;
        AZStd::string m_sceneRecordId;
        AZStd::string m_roadRecordId;
        AZStd::string m_description;
        AZStd::string m_travelConstraints;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct WorldPathNode
    {
        AZ_TYPE_INFO(WorldPathNode, "{B490F581-A92D-4594-8C52-EFAE13404D40}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_nodeId;
        AZStd::string m_pathRecordId;
        AZStd::string m_locationRecordId;
        AZStd::string m_notes;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct WorldPathEdge
    {
        AZ_TYPE_INFO(WorldPathEdge, "{82B50EBB-3033-4BB7-89B1-740062A648D1}");
        static void Reflect(AZ::ReflectContext* context);
        AZStd::string m_edgeId;
        AZStd::string m_pathRecordId;
        AZStd::string m_fromNodeId;
        AZStd::string m_toNodeId;
        AZStd::string m_travelMode = "walk";
        bool m_bidirectional = true;
        double m_travelCost = 1.0;
        AZStd::string m_notes;
        AZStd::vector<AZStd::string> m_evidenceIds;
    };
    struct WorldPathDefinition
    {
        WorldPathProfile m_profile;
        AZStd::vector<WorldPathNode> m_nodes;
        AZStd::vector<WorldPathEdge> m_edges;
    };
    struct WorldPathAnalysis
    {
        bool m_usesPlanPositions = false;
        AZ::u32 m_components = 0;
        AZStd::vector<AZStd::string> m_errors;
        AZStd::vector<AZStd::string> m_warnings;
        bool IsValid() const { return m_errors.empty(); }
    };
}
