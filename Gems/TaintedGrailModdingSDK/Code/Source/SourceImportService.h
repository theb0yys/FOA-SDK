/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/Outcome/Outcome.h>

class QByteArray;

namespace TaintedGrailModdingSDK
{
    class SourceImportService
    {
    public:
        AZStd::vector<SourceImporterContract> GetContracts() const;
        AZ::Outcome<SourceImportResult, AZStd::string> Import(
            const SourceImportRequest& request,
            const GameProfile& profile) const;

    private:
        static AZStd::string SelectImporterId(const SourceImportRequest& request);
        static AZStd::string SanitizeIdentifier(const AZStd::string& value);
        static AZStd::string MediaTypeForPath(const AZStd::string& path);
        static void AddIssue(
            AZStd::vector<ImportIssue>& issues,
            const AZStd::string& sourceId,
            AZStd::string severity,
            AZStd::string code,
            AZStd::string message,
            AZStd::string locator,
            AZStd::string recordPath = {},
            AZ::s64 line = 0);
        static void ExtractJsonEvidence(
            const QByteArray& data,
            const SourceRecord& source,
            EvidenceDocument& evidenceDocument);
        static void ExtractCsvEvidence(
            const QByteArray& data,
            const SourceRecord& source,
            EvidenceDocument& evidenceDocument);
    };
} // namespace TaintedGrailModdingSDK
