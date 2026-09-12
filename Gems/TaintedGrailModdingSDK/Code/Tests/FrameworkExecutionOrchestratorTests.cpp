/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExecutionFramework/FrameworkTargetOwnershipLedger.h"
#include "FrameworkExecutionTestFixtures.h"
using namespace TaintedGrailModdingSDK::ExecutionFramework;
using namespace TaintedGrailModdingSDK::ExecutionFramework::Tests;

TEST(FrameworkOwnership, SameLocationDifferentOwnerConflicts)
{
    Fixture f;
    auto plan = f.Plan();
    FrameworkTargetOwnershipLedger ledger;
    ASSERT_TRUE(ledger.Reserve(plan.m_plan));
    auto changed = plan.m_plan;
    changed.m_phases[0].m_expectedOutputs[0].m_ownerPackId = "pack.other";
    ASSERT_TRUE(Seal(changed.m_phases[0].m_expectedOutputs[0]));
    ASSERT_TRUE(Seal(changed.m_phases[0]));
    ASSERT_TRUE(Seal(changed));
    EXPECT_EQ(ledger.Reserve(changed).m_error, Error::Conflict);
    ledger.Release(plan.m_plan.m_fingerprint, false);
    EXPECT_EQ(ledger.Reserve(changed).m_error, Error::Conflict);
    ledger.Release(plan.m_plan.m_fingerprint, true);
    EXPECT_TRUE(ledger.Reserve(changed));
}
TEST(FrameworkService, InertUntilOpenAndUnknownPlanRejected)
{
    Fixture f;
    FrameworkExecutionService service(f.m_context, {});
    Snapshot result;
    EXPECT_EQ(service.Submit(Digest(), result).m_error, Error::Stopped);
    EXPECT_EQ(service.Status("execution.missing", result).m_error, Error::NotFound);
    EXPECT_TRUE(service.Page(0, 1000).empty());
    EXPECT_FALSE(service.Busy());
    service.Shutdown();
}

TEST(FrameworkOwnership, VerifiedPreimageBindsLocationMutationOwnerAndContent)
{
    FrameworkTargetOwnershipLedger ledger;
    CE::ArtifactReferenceV1 backup;
    backup.m_id = "artifact.backup";
    backup.m_payloadContractId = "data.text";
    backup.m_ownerPackId = "pack.owner";
    backup.m_storageRootId = "root.backups";
    backup.m_relativePath = "previous.txt";
    backup.m_digest = Digest("old");
    backup.m_byteSize = 3;
    backup.m_custodianId = "custodian.framework";
    backup.m_lifecycle = CE::ArtifactLifecycle::BACKUP;
    ASSERT_TRUE(Seal(backup));
    CE::TargetMutationClaimV1 claim;
    claim.m_id = "mutation.remove";
    claim.m_targetRootId = "root.target";
    claim.m_relativePath = "file.txt";
    claim.m_operation = CE::MutationOperation::REMOVE;
    claim.m_ownerPackId = backup.m_ownerPackId;
    claim.m_preimagePresence = CE::PreimagePresence::PRESENT;
    claim.m_preimageFingerprint = backup.m_digest;
    claim.m_preimageOwnerId = backup.m_ownerPackId;
    claim.m_backupArtifact = backup;
    claim.m_rollbackStepId = "rollback.restore";
    ASSERT_TRUE(Seal(claim));
    CE::TargetObservationV1 observation;
    observation.m_id = "observation.target";
    observation.m_mutationId = claim.m_id;
    observation.m_targetRootId = claim.m_targetRootId;
    observation.m_relativePath = claim.m_relativePath;
    observation.m_ownerPackId = claim.m_ownerPackId;
    observation.m_presence = claim.m_preimagePresence;
    observation.m_contentFingerprint = claim.m_preimageFingerprint;
    observation.m_verification = CE::VerificationState::PASSED;
    ASSERT_TRUE(Seal(observation));
    EXPECT_TRUE(ledger.CheckPreimage(claim, observation));
    for (auto member : { &CE::TargetObservationV1::m_mutationId,
                         &CE::TargetObservationV1::m_targetRootId,
                         &CE::TargetObservationV1::m_relativePath,
                         &CE::TargetObservationV1::m_ownerPackId })
    {
        auto changed = observation;
        changed.*member += ".other";
        ASSERT_TRUE(Seal(changed));
        EXPECT_EQ(ledger.CheckPreimage(claim, changed).m_error, Error::Drifted);
    }
    observation.m_contentFingerprint = Digest("drifted");
    ASSERT_TRUE(Seal(observation));
    EXPECT_EQ(ledger.CheckPreimage(claim, observation).m_error, Error::Drifted);
}
