/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */


#include "CapabilityExecutionContracts.h"
#include <AzCore/UnitTest/UnitTest.h>
#include <AzTest/AzTest.h>
#include <limits>
AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
namespace CE = TaintedGrailModdingSDK::CapabilityExecution;
using namespace CE;

TEST(CapabilityExecutionTokens, Phase)
{
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("MATERIALIZE", parsed)); EXPECT_EQ(parsed, Phase::MATERIALIZE); EXPECT_EQ(Token(parsed), "MATERIALIZE"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("BUILD", parsed)); EXPECT_EQ(parsed, Phase::BUILD); EXPECT_EQ(Token(parsed), "BUILD"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("PACKAGE", parsed)); EXPECT_EQ(parsed, Phase::PACKAGE); EXPECT_EQ(Token(parsed), "PACKAGE"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("DEPLOY", parsed)); EXPECT_EQ(parsed, Phase::DEPLOY); EXPECT_EQ(Token(parsed), "DEPLOY"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("LAUNCH", parsed)); EXPECT_EQ(parsed, Phase::LAUNCH); EXPECT_EQ(Token(parsed), "LAUNCH"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("VERIFY", parsed)); EXPECT_EQ(parsed, Phase::VERIFY); EXPECT_EQ(Token(parsed), "VERIFY"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("ASSESS", parsed)); EXPECT_EQ(parsed, Phase::ASSESS); EXPECT_EQ(Token(parsed), "ASSESS"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("RECONCILE", parsed)); EXPECT_EQ(parsed, Phase::RECONCILE); EXPECT_EQ(Token(parsed), "RECONCILE"); }
    { Phase parsed = Phase::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK", parsed)); EXPECT_EQ(parsed, Phase::ROLLBACK); EXPECT_EQ(Token(parsed), "ROLLBACK"); }
    Phase untouched = Phase::MATERIALIZE;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, Phase::MATERIALIZE);
    EXPECT_TRUE(Token(static_cast<Phase>(999)).empty());
    EXPECT_TRUE(Token(Phase::INVALID).empty());
}

TEST(CapabilityExecutionTokens, SideEffect)
{
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("READ_ONLY", parsed)); EXPECT_EQ(parsed, SideEffect::READ_ONLY); EXPECT_EQ(Token(parsed), "READ_ONLY"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("WORKSPACE_WRITE", parsed)); EXPECT_EQ(parsed, SideEffect::WORKSPACE_WRITE); EXPECT_EQ(Token(parsed), "WORKSPACE_WRITE"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("STAGING_WRITE", parsed)); EXPECT_EQ(parsed, SideEffect::STAGING_WRITE); EXPECT_EQ(Token(parsed), "STAGING_WRITE"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("PROCESS_LAUNCH", parsed)); EXPECT_EQ(parsed, SideEffect::PROCESS_LAUNCH); EXPECT_EQ(Token(parsed), "PROCESS_LAUNCH"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("INSTALLATION_MUTATION", parsed)); EXPECT_EQ(parsed, SideEffect::INSTALLATION_MUTATION); EXPECT_EQ(Token(parsed), "INSTALLATION_MUTATION"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("RUNTIME_MUTATION", parsed)); EXPECT_EQ(parsed, SideEffect::RUNTIME_MUTATION); EXPECT_EQ(Token(parsed), "RUNTIME_MUTATION"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("SAVE_MUTATION", parsed)); EXPECT_EQ(parsed, SideEffect::SAVE_MUTATION); EXPECT_EQ(Token(parsed), "SAVE_MUTATION"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("SECRET_USE", parsed)); EXPECT_EQ(parsed, SideEffect::SECRET_USE); EXPECT_EQ(Token(parsed), "SECRET_USE"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("NETWORK_PUBLICATION", parsed)); EXPECT_EQ(parsed, SideEffect::NETWORK_PUBLICATION); EXPECT_EQ(Token(parsed), "NETWORK_PUBLICATION"); }
    { SideEffect parsed = SideEffect::INVALID; EXPECT_TRUE(ParseToken("DESTRUCTIVE_DELETE", parsed)); EXPECT_EQ(parsed, SideEffect::DESTRUCTIVE_DELETE); EXPECT_EQ(Token(parsed), "DESTRUCTIVE_DELETE"); }
    SideEffect untouched = SideEffect::READ_ONLY;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, SideEffect::READ_ONLY);
    EXPECT_TRUE(Token(static_cast<SideEffect>(999)).empty());
    EXPECT_TRUE(Token(SideEffect::INVALID).empty());
}

TEST(CapabilityExecutionTokens, RollbackSupport)
{
    { RollbackSupport parsed = RollbackSupport::INVALID; EXPECT_TRUE(ParseToken("NONE", parsed)); EXPECT_EQ(parsed, RollbackSupport::NONE); EXPECT_EQ(Token(parsed), "NONE"); }
    { RollbackSupport parsed = RollbackSupport::INVALID; EXPECT_TRUE(ParseToken("CLEANUP_ONLY", parsed)); EXPECT_EQ(parsed, RollbackSupport::CLEANUP_ONLY); EXPECT_EQ(Token(parsed), "CLEANUP_ONLY"); }
    { RollbackSupport parsed = RollbackSupport::INVALID; EXPECT_TRUE(ParseToken("COMPENSATING", parsed)); EXPECT_EQ(parsed, RollbackSupport::COMPENSATING); EXPECT_EQ(Token(parsed), "COMPENSATING"); }
    { RollbackSupport parsed = RollbackSupport::INVALID; EXPECT_TRUE(ParseToken("EXACT_RESTORE", parsed)); EXPECT_EQ(parsed, RollbackSupport::EXACT_RESTORE); EXPECT_EQ(Token(parsed), "EXACT_RESTORE"); }
    RollbackSupport untouched = RollbackSupport::NONE;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, RollbackSupport::NONE);
    EXPECT_TRUE(Token(static_cast<RollbackSupport>(999)).empty());
    EXPECT_TRUE(Token(RollbackSupport::INVALID).empty());
}

TEST(CapabilityExecutionTokens, SupportState)
{
    { SupportState parsed = SupportState::INVALID; EXPECT_TRUE(ParseToken("SUPPORTED", parsed)); EXPECT_EQ(parsed, SupportState::SUPPORTED); EXPECT_EQ(Token(parsed), "SUPPORTED"); }
    { SupportState parsed = SupportState::INVALID; EXPECT_TRUE(ParseToken("UNSUPPORTED", parsed)); EXPECT_EQ(parsed, SupportState::UNSUPPORTED); EXPECT_EQ(Token(parsed), "UNSUPPORTED"); }
    SupportState untouched = SupportState::SUPPORTED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, SupportState::SUPPORTED);
    EXPECT_TRUE(Token(static_cast<SupportState>(999)).empty());
    EXPECT_TRUE(Token(SupportState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, QualificationState)
{
    { QualificationState parsed = QualificationState::INVALID; EXPECT_TRUE(ParseToken("QUALIFIED", parsed)); EXPECT_EQ(parsed, QualificationState::QUALIFIED); EXPECT_EQ(Token(parsed), "QUALIFIED"); }
    { QualificationState parsed = QualificationState::INVALID; EXPECT_TRUE(ParseToken("UNQUALIFIED", parsed)); EXPECT_EQ(parsed, QualificationState::UNQUALIFIED); EXPECT_EQ(Token(parsed), "UNQUALIFIED"); }
    { QualificationState parsed = QualificationState::INVALID; EXPECT_TRUE(ParseToken("STALE", parsed)); EXPECT_EQ(parsed, QualificationState::STALE); EXPECT_EQ(Token(parsed), "STALE"); }
    { QualificationState parsed = QualificationState::INVALID; EXPECT_TRUE(ParseToken("UNKNOWN", parsed)); EXPECT_EQ(parsed, QualificationState::UNKNOWN); EXPECT_EQ(Token(parsed), "UNKNOWN"); }
    QualificationState untouched = QualificationState::QUALIFIED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, QualificationState::QUALIFIED);
    EXPECT_TRUE(Token(static_cast<QualificationState>(999)).empty());
    EXPECT_TRUE(Token(QualificationState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, EnvironmentState)
{
    { EnvironmentState parsed = EnvironmentState::INVALID; EXPECT_TRUE(ParseToken("AVAILABLE", parsed)); EXPECT_EQ(parsed, EnvironmentState::AVAILABLE); EXPECT_EQ(Token(parsed), "AVAILABLE"); }
    { EnvironmentState parsed = EnvironmentState::INVALID; EXPECT_TRUE(ParseToken("UNAVAILABLE", parsed)); EXPECT_EQ(parsed, EnvironmentState::UNAVAILABLE); EXPECT_EQ(Token(parsed), "UNAVAILABLE"); }
    { EnvironmentState parsed = EnvironmentState::INVALID; EXPECT_TRUE(ParseToken("DRIFTED", parsed)); EXPECT_EQ(parsed, EnvironmentState::DRIFTED); EXPECT_EQ(Token(parsed), "DRIFTED"); }
    { EnvironmentState parsed = EnvironmentState::INVALID; EXPECT_TRUE(ParseToken("UNKNOWN", parsed)); EXPECT_EQ(parsed, EnvironmentState::UNKNOWN); EXPECT_EQ(Token(parsed), "UNKNOWN"); }
    EnvironmentState untouched = EnvironmentState::AVAILABLE;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, EnvironmentState::AVAILABLE);
    EXPECT_TRUE(Token(static_cast<EnvironmentState>(999)).empty());
    EXPECT_TRUE(Token(EnvironmentState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, PolicyState)
{
    { PolicyState parsed = PolicyState::INVALID; EXPECT_TRUE(ParseToken("ALLOWED", parsed)); EXPECT_EQ(parsed, PolicyState::ALLOWED); EXPECT_EQ(Token(parsed), "ALLOWED"); }
    { PolicyState parsed = PolicyState::INVALID; EXPECT_TRUE(ParseToken("CONFIRMATION_REQUIRED", parsed)); EXPECT_EQ(parsed, PolicyState::CONFIRMATION_REQUIRED); EXPECT_EQ(Token(parsed), "CONFIRMATION_REQUIRED"); }
    { PolicyState parsed = PolicyState::INVALID; EXPECT_TRUE(ParseToken("DENIED", parsed)); EXPECT_EQ(parsed, PolicyState::DENIED); EXPECT_EQ(Token(parsed), "DENIED"); }
    PolicyState untouched = PolicyState::ALLOWED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, PolicyState::ALLOWED);
    EXPECT_TRUE(Token(static_cast<PolicyState>(999)).empty());
    EXPECT_TRUE(Token(PolicyState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, AuthorizationState)
{
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("NOT_REQUIRED", parsed)); EXPECT_EQ(parsed, AuthorizationState::NOT_REQUIRED); EXPECT_EQ(Token(parsed), "NOT_REQUIRED"); }
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("PENDING", parsed)); EXPECT_EQ(parsed, AuthorizationState::PENDING); EXPECT_EQ(Token(parsed), "PENDING"); }
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("GRANTED", parsed)); EXPECT_EQ(parsed, AuthorizationState::GRANTED); EXPECT_EQ(Token(parsed), "GRANTED"); }
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("EXPIRED", parsed)); EXPECT_EQ(parsed, AuthorizationState::EXPIRED); EXPECT_EQ(Token(parsed), "EXPIRED"); }
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("REVOKED", parsed)); EXPECT_EQ(parsed, AuthorizationState::REVOKED); EXPECT_EQ(Token(parsed), "REVOKED"); }
    { AuthorizationState parsed = AuthorizationState::INVALID; EXPECT_TRUE(ParseToken("SCOPE_MISMATCH", parsed)); EXPECT_EQ(parsed, AuthorizationState::SCOPE_MISMATCH); EXPECT_EQ(Token(parsed), "SCOPE_MISMATCH"); }
    AuthorizationState untouched = AuthorizationState::NOT_REQUIRED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, AuthorizationState::NOT_REQUIRED);
    EXPECT_TRUE(Token(static_cast<AuthorizationState>(999)).empty());
    EXPECT_TRUE(Token(AuthorizationState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, Outcome)
{
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("NOT_ATTEMPTED", parsed)); EXPECT_EQ(parsed, Outcome::NOT_ATTEMPTED); EXPECT_EQ(Token(parsed), "NOT_ATTEMPTED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("RUNNING", parsed)); EXPECT_EQ(parsed, Outcome::RUNNING); EXPECT_EQ(Token(parsed), "RUNNING"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("SUCCEEDED", parsed)); EXPECT_EQ(parsed, Outcome::SUCCEEDED); EXPECT_EQ(Token(parsed), "SUCCEEDED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, Outcome::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("SKIPPED", parsed)); EXPECT_EQ(parsed, Outcome::SKIPPED); EXPECT_EQ(Token(parsed), "SKIPPED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("BLOCKED", parsed)); EXPECT_EQ(parsed, Outcome::BLOCKED); EXPECT_EQ(Token(parsed), "BLOCKED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("CANCELLED", parsed)); EXPECT_EQ(parsed, Outcome::CANCELLED); EXPECT_EQ(Token(parsed), "CANCELLED"); }
    { Outcome parsed = Outcome::INVALID; EXPECT_TRUE(ParseToken("PARTIAL", parsed)); EXPECT_EQ(parsed, Outcome::PARTIAL); EXPECT_EQ(Token(parsed), "PARTIAL"); }
    Outcome untouched = Outcome::NOT_ATTEMPTED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, Outcome::NOT_ATTEMPTED);
    EXPECT_TRUE(Token(static_cast<Outcome>(999)).empty());
    EXPECT_TRUE(Token(Outcome::INVALID).empty());
}

TEST(CapabilityExecutionTokens, VerificationState)
{
    { VerificationState parsed = VerificationState::INVALID; EXPECT_TRUE(ParseToken("NOT_CHECKED", parsed)); EXPECT_EQ(parsed, VerificationState::NOT_CHECKED); EXPECT_EQ(Token(parsed), "NOT_CHECKED"); }
    { VerificationState parsed = VerificationState::INVALID; EXPECT_TRUE(ParseToken("PASSED", parsed)); EXPECT_EQ(parsed, VerificationState::PASSED); EXPECT_EQ(Token(parsed), "PASSED"); }
    { VerificationState parsed = VerificationState::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, VerificationState::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { VerificationState parsed = VerificationState::INVALID; EXPECT_TRUE(ParseToken("UNKNOWN", parsed)); EXPECT_EQ(parsed, VerificationState::UNKNOWN); EXPECT_EQ(Token(parsed), "UNKNOWN"); }
    VerificationState untouched = VerificationState::NOT_CHECKED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, VerificationState::NOT_CHECKED);
    EXPECT_TRUE(Token(static_cast<VerificationState>(999)).empty());
    EXPECT_TRUE(Token(VerificationState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, AssessmentState)
{
    { AssessmentState parsed = AssessmentState::INVALID; EXPECT_TRUE(ParseToken("NOT_ASSESSED", parsed)); EXPECT_EQ(parsed, AssessmentState::NOT_ASSESSED); EXPECT_EQ(Token(parsed), "NOT_ASSESSED"); }
    { AssessmentState parsed = AssessmentState::INVALID; EXPECT_TRUE(ParseToken("ACCEPTED", parsed)); EXPECT_EQ(parsed, AssessmentState::ACCEPTED); EXPECT_EQ(Token(parsed), "ACCEPTED"); }
    { AssessmentState parsed = AssessmentState::INVALID; EXPECT_TRUE(ParseToken("REJECTED", parsed)); EXPECT_EQ(parsed, AssessmentState::REJECTED); EXPECT_EQ(Token(parsed), "REJECTED"); }
    { AssessmentState parsed = AssessmentState::INVALID; EXPECT_TRUE(ParseToken("NEEDS_REVIEW", parsed)); EXPECT_EQ(parsed, AssessmentState::NEEDS_REVIEW); EXPECT_EQ(Token(parsed), "NEEDS_REVIEW"); }
    AssessmentState untouched = AssessmentState::NOT_ASSESSED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, AssessmentState::NOT_ASSESSED);
    EXPECT_TRUE(Token(static_cast<AssessmentState>(999)).empty());
    EXPECT_TRUE(Token(AssessmentState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, PromotionState)
{
    { PromotionState parsed = PromotionState::INVALID; EXPECT_TRUE(ParseToken("NOT_PROMOTED", parsed)); EXPECT_EQ(parsed, PromotionState::NOT_PROMOTED); EXPECT_EQ(Token(parsed), "NOT_PROMOTED"); }
    { PromotionState parsed = PromotionState::INVALID; EXPECT_TRUE(ParseToken("CANDIDATE", parsed)); EXPECT_EQ(parsed, PromotionState::CANDIDATE); EXPECT_EQ(Token(parsed), "CANDIDATE"); }
    { PromotionState parsed = PromotionState::INVALID; EXPECT_TRUE(ParseToken("PROMOTED", parsed)); EXPECT_EQ(parsed, PromotionState::PROMOTED); EXPECT_EQ(Token(parsed), "PROMOTED"); }
    { PromotionState parsed = PromotionState::INVALID; EXPECT_TRUE(ParseToken("REJECTED", parsed)); EXPECT_EQ(parsed, PromotionState::REJECTED); EXPECT_EQ(Token(parsed), "REJECTED"); }
    PromotionState untouched = PromotionState::NOT_PROMOTED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, PromotionState::NOT_PROMOTED);
    EXPECT_TRUE(Token(static_cast<PromotionState>(999)).empty());
    EXPECT_TRUE(Token(PromotionState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, ReleaseDecisionState)
{
    { ReleaseDecisionState parsed = ReleaseDecisionState::INVALID; EXPECT_TRUE(ParseToken("NOT_DECIDED", parsed)); EXPECT_EQ(parsed, ReleaseDecisionState::NOT_DECIDED); EXPECT_EQ(Token(parsed), "NOT_DECIDED"); }
    { ReleaseDecisionState parsed = ReleaseDecisionState::INVALID; EXPECT_TRUE(ParseToken("APPROVED", parsed)); EXPECT_EQ(parsed, ReleaseDecisionState::APPROVED); EXPECT_EQ(Token(parsed), "APPROVED"); }
    { ReleaseDecisionState parsed = ReleaseDecisionState::INVALID; EXPECT_TRUE(ParseToken("REJECTED", parsed)); EXPECT_EQ(parsed, ReleaseDecisionState::REJECTED); EXPECT_EQ(Token(parsed), "REJECTED"); }
    ReleaseDecisionState untouched = ReleaseDecisionState::NOT_DECIDED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, ReleaseDecisionState::NOT_DECIDED);
    EXPECT_TRUE(Token(static_cast<ReleaseDecisionState>(999)).empty());
    EXPECT_TRUE(Token(ReleaseDecisionState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, ArtifactLifecycle)
{
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("DECLARED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::DECLARED); EXPECT_EQ(Token(parsed), "DECLARED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("PRODUCED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::PRODUCED); EXPECT_EQ(Token(parsed), "PRODUCED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("VERIFIED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::VERIFIED); EXPECT_EQ(Token(parsed), "VERIFIED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("STAGED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::STAGED); EXPECT_EQ(Token(parsed), "STAGED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("DEPLOYED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::DEPLOYED); EXPECT_EQ(Token(parsed), "DEPLOYED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("RETIRED", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::RETIRED); EXPECT_EQ(Token(parsed), "RETIRED"); }
    { ArtifactLifecycle parsed = ArtifactLifecycle::INVALID; EXPECT_TRUE(ParseToken("BACKUP", parsed)); EXPECT_EQ(parsed, ArtifactLifecycle::BACKUP); EXPECT_EQ(Token(parsed), "BACKUP"); }
    ArtifactLifecycle untouched = ArtifactLifecycle::DECLARED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, ArtifactLifecycle::DECLARED);
    EXPECT_TRUE(Token(static_cast<ArtifactLifecycle>(999)).empty());
    EXPECT_TRUE(Token(ArtifactLifecycle::INVALID).empty());
}

TEST(CapabilityExecutionTokens, RedistributionState)
{
    { RedistributionState parsed = RedistributionState::INVALID; EXPECT_TRUE(ParseToken("UNKNOWN", parsed)); EXPECT_EQ(parsed, RedistributionState::UNKNOWN); EXPECT_EQ(Token(parsed), "UNKNOWN"); }
    { RedistributionState parsed = RedistributionState::INVALID; EXPECT_TRUE(ParseToken("PERMITTED", parsed)); EXPECT_EQ(parsed, RedistributionState::PERMITTED); EXPECT_EQ(Token(parsed), "PERMITTED"); }
    { RedistributionState parsed = RedistributionState::INVALID; EXPECT_TRUE(ParseToken("FORBIDDEN", parsed)); EXPECT_EQ(parsed, RedistributionState::FORBIDDEN); EXPECT_EQ(Token(parsed), "FORBIDDEN"); }
    RedistributionState untouched = RedistributionState::UNKNOWN;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, RedistributionState::UNKNOWN);
    EXPECT_TRUE(Token(static_cast<RedistributionState>(999)).empty());
    EXPECT_TRUE(Token(RedistributionState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, PreimagePresence)
{
    { PreimagePresence parsed = PreimagePresence::INVALID; EXPECT_TRUE(ParseToken("ABSENT", parsed)); EXPECT_EQ(parsed, PreimagePresence::ABSENT); EXPECT_EQ(Token(parsed), "ABSENT"); }
    { PreimagePresence parsed = PreimagePresence::INVALID; EXPECT_TRUE(ParseToken("PRESENT", parsed)); EXPECT_EQ(parsed, PreimagePresence::PRESENT); EXPECT_EQ(Token(parsed), "PRESENT"); }
    PreimagePresence untouched = PreimagePresence::ABSENT;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, PreimagePresence::ABSENT);
    EXPECT_TRUE(Token(static_cast<PreimagePresence>(999)).empty());
    EXPECT_TRUE(Token(PreimagePresence::INVALID).empty());
}

TEST(CapabilityExecutionTokens, MutationOperation)
{
    { MutationOperation parsed = MutationOperation::INVALID; EXPECT_TRUE(ParseToken("CREATE", parsed)); EXPECT_EQ(parsed, MutationOperation::CREATE); EXPECT_EQ(Token(parsed), "CREATE"); }
    { MutationOperation parsed = MutationOperation::INVALID; EXPECT_TRUE(ParseToken("REPLACE", parsed)); EXPECT_EQ(parsed, MutationOperation::REPLACE); EXPECT_EQ(Token(parsed), "REPLACE"); }
    { MutationOperation parsed = MutationOperation::INVALID; EXPECT_TRUE(ParseToken("REMOVE", parsed)); EXPECT_EQ(parsed, MutationOperation::REMOVE); EXPECT_EQ(Token(parsed), "REMOVE"); }
    MutationOperation untouched = MutationOperation::CREATE;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, MutationOperation::CREATE);
    EXPECT_TRUE(Token(static_cast<MutationOperation>(999)).empty());
    EXPECT_TRUE(Token(MutationOperation::INVALID).empty());
}

TEST(CapabilityExecutionTokens, RollbackAction)
{
    { RollbackAction parsed = RollbackAction::INVALID; EXPECT_TRUE(ParseToken("REMOVE_CREATED", parsed)); EXPECT_EQ(parsed, RollbackAction::REMOVE_CREATED); EXPECT_EQ(Token(parsed), "REMOVE_CREATED"); }
    { RollbackAction parsed = RollbackAction::INVALID; EXPECT_TRUE(ParseToken("RESTORE_BACKUP", parsed)); EXPECT_EQ(parsed, RollbackAction::RESTORE_BACKUP); EXPECT_EQ(Token(parsed), "RESTORE_BACKUP"); }
    { RollbackAction parsed = RollbackAction::INVALID; EXPECT_TRUE(ParseToken("COMPENSATE", parsed)); EXPECT_EQ(parsed, RollbackAction::COMPENSATE); EXPECT_EQ(Token(parsed), "COMPENSATE"); }
    RollbackAction untouched = RollbackAction::REMOVE_CREATED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, RollbackAction::REMOVE_CREATED);
    EXPECT_TRUE(Token(static_cast<RollbackAction>(999)).empty());
    EXPECT_TRUE(Token(RollbackAction::INVALID).empty());
}

TEST(CapabilityExecutionTokens, CleanupState)
{
    { CleanupState parsed = CleanupState::INVALID; EXPECT_TRUE(ParseToken("NOT_REQUIRED", parsed)); EXPECT_EQ(parsed, CleanupState::NOT_REQUIRED); EXPECT_EQ(Token(parsed), "NOT_REQUIRED"); }
    { CleanupState parsed = CleanupState::INVALID; EXPECT_TRUE(ParseToken("PENDING", parsed)); EXPECT_EQ(parsed, CleanupState::PENDING); EXPECT_EQ(Token(parsed), "PENDING"); }
    { CleanupState parsed = CleanupState::INVALID; EXPECT_TRUE(ParseToken("SUCCEEDED", parsed)); EXPECT_EQ(parsed, CleanupState::SUCCEEDED); EXPECT_EQ(Token(parsed), "SUCCEEDED"); }
    { CleanupState parsed = CleanupState::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, CleanupState::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { CleanupState parsed = CleanupState::INVALID; EXPECT_TRUE(ParseToken("SKIPPED", parsed)); EXPECT_EQ(parsed, CleanupState::SKIPPED); EXPECT_EQ(Token(parsed), "SKIPPED"); }
    CleanupState untouched = CleanupState::NOT_REQUIRED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, CleanupState::NOT_REQUIRED);
    EXPECT_TRUE(Token(static_cast<CleanupState>(999)).empty());
    EXPECT_TRUE(Token(CleanupState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, RollbackState)
{
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("NOT_REQUIRED", parsed)); EXPECT_EQ(parsed, RollbackState::NOT_REQUIRED); EXPECT_EQ(Token(parsed), "NOT_REQUIRED"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("NOT_ATTEMPTED", parsed)); EXPECT_EQ(parsed, RollbackState::NOT_ATTEMPTED); EXPECT_EQ(Token(parsed), "NOT_ATTEMPTED"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("SUCCEEDED", parsed)); EXPECT_EQ(parsed, RollbackState::SUCCEEDED); EXPECT_EQ(Token(parsed), "SUCCEEDED"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, RollbackState::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("PARTIAL", parsed)); EXPECT_EQ(parsed, RollbackState::PARTIAL); EXPECT_EQ(Token(parsed), "PARTIAL"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("SKIPPED", parsed)); EXPECT_EQ(parsed, RollbackState::SKIPPED); EXPECT_EQ(Token(parsed), "SKIPPED"); }
    { RollbackState parsed = RollbackState::INVALID; EXPECT_TRUE(ParseToken("CANCELLED", parsed)); EXPECT_EQ(parsed, RollbackState::CANCELLED); EXPECT_EQ(Token(parsed), "CANCELLED"); }
    RollbackState untouched = RollbackState::NOT_REQUIRED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, RollbackState::NOT_REQUIRED);
    EXPECT_TRUE(Token(static_cast<RollbackState>(999)).empty());
    EXPECT_TRUE(Token(RollbackState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, ExecutionState)
{
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("DRAFT", parsed)); EXPECT_EQ(parsed, ExecutionState::DRAFT); EXPECT_EQ(Token(parsed), "DRAFT"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("VALIDATED", parsed)); EXPECT_EQ(parsed, ExecutionState::VALIDATED); EXPECT_EQ(Token(parsed), "VALIDATED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("RESOLVED", parsed)); EXPECT_EQ(parsed, ExecutionState::RESOLVED); EXPECT_EQ(Token(parsed), "RESOLVED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("QUALIFIED", parsed)); EXPECT_EQ(parsed, ExecutionState::QUALIFIED); EXPECT_EQ(Token(parsed), "QUALIFIED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("PLANNED", parsed)); EXPECT_EQ(parsed, ExecutionState::PLANNED); EXPECT_EQ(Token(parsed), "PLANNED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("AWAITING_AUTHORIZATION", parsed)); EXPECT_EQ(parsed, ExecutionState::AWAITING_AUTHORIZATION); EXPECT_EQ(Token(parsed), "AWAITING_AUTHORIZATION"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("READY", parsed)); EXPECT_EQ(parsed, ExecutionState::READY); EXPECT_EQ(Token(parsed), "READY"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("EXECUTING", parsed)); EXPECT_EQ(parsed, ExecutionState::EXECUTING); EXPECT_EQ(Token(parsed), "EXECUTING"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("VERIFYING", parsed)); EXPECT_EQ(parsed, ExecutionState::VERIFYING); EXPECT_EQ(Token(parsed), "VERIFYING"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("SUCCEEDED", parsed)); EXPECT_EQ(parsed, ExecutionState::SUCCEEDED); EXPECT_EQ(Token(parsed), "SUCCEEDED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("REJECTED", parsed)); EXPECT_EQ(parsed, ExecutionState::REJECTED); EXPECT_EQ(Token(parsed), "REJECTED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("RESOLUTION_FAILED", parsed)); EXPECT_EQ(parsed, ExecutionState::RESOLUTION_FAILED); EXPECT_EQ(Token(parsed), "RESOLUTION_FAILED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("QUALIFICATION_FAILED", parsed)); EXPECT_EQ(parsed, ExecutionState::QUALIFICATION_FAILED); EXPECT_EQ(Token(parsed), "QUALIFICATION_FAILED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("POLICY_DENIED", parsed)); EXPECT_EQ(parsed, ExecutionState::POLICY_DENIED); EXPECT_EQ(Token(parsed), "POLICY_DENIED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("AUTHORIZATION_EXPIRED", parsed)); EXPECT_EQ(parsed, ExecutionState::AUTHORIZATION_EXPIRED); EXPECT_EQ(Token(parsed), "AUTHORIZATION_EXPIRED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ENVIRONMENT_DRIFTED", parsed)); EXPECT_EQ(parsed, ExecutionState::ENVIRONMENT_DRIFTED); EXPECT_EQ(Token(parsed), "ENVIRONMENT_DRIFTED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, ExecutionState::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("PARTIAL", parsed)); EXPECT_EQ(parsed, ExecutionState::PARTIAL); EXPECT_EQ(Token(parsed), "PARTIAL"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("CANCELLATION_REQUESTED", parsed)); EXPECT_EQ(parsed, ExecutionState::CANCELLATION_REQUESTED); EXPECT_EQ(Token(parsed), "CANCELLATION_REQUESTED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("CANCELLED", parsed)); EXPECT_EQ(parsed, ExecutionState::CANCELLED); EXPECT_EQ(Token(parsed), "CANCELLED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_REQUIRED", parsed)); EXPECT_EQ(parsed, ExecutionState::ROLLBACK_REQUIRED); EXPECT_EQ(Token(parsed), "ROLLBACK_REQUIRED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ROLLING_BACK", parsed)); EXPECT_EQ(parsed, ExecutionState::ROLLING_BACK); EXPECT_EQ(Token(parsed), "ROLLING_BACK"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ROLLED_BACK", parsed)); EXPECT_EQ(parsed, ExecutionState::ROLLED_BACK); EXPECT_EQ(Token(parsed), "ROLLED_BACK"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_FAILED", parsed)); EXPECT_EQ(parsed, ExecutionState::ROLLBACK_FAILED); EXPECT_EQ(Token(parsed), "ROLLBACK_FAILED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("SUPERSEDED", parsed)); EXPECT_EQ(parsed, ExecutionState::SUPERSEDED); EXPECT_EQ(Token(parsed), "SUPERSEDED"); }
    { ExecutionState parsed = ExecutionState::INVALID; EXPECT_TRUE(ParseToken("ARCHIVED", parsed)); EXPECT_EQ(parsed, ExecutionState::ARCHIVED); EXPECT_EQ(Token(parsed), "ARCHIVED"); }
    ExecutionState untouched = ExecutionState::DRAFT;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, ExecutionState::DRAFT);
    EXPECT_TRUE(Token(static_cast<ExecutionState>(999)).empty());
    EXPECT_TRUE(Token(ExecutionState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, PhaseState)
{
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("NOT_PLANNED", parsed)); EXPECT_EQ(parsed, PhaseState::NOT_PLANNED); EXPECT_EQ(Token(parsed), "NOT_PLANNED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("PENDING", parsed)); EXPECT_EQ(parsed, PhaseState::PENDING); EXPECT_EQ(Token(parsed), "PENDING"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("READY", parsed)); EXPECT_EQ(parsed, PhaseState::READY); EXPECT_EQ(Token(parsed), "READY"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("RUNNING", parsed)); EXPECT_EQ(parsed, PhaseState::RUNNING); EXPECT_EQ(Token(parsed), "RUNNING"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("SUCCEEDED", parsed)); EXPECT_EQ(parsed, PhaseState::SUCCEEDED); EXPECT_EQ(Token(parsed), "SUCCEEDED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("FAILED", parsed)); EXPECT_EQ(parsed, PhaseState::FAILED); EXPECT_EQ(Token(parsed), "FAILED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("SKIPPED", parsed)); EXPECT_EQ(parsed, PhaseState::SKIPPED); EXPECT_EQ(Token(parsed), "SKIPPED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("BLOCKED", parsed)); EXPECT_EQ(parsed, PhaseState::BLOCKED); EXPECT_EQ(Token(parsed), "BLOCKED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("CANCELLED", parsed)); EXPECT_EQ(parsed, PhaseState::CANCELLED); EXPECT_EQ(Token(parsed), "CANCELLED"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_PENDING", parsed)); EXPECT_EQ(parsed, PhaseState::ROLLBACK_PENDING); EXPECT_EQ(Token(parsed), "ROLLBACK_PENDING"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("ROLLED_BACK", parsed)); EXPECT_EQ(parsed, PhaseState::ROLLED_BACK); EXPECT_EQ(Token(parsed), "ROLLED_BACK"); }
    { PhaseState parsed = PhaseState::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_FAILED", parsed)); EXPECT_EQ(parsed, PhaseState::ROLLBACK_FAILED); EXPECT_EQ(Token(parsed), "ROLLBACK_FAILED"); }
    PhaseState untouched = PhaseState::NOT_PLANNED;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, PhaseState::NOT_PLANNED);
    EXPECT_TRUE(Token(static_cast<PhaseState>(999)).empty());
    EXPECT_TRUE(Token(PhaseState::INVALID).empty());
}

TEST(CapabilityExecutionTokens, ContractKind)
{
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_DESCRIPTOR", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_DESCRIPTOR); EXPECT_EQ(Token(parsed), "CAPABILITY_DESCRIPTOR"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_PROVIDER_BINDING", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_PROVIDER_BINDING); EXPECT_EQ(Token(parsed), "CAPABILITY_PROVIDER_BINDING"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ARTIFACT_REFERENCE", parsed)); EXPECT_EQ(parsed, ContractKind::ARTIFACT_REFERENCE); EXPECT_EQ(Token(parsed), "ARTIFACT_REFERENCE"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("EXPECTED_ARTIFACT", parsed)); EXPECT_EQ(parsed, ContractKind::EXPECTED_ARTIFACT); EXPECT_EQ(Token(parsed), "EXPECTED_ARTIFACT"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ARTIFACT_RECORD", parsed)); EXPECT_EQ(parsed, ContractKind::ARTIFACT_RECORD); EXPECT_EQ(Token(parsed), "ARTIFACT_RECORD"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("OPTION", parsed)); EXPECT_EQ(parsed, ContractKind::OPTION); EXPECT_EQ(Token(parsed), "OPTION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_EXECUTION_REQUEST", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_EXECUTION_REQUEST); EXPECT_EQ(Token(parsed), "CAPABILITY_EXECUTION_REQUEST"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_SUPPORT_DECISION", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_SUPPORT_DECISION); EXPECT_EQ(Token(parsed), "CAPABILITY_SUPPORT_DECISION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_QUALIFICATION_DECISION", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_QUALIFICATION_DECISION); EXPECT_EQ(Token(parsed), "CAPABILITY_QUALIFICATION_DECISION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_ENVIRONMENT_DECISION", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_ENVIRONMENT_DECISION); EXPECT_EQ(Token(parsed), "CAPABILITY_ENVIRONMENT_DECISION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_POLICY_DECISION", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_POLICY_DECISION); EXPECT_EQ(Token(parsed), "CAPABILITY_POLICY_DECISION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_AUTHORIZATION_RECEIPT", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_AUTHORIZATION_RECEIPT); EXPECT_EQ(Token(parsed), "CAPABILITY_AUTHORIZATION_RECEIPT"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("TARGET_MUTATION_CLAIM", parsed)); EXPECT_EQ(parsed, ContractKind::TARGET_MUTATION_CLAIM); EXPECT_EQ(Token(parsed), "TARGET_MUTATION_CLAIM"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_STEP", parsed)); EXPECT_EQ(parsed, ContractKind::ROLLBACK_STEP); EXPECT_EQ(Token(parsed), "ROLLBACK_STEP"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_PLAN", parsed)); EXPECT_EQ(parsed, ContractKind::ROLLBACK_PLAN); EXPECT_EQ(Token(parsed), "ROLLBACK_PLAN"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_PHASE_PLAN", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_PHASE_PLAN); EXPECT_EQ(Token(parsed), "CAPABILITY_PHASE_PLAN"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_EXECUTION_PLAN", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_EXECUTION_PLAN); EXPECT_EQ(Token(parsed), "CAPABILITY_EXECUTION_PLAN"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("FAILURE_RECORD", parsed)); EXPECT_EQ(parsed, ContractKind::FAILURE_RECORD); EXPECT_EQ(Token(parsed), "FAILURE_RECORD"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("DIAGNOSTIC_REFERENCE", parsed)); EXPECT_EQ(parsed, ContractKind::DIAGNOSTIC_REFERENCE); EXPECT_EQ(Token(parsed), "DIAGNOSTIC_REFERENCE"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("TARGET_OBSERVATION", parsed)); EXPECT_EQ(parsed, ContractKind::TARGET_OBSERVATION); EXPECT_EQ(Token(parsed), "TARGET_OBSERVATION"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("PHASE_EXTENSION_REFERENCE", parsed)); EXPECT_EQ(parsed, ContractKind::PHASE_EXTENSION_REFERENCE); EXPECT_EQ(Token(parsed), "PHASE_EXTENSION_REFERENCE"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_PHASE_RECEIPT", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_PHASE_RECEIPT); EXPECT_EQ(Token(parsed), "CAPABILITY_PHASE_RECEIPT"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_STEP_RECEIPT", parsed)); EXPECT_EQ(parsed, ContractKind::ROLLBACK_STEP_RECEIPT); EXPECT_EQ(Token(parsed), "ROLLBACK_STEP_RECEIPT"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("ROLLBACK_RECEIPT", parsed)); EXPECT_EQ(parsed, ContractKind::ROLLBACK_RECEIPT); EXPECT_EQ(Token(parsed), "ROLLBACK_RECEIPT"); }
    { ContractKind parsed = ContractKind::INVALID; EXPECT_TRUE(ParseToken("CAPABILITY_EXECUTION_RECEIPT", parsed)); EXPECT_EQ(parsed, ContractKind::CAPABILITY_EXECUTION_RECEIPT); EXPECT_EQ(Token(parsed), "CAPABILITY_EXECUTION_RECEIPT"); }
    ContractKind untouched = ContractKind::CAPABILITY_DESCRIPTOR;
    EXPECT_FALSE(ParseToken("unknown", untouched));
    EXPECT_EQ(untouched, ContractKind::CAPABILITY_DESCRIPTOR);
    EXPECT_TRUE(Token(static_cast<ContractKind>(999)).empty());
    EXPECT_TRUE(Token(ContractKind::INVALID).empty());
}

TEST(CapabilityExecutionPrimitives, RejectsUnsafeLocatorsAndIdentityAliases)
{
    for (const char* path : {"../a", "/tmp/a", "C:/a", "C:a", "a\\b", "a//b", "a/./b", "a/../b", "a.", "a ", "CON", "a/NUL.txt", "https://a"})
    { EXPECT_FALSE(IsRelativeLocator(path)) << path; }
    EXPECT_TRUE(IsRelativeLocator("artifacts/mesh-1.bin"));
    EXPECT_TRUE(IsRelativeLocator(AZStd::string(MaximumPathBytes, 'a')));
    EXPECT_FALSE(IsRelativeLocator(AZStd::string(MaximumPathBytes + 1, 'a')));
    EXPECT_TRUE(IsStableId("pack.123.test")); EXPECT_FALSE(IsStableId("unqualified"));
    EXPECT_FALSE(IsStableId("pack..test")); EXPECT_FALSE(IsStableId("1pack.test"));
    EXPECT_TRUE(IsStableId("p." + AZStd::string(MaximumIdBytes - 2, 'a')));
    EXPECT_FALSE(IsStableId("p." + AZStd::string(MaximumIdBytes - 1, 'a')));
}
TEST(CapabilityExecutionPrimitives, ExactVersionDigestUtcAndUtf8)
{
    EXPECT_TRUE(IsExactVersion("1.2.3")); EXPECT_FALSE(IsExactVersion("01.2.3"));
    EXPECT_FALSE(IsExactVersion("^1.2.3")); EXPECT_FALSE(IsExactVersion("1.2.3-beta"));
    EXPECT_TRUE(IsUtcTimestamp("2024-02-29T23:59:59Z")); EXPECT_FALSE(IsUtcTimestamp("2025-02-29T00:00:00Z"));
    EXPECT_FALSE(IsUtcTimestamp("2024-01-01T00:00:60Z")); EXPECT_FALSE(IsUtcTimestamp("2024-01-01T00:00:00+00:00"));
    EXPECT_TRUE(IsDigest("sha256:" + AZStd::string(64, 'a'))); EXPECT_FALSE(IsDigest("sha256:" + AZStd::string(64, 'A')));
    EXPECT_TRUE(IsSafeText("caf\xc3\xa9")); EXPECT_FALSE(IsSafeText("\xc0\xaf"));
    EXPECT_FALSE(IsSafeText("\xed\xa0\x80")); EXPECT_FALSE(IsSafeText("bad\ntext"));
    for (const char* value : {"token=private", "password=private", "C:/Users/private", "cmd.exe", "$(run)", "a;run"})
    { EXPECT_FALSE(IsSafeText(value)); }
}
TEST(CapabilityExecutionBudgets, ExactLimitAndOverflowLeaveBudgetUnchanged)
{
    SizeBudget budget{0, 64};
    EXPECT_TRUE(budget.Add(8, 8)); EXPECT_EQ(budget.m_used, 64);
    EXPECT_FALSE(budget.Add(1)); EXPECT_EQ(budget.m_used, 64);
    SizeBudget huge{7, (std::numeric_limits<size_t>::max)()};
    EXPECT_FALSE(huge.Add((std::numeric_limits<size_t>::max)(), 2)); EXPECT_EQ(huge.m_used, 7);
    EXPECT_TRUE(huge.Add((std::numeric_limits<size_t>::max)(), 0)); EXPECT_EQ(huge.m_used, 7);
}
