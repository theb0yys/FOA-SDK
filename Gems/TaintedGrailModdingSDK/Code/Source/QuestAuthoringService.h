/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "QuestAuthoringModels.h"
namespace TaintedGrailModdingSDK
{
    class CatalogDatabase;
    class QuestAuthoringService
    {
    public:
        static QuestInspection Inspect(const QuestAuthoringDraft&, const CatalogDatabase&);
        static QuestAuthoringDraft Read(const QuestAuthoringProfile&);
        static QuestAuthoringProfile CanonicalProfile(const QuestAuthoringDraft&);
        static AZStd::string Label(const QuestAuthoringDraft&, const AZStd::string&);
        static AZStd::string Revision(const QuestAuthoringProfile&);
        static bool IsReferencedSubject(const QuestDefinitionV1&, const AZStd::string&);
        static AZStd::vector<AZStd::string> InternalIds(const QuestDefinitionV1&);
    };
}
