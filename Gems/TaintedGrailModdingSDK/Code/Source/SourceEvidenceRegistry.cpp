/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    bool SourceEvidenceRegistry::RegisterSource(const SourceRecord& source, AZStd::string* error)
    {
        if (source.m_sourceId.empty())
        {
            if (error)
            {
                *error = "Source ID is required.";
            }
            return false;
        }
        if (FindSource(source.m_sourceId))
        {
            if (error)
            {
                *error = "Source ID already exists.";
            }
            return false;
        }
        m_sources.push_back(source);
        return true;
    }

    bool SourceEvidenceRegistry::RegisterEvidence(const EvidenceRecord& evidence, AZStd::string* error)
    {
        if (evidence.m_evidenceId.empty() || evidence.m_sourceId.empty())
        {
            if (error)
            {
                *error = "Evidence ID and source ID are required.";
            }
            return false;
        }
        if (!FindSource(evidence.m_sourceId))
        {
            if (error)
            {
                *error = "Evidence references an unknown source.";
            }
            return false;
        }
        if (FindEvidence(evidence.m_evidenceId))
        {
            if (error)
            {
                *error = "Evidence ID already exists.";
            }
            return false;
        }
        m_evidence.push_back(evidence);
        return true;
    }

    void SourceEvidenceRegistry::Clear()
    {
        m_sources.clear();
        m_evidence.clear();
    }

    const SourceRecord* SourceEvidenceRegistry::FindSource(const AZStd::string& sourceId) const
    {
        for (const SourceRecord& source : m_sources)
        {
            if (source.m_sourceId == sourceId)
            {
                return &source;
            }
        }
        return nullptr;
    }

    const EvidenceRecord* SourceEvidenceRegistry::FindEvidence(const AZStd::string& evidenceId) const
    {
        for (const EvidenceRecord& evidence : m_evidence)
        {
            if (evidence.m_evidenceId == evidenceId)
            {
                return &evidence;
            }
        }
        return nullptr;
    }

    AZStd::vector<EvidenceRecord> SourceEvidenceRegistry::FindEvidenceForSubject(const AZStd::string& subjectRef) const
    {
        AZStd::vector<EvidenceRecord> matches;
        for (const EvidenceRecord& evidence : m_evidence)
        {
            if (evidence.m_subjectRef == subjectRef)
            {
                matches.push_back(evidence);
            }
        }
        return matches;
    }

    const AZStd::vector<SourceRecord>& SourceEvidenceRegistry::GetSources() const
    {
        return m_sources;
    }

    const AZStd::vector<EvidenceRecord>& SourceEvidenceRegistry::GetEvidence() const
    {
        return m_evidence;
    }
} // namespace TaintedGrailModdingSDK
