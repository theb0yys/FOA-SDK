/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackDraftRecoveryService.h"

#include <AzCore/PlatformIncl.h>
#include <AzTest/AzTest.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QByteArray ReadRecoveryTestFile(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                return {};
            }
            return file.readAll();
        }

        bool WriteRecoveryTestFile(const QString& path, const QByteArray& bytes)
        {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
        }

        class PackDraftRecoveryTests : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                ASSERT_TRUE(m_temp.isValid());
                m_workspace = m_temp.path() + "/workspace";
                m_document = m_workspace + "/workspace.tgworkspace.json";
                ASSERT_TRUE(QDir().mkpath(m_workspace));
                ASSERT_TRUE(WriteRecoveryTestFile(m_document, "synthetic workspace marker"));
                m_store = std::make_unique<PackDraftRecoveryService>(m_temp.path() + "/recovery");
                ASSERT_TRUE(m_store->Bind("sdkqa.recovery", m_workspace, m_document, m_error));
                ASSERT_FALSE(m_store->Read().m_exists);
                for (const QString& key : PackDraftRecoveryService::FieldNames())
                {
                    m_draft.m_baseline.insert(key, QString());
                    m_draft.m_fields.insert(key, QString::fromUtf8("  unfinished \xce\xa9\n\n") + key);
                }
                m_draft.m_fields["saveImpact"] = "unknown";
                m_draft.m_baseline["saveImpact"] = "unknown";
                m_draft.m_fields["releaseChannel"] = "beta";
                m_draft.m_baseline["releaseChannel"] = "development";
                m_draft.m_packId = "sdkqa.saved";
                m_draft.m_isNew = false;
                m_draft.m_advancedExpanded = true;
            }
            QTemporaryDir m_temp;
            QString m_workspace;
            QString m_document;
            QString m_error;
            std::unique_ptr<PackDraftRecoveryService> m_store;
            PackDraftRecovery m_draft;
        };
    } // namespace

    TEST_F(PackDraftRecoveryTests, RawFieldsBaselineAndPresentationRoundTripWithoutManifestWrites)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto result = m_store->Read();
        ASSERT_TRUE(result.m_valid);
        EXPECT_EQ(result.m_draft.m_fields, m_draft.m_fields);
        EXPECT_EQ(result.m_draft.m_baseline, m_draft.m_baseline);
        EXPECT_EQ(result.m_draft.m_packId, m_draft.m_packId);
        EXPECT_FALSE(result.m_draft.m_isNew);
        EXPECT_TRUE(result.m_draft.m_advancedExpanded);
        EXPECT_EQ(ReadRecoveryTestFile(m_document), "synthetic workspace marker");
        EXPECT_FALSE(QDir(m_workspace + "/Packs").exists());
    }

    TEST_F(PackDraftRecoveryTests, NewDraftMayHaveEmptyIdentityAndInvalidManifestValues)
    {
        m_draft.m_isNew = true;
        m_draft.m_packId.clear();
        m_draft.m_fields["owner"].clear();
        m_draft.m_fields["version"] = "unfinished version";
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto result = m_store->Read();
        ASSERT_TRUE(result.m_valid);
        EXPECT_TRUE(result.m_draft.m_isNew);
        EXPECT_TRUE(result.m_draft.m_packId.isEmpty());
        EXPECT_EQ(result.m_draft.m_fields, m_draft.m_fields);
    }

    TEST_F(PackDraftRecoveryTests, WritesAreDeterministicAndSurviveFreshService)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString path = m_store->Path();
        const QByteArray first = ReadRecoveryTestFile(path);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(first, ReadRecoveryTestFile(path));
        m_store.reset();
        PackDraftRecoveryService next(m_temp.path() + "/recovery");
        ASSERT_TRUE(next.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        ASSERT_TRUE(next.Read().m_valid);
    }

    TEST_F(PackDraftRecoveryTests, SameIdInAnotherRootOrDocumentHasIndependentRecovery)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString secondRoot = m_temp.path() + "/second";
        ASSERT_TRUE(QDir().mkpath(secondRoot));
        PackDraftRecoveryService second(m_temp.path() + "/recovery");
        ASSERT_TRUE(second.Bind("sdkqa.recovery", secondRoot, {}, m_error));
        EXPECT_NE(second.Path(), m_store->Path());
        EXPECT_FALSE(second.Read().m_exists);
        PackDraftRecoveryService third(m_temp.path() + "/recovery");
        ASSERT_TRUE(third.Bind("sdkqa.recovery", m_workspace, {}, m_error));
        EXPECT_NE(third.Path(), m_store->Path());
        EXPECT_FALSE(third.Read().m_exists);
        EXPECT_TRUE(m_store->Read().m_valid);
    }

    TEST_F(PackDraftRecoveryTests, ForeignWorkspacePayloadIsPreservedAndRejected)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        PackDraftRecoveryService second(m_temp.path() + "/recovery");
        ASSERT_TRUE(second.Bind("sdkqa.other", m_workspace, m_document, m_error));
        const QByteArray bytes = ReadRecoveryTestFile(m_store->Path());
        ASSERT_TRUE(WriteRecoveryTestFile(second.Path(), bytes));
        EXPECT_FALSE(second.Read().m_valid);
        EXPECT_FALSE(second.Write(m_draft, m_error));
        EXPECT_EQ(ReadRecoveryTestFile(second.Path()), bytes);
    }

    TEST_F(PackDraftRecoveryTests, MalformedFutureAndIncompleteCopiesCannotBeOverwritten)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QJsonObject original = QJsonDocument::fromJson(ReadRecoveryTestFile(m_store->Path())).object();
        for (int mutation = 0; mutation < 8; ++mutation)
        {
            QJsonObject object = original;
            switch (mutation)
            {
            case 0: object["SchemaVersion"] = 2; break;
            case 1: object["SchemaVersion"] = "1"; break;
            case 2: object.remove("Baseline"); break;
            case 3: object["IsNew"] = "true"; break;
            case 4: object["Format"] = "another format"; break;
            case 5: object["Extra"] = "unrecognized"; break;
            case 6:
            {
                QJsonObject fields = object["Fields"].toObject();
                fields["version"] = 42;
                object["Fields"] = fields;
                break;
            }
            case 7:
            {
                QJsonObject fields = object["Fields"].toObject();
                fields["saveImpact"] = "not a combo option";
                object["Fields"] = fields;
                break;
            }
            }
            const QByteArray bytes = QJsonDocument(object).toJson();
            ASSERT_TRUE(WriteRecoveryTestFile(m_store->Path(), bytes));
            EXPECT_FALSE(m_store->Read().m_valid) << mutation;
            EXPECT_FALSE(m_store->Write(m_draft, m_error)) << mutation;
            EXPECT_EQ(ReadRecoveryTestFile(m_store->Path()), bytes);
        }
        ASSERT_TRUE(WriteRecoveryTestFile(m_store->Path(), "{broken"));
        EXPECT_FALSE(m_store->Read().m_valid);
        EXPECT_FALSE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(ReadRecoveryTestFile(m_store->Path()), "{broken");
    }

    TEST_F(PackDraftRecoveryTests, OversizedWritesKeepPreviousGoodBytesAndOversizedReadsAreRejected)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray original = ReadRecoveryTestFile(m_store->Path());
        m_draft.m_fields["displayName"] = QString(PackDraftRecoveryService::MaximumFieldCharacters + 1, 'x');
        EXPECT_FALSE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(ReadRecoveryTestFile(m_store->Path()), original);
        ASSERT_TRUE(WriteRecoveryTestFile(m_store->Path(), QByteArray(PackDraftRecoveryService::MaximumDocumentBytes + 1, 'x')));
        EXPECT_FALSE(m_store->Read().m_valid);
    }

    TEST_F(PackDraftRecoveryTests, ExplicitDiscardRemovesInvalidCopyAndAllowsFutureCheckpoints)
    {
        ASSERT_TRUE(WriteRecoveryTestFile(m_store->Path(), "{broken"));
        EXPECT_FALSE(m_store->Read().m_valid);
        ASSERT_TRUE(m_store->Clear(m_error));
        EXPECT_FALSE(QFile::exists(m_store->Path()));
        ASSERT_TRUE(m_store->Clear(m_error));
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_TRUE(m_store->Read().m_valid);
    }

    TEST_F(PackDraftRecoveryTests, ConcurrentSessionCannotReadWriteOrDeleteOwnersCopy)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray original = ReadRecoveryTestFile(m_store->Path());
        PackDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_FALSE(other.Read().m_valid);
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_FALSE(other.Clear(m_error));
        EXPECT_EQ(ReadRecoveryTestFile(m_store->Path()), original);
        m_store.reset();
        ASSERT_TRUE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_TRUE(other.Read().m_valid);
    }

    TEST_F(PackDraftRecoveryTests, MissingWorkspaceAndUnavailableRecoveryFolderFailClosed)
    {
        PackDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_temp.path() + "/missing", {}, m_error));
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_FALSE(other.Bind({}, m_workspace, m_document, m_error));
        PackDraftRecoveryService blocked(m_document);
        EXPECT_FALSE(blocked.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_EQ(ReadRecoveryTestFile(m_document), "synthetic workspace marker");
    }

#if defined(Q_OS_WIN)
    TEST_F(PackDraftRecoveryTests, LockedDestinationPreservesPreviousBytesOnWriteAndDiscardFailure)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString path = m_store->Path();
        const QByteArray original = ReadRecoveryTestFile(path);
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ASSERT_NE(handle, INVALID_HANDLE_VALUE);
        m_draft.m_fields["displayName"] = "later draft";
        EXPECT_FALSE(m_store->Write(m_draft, m_error));
        EXPECT_FALSE(m_store->Clear(m_error));
        EXPECT_EQ(ReadRecoveryTestFile(path), original);
        CloseHandle(handle);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(m_store->Read().m_draft.m_fields["displayName"], "later draft");
    }
#endif
} // namespace TaintedGrailModdingSDK
