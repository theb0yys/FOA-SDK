/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExecutionFramework/FrameworkToolExecutionAdapter.h"
#include "FrameworkExecutionTestFixtures.h"
using namespace TaintedGrailModdingSDK::ExecutionFramework;
using namespace TaintedGrailModdingSDK::ExecutionFramework::Tests;

TEST(FrameworkProvider, ExactResolutionAndRegistrationFinalization)
{
    Fixture f;
    FrameworkProviderService service;
    ASSERT_TRUE(service.Register(f.m_host));
    ASSERT_TRUE(service.Register(f.m_host));
    service.Finalize();
    EXPECT_EQ(service.Register(f.m_host).m_error, Error::Closed);
    HostBinding selected;
    EXPECT_TRUE(service.Resolve(f.m_descriptor, f.m_request, CE::Phase::BUILD, {}, selected));
    EXPECT_EQ(selected.m_binding.m_fingerprint, f.m_host.m_binding.m_fingerprint);
    EXPECT_EQ(service.Resolve(f.m_descriptor, f.m_request, CE::Phase::BUILD, Digest("missing"), selected).m_error, Error::MissingProvider);
}
TEST(FrameworkProvider, AmbiguityNeverUsesRegistrationOrder)
{
    Fixture f;
    FrameworkProviderService service;
    ASSERT_TRUE(service.Register(f.m_host));
    auto other = f.m_host;
    other.m_binding.m_id = "binding.other";
    ASSERT_TRUE(Seal(other.m_binding));
    ASSERT_TRUE(service.Register(other));
    service.Finalize();
    HostBinding selected;
    EXPECT_EQ(service.Resolve(f.m_descriptor, f.m_request, CE::Phase::BUILD, {}, selected).m_error, Error::AmbiguousProvider);
    f.m_request.m_preferredBindings = { CE::Reference(other.m_binding).GetValue() };
    ASSERT_TRUE(Seal(f.m_request));
    EXPECT_TRUE(service.Resolve(f.m_descriptor, f.m_request, CE::Phase::BUILD, f.m_host.m_binding.m_fingerprint, selected));
    EXPECT_EQ(selected.m_binding.m_id, "binding.other");
}
TEST(FrameworkProvider, UnsupportedEffectsAndMissingQualificationFailClosed)
{
    Fixture f;
    auto descriptor = f.m_descriptor;
    descriptor.m_sideEffects.push_back(CE::SideEffect::INSTALLATION_MUTATION);
    ASSERT_TRUE(Seal(descriptor));
    EXPECT_FALSE(FrameworkProviderService::Supported(descriptor));
    FrameworkProviderService service;
    ASSERT_TRUE(service.Register(f.m_host));
    service.Finalize();
    PreparedPhase phase;
    EXPECT_FALSE(service.Prepare(f.m_host, f.m_request, phase));
    Qualification q;
    q.m_bindingFingerprint = f.m_host.m_binding.m_fingerprint;
    q.m_executableDigest = Digest();
    q.m_profileFingerprint = Digest();
    q.m_observationId = "observation.review";
    q.m_evidenceIds = { "evidence.review" };
    q.m_from = Clock::now();
    q.m_until = q.m_from;
    EXPECT_EQ(service.ReviewQualification(q).m_error, Error::Invalid);
}
TEST(FrameworkPolicy, MissingPolicyStoredReceiptAndRevocationNeverAdmit)
{
    Fixture f;
    auto plan = f.Plan();
    FrameworkExecutionPolicyService service;
    CE::CapabilityAuthorizationReceiptV1 receipt;
    EXPECT_EQ(service.Authorize(plan, receipt).m_error, Error::PolicyDenied);
    ASSERT_TRUE(service.SetPolicy(f.Policy()));
    CE::PolicyState state;
    ASSERT_TRUE(service.Evaluate(plan, state, plan.m_policyRevision));
    EXPECT_EQ(service.Authorize(plan, receipt).m_error, Error::AuthorizationRequired);
    ASSERT_TRUE(service.Confirm(plan, "actor.host", std::chrono::seconds(60), receipt));
    EXPECT_TRUE(service.Authorize(plan, receipt));
    FrameworkExecutionPolicyService restarted;
    ASSERT_TRUE(restarted.SetPolicy(f.Policy()));
    EXPECT_EQ(restarted.Authorize(plan, receipt).m_error, Error::AuthorizationRequired);
    service.Revoke();
    EXPECT_EQ(service.Authorize(plan, receipt).m_error, Error::PolicyDenied);
}
TEST(FrameworkPolicy, ExactScopeAndPolicyRevisionAreBound)
{
    Fixture f;
    auto plan = f.Plan();
    FrameworkExecutionPolicyService service;
    ASSERT_TRUE(service.SetPolicy(f.Policy()));
    CE::PolicyState state;
    ASSERT_TRUE(service.Evaluate(plan, state, plan.m_policyRevision));
    CE::CapabilityAuthorizationReceiptV1 receipt;
    ASSERT_TRUE(service.Confirm(plan, "actor.host", std::chrono::seconds(30), receipt));
    auto changed = plan;
    changed.m_request.m_packId = "pack.other";
    EXPECT_EQ(service.Authorize(changed, receipt).m_error, Error::PolicyDenied);
    ASSERT_TRUE(service.SetPolicy(f.Policy()));
    EXPECT_EQ(service.Authorize(plan, receipt).m_error, Error::Drifted);
}
TEST(FrameworkAdmission, DirectSubmissionReplayExpiryRevocationAndFullContextMismatchAreDenied)
{
    FrameworkPendingAdmission gate;
    ET::ToolAdmissionContext context;
    context.m_attemptId = "tool.pending";
    context.m_requestFingerprint = Digest("request");
    context.m_commandFingerprint = Digest("command");
    context.m_profileFingerprint = Digest("profile");
    context.m_executableDigest = Digest("executable");
    context.m_configurationFingerprint = Digest("configuration");
    context.m_hostInstanceId = "host.session";
    context.m_rootIdentities = { "root.identity" };
    EXPECT_FALSE(gate.Acquire(context)); // Otherwise valid direct M2 submission has no Framework pending attempt.
    bool current = true;
    ASSERT_TRUE(gate.Arm(
        context,
        [&]
        {
            return current;
        }));
    for (auto member : { &ET::ToolAdmissionContext::m_attemptId,
                         &ET::ToolAdmissionContext::m_requestFingerprint,
                         &ET::ToolAdmissionContext::m_commandFingerprint,
                         &ET::ToolAdmissionContext::m_profileFingerprint,
                         &ET::ToolAdmissionContext::m_executableDigest,
                         &ET::ToolAdmissionContext::m_configurationFingerprint })
    {
        auto changed = context;
        changed.*member += ".changed";
        EXPECT_FALSE(gate.Acquire(changed));
    }
    auto changed = context;
    changed.m_rootIdentities = { "root.other" };
    EXPECT_FALSE(gate.Acquire(changed));
    auto lease = gate.Acquire(context);
    ASSERT_TRUE(lease);
    EXPECT_FALSE(gate.Acquire(context));
    EXPECT_TRUE(lease->Consume(context, Clock::now()));
    EXPECT_FALSE(lease->Consume(context, Clock::now()));
    gate.Revoke();
    ASSERT_TRUE(gate.Arm(
        context,
        [&]
        {
            return current;
        }));
    lease = gate.Acquire(context);
    ASSERT_TRUE(lease);
    current = false;
    EXPECT_FALSE(lease->Consume(context, Clock::now()));
    current = true;
    gate.Revoke();
    ASSERT_TRUE(gate.Arm(
        context,
        [&]
        {
            return current;
        }));
    lease = gate.Acquire(context);
    ASSERT_TRUE(lease);
    EXPECT_FALSE(lease->Consume(context, Clock::now() + std::chrono::seconds(11)));
    gate.Revoke();
    ASSERT_TRUE(gate.Arm(
        context,
        [&]
        {
            return current;
        }));
    lease = gate.Acquire(context);
    ASSERT_TRUE(lease);
    changed = context;
    changed.m_hostInstanceId = "host.other";
    EXPECT_FALSE(lease->Consume(changed, Clock::now()));
    gate.Revoke();
    ASSERT_TRUE(gate.Arm(
        context,
        [&]
        {
            return current;
        }));
    lease = gate.Acquire(context);
    ASSERT_TRUE(lease);
    gate.Revoke();
    EXPECT_FALSE(lease->Consume(context, Clock::now()));
    EXPECT_FALSE(gate.Acquire(context));
}
AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);
