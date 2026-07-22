/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SourceEvidenceRegistry.h"

#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
        {
            if (value.size() > maximumLength || (!allowEmpty && value.empty()))
            {
                return false;
            }
            return AZStd::all_of(
                value.begin(), value.end(),
                [](char character)
                {
                    const unsigned char byte = static_cast<unsigned char>(character);
                    return byte >= 0x20 && byte != 0x7f;
                });
        }

        void SetError(AZStd::string* error, const char* message)
        {
            if (error)
            {
                *error = message;
            }
        }
    } // namespace

    SourceEvidenceRegistry::SourceEvidenceRegistry()
    {
        ReserveStableStorage();
    }

    SourceEvidenceRegistry::SourceEvidenceRegistry(
        const SourceEvidenceRegistry& other)
        : m_sources(other.m_sources)
        , m_evidence(other.m_evidence)
        , m_candidateEvidence(other.m_candidateEvidence)
    {
        ReserveStableStorage();
    }

    SourceEvidenceRegistry& SourceEvidenceRegistry::operator=(
        const SourceEvidenceRegistry& other)
    {
        if (this == &other)
        {
            return *this;
        }
        AZStd::vector<SourceRecord> sources;
        AZStd::vector<EvidenceRecord> evidence;
        AZStd::vector<EvidenceRecord> candidates;
        sources.reserve(MaximumSourceCount);
        evidence.reserve(MaximumEvidenceCount);
        candidates.reserve(MaximumCandidateEvidenceCount);
        sources.assign(other.m_sources.begin(), other.m_sources.end());
        evidence.assign(other.m_evidence.begin(), other.m_evidence.end());
        candidates.assign(
            other.m_candidateEvidence.begin(),
            other.m_candidateEvidence.end());
        m_sources.swap(sources);
        m_evidence.swap(evidence);
        m_candidateEvidence.swap(candidates);
        return *this;
    }

    void SourceEvidenceRegistry::ReserveStableStorage()
    {
        m_sources.reserve(MaximumSourceCount);
        m_evidence.reserve(MaximumEvidenceCount);
        m_candidateEvidence.reserve(MaximumCandidateEvidenceCount);
    }

    bool SourceEvidenceRegistry::RegisterSource(
        const SourceRecord& source,
        AZStd::string* error)
    {
        if (m_sources.size() >= MaximumSourceCount)
        {
            SetError(error, "The source registry reached its bounded capacity.");
            return false;
        }
        if (!IsSafePersistenceId(source.m_sourceId))
        {
            SetError(
                error,
                "Source ID must be a bounded lowercase namespaced persistence-safe ID.");
            return false;
        }
        if (!IsSha256Fingerprint(source.m_fingerprint)
            || !IsStableContractId(source.m_profileId)
            || !IsBoundedText(source.m_gameVersion, 128)
            || !IsBoundedText(source.m_branch, 128)
            || !IsSupportedRuntimeTarget(source.m_runtimeTarget))
        {
            SetError(
                error,
                "Source fingerprint and exact valid game-profile binding are required.");
            return false;
        }
        if (!IsBoundedText(source.m_title, 512)
            || !IsBoundedText(source.m_sourceKind, 128)
            || !IsBoundedText(source.m_locator, 2048)
            || !IsBoundedText(source.m_mediaType, 256)
            || !IsStableContractId(source.m_importerId)
            || !IsStrictSemanticVersion(source.m_importerVersion)
            || (!source.m_toolVersion.empty()
                && !IsStrictSemanticVersion(source.m_toolVersion))
            || !IsStrictUtcTimestamp(source.m_capturedAt)
            || !IsStrictUtcTimestamp(source.m_importedAt)
            || !IsUsableImportStatus(source.m_importStatus)
            || !IsBoundedText(source.m_limitations, 4096, true))
        {
            SetError(
                error,
                "Source metadata requires bounded control-free identity, locator, media type, strict versions, valid UTC times, and a usable import status.");
            return false;
        }
        if (source.m_importedAt < source.m_capturedAt)
        {
            SetError(error, "Source import time cannot precede capture time.");
            return false;
        }
        if (FindSource(source.m_sourceId))
        {
            SetError(error, "Source ID already exists.");
            return false;
        }
        if (FindSourceByFingerprint(source.m_fingerprint, source.m_profileId))
        {
            SetError(
                error,
                "This artifact fingerprint is already registered for the active game profile.");
            return false;
        }

        m_sources.push_back(source);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool SourceEvidenceRegistry::ValidateEvidenceRecord(
        const EvidenceRecord& evidence,
        AZStd::string* error) const
    {
        if (!IsStableContractId(evidence.m_evidenceId)
            || !IsSafePersistenceId(evidence.m_sourceId))
        {
            SetError(
                error,
                "Evidence and source identities must be bounded stable IDs.");
            return false;
        }

        const SourceRecord* source = FindSource(evidence.m_sourceId);
        if (!source)
        {
            SetError(error, "Evidence references an unknown source.");
            return false;
        }
        if (!IsUsableImportStatus(source->m_importStatus))
        {
            SetError(error, "Evidence cannot be registered from an unusable source import.");
            return false;
        }
        if (evidence.m_sourceFingerprint != source->m_fingerprint
            || evidence.m_profileId != source->m_profileId
            || evidence.m_gameVersion != source->m_gameVersion
            || evidence.m_branch != source->m_branch)
        {
            SetError(
                error,
                "Evidence profile, build, branch, or fingerprint does not match its source.");
            return false;
        }
        if (!IsSha256Fingerprint(evidence.m_sourceFingerprint)
            || !IsBoundedText(evidence.m_subjectRef, 1024)
            || !IsBoundedText(evidence.m_claim, 8192)
            || !IsBoundedText(evidence.m_evidenceKind, 128)
            || !IsBoundedText(evidence.m_confidence, 64)
            || !IsBoundedText(evidence.m_locator, 2048)
            || !IsBoundedText(evidence.m_recordPath, 2048)
            || !IsStrictUtcTimestamp(evidence.m_extractedAt))
        {
            SetError(
                error,
                "Evidence requires a stable fingerprint, bounded control-free provenance fields, and a valid UTC extraction time.");
            return false;
        }
        if (evidence.m_extractedAt < source->m_capturedAt
            || evidence.m_extractedAt > source->m_importedAt)
        {
            SetError(
                error,
                "Evidence extraction time must fall between source capture and import.");
            return false;
        }
        return true;
    }

    bool SourceEvidenceRegistry::RegisterEvidence(
        const EvidenceRecord& evidence,
        AZStd::string* error)
    {
        if (m_evidence.size() >= MaximumEvidenceCount)
        {
            SetError(error, "The evidence registry reached its bounded capacity.");
            return false;
        }
        if (!ValidateEvidenceRecord(evidence, error))
        {
            return false;
        }
        if (FindEvidence(evidence.m_evidenceId)
            || FindCandidateEvidence(evidence.m_evidenceId))
        {
            SetError(error, "Evidence ID already exists in the active or candidate registry.");
            return false;
        }

        m_evidence.push_back(evidence);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool SourceEvidenceRegistry::RegisterCandidateEvidence(
        const EvidenceRecord& evidence,
        AZStd::string* error)
    {
        if (m_candidateEvidence.size() >= MaximumCandidateEvidenceCount)
        {
            SetError(error, "The candidate-evidence registry reached its bounded capacity.");
            return false;
        }
        if (!ValidateEvidenceRecord(evidence, error))
        {
            return false;
        }
        if (FindEvidence(evidence.m_evidenceId)
            || FindCandidateEvidence(evidence.m_evidenceId))
        {
            SetError(error, "Evidence ID already exists in the active or candidate registry.");
            return false;
        }

        m_candidateEvidence.push_back(evidence);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool SourceEvidenceRegistry::PromoteCandidateEvidence(
        const AZStd::string& evidenceId,
        AZStd::string* error)
    {
        const auto candidate = AZStd::find_if(
            m_candidateEvidence.begin(), m_candidateEvidence.end(),
            [&evidenceId](const EvidenceRecord& evidence)
            {
                return evidence.m_evidenceId == evidenceId;
            });
        if (candidate == m_candidateEvidence.end())
        {
            SetError(error, "Candidate evidence does not exist.");
            return false;
        }
        if (m_evidence.size() >= MaximumEvidenceCount)
        {
            SetError(error, "The evidence registry reached its bounded capacity.");
            return false;
        }
        if (!ValidateEvidenceRecord(*candidate, error)
            || FindEvidence(evidenceId))
        {
            return false;
        }

        m_evidence.push_back(*candidate);
        m_candidateEvidence.erase(candidate);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool SourceEvidenceRegistry::RejectCandidateEvidence(
        const AZStd::string& evidenceId,
        AZStd::string* error)
    {
        const auto candidate = AZStd::find_if(
            m_candidateEvidence.begin(), m_candidateEvidence.end(),
            [&evidenceId](const EvidenceRecord& evidence)
            {
                return evidence.m_evidenceId == evidenceId;
            });
        if (candidate == m_candidateEvidence.end())
        {
            SetError(error, "Candidate evidence does not exist.");
            return false;
        }
        m_candidateEvidence.erase(candidate);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void SourceEvidenceRegistry::Clear()
    {
        m_sources.clear();
        m_evidence.clear();
        m_candidateEvidence.clear();
    }

    const SourceRecord* SourceEvidenceRegistry::FindSource(
        const AZStd::string& sourceId) const
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
            if (source.m_fingerprint == fingerprint
                && source.m_profileId == profileId)
            {
                return &source;
            }
        }
        return nullptr;
    }

    const EvidenceRecord* SourceEvidenceRegistry::FindEvidence(
        const AZStd::string& evidenceId) const
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

    const EvidenceRecord* SourceEvidenceRegistry::FindCandidateEvidence(
        const AZStd::string& evidenceId) const
    {
        for (const EvidenceRecord& evidence : m_candidateEvidence)
        {
            if (evidence.m_evidenceId == evidenceId)
            {
                return &evidence;
            }
        }
        return nullptr;
    }

    AZStd::vector<EvidenceRecord> SourceEvidenceRegistry::FindEvidenceForSource(
        const AZStd::string& sourceId) const
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

    AZStd::vector<EvidenceRecord> SourceEvidenceRegistry::FindEvidenceForSubject(
        const AZStd::string& subjectRef) const
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

    const AZStd::vector<EvidenceRecord>& SourceEvidenceRegistry::GetCandidateEvidence() const
    {
        return m_candidateEvidence;
    }
} // namespace TaintedGrailModdingSDK
