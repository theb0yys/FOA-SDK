/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetBrowserPreviewService.h"

#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QTemporaryDir>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* ProfileId = "tgfoa.profile.test";
        constexpr const char* GameVersion = "1.0.0";
        constexpr const char* Branch = "mono";
        constexpr const char* RuntimeTarget = "Mono";

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        bool WriteTextFile(const QString& path, const QByteArray& contents)
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly))
            {
                return false;
            }
            return file.write(contents) == contents.size();
        }

        QString WriteJsonFile(
            const QTemporaryDir& temporary,
            const QString& relativePath,
            const QJsonObject& object)
        {
            const QString path = QDir(temporary.path()).filePath(relativePath);
            EXPECT_TRUE(WriteTextFile(path, QJsonDocument(object).toJson(QJsonDocument::Indented)));
            return path;
        }

        QJsonObject AuthorityFalse()
        {
            return {
                { "RuntimePermissionGranted", false },
                { "DeploymentPermissionGranted", false },
                { "CatalogPromotionAllowed", false },
                { "TypedAuthoringBindingCreated", false },
                { "O3deAssetBrowserEntryCreated", false },
            };
        }

        QJsonObject PreviewStageFalse()
        {
            return {
                { "FunctionCompleteAllowed", false },
                { "TypedAuthoringBindingCreated", false },
                { "O3deAssetBrowserEntryCreated", false },
            };
        }

        QJsonObject SelectionPolicyFalse()
        {
            return {
                { "CanCreateTypedAuthoringBinding", false },
                { "RequiresExplicitBindingStep", true },
                { "CatalogPromotionAllowed", false },
                { "RuntimePermissionGranted", false },
            };
        }

        QJsonObject BaseDocument(const QString& documentKind)
        {
            return {
                { "DocumentKind", documentKind },
                { "SchemaVersion", 1 },
                { "ProfileId", ProfileId },
                { "GameVersion", GameVersion },
                { "Branch", Branch },
                { "RuntimeTarget", RuntimeTarget },
                { "OperationalAuthority", AuthorityFalse() },
                { "PreviewStageStatus", PreviewStageFalse() },
            };
        }

        QJsonObject ProductEntry(
            const QString& entryId,
            const QString& displayName,
            const QString& sourceRecordId,
            const QString& productPath)
        {
            return {
                { "PaneEntryId", entryId },
                { "EntryKind", "o3de-preview-product" },
                { "DisplayName", displayName },
                { "PreviewAvailability", "available" },
                { "ProductAssetIds", QJsonArray({ QString("product.%1").arg(entryId) }) },
                { "ProductCachePaths", QJsonArray({ productPath }) },
                { "PrimarySourceAssetRecordId", sourceRecordId },
                { "EvidenceRefs", QJsonArray({ QString("evidence.%1").arg(entryId) }) },
                { "IssueSeverity", "info" },
                { "Issues", QJsonArray() },
                { "SelectionPolicy", SelectionPolicyFalse() },
            };
        }

        QJsonObject BlockedEntry()
        {
            return {
                { "PaneEntryId", "pane.blocked.mesh" },
                { "EntryKind", "o3de-import-failure" },
                { "DisplayName", "unsupported_mesh.cgf" },
                { "PreviewAvailability", "blocked" },
                { "ProductAssetIds", QJsonArray() },
                { "ProductCachePaths", QJsonArray() },
                { "PrimarySourceAssetRecordId", "source.blocked.mesh" },
                { "EvidenceRefs", QJsonArray({ "evidence.blocked.mesh" }) },
                { "IssueSeverity", "error" },
                { "Issues", QJsonArray({
                    QJsonObject({
                        { "Severity", "error" },
                        { "Code", "unsupported-format" },
                        { "Message", "No reviewed preview conversion exists." },
                    })
                }) },
                { "SelectionPolicy", SelectionPolicyFalse() },
            };
        }

        QJsonObject PaneModelDocument(const QJsonArray& entries)
        {
            QJsonObject document = BaseDocument("foa-asset-browser-pane-model");
            document.insert(
                "InputContract",
                QJsonObject({
                    { "ImportProofEvidenceConsumed", true },
                    { "RawConversionEvidenceConsumed", false },
                    { "RawO3dePreviewSourceConsumed", false },
                    { "RawFoASourceConsumed", false },
                }));
            document.insert("PaneEntries", entries);
            return document;
        }

        QJsonObject ThumbnailEvidenceDocument(const QString& generatedPath)
        {
            QJsonObject document = BaseDocument("foa-thumbnail-artifact-evidence");
            document.insert(
                "ThumbnailArtifacts",
                QJsonArray({
                    QJsonObject({
                        { "AssetRecordId", "source.icon.texture" },
                        { "Status", "generated" },
                        { "Fidelity", "native-icon-byte-preserved" },
                        { "ArtifactPath", generatedPath },
                    })
                }));
            return document;
        }

        QJsonObject ViewportEvidenceDocument()
        {
            QJsonObject document = BaseDocument("foa-3d-preview-viewport-render");
            document.insert(
                "ViewportEntries",
                QJsonArray({
                    QJsonObject({
                        { "SourcePaneEntryId", "pane.icon.texture" },
                        { "ViewportState", "central-viewport-route-ready" },
                    })
                }));
            return document;
        }

        AssetBrowserPreviewLoadRequest BuildRequest(
            const QTemporaryDir& temporary,
            const QString& paneModelPath,
            const QString& thumbnailPath = {},
            const QString& viewportPath = {})
        {
            AssetBrowserPreviewLoadRequest request;
            request.m_profileId = ProfileId;
            request.m_gameVersion = GameVersion;
            request.m_branch = Branch;
            request.m_runtimeTarget = RuntimeTarget;
            request.m_extractedDataPath = ToAzString(temporary.path());
            request.m_paneModelPath = ToAzString(paneModelPath);
            request.m_thumbnailEvidencePath = ToAzString(thumbnailPath);
            request.m_thumbnailPreviewRootPath = ToAzString(temporary.path());
            request.m_viewportEvidencePath = ToAzString(viewportPath);
            return request;
        }

        const AssetBrowserPreviewEntry* FindEntry(
            const AssetBrowserPreviewSnapshot& snapshot,
            const AZStd::string& entryId)
        {
            const auto iterator = AZStd::find_if(
                snapshot.m_entries.begin(),
                snapshot.m_entries.end(),
                [&entryId](const AssetBrowserPreviewEntry& entry)
                {
                    return entry.m_entryId == entryId;
                });
            return iterator == snapshot.m_entries.end() ? nullptr : &*iterator;
        }
    } // namespace

    TEST(AssetBrowserPreviewServiceTests, CategorizesThumbnailEvidenceAndViewportRoute)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());

        const QString thumbnailFile = QDir(temporary.path()).filePath("thumbs/icon.png");
        EXPECT_TRUE(WriteTextFile(
            thumbnailFile,
            QByteArray::fromBase64(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=")));

        const QString panePath = WriteJsonFile(
            temporary,
            "asset-browser-pane-model.json",
            PaneModelDocument(QJsonArray({
                ProductEntry(
                    "pane.icon.texture",
                    "foa_inventory_icon.png",
                    "source.icon.texture",
                    "$assetcache/foa/textures/foa_inventory_icon.streamingimage")
            })));
        const QString thumbnailPath = WriteJsonFile(
            temporary,
            "thumbnail-artifact-evidence.json",
            ThumbnailEvidenceDocument("$preview/thumbs/icon.png"));
        const QString viewportPath = WriteJsonFile(
            temporary,
            "viewport-render.json",
            ViewportEvidenceDocument());

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(BuildRequest(temporary, panePath, thumbnailPath, viewportPath));
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();

        const AssetBrowserPreviewEntry* entry = FindEntry(result.GetValue(), "pane.icon.texture");
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->m_category, "Textures and icons");
        EXPECT_EQ(entry->m_fidelityState, "exact");
        EXPECT_EQ(entry->m_thumbnailStatus, "generated");
        EXPECT_FALSE(entry->m_thumbnailPath.empty());
        EXPECT_TRUE(entry->m_canRouteToViewport);
        EXPECT_FALSE(entry->m_canCreateTypedAuthoringBinding);
        EXPECT_TRUE(entry->m_requiresExplicitBindingStep);
        EXPECT_EQ(entry->m_viewportRouteState, "central-viewport-route-ready");

        auto route = service.PrepareViewportRoute(
            BuildRequest(temporary, panePath, thumbnailPath, viewportPath),
            *entry);
        ASSERT_TRUE(route.IsSuccess()) << route.GetError().c_str();
        EXPECT_EQ(route.GetValue().m_routeId, "assetbrowser.viewport-route:pane.icon.texture");
        EXPECT_EQ(route.GetValue().m_profileId, ProfileId);
        EXPECT_EQ(route.GetValue().m_productAssetId, "product.pane.icon.texture");
        EXPECT_EQ(route.GetValue().m_productCachePath, "$assetcache/foa/textures/foa_inventory_icon.streamingimage");
        ASSERT_EQ(route.GetValue().m_evidenceRefs.size(), 1);
        EXPECT_EQ(route.GetValue().m_evidenceRefs[0], "evidence.pane.icon.texture");
        EXPECT_FALSE(route.GetValue().m_o3deViewportMutationAllowed);
        EXPECT_FALSE(route.GetValue().m_typedAuthoringBindingCreated);
        EXPECT_FALSE(route.GetValue().m_catalogPromotionAllowed);
        EXPECT_FALSE(route.GetValue().m_runtimePermissionGranted);

        AssetBrowserPreviewRouteRegistry::Get().Clear();
        AZStd::string error;
        EXPECT_TRUE(AssetBrowserPreviewRouteRegistry::Get().RegisterRoute(route.GetValue(), &error))
            << error.c_str();
        const AssetBrowserPreviewViewportRoute* stored =
            AssetBrowserPreviewRouteRegistry::Get().FindByRouteId(route.GetValue().m_routeId);
        ASSERT_NE(stored, nullptr);
        EXPECT_EQ(stored->m_entryId, "pane.icon.texture");
        ASSERT_NE(AssetBrowserPreviewRouteRegistry::Get().GetLatestRoute(), nullptr);
        EXPECT_EQ(AssetBrowserPreviewRouteRegistry::Get().GetRoutes().size(), 1);
        AssetBrowserPreviewRouteRegistry::Get().Clear();
    }

    TEST(AssetBrowserPreviewServiceTests, ThumbnailEvidenceAloneLoadsAsViewerEntries)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());

        const QString thumbnailFile = QDir(temporary.path()).filePath("thumbs/icon.png");
        EXPECT_TRUE(WriteTextFile(
            thumbnailFile,
            QByteArray::fromBase64(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=")));

        const QString thumbnailPath = WriteJsonFile(
            temporary,
            "thumbnail-artifact-evidence.json",
            ThumbnailEvidenceDocument("$preview/thumbs/icon.png"));

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(BuildRequest(temporary, {}, thumbnailPath));
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();

        ASSERT_EQ(result.GetValue().m_entries.size(), 1);
        const AssetBrowserPreviewEntry& entry = result.GetValue().m_entries[0];
        EXPECT_EQ(entry.m_entryId, "source.icon.texture");
        EXPECT_EQ(entry.m_category, "Textures and icons");
        EXPECT_EQ(entry.m_entryKind, "thumbnail-artifact");
        EXPECT_EQ(entry.m_primarySourceAssetRecordId, "source.icon.texture");
        EXPECT_EQ(entry.m_thumbnailStatus, "generated");
        EXPECT_FALSE(entry.m_thumbnailPath.empty());
        EXPECT_FALSE(entry.m_canRouteToViewport);
        EXPECT_FALSE(entry.m_canCreateTypedAuthoringBinding);
        EXPECT_TRUE(entry.m_requiresExplicitBindingStep);
    }

    TEST(AssetBrowserPreviewServiceTests, CustomAssetsFolderLoadsWithoutEvidenceDocuments)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());

        const QString assetsRoot = QDir(temporary.path()).filePath("Assets");
        const QString iconFile = QDir(assetsRoot).filePath("Icons/inventory.png");
        EXPECT_TRUE(WriteTextFile(
            iconFile,
            QByteArray::fromBase64(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=")));
        EXPECT_TRUE(WriteTextFile(QDir(assetsRoot).filePath("Meshes/rock.fbx"), "fbx"));

        AssetBrowserPreviewLoadRequest request;
        request.m_customAssetsPath = ToAzString(assetsRoot);

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(request);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();

        ASSERT_EQ(result.GetValue().m_entries.size(), 2);
        const AssetBrowserPreviewEntry* icon = FindEntry(
            result.GetValue(),
            "custom.asset:Icons/inventory.png");
        ASSERT_NE(icon, nullptr);
        EXPECT_EQ(icon->m_category, "Textures and icons");
        EXPECT_EQ(icon->m_fidelityState, "source");
        EXPECT_EQ(icon->m_thumbnailStatus, "source-image");
        EXPECT_FALSE(icon->m_thumbnailPath.empty());
        EXPECT_FALSE(icon->m_canRouteToViewport);

        const AssetBrowserPreviewEntry* mesh = FindEntry(
            result.GetValue(),
            "custom.asset:Meshes/rock.fbx");
        ASSERT_NE(mesh, nullptr);
        EXPECT_EQ(mesh->m_category, "Meshes");
        EXPECT_EQ(mesh->m_thumbnailStatus, "indexed");
        EXPECT_TRUE(mesh->m_thumbnailPath.empty());
        EXPECT_FALSE(mesh->m_canRouteToViewport);
    }

    TEST(AssetBrowserPreviewServiceTests, ImportFailuresBecomeBlockedAndUnroutable)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());
        const QString panePath = WriteJsonFile(
            temporary,
            "asset-browser-pane-model.json",
            PaneModelDocument(QJsonArray({ BlockedEntry() })));

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(BuildRequest(temporary, panePath));
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();

        const AssetBrowserPreviewEntry* entry = FindEntry(result.GetValue(), "pane.blocked.mesh");
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->m_category, "Blocked or unsupported");
        EXPECT_EQ(entry->m_fidelityState, "blocked");
        EXPECT_FALSE(entry->m_canRouteToViewport);
        EXPECT_EQ(entry->m_viewportRouteState, "not-routable");
        EXPECT_EQ(entry->m_blocker, "No reviewed preview conversion exists.");

        auto route = service.PrepareViewportRoute(BuildRequest(temporary, panePath), *entry);
        EXPECT_FALSE(route.IsSuccess());
    }

    TEST(AssetBrowserPreviewServiceTests, SelectionAuthorityEscalationFailsClosed)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());

        QJsonObject entry = ProductEntry(
            "pane.material",
            "reviewed.material",
            "source.material",
            "$assetcache/foa/materials/reviewed.material");
        QJsonObject policy = entry.value("SelectionPolicy").toObject();
        policy.insert("CanCreateTypedAuthoringBinding", true);
        entry.insert("SelectionPolicy", policy);
        const QString panePath = WriteJsonFile(
            temporary,
            "asset-browser-pane-model.json",
            PaneModelDocument(QJsonArray({ entry })));

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(BuildRequest(temporary, panePath));
        EXPECT_FALSE(result.IsSuccess());
    }

    TEST(AssetBrowserPreviewServiceTests, ThumbnailTraversalOutsidePreviewRootFailsClosed)
    {
        QTemporaryDir temporary;
        ASSERT_TRUE(temporary.isValid());

        const QString panePath = WriteJsonFile(
            temporary,
            "asset-browser-pane-model.json",
            PaneModelDocument(QJsonArray({
                ProductEntry(
                    "pane.icon.texture",
                    "foa_inventory_icon.png",
                    "source.icon.texture",
                    "$assetcache/foa/textures/foa_inventory_icon.streamingimage")
            })));
        const QString thumbnailPath = WriteJsonFile(
            temporary,
            "thumbnail-artifact-evidence.json",
            ThumbnailEvidenceDocument("$preview/../outside.png"));

        AssetBrowserPreviewService service;
        auto result = service.LoadPreview(BuildRequest(temporary, panePath, thumbnailPath));
        EXPECT_FALSE(result.IsSuccess());
    }
} // namespace TaintedGrailModdingSDK
