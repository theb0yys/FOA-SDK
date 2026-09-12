/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopDraftRecoveryService.h"

#include <AzCore/PlatformIncl.h>
#include <AzTest/AzTest.h>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QByteArray ReadActorRecoveryFile(const QString& path)
        {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }
        bool WriteActorRecoveryFile(const QString& path, const QByteArray& bytes)
        {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
        }
        class ActorTroopDraftRecoveryTests : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                ASSERT_TRUE(m_temp.isValid());
                m_workspace = m_temp.path() + "/workspace";
                m_document = m_workspace + "/workspace.tgworkspace.json";
                ASSERT_TRUE(QDir().mkpath(m_workspace));
                ASSERT_TRUE(WriteActorRecoveryFile(m_document, "synthetic workspace marker"));
                m_store = std::make_unique<ActorTroopDraftRecoveryService>(m_temp.path() + "/recovery");
                ASSERT_TRUE(m_store->Bind("sdkqa.recovery", m_workspace, m_document, m_error));
                ASSERT_FALSE(m_store->Read().m_exists);
                m_draft.m_values = {
                    {"actorText", QString::fromUtf8(" raw \xce\xa9\n\n")}, {"troopSize", 7}, {"memberRequired", true},
                    {"memberEvidence", QStringList{"evidence:a", "evidence:b"}},
                    {"actorRecord", QVariantList{QString("actor:a"), QString("Actor A"),
                        QVariantList{QVariantList{QString(), QString("Select")}, QVariantList{QString("actor:a"), QString("Actor A")}}}},
                    {"memberRole", QVariantList{QVariant(), QString("melee"), QVariantList{}}}};
                m_draft.m_actor = "actor:a";
                m_draft.m_troop = "troop:a";
                m_draft.m_member = "member:a";
                m_draft.m_actorDirty = m_draft.m_troopDirty = m_draft.m_memberDirty = m_draft.m_refreshPending = true;
                m_draft.m_tab = 1;
                m_draft.m_removed = {"removed:a"};
                m_draft.m_members = {QJsonObject{{"LinkId", "member:a"}, {"TroopRecordId", "troop:a"},
                    {"ActorRecordId", "actor:a"}, {"ActorSubjectRef", ""}, {"Role", "melee"},
                    {"MinimumCount", 1}, {"MaximumCount", 2}, {"Weight", 2.75}, {"Required", true},
                    {"Conditions", QJsonArray{"unfinished condition"}}, {"EvidenceIds", QJsonArray{"evidence:a"}}}};
            }
            QTemporaryDir m_temp;
            QString m_workspace, m_document, m_error;
            std::unique_ptr<ActorTroopDraftRecoveryService> m_store;
            ActorTroopDraftRecovery m_draft;
        };
    }

    TEST_F(ActorTroopDraftRecoveryTests, RawTypedFieldsStagingRemovalFlagsAndSelectionRoundTrip)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error)) << m_error.toUtf8().constData();
        const auto read = m_store->Read();
        ASSERT_TRUE(read.m_valid) << read.m_error.toUtf8().constData();
        EXPECT_TRUE(read.m_draft == m_draft);
        EXPECT_EQ(read.m_draft.m_values["troopSize"].metaType(), QMetaType::fromType<int>());
        EXPECT_EQ(read.m_draft.m_values["memberEvidence"].metaType(), QMetaType::fromType<QStringList>());
        EXPECT_FALSE(read.m_draft.m_values["memberRole"].toList()[0].isValid());
        EXPECT_EQ(ReadActorRecoveryFile(m_document), "synthetic workspace marker");
        EXPECT_FALSE(QDir(m_workspace + "/Catalog").exists());
    }

    TEST_F(ActorTroopDraftRecoveryTests, DeterministicBytesFreshReaderAndReadBeforeWriteAdmission)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto path = m_store->Path();
        const auto bytes = ReadActorRecoveryFile(path);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(ReadActorRecoveryFile(path), bytes);
        m_store->Release();
        ActorTroopDraftRecoveryService next(m_temp.path() + "/recovery");
        ASSERT_TRUE(next.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_FALSE(next.Write(m_draft, m_error));
        ASSERT_TRUE(next.Read().m_valid);
        EXPECT_TRUE(next.Write(m_draft, m_error));
    }

    TEST_F(ActorTroopDraftRecoveryTests, WorkspaceIdRootAndDocumentEachIsolateCopies)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto secondRoot = m_temp.path() + "/second";
        const auto secondDocument = m_workspace + "/other.tgworkspace.json";
        ASSERT_TRUE(QDir().mkpath(secondRoot));
        ASSERT_TRUE(WriteActorRecoveryFile(secondDocument, "synthetic"));
        for (int variant = 0; variant < 3; ++variant)
        {
            ActorTroopDraftRecoveryService other(m_temp.path() + "/recovery");
            ASSERT_TRUE(other.Bind(variant == 0 ? "other.id" : "sdkqa.recovery",
                variant == 1 ? secondRoot : m_workspace, variant == 2 ? secondDocument : m_document, m_error));
            EXPECT_NE(other.Path(), m_store->Path());
            EXPECT_FALSE(other.Read().m_exists);
        }
    }

    TEST_F(ActorTroopDraftRecoveryTests, ForeignBindingCannotBeReadOrOverwritten)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto bytes = ReadActorRecoveryFile(m_store->Path());
        ActorTroopDraftRecoveryService other(m_temp.path() + "/recovery");
        ASSERT_TRUE(other.Bind("other.id", m_workspace, m_document, m_error));
        ASSERT_TRUE(WriteActorRecoveryFile(other.Path(), bytes));
        EXPECT_FALSE(other.Read().m_valid);
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_EQ(ReadActorRecoveryFile(other.Path()), bytes);
    }

    TEST_F(ActorTroopDraftRecoveryTests, FutureMalformedWrongTypesAndInconsistentStagingStayUntouched)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto original = QJsonDocument::fromJson(ReadActorRecoveryFile(m_store->Path())).object();
        for (int mutation = 0; mutation < 19; ++mutation)
        {
            auto object = original;
            auto values = object["Values"].toObject();
            auto members = object["Members"].toArray();
            auto member = members[0].toObject();
            switch (mutation)
            {
            case 0: object["SchemaVersion"] = 2; break;
            case 1: object["SchemaVersion"] = "1"; break;
            case 2: object.remove("Actor"); break;
            case 3: object["ActorDirty"] = "true"; break;
            case 4: object["Format"] = "FOA-SDK.ItemRecipeDraftRecovery"; break;
            case 5: object["Extra"] = true; break;
            case 6: values["troopSize"] = QJsonArray{"integer", 1.5}; break;
            case 7: values["actorRecord"] = QJsonArray{"selection", true, "wrong", QJsonArray{}}; break;
            case 8: values["memberRequired"] = QJsonArray{"boolean", "true"}; break;
            case 9: values["actorText"] = QJsonArray{"unknown", "text"}; break;
            case 10: member["TroopRecordId"] = "other.troop"; break;
            case 11: object["Tab"] = 0.5; break;
            case 12: object["Member"] = "missing.member"; break;
            case 13: member["MinimumCount"] = 1.5; break;
            case 14: member["Weight"] = "2.75"; break;
            case 15: member["EvidenceIds"] = QJsonArray{4}; break;
            case 16: object["RemovedMembers"] = QJsonArray{"member:a"}; break;
            case 17: members.append(member); break;
            case 18: object["RemovedMembers"] = QJsonArray{"removed:a", "removed:a"}; break;
            }
            members[0] = member;
            object["Values"] = values; object["Members"] = members;
            const auto bytes = QJsonDocument(object).toJson();
            ASSERT_TRUE(WriteActorRecoveryFile(m_store->Path(), bytes));
            EXPECT_FALSE(m_store->Read().m_valid) << mutation;
            EXPECT_FALSE(m_store->Write(m_draft, m_error)) << mutation;
            EXPECT_EQ(ReadActorRecoveryFile(m_store->Path()), bytes);
        }
        for (const QByteArray& bytes : {QByteArray("{broken"), QByteArray("[]"), QByteArray()})
        {
            ASSERT_TRUE(WriteActorRecoveryFile(m_store->Path(), bytes));
            EXPECT_FALSE(m_store->Read().m_valid);
            EXPECT_FALSE(m_store->Write(m_draft, m_error));
            EXPECT_EQ(ReadActorRecoveryFile(m_store->Path()), bytes);
        }
    }

    TEST_F(ActorTroopDraftRecoveryTests, OversizedAndInvalidWritesKeepPreviousGoodCopy)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto bytes = ReadActorRecoveryFile(m_store->Path());
        for (int mutation = 0; mutation < 8; ++mutation)
        {
            auto invalid = m_draft;
            if (mutation == 0) { invalid.m_values["actorText"] = QString(ActorTroopDraftRecoveryService::MaximumFieldCharacters + 1, 'x'); }
            if (mutation == 1) { invalid.m_values["troopSize"] = 1.5; }
            if (mutation == 2) { invalid.m_tab = 2; }
            if (mutation == 3) { invalid.m_actor = QString(513, 'x'); }
            if (mutation == 4) { for (int i = 0; i < 65; ++i) { invalid.m_values["actorExtra" + QString::number(i)] = i; } }
            if (mutation == 5) { for (int i = 0; i < 4097; ++i) { invalid.m_removed.push_back(QString::number(i)); } }
            if (mutation == 6) { invalid.m_actorDirty = invalid.m_troopDirty = invalid.m_memberDirty = false; }
            if (mutation == 7) { invalid.m_values["actorRecord"] = QVariantList{4, QString("label"), QVariantList{}}; }
            EXPECT_FALSE(m_store->Write(invalid, m_error)) << mutation;
            EXPECT_EQ(ReadActorRecoveryFile(m_store->Path()), bytes);
        }
    }

    TEST_F(ActorTroopDraftRecoveryTests, AggregateLimitAndOversizedReadAreEnforced)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto bytes = ReadActorRecoveryFile(m_store->Path());
        auto invalid = m_draft;
        QVariantList options;
        for (int i = 0; i < 200; ++i) { options.push_back(QVariantList{QString::number(i), QString(16384, 'x')}); }
        invalid.m_values["actorRecord"] = QVariantList{QString("0"), QString("label"), options};
        EXPECT_FALSE(m_store->Write(invalid, m_error));
        EXPECT_EQ(ReadActorRecoveryFile(m_store->Path()), bytes);
        ASSERT_TRUE(WriteActorRecoveryFile(m_store->Path(), QByteArray(ActorTroopDraftRecoveryService::MaximumDocumentBytes + 1, 'x')));
        EXPECT_FALSE(m_store->Read().m_valid);
    }

    TEST_F(ActorTroopDraftRecoveryTests, ExplicitDiscardClearsInvalidCopyAndAllowsNewCheckpoint)
    {
        ASSERT_TRUE(WriteActorRecoveryFile(m_store->Path(), "{broken"));
        EXPECT_FALSE(m_store->Read().m_valid);
        ASSERT_TRUE(m_store->Clear(m_error));
        EXPECT_FALSE(QFile::exists(m_store->Path()));
        EXPECT_TRUE(m_store->Write(m_draft, m_error));
    }

    TEST_F(ActorTroopDraftRecoveryTests, ConcurrentOwnerCannotReadWriteOrDiscardAndCanRetryAfterRelease)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        ActorTroopDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_FALSE(other.Read().m_valid);
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_FALSE(other.Clear(m_error));
        m_store.reset();
        ASSERT_TRUE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_TRUE(other.Read().m_valid);
    }

    TEST_F(ActorTroopDraftRecoveryTests, MissingWorkspaceAndUnavailableFolderFailClosed)
    {
        ActorTroopDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_temp.path() + "/missing", {}, m_error));
        EXPECT_FALSE(other.Bind({}, m_workspace, m_document, m_error));
        ActorTroopDraftRecoveryService blocked(m_document);
        EXPECT_FALSE(blocked.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_EQ(ReadActorRecoveryFile(m_document), "synthetic workspace marker");
    }

#if defined(Q_OS_WIN)
    TEST_F(ActorTroopDraftRecoveryTests, LockedDestinationPreservesBytesForWriteAndDiscardRetry)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto path = m_store->Path();
        const auto bytes = ReadActorRecoveryFile(path);
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ASSERT_NE(handle, INVALID_HANDLE_VALUE);
        m_draft.m_values["actorText"] = "later draft";
        EXPECT_FALSE(m_store->Write(m_draft, m_error));
        EXPECT_FALSE(m_store->Clear(m_error));
        EXPECT_EQ(ReadActorRecoveryFile(path), bytes);
        CloseHandle(handle);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(m_store->Read().m_draft.m_values["actorText"].toString(), "later draft");
    }
#endif
} // namespace TaintedGrailModdingSDK
