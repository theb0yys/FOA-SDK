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
        if (source.m_fingerprint.empty()
            || source.m_profileId.empty()
            || source.m_gameVersion.empty()
            || source.m_branch.empty()
            || source.m_runtimeTarget.empty())
        {
            if (error)
            {
                *error = "Source fingerprint and exact game-profile binding are required.";
            }
            return false;
        }
        if (source.m_importerId.empty() || source.m_importerVersion.empty())
        {
            if (error)
            {
                *error = "Source importer identity and version are required.";
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
        if (FindSourceByFingerprint(source.m_fingerprint, source.m_profileId))
        {
            if (error)
            {
                *error = "This artifact fingerprint is already registered for the active game profile.";
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

        const SourceRecord* source = FindSource(evidence.m_sourceId);
        if (!source)
        {
            if (error)
            {
                *error = "Evidence references an unknown source.";
            }
            return false;
        }
        if (evidence.m_sourceFingerprint != source->m_fingerprint
            || evidence.m_profileId != source->m_profileId
            || evidence.m_gameVersion != source->m_gameVersion
            || evidence.m_branch != source->m_branch)
        {
            if (error)
            {
                *error = "Evidence profile, build, branch, or fingerprint does not match its source.";
            }
            return false;
        }
        if (evidence.m_subjectRef.empty() || evidence.m_claim.empty())
        {
            if (error)
            {
                *error = "Evidence subject reference and claim are required.";
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

    const SourceRecord* SourceEvidenceRegistry::FindSourceByFingerprint(
        const AZStd::string& fingerprint,
        const AZStd::string& profileId) const
    {
        for (const SourceRecord& source : m_sources)
        {
            if (source.m_fingerprint == fingerprint && source.m_profileId == profileId)
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

    AZStd::vector<EvidenceRecord> SourceEvidenceRegistry::FindEvidenceForSource(const AZStd::string& sourceId) const
    {
        AZStd::vector<EvidenceRecord> matches;
        for (const EvidenceRecord& evidence : m_evidence)
        {
            if (evidence.m_sourceId == sourceId)
            {
                matches.push_back(evidence);
            }
        }
        return matches;
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
