/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */


#include "CapabilityExecutionValidation.h"
#include "CanonicalFingerprint.h"
#include <AzTest/AzTest.h>
#include <AzCore/std/algorithm.h>
namespace CE = TaintedGrailModdingSDK::CapabilityExecution;
using namespace CE;
namespace
{
    CapabilityDescriptorV1 Descriptor()
    {
        CapabilityDescriptorV1 d; d.m_id = "descriptor.test"; d.m_capabilityId = "capability.test";
        d.m_inputContracts = {"data.z", "data.a"}; d.m_outputContracts = {"data.result"};
        d.m_requiredPhases = {Phase::BUILD, Phase::VERIFY}; d.m_optionalPhases = {Phase::PACKAGE, Phase::DEPLOY};
        d.m_terminalPhase = Phase::VERIFY; d.m_sideEffects = {SideEffect::STAGING_WRITE, SideEffect::READ_ONLY};
        d.m_saveImpact = SideEffect::READ_ONLY; d.m_rollbackRequired = RollbackSupport::NONE; return d;
    }
}
TEST(CapabilityExecutionCanonical, GoldenOptionPreservesUtf8AndExcludesOwnFingerprint)
{
    OptionV1 option; option.m_id = "option.test"; option.m_value = "caf\xc3\xa9";
    const AZStd::string golden = R"({"contract_id":"foa-capability-execution-v1","canonical_profile":"foa-capability-execution-canonical-json-v1","version":1,"kind":"OPTION","id":"option.test","value":"caf)" "\xc3\xa9" R"("})";
    auto result = Canonicalize(option); ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
    EXPECT_EQ(result.GetValue().m_json, golden);
    EXPECT_EQ(result.GetValue().m_fingerprint, "sha256:8736bd56535fbfbff6c8b35924b4850bd101f15b7c47bb5b7840f2958102b8c2");
    option.m_fingerprint = "sha256:" + AZStd::string(64, '0');
    EXPECT_EQ(Canonicalize(option).GetValue().m_json, golden);
    EXPECT_FALSE(Validate(option).IsSuccess());
    option.m_fingerprint = result.GetValue().m_fingerprint;
    EXPECT_TRUE(Validate(option).IsSuccess()); EXPECT_TRUE(Reference(option).IsSuccess());
}
TEST(CapabilityExecutionCanonical, SetsSortButPhaseSequenceAndSourceOrderStayMeaningful)
{
    auto d = Descriptor(); const auto before = Canonicalize(d); ASSERT_TRUE(before.IsSuccess());
    AZStd::reverse(d.m_inputContracts.begin(), d.m_inputContracts.end());
    AZStd::reverse(d.m_optionalPhases.begin(), d.m_optionalPhases.end());
    AZStd::reverse(d.m_sideEffects.begin(), d.m_sideEffects.end());
    EXPECT_EQ(before.GetValue().m_json, Canonicalize(d).GetValue().m_json);
    EXPECT_EQ(d.m_inputContracts.front(), "data.a");
    AZStd::reverse(d.m_requiredPhases.begin(), d.m_requiredPhases.end());
    EXPECT_NE(before.GetValue().m_fingerprint, Canonicalize(d).GetValue().m_fingerprint);
}
TEST(CapabilityExecutionCanonical, RejectsUnknownVersionProfileEnumsAndDuplicateIds)
{
    auto d = Descriptor(); d.m_header.m_version = 2; EXPECT_FALSE(Canonicalize(d).IsSuccess());
    d = Descriptor(); d.m_header.m_canonicalProfile = "future"; EXPECT_FALSE(Canonicalize(d).IsSuccess());
    d = Descriptor(); d.m_terminalPhase = static_cast<Phase>(999); EXPECT_FALSE(Canonicalize(d).IsSuccess());
    d = Descriptor(); d.m_inputContracts.push_back(d.m_inputContracts.front()); EXPECT_FALSE(Canonicalize(d).IsSuccess());
    d = Descriptor(); d.m_requiredPhases.push_back(Phase::BUILD); EXPECT_FALSE(Canonicalize(d).IsSuccess());
}
TEST(CapabilityExecutionCanonical, TextAndCollectionBoundaries)
{
    OptionV1 v; v.m_id = "option.test"; v.m_value.assign(MaximumStringBytes, 'a');
    EXPECT_TRUE(Canonicalize(v).IsSuccess()); v.m_value.push_back('a'); EXPECT_FALSE(Canonicalize(v).IsSuccess());
    auto d = Descriptor(); d.m_inputContracts.clear();
    for (size_t i = 0; i < MaximumCollection; ++i) { d.m_inputContracts.push_back(AZStd::string::format("data.%zu", i)); }
    EXPECT_TRUE(Canonicalize(d).IsSuccess()); d.m_inputContracts.push_back("data.overflow"); EXPECT_FALSE(Canonicalize(d).IsSuccess());
}
TEST(CapabilityExecutionCanonical, OpaqueBytesBoundEscapingAndNesting)
{
    PhaseExtensionReferenceV1 e; e.m_id = "extension.test"; e.m_extensionContractId = "manifest.test";
    e.m_extensionFingerprint = "sha256:" + AZStd::string(64, '0');
    e.m_canonicalJson = "{\"a\":\"" + AZStd::string(MaximumEmbeddedBytes - 8, 'a') + "\"}";
    ASSERT_EQ(e.m_canonicalJson.size(), MaximumEmbeddedBytes); EXPECT_TRUE(Canonicalize(e).IsSuccess());
    e.m_canonicalJson.insert(6, "a"); EXPECT_FALSE(Canonicalize(e).IsSuccess());
    e.m_canonicalJson = AZStd::string(MaximumNesting, '{') + AZStd::string(MaximumNesting, '}');
    EXPECT_TRUE(Canonicalize(e).IsSuccess()); // lexical bound only; owner validates its opaque format
    e.m_canonicalJson = "{" + e.m_canonicalJson + "}"; EXPECT_FALSE(Canonicalize(e).IsSuccess());
    e.m_canonicalJson = "{\"x\":\"unterminated}"; EXPECT_FALSE(Canonicalize(e).IsSuccess());
}

TEST(CapabilityExecutionCanonical, AggregateCanonicalBytesAtLimitAndOneOver)
{
    CapabilityExecutionRequestV1 r; r.m_id = "request.bound"; r.m_workspaceId = "workspace.test"; r.m_packId = "pack.test";
    r.m_profileFingerprint = "sha256:" + AZStd::string(64, 'a'); r.m_capabilityId = "capability.test"; r.m_terminalPhase = Phase::BUILD;
    for (int i = 0; i < 3; ++i)
    {
        UpstreamReferenceV1 ref; ref.m_id = AZStd::string::format("binding.%d", i);
        ref.m_kind = ContractKind::CAPABILITY_PROVIDER_BINDING; ref.m_fingerprint = r.m_profileFingerprint;
        ref.m_canonicalJson = "{\"data\":\"" + AZStd::string(600000, 'a') + "\"}"; r.m_preferredBindings.push_back(ref);
    }
    auto baseline = Canonicalize(r); ASSERT_TRUE(baseline.IsSuccess());
    auto& bytes = r.m_preferredBindings.front().m_canonicalJson;
    bytes.insert(9, MaximumCanonicalBytes - baseline.GetValue().m_json.size(), 'a');
    ASSERT_LT(bytes.size(), MaximumEmbeddedBytes);
    auto exact = Canonicalize(r); ASSERT_TRUE(exact.IsSuccess()); EXPECT_EQ(exact.GetValue().m_json.size(), MaximumCanonicalBytes);
    bytes.insert(9, 1, 'a'); EXPECT_FALSE(Canonicalize(r).IsSuccess());
}
