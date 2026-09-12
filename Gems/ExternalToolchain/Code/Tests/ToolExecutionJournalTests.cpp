/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "Execution/ToolExecutionJournal.h"
#include <AzTest/AzTest.h>
#if AZ_TRAIT_EXTERNAL_TOOLCHAIN_PRIVATE_JOURNAL
#include "Execution/Platform/Windows/ToolSandbox_Windows.h"
#include <winioctl.h>
namespace ExternalToolchain
{
    TEST(ToolExecutionJournal, AtomicSnapshotsRecoverLastValidRecordAndRefuseDuplicateSequence)
    {
        wchar_t temp[32768];
        ASSERT_GT(GetTempPathW(32768, temp), 0u);
        auto root = std::wstring(temp) + L"foa-m2-journal-" + Windows::Wide(Windows::NewId());
        AZStd::string identity;
        {
            ToolExecutionJournal journal;
            ASSERT_TRUE(journal.Open(Windows::Utf8(root)));
            {
                Windows::PinnedPath pin;
                ASSERT_TRUE(pin.Open(Windows::Utf8(root), true));
                identity = pin.m_identity;
            }
            ToolInvocationRecordV2 r;
            r.m_status.m_attemptId = "test.attempt";
            r.m_status.m_sequence = 1;
            r.m_requestFingerprint = ToolDigest("request");
            r.m_commandFingerprint = ToolDigest("command");
            r.m_profileFingerprint = ToolDigest(ToolExecutionProfile);
            ASSERT_TRUE(journal.Save(r));
            EXPECT_EQ(journal.Save(r).m_error, ToolError::InvalidContract);
            r.m_status.m_sequence = 2;
            ASSERT_TRUE(journal.Save(r));
            ASSERT_TRUE(Windows::WriteFileAtomic(root + L"\\records\\test.attempt.record.0", "corrupt"));
            AZStd::vector<ToolInvocationRecordV2> records;
            ASSERT_TRUE(journal.Load(records));
            ASSERT_EQ(records.size(), 1);
            EXPECT_EQ(records[0].m_status.m_sequence, 1);
            ToolExecutionJournal second;
            EXPECT_EQ(second.Open(Windows::Utf8(root)).m_error, ToolError::Busy);
            EXPECT_FALSE(journal.SaveLogs("../escape", "", ""));
            EXPECT_FALSE(journal.SaveLogs("test.attempt", AZStd::string(ToolMaxLogBytes + 1, 'x'), ""));
        }
        EXPECT_TRUE(Windows::RemoveOwnedTree(root, identity));
    }
    TEST(ToolExecutionJournal, RecoveryInventoryRejectsForeignProfileNames)
    {
        wchar_t temp[32768];
        ASSERT_GT(GetTempPathW(32768, temp), 0u);
        auto root = std::wstring(temp) + L"foa-m2-intent-" + Windows::Wide(Windows::NewId());
        AZStd::string identity;
        {
            ToolExecutionJournal journal;
            ASSERT_TRUE(journal.Open(Windows::Utf8(root)));
            {
                Windows::PinnedPath pin;
                ASSERT_TRUE(pin.Open(Windows::Utf8(root), true));
                identity = pin.m_identity;
            }
            ToolRecoveryIntent intent{ "test.attempt", Windows::NewId(), "foreign.profile", {}, false };
            EXPECT_FALSE(journal.WriteIntent(intent));
            intent.m_profileName = "foa.m2." + intent.m_stageName;
            ASSERT_TRUE(journal.WriteIntent(intent));
            AZStd::vector<ToolRecoveryIntent> values;
            ASSERT_TRUE(journal.ReadIntents(values));
            ASSERT_EQ(values.size(), 1);
            EXPECT_FALSE(values[0].m_profileCreated);
            EXPECT_EQ(values[0].m_stageName, intent.m_stageName);
            EXPECT_TRUE(journal.ClearIntent(intent.m_attemptId));
        }
        EXPECT_TRUE(Windows::RemoveOwnedTree(root, identity));
    }

    TEST(ToolExecutionJournal, OversizedSparseStoreFailsClosedWithoutFillingTheDisk)
    {
        wchar_t temp[32768];
        ASSERT_GT(GetTempPathW(32768, temp), 0u);
        auto root = std::wstring(temp) + L"foa-m2-store-limit-" + Windows::Wide(Windows::NewId());
        AZStd::string identity;
        ASSERT_TRUE(Windows::CreatePrivateDirectory(root));
        ASSERT_TRUE(Windows::CreatePrivateDirectory(root + L"\\records"));
        {
            Windows::PinnedPath pin;
            ASSERT_TRUE(pin.Open(Windows::Utf8(root), true));
            identity = pin.m_identity;
        }
        {
            Windows::Handle file(
                CreateFileW((root + L"\\records\\oversized.record.0").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr));
            ASSERT_TRUE(file);
            DWORD returned = 0;
            ASSERT_TRUE(DeviceIoControl(file.Get(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr));
            LARGE_INTEGER size{};
            size.QuadPart = 256LL * 1024 * 1024 + 1;
            ASSERT_TRUE(SetFilePointerEx(file.Get(), size, nullptr, FILE_BEGIN));
            ASSERT_TRUE(SetEndOfFile(file.Get()));
        }
        {
            ToolExecutionJournal journal;
            EXPECT_EQ(journal.Open(Windows::Utf8(root)).m_error, ToolError::StoreFull);
        }
        EXPECT_TRUE(Windows::RemoveOwnedTree(root, identity));
    }

} // namespace ExternalToolchain
#endif
