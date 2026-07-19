/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    class SourceEvidenceRegistry
    {
    public:
        static constexpr size_t MaximumSourceCount = 4096;
        static constexpr size_t MaximumEvidenceCount = 65536;

        SourceEvidenceRegistry();
        SourceEvidenceRegistry(const SourceEvidenceRegistry& other);
        SourceEvidenceRegistry& operator=(const SourceEvidenceRegistry& other);
        SourceEvidenceRegistry(SourceEvidenceRegistry&&) noexcept = default;
        SourceEvidenceRegistry& operator=(SourceEvidenceRegistry&&) noexcept = default;

        bool RegisterSource(const SourceRecord& source, AZStd::string* error = nullptr);
        bool RegisterEvidence(const EvidenceRecord& evidence, AZStd::string* error = nullptr);
        void Clear();

        const SourceRecord* FindSource(const AZStd::string& sourceId) const;
        const SourceRecord* FindSourceByFingerprint(
            const AZStd::string& fingerprint,
            const AZStd::string& profileId) const;
        const EvidenceRecord* FindEvidence(const AZStd::string& evidenceId) const;
        AZStd::vector<EvidenceRecord> FindEvidenceForSource(const AZStd::string& sourceId) const;
        AZStd::vector<EvidenceRecord> FindEvidenceForSubject(const AZStd::string& subjectRef) const;

        const AZStd::vector<SourceRecord>& GetSources() const;
        const AZStd::vector<EvidenceRecord>& GetEvidence() const;

    private:
        void ReserveStableStorage();

        AZStd::vector<SourceRecord> m_sources;
        AZStd::vector<EvidenceRecord> m_evidence;
    };
} // namespace TaintedGrailModdingSDK
