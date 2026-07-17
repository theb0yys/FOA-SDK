/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/utility/move.h>

#include <QHash>

namespace TaintedGrailModdingSDK
{
    struct SourceEvidenceDocumentPaths
    {
        AZStd::string m_sourceDocumentPath;
        AZStd::string m_evidenceDocumentPath;
    };

    class SourceEvidencePersistenceService
    {
    public:
        AZ::Outcome<SourceEvidenceDocumentPaths, AZStd::string> SaveDocuments(
            const SourceDocument& sourceDocument,
            const EvidenceDocument& evidenceDocument,
            const AZStd::string& workspaceRoot) const;

        AZ::Outcome<void, AZStd::string> LoadWorkspaceDocuments(
            const AZStd::string& workspaceRoot,
            AZStd::vector<SourceDocument>& sourceDocuments,
            AZStd::vector<EvidenceDocument>& evidenceDocuments,
            AZStd::vector<ImportIssue>& loadIssues) const;
    };
} // namespace TaintedGrailModdingSDK
