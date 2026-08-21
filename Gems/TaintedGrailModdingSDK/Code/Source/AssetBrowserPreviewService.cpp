/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AssetBrowserPreviewService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>

#include <cctype>
#include <initializer_list>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* PaneModelDocumentKind = "foa-asset-browser-pane-model";
        constexpr const char* ThumbnailEvidenceDocumentKind = "foa-thumbnail-artifact-evidence";
        constexpr const char* ViewportEvidenceDocumentKind = "foa-3d-preview-viewport-render";

        struct ThumbnailBinding
        {
            QString m_status;
            QString m_fidelity;
            QString m_path;
        };

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::vector<AZStd::string> StringArray(const QJsonValue& value)
        {
            AZStd::vector<AZStd::string> values;
            if (!value.isArray())
            {
                return values;
            }

            const QJsonArray array = value.toArray();
            values.reserve(static_cast<size_t>(array.size()));
            for (const QJsonValue& item : array)
            {
                if (item.isString())
                {
                    values.push_back(ToAzString(item.toString()));
                }
            }
            return values;
        }

        QString FirstString(const QJsonObject& object, std::initializer_list<const char*> keys)
        {
            for (const char* key : keys)
            {
                const QJsonValue value = object.value(QString::fromUtf8(key));
                if (value.isString() && !value.toString().isEmpty())
                {
                    return value.toString();
                }
            }
            return {};
        }

        bool HasNonFalseFlag(const QJsonObject& object)
        {
            if (object.isEmpty())
            {
                return true;
            }
            for (auto iterator = object.begin(); iterator != object.end(); ++iterator)
            {
                if (!iterator.value().isBool() || iterator.value().toBool())
                {
                    return true;
                }
            }
            return false;
        }

        bool ExplicitFalse(const QJsonObject& object, const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            return value.isBool() && !value.toBool();
        }

        QString CleanAbsolutePath(const QString& path)
        {
            return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        }

        bool IsInsideOrEqual(const QString& path, const QString& root)
        {
            if (path.trimmed().isEmpty() || root.trimmed().isEmpty())
            {
                return false;
            }

            const QString cleanPath = CleanAbsolutePath(path);
            const QString cleanRoot = CleanAbsolutePath(root);
            const QString relative = QDir(cleanRoot).relativeFilePath(cleanPath);
            return relative == "."
                || (!relative.isEmpty()
                    && !QDir::isAbsolutePath(relative)
                    && relative != ".."
                    && !relative.startsWith(QStringLiteral("../"))
                    && !relative.startsWith(QStringLiteral("..\\")));
        }

        AZ::Outcome<QJsonObject, AZStd::string> ReadJsonObject(
            const QString& path,
            const QString& extractedRoot,
            const char* label)
        {
            if (!IsInsideOrEqual(path, extractedRoot))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s must be inside the configured extracted-data directory.",
                    label));
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                return AZ::Failure(AZStd::string::format(
                    "Unable to open %s: %s",
                    label,
                    ToAzString(path).c_str()));
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError)
            {
                return AZ::Failure(AZStd::string::format(
                    "%s is not valid JSON: %s",
                    label,
                    ToAzString(parseError.errorString()).c_str()));
            }
            if (!document.isObject())
            {
                return AZ::Failure(AZStd::string::format("%s must be a JSON object.", label));
            }
            return AZ::Success(document.object());
        }

        AZ::Outcome<void, AZStd::string> RequireDocumentHeader(
            const QJsonObject& document,
            const char* expectedKind,
            const AssetBrowserPreviewLoadRequest& request,
            const char* label)
        {
            if (document.value(QStringLiteral("DocumentKind")).toString() != QString::fromUtf8(expectedKind)
                || document.value(QStringLiteral("SchemaVersion")).toInt() != 1)
            {
                return AZ::Failure(AZStd::string::format(
                    "%s is not the expected schema-1 %s document.",
                    label,
                    expectedKind));
            }
            if (document.value(QStringLiteral("ProfileId")).toString() != ToQString(request.m_profileId)
                || document.value(QStringLiteral("GameVersion")).toString() != ToQString(request.m_gameVersion)
                || document.value(QStringLiteral("Branch")).toString() != ToQString(request.m_branch))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s does not match the active exact FoA profile.",
                    label));
            }

            const QJsonValue runtimeTarget = document.value(QStringLiteral("RuntimeTarget"));
            if (runtimeTarget.isString()
                && runtimeTarget.toString() != ToQString(request.m_runtimeTarget))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s runtime target does not match the active profile.",
                    label));
            }

            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> RequireNoAuthority(
            const QJsonObject& document,
            const char* label)
        {
            const QJsonObject authority = document.value(QStringLiteral("OperationalAuthority")).toObject();
            if (HasNonFalseFlag(authority))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s declares runtime, deployment, catalog, or binding authority.",
                    label));
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> RequireStageStillPreview(
            const QJsonObject& document,
            const char* label)
        {
            const QJsonObject stage = document.value(QStringLiteral("PreviewStageStatus")).toObject();
            if (!ExplicitFalse(stage, "FunctionCompleteAllowed"))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s must explicitly keep FunctionCompleteAllowed false.",
                    label));
            }
            if (stage.contains(QStringLiteral("TypedAuthoringBindingCreated"))
                && !ExplicitFalse(stage, "TypedAuthoringBindingCreated"))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s cannot create typed authoring bindings.",
                    label));
            }
            if (stage.contains(QStringLiteral("O3deAssetBrowserEntryCreated"))
                && !ExplicitFalse(stage, "O3deAssetBrowserEntryCreated"))
            {
                return AZ::Failure(AZStd::string::format(
                    "%s cannot claim O3DE Asset Browser entries.",
                    label));
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> RequirePaneInputContract(
            const QJsonObject& document)
        {
            const QJsonObject inputContract = document.value(QStringLiteral("InputContract")).toObject();
            if (!inputContract.value(QStringLiteral("ImportProofEvidenceConsumed")).toBool())
            {
                return AZ::Failure(AZStd::string(
                    "Asset Browser pane model must consume import-proof evidence."));
            }
            for (const char* forbidden : {
                     "RawConversionFileConsumed",
                     "RawConversionEvidenceConsumed",
                     "RawO3dePreviewSourceConsumed",
                     "RawFoASourceConsumed" })
            {
                if (inputContract.value(QString::fromUtf8(forbidden)).toBool())
                {
                    return AZ::Failure(AZStd::string::format(
                        "Asset Browser pane model cannot consume %s.",
                        forbidden));
                }
            }
            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> RequireSelectionPolicy(
            const QJsonObject& policy,
            const QString& entryId)
        {
            if (policy.value(QStringLiteral("CanCreateTypedAuthoringBinding")).toBool()
                || policy.value(QStringLiteral("CatalogPromotionAllowed")).toBool()
                || policy.value(QStringLiteral("RuntimePermissionGranted")).toBool())
            {
                return AZ::Failure(AZStd::string::format(
                    "Pane entry %s escalates selection authority.",
                    ToAzString(entryId).c_str()));
            }
            if (!policy.value(QStringLiteral("RequiresExplicitBindingStep")).toBool())
            {
                return AZ::Failure(AZStd::string::format(
                    "Pane entry %s must require a separate explicit binding step.",
                    ToAzString(entryId).c_str()));
            }
            return AZ::Success();
        }

        bool HasProductPath(const AssetBrowserPreviewEntry& entry)
        {
            return AZStd::any_of(
                entry.m_productCachePaths.begin(),
                entry.m_productCachePaths.end(),
                [](const AZStd::string& path)
                {
                    return !path.empty();
                });
        }

        bool HasBlockedIssue(const QJsonArray& issues)
        {
            for (const QJsonValue& value : issues)
            {
                const QJsonObject issue = value.toObject();
                const QString severity = issue.value(QStringLiteral("Severity")).toString();
                if (severity == "error" || severity == "blocked" || severity == "unsupported")
                {
                    return true;
                }
            }
            return false;
        }

        QString ExtractPrimaryIssue(const QJsonArray& issues)
        {
            for (const QJsonValue& value : issues)
            {
                const QJsonObject issue = value.toObject();
                const QString message = issue.value(QStringLiteral("Message")).toString();
                if (!message.isEmpty())
                {
                    return message;
                }
                const QString code = issue.value(QStringLiteral("Code")).toString();
                if (!code.isEmpty())
                {
                    return code;
                }
            }
            return {};
        }

        bool IsPreviewProductPath(const AZStd::string& path)
        {
            return !path.empty()
                && (path.rfind("$assetcache/", 0) == 0
                    || path.rfind("$assetcache\\", 0) == 0);
        }

        AZ::Outcome<QHash<QString, ThumbnailBinding>, AZStd::string> LoadThumbnails(
            const AssetBrowserPreviewLoadRequest& request,
            const QString& extractedRoot)
        {
            QHash<QString, ThumbnailBinding> thumbnails;
            if (request.m_thumbnailEvidencePath.empty())
            {
                return AZ::Success(thumbnails);
            }

            const QString evidencePath = ToQString(request.m_thumbnailEvidencePath);
            auto documentResult = ReadJsonObject(
                evidencePath,
                extractedRoot,
                "thumbnail evidence");
            if (!documentResult.IsSuccess())
            {
                return AZ::Failure(documentResult.GetError());
            }
            const QJsonObject document = documentResult.TakeValue();
            auto header = RequireDocumentHeader(
                document,
                ThumbnailEvidenceDocumentKind,
                request,
                "thumbnail evidence");
            if (!header.IsSuccess())
            {
                return AZ::Failure(header.GetError());
            }
            auto authority = RequireNoAuthority(document, "thumbnail evidence");
            if (!authority.IsSuccess())
            {
                return AZ::Failure(authority.GetError());
            }
            auto stage = RequireStageStillPreview(document, "thumbnail evidence");
            if (!stage.IsSuccess())
            {
                return AZ::Failure(stage.GetError());
            }

            QString previewRoot = ToQString(request.m_thumbnailPreviewRootPath);
            if (previewRoot.trimmed().isEmpty())
            {
                previewRoot = QFileInfo(evidencePath).absolutePath();
            }
            if (!IsInsideOrEqual(previewRoot, extractedRoot))
            {
                return AZ::Failure(AZStd::string(
                    "Thumbnail preview root must be inside the extracted-data directory."));
            }

            const QJsonArray artifacts = document.value(QStringLiteral("ThumbnailArtifacts")).toArray();
            for (const QJsonValue& value : artifacts)
            {
                const QJsonObject artifact = value.toObject();
                const QString assetRecordId = artifact.value(QStringLiteral("AssetRecordId")).toString();
                if (assetRecordId.isEmpty())
                {
                    continue;
                }
                ThumbnailBinding binding;
                binding.m_status = artifact.value(QStringLiteral("Status")).toString();
                binding.m_fidelity = artifact.value(QStringLiteral("Fidelity")).toString();

                const QString artifactPath = FirstString(
                    artifact,
                    { "GeneratedArtifactPath", "ArtifactPath" });
                if (!artifactPath.isEmpty() && artifactPath.startsWith(QStringLiteral("$preview/")))
                {
                    const QString relative = artifactPath.mid(QStringLiteral("$preview/").size());
                    const QString resolved = QDir(previewRoot).filePath(relative);
                    if (!IsInsideOrEqual(resolved, previewRoot) || !QFileInfo::exists(resolved))
                    {
                        return AZ::Failure(AZStd::string::format(
                            "Thumbnail artifact for %s is missing or escapes the preview root.",
                            ToAzString(assetRecordId).c_str()));
                    }
                    binding.m_path = CleanAbsolutePath(resolved);
                }

                thumbnails.insert(assetRecordId, binding);
            }

            return AZ::Success(thumbnails);
        }

        AZ::Outcome<QHash<QString, QString>, AZStd::string> LoadViewportRoutes(
            const AssetBrowserPreviewLoadRequest& request,
            const QString& extractedRoot)
        {
            QHash<QString, QString> routes;
            if (request.m_viewportEvidencePath.empty())
            {
                return AZ::Success(routes);
            }

            auto documentResult = ReadJsonObject(
                ToQString(request.m_viewportEvidencePath),
                extractedRoot,
                "viewport evidence");
            if (!documentResult.IsSuccess())
            {
                return AZ::Failure(documentResult.GetError());
            }
            const QJsonObject document = documentResult.TakeValue();
            auto header = RequireDocumentHeader(
                document,
                ViewportEvidenceDocumentKind,
                request,
                "viewport evidence");
            if (!header.IsSuccess())
            {
                return AZ::Failure(header.GetError());
            }
            auto authority = RequireNoAuthority(document, "viewport evidence");
            if (!authority.IsSuccess())
            {
                return AZ::Failure(authority.GetError());
            }
            auto stage = RequireStageStillPreview(document, "viewport evidence");
            if (!stage.IsSuccess())
            {
                return AZ::Failure(stage.GetError());
            }

            const QJsonArray entries = document.value(QStringLiteral("ViewportEntries")).toArray();
            for (const QJsonValue& value : entries)
            {
                const QJsonObject entry = value.toObject();
                const QString paneEntryId = entry.value(QStringLiteral("SourcePaneEntryId")).toString();
                if (!paneEntryId.isEmpty())
                {
                    routes.insert(paneEntryId, entry.value(QStringLiteral("ViewportState")).toString());
                }
            }
            return AZ::Success(routes);
        }

        AZ::Outcome<AssetBrowserPreviewEntry, AZStd::string> BuildEntry(
            const QJsonObject& object,
            const QHash<QString, ThumbnailBinding>& thumbnails,
            const QHash<QString, QString>& viewportRoutes)
        {
            const QString entryId = object.value(QStringLiteral("PaneEntryId")).toString();
            if (entryId.isEmpty())
            {
                return AZ::Failure(AZStd::string("Pane entry is missing PaneEntryId."));
            }

            const QJsonObject selectionPolicy = object.value(QStringLiteral("SelectionPolicy")).toObject();
            auto policy = RequireSelectionPolicy(selectionPolicy, entryId);
            if (!policy.IsSuccess())
            {
                return AZ::Failure(policy.GetError());
            }

            AssetBrowserPreviewEntry entry;
            entry.m_entryId = ToAzString(entryId);
            entry.m_displayName = ToAzString(object.value(QStringLiteral("DisplayName")).toString());
            if (entry.m_displayName.empty())
            {
                entry.m_displayName = entry.m_entryId;
            }
            entry.m_entryKind = ToAzString(object.value(QStringLiteral("EntryKind")).toString());
            entry.m_previewAvailability =
                ToAzString(object.value(QStringLiteral("PreviewAvailability")).toString());
            entry.m_productAssetIds = StringArray(object.value(QStringLiteral("ProductAssetIds")));
            entry.m_productCachePaths = StringArray(object.value(QStringLiteral("ProductCachePaths")));
            entry.m_evidenceRefs = StringArray(object.value(QStringLiteral("EvidenceRefs")));
            entry.m_primarySourceAssetRecordId =
                ToAzString(object.value(QStringLiteral("PrimarySourceAssetRecordId")).toString());
            entry.m_canCreateTypedAuthoringBinding = false;
            entry.m_requiresExplicitBindingStep = true;

            if (!entry.m_productAssetIds.empty())
            {
                entry.m_productAssetId = entry.m_productAssetIds.front();
            }
            if (!entry.m_productCachePaths.empty())
            {
                entry.m_productCachePath = entry.m_productCachePaths.front();
            }
            for (const AZStd::string& path : entry.m_productCachePaths)
            {
                if (!IsPreviewProductPath(path))
                {
                    return AZ::Failure(AZStd::string::format(
                        "Pane entry %s has a product cache path outside the O3DE asset-cache token boundary.",
                        entry.m_entryId.c_str()));
                }
            }

            const QJsonArray issues = object.value(QStringLiteral("Issues")).toArray();
            if (HasBlockedIssue(issues))
            {
                entry.m_blocker = ToAzString(ExtractPrimaryIssue(issues));
            }
            if (entry.m_blocker.empty())
            {
                entry.m_blocker = ToAzString(object.value(QStringLiteral("IssueSeverity")).toString());
                if (entry.m_blocker == "info" || entry.m_blocker == "warning")
                {
                    entry.m_blocker.clear();
                }
            }

            const QString sourceRecordId = QString::fromUtf8(entry.m_primarySourceAssetRecordId.c_str());
            if (thumbnails.contains(sourceRecordId))
            {
                const ThumbnailBinding thumbnail = thumbnails.value(sourceRecordId);
                entry.m_thumbnailStatus = ToAzString(thumbnail.m_status);
                entry.m_thumbnailFidelity = ToAzString(thumbnail.m_fidelity);
                entry.m_thumbnailPath = ToAzString(thumbnail.m_path);
            }
            else
            {
                entry.m_thumbnailStatus = "not-provided";
            }

            if (viewportRoutes.contains(entryId))
            {
                entry.m_viewportRouteState = ToAzString(viewportRoutes.value(entryId));
            }
            else if (HasProductPath(entry))
            {
                entry.m_viewportRouteState = "preview-product-reference-available";
            }
            else
            {
                entry.m_viewportRouteState = "not-routable";
            }

            entry.m_canRouteToViewport = HasProductPath(entry) && entry.m_blocker.empty();
            entry.m_fidelityState = AssetBrowserPreviewService::DetermineFidelityState(
                entry,
                viewportRoutes.contains(entryId));
            entry.m_category = AssetBrowserPreviewService::ClassifyCategory(entry);
            return AZ::Success(AZStd::move(entry));
        }

        void AddUnique(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            if (AZStd::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(value);
            }
        }
    } // namespace

    AZ::Outcome<AssetBrowserPreviewSnapshot, AZStd::string> AssetBrowserPreviewService::LoadPreview(
        const AssetBrowserPreviewLoadRequest& request) const
    {
        if (request.m_profileId.empty()
            || request.m_gameVersion.empty()
            || request.m_branch.empty()
            || request.m_runtimeTarget.empty())
        {
            return AZ::Failure(AZStd::string(
                "Configure an exact active FoA game profile before loading Asset Browser preview evidence."));
        }
        if (request.m_extractedDataPath.empty() || request.m_paneModelPath.empty())
        {
            return AZ::Failure(AZStd::string(
                "Asset Browser preview requires an extracted-data root and pane-model evidence path."));
        }
        if (request.m_maximumEntries == 0)
        {
            return AZ::Failure(AZStd::string("Asset Browser preview maximum entry count must be greater than zero."));
        }

        const QString extractedRoot = ToQString(request.m_extractedDataPath);
        auto paneDocumentResult = ReadJsonObject(
            ToQString(request.m_paneModelPath),
            extractedRoot,
            "Asset Browser pane model");
        if (!paneDocumentResult.IsSuccess())
        {
            return AZ::Failure(paneDocumentResult.GetError());
        }
        const QJsonObject paneDocument = paneDocumentResult.TakeValue();
        auto header = RequireDocumentHeader(
            paneDocument,
            PaneModelDocumentKind,
            request,
            "Asset Browser pane model");
        if (!header.IsSuccess())
        {
            return AZ::Failure(header.GetError());
        }
        auto authority = RequireNoAuthority(paneDocument, "Asset Browser pane model");
        if (!authority.IsSuccess())
        {
            return AZ::Failure(authority.GetError());
        }
        auto stage = RequireStageStillPreview(paneDocument, "Asset Browser pane model");
        if (!stage.IsSuccess())
        {
            return AZ::Failure(stage.GetError());
        }
        auto inputContract = RequirePaneInputContract(paneDocument);
        if (!inputContract.IsSuccess())
        {
            return AZ::Failure(inputContract.GetError());
        }

        auto thumbnailResult = LoadThumbnails(request, extractedRoot);
        if (!thumbnailResult.IsSuccess())
        {
            return AZ::Failure(thumbnailResult.GetError());
        }
        auto viewportResult = LoadViewportRoutes(request, extractedRoot);
        if (!viewportResult.IsSuccess())
        {
            return AZ::Failure(viewportResult.GetError());
        }

        const QHash<QString, ThumbnailBinding> thumbnails = thumbnailResult.TakeValue();
        const QHash<QString, QString> viewportRoutes = viewportResult.TakeValue();
        const QJsonArray paneEntries = paneDocument.value(QStringLiteral("PaneEntries")).toArray();
        if (paneEntries.isEmpty())
        {
            return AZ::Failure(AZStd::string("Asset Browser pane model has no PaneEntries."));
        }
        if (static_cast<size_t>(paneEntries.size()) > request.m_maximumEntries)
        {
            return AZ::Failure(AZStd::string::format(
                "Asset Browser pane model has %d entries, exceeding the bounded UI limit of %zu.",
                paneEntries.size(),
                request.m_maximumEntries));
        }

        AssetBrowserPreviewSnapshot snapshot;
        snapshot.m_entries.reserve(static_cast<size_t>(paneEntries.size()));
        for (const QJsonValue& value : paneEntries)
        {
            auto entry = BuildEntry(value.toObject(), thumbnails, viewportRoutes);
            if (!entry.IsSuccess())
            {
                return AZ::Failure(entry.GetError());
            }
            AddUnique(snapshot.m_categories, entry.GetValue().m_category);
            snapshot.m_entries.push_back(entry.TakeValue());
        }

        AZStd::sort(
            snapshot.m_entries.begin(),
            snapshot.m_entries.end(),
            [](const AssetBrowserPreviewEntry& left, const AssetBrowserPreviewEntry& right)
            {
                if (left.m_category != right.m_category)
                {
                    return left.m_category < right.m_category;
                }
                if (left.m_displayName != right.m_displayName)
                {
                    return left.m_displayName < right.m_displayName;
                }
                return left.m_entryId < right.m_entryId;
            });
        AZStd::sort(snapshot.m_categories.begin(), snapshot.m_categories.end());
        return AZ::Success(AZStd::move(snapshot));
    }

    AZStd::string AssetBrowserPreviewService::ClassifyCategory(
        const AssetBrowserPreviewEntry& entry)
    {
        if (!entry.m_blocker.empty() || entry.m_entryKind == "o3de-import-failure")
        {
            return "Blocked or unsupported";
        }

        AZStd::string text = entry.m_displayName + " " + entry.m_productCachePath + " "
            + entry.m_primarySourceAssetRecordId + " " + entry.m_entryKind;
        AZStd::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](char value)
            {
                return static_cast<char>(::tolower(static_cast<unsigned char>(value)));
            });

        if (text.find("material") != AZStd::string::npos || text.find(".material") != AZStd::string::npos)
        {
            return "Materials";
        }
        if (text.find("terrain") != AZStd::string::npos || text.find("heightmap") != AZStd::string::npos)
        {
            return "Terrain";
        }
        if (text.find("prefab") != AZStd::string::npos || text.find("spawnable") != AZStd::string::npos)
        {
            return "Prefabs";
        }
        if (text.find("mesh") != AZStd::string::npos
            || text.find(".fbx") != AZStd::string::npos
            || text.find(".obj") != AZStd::string::npos
            || text.find(".gltf") != AZStd::string::npos
            || text.find(".glb") != AZStd::string::npos
            || text.find(".azmodel") != AZStd::string::npos)
        {
            return "Meshes";
        }
        if (text.find("texture") != AZStd::string::npos
            || text.find("icon") != AZStd::string::npos
            || text.find(".png") != AZStd::string::npos
            || text.find(".jpg") != AZStd::string::npos
            || text.find(".jpeg") != AZStd::string::npos
            || text.find(".webp") != AZStd::string::npos
            || text.find(".dds") != AZStd::string::npos
            || text.find(".tga") != AZStd::string::npos)
        {
            return "Textures and icons";
        }
        return "Preview products";
    }

    AZStd::string AssetBrowserPreviewService::DetermineFidelityState(
        const AssetBrowserPreviewEntry& entry,
        bool hasViewportEvidence)
    {
        if (!entry.m_blocker.empty() || entry.m_entryKind == "o3de-import-failure")
        {
            return "blocked";
        }
        if (entry.m_thumbnailStatus == "unsupported")
        {
            return "unsupported";
        }
        if (entry.m_thumbnailStatus == "generated"
            && entry.m_thumbnailFidelity == "native-icon-byte-preserved"
            && !entry.m_thumbnailPath.empty())
        {
            return "exact";
        }
        if (entry.m_thumbnailStatus == "generated" && !entry.m_thumbnailPath.empty())
        {
            return "approximate";
        }
        if (HasProductPath(entry) || hasViewportEvidence)
        {
            return "partial";
        }
        return "placeholder";
    }

    AZ::Outcome<AssetBrowserPreviewViewportRoute, AZStd::string>
    AssetBrowserPreviewService::PrepareViewportRoute(
        const AssetBrowserPreviewLoadRequest& request,
        const AssetBrowserPreviewEntry& entry) const
    {
        if (request.m_profileId.empty()
            || request.m_gameVersion.empty()
            || request.m_branch.empty()
            || request.m_runtimeTarget.empty())
        {
            return AZ::Failure(AZStd::string(
                "Configure an exact active FoA game profile before routing preview evidence."));
        }
        if (entry.m_entryId.empty())
        {
            return AZ::Failure(AZStd::string("Asset Browser route requires a stable pane entry identity."));
        }
        if (!entry.m_canRouteToViewport || !HasProductPath(entry))
        {
            return AZ::Failure(AZStd::string(
                "The selected preview product is not routable to the central viewport."));
        }
        if (entry.m_canCreateTypedAuthoringBinding || !entry.m_requiresExplicitBindingStep)
        {
            return AZ::Failure(AZStd::string(
                "Asset Browser route cannot create typed authoring bindings."));
        }
        if (!entry.m_blocker.empty())
        {
            return AZ::Failure(AZStd::string(
                "Blocked Asset Browser preview entries cannot be routed."));
        }

        AssetBrowserPreviewViewportRoute route;
        route.m_routeId = "assetbrowser.viewport-route:" + entry.m_entryId;
        route.m_profileId = request.m_profileId;
        route.m_gameVersion = request.m_gameVersion;
        route.m_branch = request.m_branch;
        route.m_runtimeTarget = request.m_runtimeTarget;
        route.m_entryId = entry.m_entryId;
        route.m_productAssetId = entry.m_productAssetId;
        route.m_productCachePath = entry.m_productCachePath;
        route.m_primarySourceAssetRecordId = entry.m_primarySourceAssetRecordId;
        route.m_fidelityState = entry.m_fidelityState;
        route.m_viewportRouteState = entry.m_viewportRouteState;
        route.m_evidenceRefs = entry.m_evidenceRefs;
        return AZ::Success(AZStd::move(route));
    }

    AssetBrowserPreviewRouteRegistry& AssetBrowserPreviewRouteRegistry::Get()
    {
        static AssetBrowserPreviewRouteRegistry registry;
        return registry;
    }

    bool AssetBrowserPreviewRouteRegistry::RegisterRoute(
        const AssetBrowserPreviewViewportRoute& route,
        AZStd::string* error)
    {
        if (route.m_routeId.empty()
            || route.m_profileId.empty()
            || route.m_entryId.empty()
            || route.m_productAssetId.empty()
            || route.m_productCachePath.empty())
        {
            if (error)
            {
                *error = "Asset Browser viewport route requires stable route, profile, entry, and product identities.";
            }
            return false;
        }
        if (route.m_o3deViewportMutationAllowed
            || route.m_typedAuthoringBindingCreated
            || route.m_catalogPromotionAllowed
            || route.m_runtimePermissionGranted)
        {
            if (error)
            {
                *error = "Asset Browser viewport route cannot grant mutation, binding, catalog, or runtime authority.";
            }
            return false;
        }

        auto iterator = AZStd::find_if(
            m_routes.begin(),
            m_routes.end(),
            [&route](const AssetBrowserPreviewViewportRoute& existing)
            {
                return existing.m_routeId == route.m_routeId;
            });
        if (iterator != m_routes.end())
        {
            *iterator = route;
            return true;
        }

        m_routes.push_back(route);
        return true;
    }

    void AssetBrowserPreviewRouteRegistry::Clear()
    {
        m_routes.clear();
    }

    const AssetBrowserPreviewViewportRoute* AssetBrowserPreviewRouteRegistry::FindByRouteId(
        const AZStd::string& routeId) const
    {
        const auto iterator = AZStd::find_if(
            m_routes.begin(),
            m_routes.end(),
            [&routeId](const AssetBrowserPreviewViewportRoute& route)
            {
                return route.m_routeId == routeId;
            });
        return iterator == m_routes.end() ? nullptr : &*iterator;
    }

    const AssetBrowserPreviewViewportRoute* AssetBrowserPreviewRouteRegistry::GetLatestRoute() const
    {
        return m_routes.empty() ? nullptr : &m_routes.back();
    }

    const AZStd::vector<AssetBrowserPreviewViewportRoute>&
    AssetBrowserPreviewRouteRegistry::GetRoutes() const
    {
        return m_routes;
    }
} // namespace TaintedGrailModdingSDK
