/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>
#include <ExternalToolchain/ToolExecutionTypes.h>
#include <algorithm>

namespace ExternalToolchain
{
    namespace
    {
        ToolExecutionCommandV2 Command()
        {
            ToolExecutionCommandV2 c;
            c.m_providerId = "test.provider";
            c.m_providerVersion = "1.0.0";
            c.m_commandId = "batch";
            c.m_probeId = "configured";
            c.m_outputKinds = { "text/plain" };
            return c;
        }
        ToolInvocationRequestV2 Request()
        {
            auto c = Command();
            ToolInvocationRequestV2 r;
            r.m_attemptId = "test.attempt";
            r.m_providerId = c.m_providerId;
            r.m_commandId = c.m_commandId;
            r.m_targetRootId = "target";
            r.m_commandFingerprint = CanonicalToolCommand(c).m_fingerprint;
            r.m_outputs = { { "result", "result.txt", "text/plain", 64 } };
            r.m_fingerprint = CanonicalToolRequest(r).m_fingerprint;
            return r;
        }
    } // namespace
    TEST(ToolExecutionContract, Sha256KnownAnswersAndChunkBoundaries)
    {
        EXPECT_EQ(ToolDigest(""), "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        EXPECT_EQ(ToolDigest("abc"), "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        AZStd::string data(1000000, 'a');
        ToolSha256 sha;
        for (size_t i = 0; i < data.size(); i += 17)
        {
            sha.Update(data.data() + i, std::min(size_t(17), data.size() - i));
        }
        EXPECT_EQ(sha.Finish(), "sha256:cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }
    TEST(ToolExecutionContract, AttemptsDoNotChangeSemanticIdentity)
    {
        auto a = Request(), b = a;
        b.m_attemptId = "test.second";
        EXPECT_EQ(CanonicalToolRequest(a).m_json, CanonicalToolRequest(b).m_json);
        EXPECT_TRUE(ValidateToolRequest(b, Command()));
    }
    TEST(ToolExecutionContract, StaleRequestAndCommandAreRejected)
    {
        auto r = Request();
        ASSERT_TRUE(ValidateToolRequest(r, Command()));
        r.m_timeoutMilliseconds = 1;
        EXPECT_FALSE(ValidateToolRequest(r, Command()));
        r = Request();
        r.m_commandFingerprint = ToolDigest("wrong");
        EXPECT_FALSE(ValidateToolRequest(r, Command()));
    }
    TEST(ToolExecutionContract, LegacyProfileAndItsRequestFingerprintCannotGainCapability)
    {
        auto command = Command();
        const auto current = CanonicalToolCommand(command);
        ASSERT_TRUE(current);
        auto legacyJson = current.m_json;
        const auto position = legacyJson.find(ToolExecutionProfile);
        ASSERT_NE(position, AZStd::string::npos);
        legacyJson.replace(position, AZStd::string(ToolExecutionProfile).size(), "windows-lpac-batch-v1");
        const auto legacyFingerprint = ToolDigest(legacyJson);
        EXPECT_NE(current.m_fingerprint, legacyFingerprint);
        command.m_profile = "windows-lpac-batch-v1";
        EXPECT_FALSE(CanonicalToolCommand(command));
        auto request = Request();
        request.m_commandFingerprint = legacyFingerprint;
        request.m_fingerprint = CanonicalToolRequest(request).m_fingerprint;
        EXPECT_FALSE(ValidateToolRequest(request, Command()));
        EXPECT_TRUE(ValidateToolRequest(Request(), Command()));
    }
    TEST(ToolExecutionContract, UnknownVersionEnumsAndSecretRequestsAreRejected)
    {
        auto r = Request();
        r.m_version = 3;
        EXPECT_FALSE(CanonicalToolRequest(r));
        r = Request();
        r.m_arguments = { { ToolArgumentKind::Secret, "key" } };
        EXPECT_EQ(CanonicalToolRequest(r).m_error, ToolError::SecretUseUnsupported);
        r.m_arguments = { { static_cast<ToolArgumentKind>(255), "x" } };
        EXPECT_FALSE(CanonicalToolRequest(r));
    }
    TEST(ToolExecutionContract, UnsafePathsAndAliasesAreRejected)
    {
        for (const char* p : { "../escape", "a/../b", "/absolute", "c:/private", "a\\b", "a//b", "con.txt", "file.", "aux", "a:b" })
        {
            EXPECT_FALSE(ToolSafeRelativePath(p)) << p;
        }
        auto r = Request();
        r.m_outputs.push_back({ "other", "result.txt", "text/plain", 64 });
        EXPECT_FALSE(CanonicalToolRequest(r));
    }
    TEST(ToolExecutionContract, BoundariesAreCheckedBeforeCanonicalAllocation)
    {
        auto r = Request();
        r.m_arguments.assign(ToolMaxArguments, { ToolArgumentKind::Literal, "" });
        EXPECT_TRUE(CanonicalToolRequest(r));
        r.m_arguments.push_back({});
        EXPECT_FALSE(CanonicalToolRequest(r));
        r = Request();
        r.m_arguments = { { ToolArgumentKind::Literal, AZStd::string(4097, 'x') } };
        EXPECT_FALSE(CanonicalToolRequest(r));
    }
    TEST(ToolExecutionContract, OversizedSetKeysAreRejectedBeforeCopying)
    {
        auto r = Request();
        r.m_inputs = { { AZStd::string(1024 * 1024, 'x'), "input", "source.txt", "text/plain", ToolDigest("source"), 6 } };
        EXPECT_FALSE(CanonicalToolRequest(r));
        auto c = Command();
        c.m_inputKinds = { AZStd::string(1024 * 1024, 'x') };
        EXPECT_FALSE(CanonicalToolCommand(c));
    }
    TEST(ToolExecutionContract, ArgumentSequenceMattersAndInputIsNotMutated)
    {
        auto r = Request();
        r.m_arguments = { { ToolArgumentKind::Literal, "b" }, { ToolArgumentKind::Literal, "a" } };
        auto first = CanonicalToolRequest(r);
        EXPECT_EQ(r.m_arguments[0].m_value, "b");
        std::swap(r.m_arguments[0], r.m_arguments[1]);
        EXPECT_NE(first.m_fingerprint, CanonicalToolRequest(r).m_fingerprint);
    }
    TEST(ToolExecutionContract, ManifestRoundTripAndMalformedInputs)
    {
        auto r = Request();
        ToolOutputManifestV2 m;
        m.m_attemptId = r.m_attemptId;
        m.m_requestFingerprint = r.m_fingerprint;
        m.m_outputs = { { "result", "output", "result.txt", "text/plain", ToolDigest("hello"), 5 } };
        auto encoded = EncodeToolManifest(m);
        ASSERT_TRUE(encoded);
        ToolOutputManifestV2 read;
        ASSERT_TRUE(DecodeToolManifest(encoded.m_json, read));
        EXPECT_EQ(EncodeToolManifest(read).m_json, encoded.m_json);
        auto bad = encoded.m_json;
        bad.insert(1, "\"version\":2,");
        EXPECT_FALSE(DecodeToolManifest(bad, read));
        bad = encoded.m_json;
        bad.insert(1, "\"extra\":false,");
        EXPECT_FALSE(DecodeToolManifest(bad, read));
        EXPECT_FALSE(DecodeToolManifest(AZStd::string(10000, '['), read));
        EXPECT_FALSE(DecodeToolManifest(encoded.m_json.substr(0, encoded.m_json.size() - 1), read));
    }
    TEST(ToolExecutionContract, InvalidUtf8ReservedOutputsAndEmbeddedNulAreRejected)
    {
        EXPECT_FALSE(ToolSafeText(AZStd::string("\xc0\xaf", 2)));
        EXPECT_FALSE(ToolSafeText(AZStd::string("\xed\xa0\x80", 3)));
        EXPECT_FALSE(ToolSafeText(AZStd::string("\xf4\x90\x80\x80", 4)));
        EXPECT_TRUE(ToolSafeText("trail\\\\"));
        EXPECT_TRUE(ToolSafeText("\xe9\x9b\xaa"));
        auto r = Request();
        r.m_outputs[0].m_relativePath = "manifest.v2.json";
        EXPECT_FALSE(CanonicalToolRequest(r));
        ToolOutputManifestV2 m;
        m.m_attemptId = "test";
        m.m_requestFingerprint = ToolDigest("test");
        auto encoded = EncodeToolManifest(m);
        ASSERT_TRUE(encoded);
        auto pos = encoded.m_json.find("foa-tool-output-manifest-v2");
        encoded.m_json.insert(pos + 25, "\\u0000suffix");
        ToolOutputManifestV2 output;
        EXPECT_FALSE(DecodeToolManifest(encoded.m_json, output));
        auto c = Command();
        c.m_environmentNames = { "FOA_INVOCATION_ID" };
        EXPECT_FALSE(CanonicalToolCommand(c));
    }
    TEST(ToolExecutionContract, ObservedExitDoesNotImplySuccess)
    {
        auto r = Request();
        ToolInvocationRecordV2 v;
        v.m_status.m_attemptId = r.m_attemptId;
        v.m_requestFingerprint = r.m_fingerprint;
        v.m_commandFingerprint = r.m_commandFingerprint;
        v.m_profileFingerprint = ToolDigest(ToolExecutionProfile);
        v.m_status.m_stage = ToolStage::Terminal;
        v.m_status.m_outcome = ToolOutcome::ExitedZero;
        v.m_exitCodeObserved = true;
        v.m_processId = 123;
        v.m_processCreationTime = 456;
        EXPECT_FALSE(ToolSucceeded(v));
        v.m_status.m_verification = ToolVerification::Passed;
        v.m_status.m_cleanup = ToolCleanup::Complete;
        v.m_status.m_persistence = ToolPersistence::Durable;
        EXPECT_TRUE(ToolSucceeded(v));
        auto encoded = EncodeToolRecord(v);
        ASSERT_TRUE(encoded);
        ToolInvocationRecordV2 read;
        ASSERT_TRUE(DecodeToolRecord(encoded.m_json, read));
        EXPECT_EQ(EncodeToolRecord(read).m_json, encoded.m_json);
        v.m_status.m_cleanup = ToolCleanup::Failed;
        EXPECT_FALSE(ToolSucceeded(v));
        v.m_status.m_outcome = static_cast<ToolOutcome>(255);
        EXPECT_FALSE(EncodeToolRecord(v));
    }
} // namespace ExternalToolchain
