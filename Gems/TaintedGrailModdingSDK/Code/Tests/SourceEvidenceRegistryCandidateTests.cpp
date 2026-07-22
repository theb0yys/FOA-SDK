/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        SourceRecord MakeSource()
        {
            SourceRecord source;
            source.m_sourceId = "source.registry.candidate";
            source.m_title = "Candidate fixture source";
            source.m_sourceKind = "compiled_fixture";
            source.m_locator = "fixture://candidate/source";
            source.m_fingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            source.m_profileId = "profile.foa.mono";
            source.m_gameVersion = "1.0.0";
            source.m_branch = "mono";
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "importer.registry.fixture";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-21T12:00:00Z";
            source.m_importedAt = "2026-07-21T12:05:00Z";
            source.m_mediaType = "application/json";
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence(AZStd::string evidenceId)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = "source.registry.candidate";
            evidence.m_sourceFingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            evidence.m_profileId = "profile.foa.mono";
            evidence.m_gameVersion = "1.0.0";
            evidence.m_branch = "mono";
            evidence.m_subjectRef = "foa:actor:candidate";
            evidence.m_claim = "Candidate evidence remains pending until reviewed.";
            evidence.m_evidenceKind = "candidate_fixture";
            evidence.m_confidence = "documented";
            evidence.m_locator = "fixture://candidate/source#record";
            evidence.m_recordPath = "records[0]";
            evidence.m_extractedAt = "2026-07-21T12:03:00Z";
            return evidence;
        }
    } // namespace

    TEST(SourceEvidenceRegistryCandidateTests, CandidateEvidenceIsNotActiveUntilPromoted)
    {
        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource()));

        const EvidenceRecord evidence = MakeEvidence("evidence.registry.pending");
        ASSERT_TRUE(registry.RegisterCandidateEvidence(evidence));
        EXPECT_EQ(registry.FindEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_NE(registry.FindCandidateEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_TRUE(registry.FindEvidenceForSubject(evidence.m_subjectRef).empty());

        ASSERT_TRUE(registry.PromoteCandidateEvidence(evidence.m_evidenceId));
        EXPECT_NE(registry.FindEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_EQ(registry.FindCandidateEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_EQ(registry.FindEvidenceForSubject(evidence.m_subjectRef).size(), 1);
    }

    TEST(SourceEvidenceRegistryCandidateTests, CandidateEvidenceMayBeRejectedWithoutPublication)
    {
        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource()));

        const EvidenceRecord evidence = MakeEvidence("evidence.registry.rejected");
        ASSERT_TRUE(registry.RegisterCandidateEvidence(evidence));
        ASSERT_TRUE(registry.RejectCandidateEvidence(evidence.m_evidenceId));
        EXPECT_EQ(registry.FindCandidateEvidence(evidence.m_evidenceId), nullptr);
        EXPECT_EQ(registry.FindEvidence(evidence.m_evidenceId), nullptr);
    }

    TEST(SourceEvidenceRegistryCandidateTests, ActiveAndCandidateEvidenceIdsShareOneNamespace)
    {
        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource()));

        const EvidenceRecord evidence = MakeEvidence("evidence.registry.unique");
        ASSERT_TRUE(registry.RegisterCandidateEvidence(evidence));
        EXPECT_FALSE(registry.RegisterEvidence(evidence));
    }

    TEST(SourceEvidenceRegistryCandidateTests, CoreRegistryRejectsControlCharacters)
    {
        SourceEvidenceRegistry registry;
        SourceRecord source = MakeSource();
        source.m_title = "unsafe\nsource";
        EXPECT_FALSE(registry.RegisterSource(source));

        ASSERT_TRUE(registry.RegisterSource(MakeSource()));
        EvidenceRecord evidence = MakeEvidence("evidence.registry.controls");
        evidence.m_claim = "unsafe\tclaim";
        EXPECT_FALSE(registry.RegisterCandidateEvidence(evidence));
        EXPECT_FALSE(registry.RegisterEvidence(evidence));
    }
} // namespace TaintedGrailModdingSDK
