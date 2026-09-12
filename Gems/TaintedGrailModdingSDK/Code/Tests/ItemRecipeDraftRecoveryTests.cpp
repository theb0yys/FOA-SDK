/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemRecipeDraftRecoveryService.h"

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
        QByteArray ReadItemRecoveryFile(const QString& path)
        {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }
        bool WriteItemRecoveryFile(const QString& path, const QByteArray& bytes)
        {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
        }
        class ItemRecipeDraftRecoveryTests : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                ASSERT_TRUE(m_temp.isValid());
                m_workspace = m_temp.path() + "/workspace";
                m_document = m_workspace + "/workspace.tgworkspace.json";
                ASSERT_TRUE(QDir().mkpath(m_workspace));
                ASSERT_TRUE(WriteItemRecoveryFile(m_document, "synthetic workspace marker"));
                m_store = std::make_unique<ItemRecipeDraftRecoveryService>(m_temp.path() + "/recovery");
                ASSERT_TRUE(m_store->Bind("sdkqa.recovery", m_workspace, m_document, m_error));
                ASSERT_FALSE(m_store->Read().m_exists);
                ItemRecipeDraft form;
                form.m_baseline = {{"economyText", QString()}, {"economyInteger", 1}, {"economyNumber", 0.25},
                    {"economyBoolean", false}, {"economyChoice", QVariantMap{{"data", QVariant()}, {"text", QString("unknown")}}}};
                form.m_values = form.m_baseline;
                form.m_values["economyText"] = QString::fromUtf8("  unfinished \xce\xa9\n\n");
                form.m_values["economyInteger"] = 123;
                form.m_values["economyNumber"] = 123.125;
                form.m_values["economyBoolean"] = true;
                form.m_values["economyChoice"] = QVariantMap{{"data", QString("sdkqa.exact-id")}, {"text", QString()}};
                for (const QString& key : {"item:a", "item:b", "recipe:c", "recipe:d", "ingredient:c", "ingredient:d", "output:c", "output:d", "acquisition:"})
                {
                    m_draft.m_drafts.insert(key, form);
                }
                m_draft.m_item = "b";
                m_draft.m_recipe = "d";
                m_draft.m_tab = 2;
                m_draft.m_recipeExpanded = true;
            }
            QTemporaryDir m_temp;
            QString m_workspace, m_document, m_error;
            std::unique_ptr<ItemRecipeDraftRecoveryService> m_store;
            ItemRecipeDraftRecovery m_draft;
        };
    } // namespace

    TEST_F(ItemRecipeDraftRecoveryTests, AllKindsTypedValuesBaselinesAndPresentationRoundTrip)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const auto read = m_store->Read();
        ASSERT_TRUE(read.m_valid);
        ASSERT_EQ(read.m_draft.m_drafts.size(), 9);
        for (auto it = m_draft.m_drafts.cbegin(); it != m_draft.m_drafts.cend(); ++it)
        {
            EXPECT_EQ(read.m_draft.m_drafts[it.key()].m_values, it->m_values);
            EXPECT_EQ(read.m_draft.m_drafts[it.key()].m_baseline, it->m_baseline);
            EXPECT_EQ(read.m_draft.m_drafts[it.key()].m_values["economyInteger"].metaType(), QMetaType::fromType<int>());
            EXPECT_EQ(read.m_draft.m_drafts[it.key()].m_values["economyNumber"].metaType(), QMetaType::fromType<double>());
        }
        EXPECT_EQ(read.m_draft.m_item, m_draft.m_item);
        EXPECT_EQ(read.m_draft.m_recipe, m_draft.m_recipe);
        EXPECT_EQ(read.m_draft.m_tab, 2);
        EXPECT_TRUE(read.m_draft.m_recipeExpanded);
        EXPECT_EQ(ReadItemRecoveryFile(m_document), "synthetic workspace marker");
        EXPECT_FALSE(QDir(m_workspace + "/Catalog").exists());
    }

    TEST_F(ItemRecipeDraftRecoveryTests, DeterministicBytesAndReleasedOwnerPermitFreshReader)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString path = m_store->Path();
        const QByteArray bytes = ReadItemRecoveryFile(path);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(ReadItemRecoveryFile(path), bytes);
        m_store->Release();
        EXPECT_FALSE(m_store->Clear(m_error));
        ItemRecipeDraftRecoveryService next(m_temp.path() + "/recovery");
        ASSERT_TRUE(next.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_TRUE(next.Read().m_valid);
    }

    TEST_F(ItemRecipeDraftRecoveryTests, WorkspaceIdRootAndDocumentEachIsolateCopies)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString secondRoot = m_temp.path() + "/second";
        ASSERT_TRUE(QDir().mkpath(secondRoot));
        for (int variant = 0; variant < 3; ++variant)
        {
            ItemRecipeDraftRecoveryService other(m_temp.path() + "/recovery");
            ASSERT_TRUE(other.Bind(variant == 0 ? "other.id" : "sdkqa.recovery",
                variant == 1 ? secondRoot : m_workspace, variant == 0 ? m_document : QString(), m_error));
            EXPECT_NE(other.Path(), m_store->Path());
            EXPECT_FALSE(other.Read().m_exists);
            EXPECT_TRUE(m_store->Read().m_valid);
        }
    }

    TEST_F(ItemRecipeDraftRecoveryTests, ForeignPayloadIsRejectedAndCannotBeOverwritten)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray bytes = ReadItemRecoveryFile(m_store->Path());
        ItemRecipeDraftRecoveryService other(m_temp.path() + "/recovery");
        ASSERT_TRUE(other.Bind("other.id", m_workspace, m_document, m_error));
        ASSERT_TRUE(WriteItemRecoveryFile(other.Path(), bytes));
        EXPECT_FALSE(other.Read().m_valid);
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_EQ(ReadItemRecoveryFile(other.Path()), bytes);
    }

    TEST_F(ItemRecipeDraftRecoveryTests, MalformedFutureWrongTypesAndMissingFieldsAreKept)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QJsonObject original = QJsonDocument::fromJson(ReadItemRecoveryFile(m_store->Path())).object();
        for (int mutation = 0; mutation < 13; ++mutation)
        {
            QJsonObject object = original;
            QJsonObject drafts = object["Drafts"].toObject();
            QJsonObject form = drafts["item:a"].toObject();
            QJsonObject values = form["Values"].toObject();
            switch (mutation)
            {
            case 0: object["SchemaVersion"] = 2; break;
            case 1: object["SchemaVersion"] = "1"; break;
            case 2: object.remove("Recipe"); break;
            case 3: object["RecipeExpanded"] = "true"; break;
            case 4: object["Format"] = "FOA-SDK.PackDraftRecovery"; break;
            case 5: object["Extra"] = 1; break;
            case 6: values["economyInteger"] = QJsonArray{"integer", 1.5}; break;
            case 7: values["economyChoice"] = QJsonArray{"selection", true, "wrong"}; break;
            case 8: values["economyBoolean"] = QJsonArray{"boolean", "true"}; break;
            case 9: values["economyText"] = QJsonArray{"unknown", "text"}; break;
            case 10: values.remove("economyText"); break;
            case 11: object["Tab"] = 1.5; break;
            case 12: values["economyNumber"] = QJsonArray{"integer", 123}; break;
            }
            form["Values"] = values; drafts["item:a"] = form; object["Drafts"] = drafts;
            const QByteArray bytes = QJsonDocument(object).toJson();
            ASSERT_TRUE(WriteItemRecoveryFile(m_store->Path(), bytes));
            EXPECT_FALSE(m_store->Read().m_valid) << mutation;
            EXPECT_FALSE(m_store->Write(m_draft, m_error)) << mutation;
            EXPECT_EQ(ReadItemRecoveryFile(m_store->Path()), bytes);
        }
        for (const QByteArray& bytes : {QByteArray("{broken"), QByteArray("[]"), QByteArray()})
        {
            ASSERT_TRUE(WriteItemRecoveryFile(m_store->Path(), bytes));
            EXPECT_FALSE(m_store->Read().m_valid);
            EXPECT_FALSE(m_store->Write(m_draft, m_error));
            EXPECT_EQ(ReadItemRecoveryFile(m_store->Path()), bytes);
        }
    }

    TEST_F(ItemRecipeDraftRecoveryTests, OversizedInvalidAndExcessiveDraftWritesKeepGoodBytes)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray bytes = ReadItemRecoveryFile(m_store->Path());
        for (int mutation = 0; mutation < 7; ++mutation)
        {
            auto invalid = m_draft;
            if (mutation == 0) { invalid.m_drafts["item:a"].m_values["economyText"] = QString(ItemRecipeDraftRecoveryService::MaximumFieldCharacters + 1, 'x'); }
            if (mutation == 1) { invalid.m_drafts["item:a"].m_values["economyNumber"] = std::numeric_limits<double>::infinity(); }
            if (mutation == 2) { for (int i = 0; i < 257; ++i) { invalid.m_drafts.insert("item:" + QString::number(i), m_draft.m_drafts["item:a"]); } }
            if (mutation == 3) { invalid.m_drafts.insert("unknown:a", invalid.m_drafts.take("item:a")); }
            if (mutation == 4) { invalid.m_drafts["item:a"].m_values["economyChoice"] = QVariantMap{{"data", 4}, {"text", QString()}}; }
            if (mutation == 5) { invalid.m_tab = 5; }
            if (mutation == 6) { invalid.m_item = QString(513, 'x'); }
            EXPECT_FALSE(m_store->Write(invalid, m_error)) << mutation;
            EXPECT_EQ(ReadItemRecoveryFile(m_store->Path()), bytes);
        }
    }

    TEST_F(ItemRecipeDraftRecoveryTests, AggregateDocumentLimitAndOversizedReadsAreEnforced)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray bytes = ReadItemRecoveryFile(m_store->Path());
        auto invalid = m_draft;
        auto largeForm = m_draft.m_drafts["item:a"];
        largeForm.m_values["economyText"] = QString(ItemRecipeDraftRecoveryService::MaximumFieldCharacters, 'x');
        for (int i = 0; i < 200; ++i) { invalid.m_drafts.insert("item:" + QString::number(i), largeForm); }
        EXPECT_FALSE(m_store->Write(invalid, m_error));
        EXPECT_EQ(ReadItemRecoveryFile(m_store->Path()), bytes);
        ASSERT_TRUE(WriteItemRecoveryFile(m_store->Path(), QByteArray(ItemRecipeDraftRecoveryService::MaximumDocumentBytes + 1, 'x')));
        EXPECT_FALSE(m_store->Read().m_valid);
    }

    TEST_F(ItemRecipeDraftRecoveryTests, ExplicitDiscardClearsInvalidCopyAndAllowsNewCheckpoint)
    {
        ASSERT_TRUE(WriteItemRecoveryFile(m_store->Path(), "{broken"));
        EXPECT_FALSE(m_store->Read().m_valid);
        ASSERT_TRUE(m_store->Clear(m_error));
        EXPECT_FALSE(QFile::exists(m_store->Path()));
        ASSERT_TRUE(m_store->Clear(m_error));
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_TRUE(m_store->Read().m_valid);
    }

    TEST_F(ItemRecipeDraftRecoveryTests, ConcurrentOwnerCannotReadWriteOrDiscard)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QByteArray bytes = ReadItemRecoveryFile(m_store->Path());
        ItemRecipeDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_FALSE(other.Read().m_valid);
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_FALSE(other.Clear(m_error));
        EXPECT_EQ(ReadItemRecoveryFile(m_store->Path()), bytes);
        m_store.reset();
        ASSERT_TRUE(other.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_TRUE(other.Read().m_valid);
    }

    TEST_F(ItemRecipeDraftRecoveryTests, MissingWorkspaceAndUnavailableFolderFailClosed)
    {
        ItemRecipeDraftRecoveryService other(m_temp.path() + "/recovery");
        EXPECT_FALSE(other.Bind("sdkqa.recovery", m_temp.path() + "/missing", {}, m_error));
        EXPECT_FALSE(other.Write(m_draft, m_error));
        EXPECT_FALSE(other.Bind({}, m_workspace, m_document, m_error));
        ItemRecipeDraftRecoveryService blocked(m_document);
        EXPECT_FALSE(blocked.Bind("sdkqa.recovery", m_workspace, m_document, m_error));
        EXPECT_EQ(ReadItemRecoveryFile(m_document), "synthetic workspace marker");
    }

#if defined(Q_OS_WIN)
    TEST_F(ItemRecipeDraftRecoveryTests, LockedDestinationPreservesBytesForWriteAndDiscardRetry)
    {
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        const QString path = m_store->Path();
        const QByteArray bytes = ReadItemRecoveryFile(path);
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ASSERT_NE(handle, INVALID_HANDLE_VALUE);
        m_draft.m_drafts["item:a"].m_values["economyText"] = "later draft";
        EXPECT_FALSE(m_store->Write(m_draft, m_error));
        EXPECT_FALSE(m_store->Clear(m_error));
        EXPECT_EQ(ReadItemRecoveryFile(path), bytes);
        CloseHandle(handle);
        ASSERT_TRUE(m_store->Write(m_draft, m_error));
        EXPECT_EQ(m_store->Read().m_draft.m_drafts["item:a"].m_values["economyText"].toString(), "later draft");
    }
#endif
} // namespace TaintedGrailModdingSDK
