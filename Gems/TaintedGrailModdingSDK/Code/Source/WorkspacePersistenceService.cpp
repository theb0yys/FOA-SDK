/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorkspacePersistenceService.h"

#include "PersistenceJsonUtils.h"
#include "WorkspaceSchemaService.h"

#include <AzCore/std/utility/move.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <cmath>
#include <limits>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZ::Outcome<AZStd::string, AZStd::string> ReadString(
            const QJsonObject& object,
            const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined() || value.isNull())
            {
                return AZ::Success(AZStd::string{});
            }
            if (!value.isString())
            {
                return AZ::Failure(AZStd::string("Workspace field must be a string: ") + key);
            }
            return AZ::Success(ToAzString(value.toString()));
        }

        AZ::Outcome<void, AZStd::string> AssignString(
            const QJsonObject& object,
            const char* key,
            AZStd::string& target)
        {
            auto result = ReadString(object, key);
            if (!result.IsSuccess())
            {
                return AZ::Failure(AZStd::string(result.GetError()));
            }
            target = result.TakeValue();
            return AZ::Success();
        }

        AZ::Outcome<AZStd::vector<AZStd::string>, AZStd::string> ReadStringArray(
            const QJsonObject& object,
            const char* key)
        {
            const QJsonValue value = object.value(QString::fromUtf8(key));
            if (value.isUndefined() || value.isNull())
            {
                return AZ::Success(AZStd::vector<AZStd::string>{});
            }
            if (!value.isArray())
            {
                return AZ::Failure(AZStd::string("Workspace field must be a string array: ") + key);
            }

            AZStd::vector<AZStd::string> result;
            const QJsonArray array = value.toArray();
            result.reserve(static_cast<size_t>(array.size()));
            for (const QJsonValue& entry : array)
            {
                if (!entry.isString())
                {
                    return AZ::Failure(AZStd::string("Workspace array contains a non-string value: ") + key);
                }
                result.push_back(ToAzString(entry.toString()));
            }
            return AZ::Success(AZStd::move(result));
        }

        AZ::Outcome<GameProfile, AZStd::string> ReadProfile(const QJsonValue& value)
        {
            if (!value.isObject())
            {
                return AZ::Failure(AZStd::string("GameProfiles entries must be JSON objects."));
            }
            const QJsonObject object = value.toObject();
            GameProfile profile;

#define TG_ASSIGN_PROFILE_FIELD(jsonName, member) \
            if (auto result = AssignString(object, jsonName, profile.member); !result.IsSuccess()) \
            { \
                return AZ::Failure(AZStd::string(result.GetError())); \
            }
            TG_ASSIGN_PROFILE_FIELD("ProfileId", m_profileId)
            TG_ASSIGN_PROFILE_FIELD("DisplayName", m_displayName)
            TG_ASSIGN_PROFILE_FIELD("InstallPath", m_installPath)
            TG_ASSIGN_PROFILE_FIELD("GameVersion", m_gameVersion)
            TG_ASSIGN_PROFILE_FIELD("Branch", m_branch)
            TG_ASSIGN_PROFILE_FIELD("RuntimeTarget", m_runtimeTarget)
            TG_ASSIGN_PROFILE_FIELD("UnityVersion", m_unityVersion)
            TG_ASSIGN_PROFILE_FIELD("BepInExVersion", m_bepInExVersion)
            TG_ASSIGN_PROFILE_FIELD("ManagedAssembliesPath", m_managedAssembliesPath)
            TG_ASSIGN_PROFILE_FIELD("PluginPath", m_pluginPath)
            TG_ASSIGN_PROFILE_FIELD("DiagnosticsPath", m_diagnosticsPath)
            TG_ASSIGN_PROFILE_FIELD("ExtractedDataPath", m_extractedDataPath)
#undef TG_ASSIGN_PROFILE_FIELD

            auto scopes = ReadStringArray(object, "DlcScopes");
            if (!scopes.IsSuccess())
            {
                return AZ::Failure(AZStd::string(scopes.GetError()));
            }
            profile.m_dlcScopes = scopes.TakeValue();
            return AZ::Success(AZStd::move(profile));
        }

        AZ::Outcome<WorkspaceModel, AZStd::string> ReadPlainWorkspace(const QJsonObject& object)
        {
            WorkspaceModel workspace;

#define TG_ASSIGN_WORKSPACE_FIELD(jsonName, member) \
            if (auto result = AssignString(object, jsonName, workspace.member); !result.IsSuccess()) \
            { \
                return AZ::Failure(AZStd::string(result.GetError())); \
            }
            TG_ASSIGN_WORKSPACE_FIELD("WorkspaceId", m_workspaceId)
            TG_ASSIGN_WORKSPACE_FIELD("DisplayName", m_displayName)
            TG_ASSIGN_WORKSPACE_FIELD("RootPath", m_rootPath)
            TG_ASSIGN_WORKSPACE_FIELD("OutputPath", m_outputPath)
            TG_ASSIGN_WORKSPACE_FIELD("StagingPath", m_stagingPath)
            TG_ASSIGN_WORKSPACE_FIELD("DeploymentPath", m_deploymentPath)
            TG_ASSIGN_WORKSPACE_FIELD("ActiveGameProfileId", m_activeGameProfileId)
#undef TG_ASSIGN_WORKSPACE_FIELD

            const QJsonValue profilesValue = object.value(QStringLiteral("GameProfiles"));
            if (!profilesValue.isArray())
            {
                return AZ::Failure(AZStd::string("GameProfiles must be a JSON array."));
            }
            const QJsonArray profiles = profilesValue.toArray();
            workspace.m_gameProfiles.reserve(static_cast<size_t>(profiles.size()));
            for (const QJsonValue& profileValue : profiles)
            {
                auto profile = ReadProfile(profileValue);
                if (!profile.IsSuccess())
                {
                    return AZ::Failure(AZStd::string(profile.GetError()));
                }
                workspace.m_gameProfiles.push_back(profile.TakeValue());
            }
            return AZ::Success(AZStd::move(workspace));
        }

        AZ::Outcome<AZ::u32, AZStd::string> DetectSchemaVersion(const QJsonObject& object)
        {
            const QJsonValue value = object.value(QStringLiteral("SchemaVersion"));
            if (value.isUndefined() || value.isNull())
            {
                return AZ::Success(WorkspaceSchemaService::LegacySchemaVersion);
            }
            if (!value.isDouble())
            {
                return AZ::Failure(AZStd::string("SchemaVersion must be an unsigned integer."));
            }
            const double number = value.toDouble();
            if (number < 0.0
                || number > static_cast<double>(std::numeric_limits<AZ::u32>::max())
                || std::floor(number) != number)
            {
                return AZ::Failure(AZStd::string("SchemaVersion must be an unsigned integer."));
            }

            const AZ::u32 schemaVersion = static_cast<AZ::u32>(number);
            if (schemaVersion != WorkspaceSchemaService::LegacySchemaVersion
                && schemaVersion != WorkspaceSchemaService::CurrentSchemaVersion)
            {
                return AZ::Failure(AZStd::string::format(
                    "Workspace schema version %u is unsupported; this editor supports schema 0 migration and schema 1.",
                    schemaVersion));
            }
            return AZ::Success(schemaVersion);
        }

        QJsonArray WriteStrings(const AZStd::vector<AZStd::string>& values)
        {
            QJsonArray array;
            for (const AZStd::string& value : values)
            {
                array.push_back(ToQString(value));
            }
            return array;
        }

        QJsonObject WriteProfile(const GameProfile& profile)
        {
            QJsonObject object;
            object.insert(QStringLiteral("ProfileId"), ToQString(profile.m_profileId));
            object.insert(QStringLiteral("DisplayName"), ToQString(profile.m_displayName));
            object.insert(QStringLiteral("InstallPath"), ToQString(profile.m_installPath));
            object.insert(QStringLiteral("GameVersion"), ToQString(profile.m_gameVersion));
            object.insert(QStringLiteral("Branch"), ToQString(profile.m_branch));
            object.insert(QStringLiteral("RuntimeTarget"), ToQString(profile.m_runtimeTarget));
            object.insert(QStringLiteral("UnityVersion"), ToQString(profile.m_unityVersion));
            object.insert(QStringLiteral("BepInExVersion"), ToQString(profile.m_bepInExVersion));
            object.insert(QStringLiteral("ManagedAssembliesPath"), ToQString(profile.m_managedAssembliesPath));
            object.insert(QStringLiteral("PluginPath"), ToQString(profile.m_pluginPath));
            object.insert(QStringLiteral("DiagnosticsPath"), ToQString(profile.m_diagnosticsPath));
            object.insert(QStringLiteral("ExtractedDataPath"), ToQString(profile.m_extractedDataPath));
            object.insert(QStringLiteral("DlcScopes"), WriteStrings(profile.m_dlcScopes));
            return object;
        }

        QJsonObject WriteWorkspace(const WorkspaceModel& workspace)
        {
            QJsonObject object;
            object.insert(
                QStringLiteral("SchemaVersion"),
                static_cast<int>(WorkspaceSchemaService::CurrentSchemaVersion));
            object.insert(QStringLiteral("WorkspaceId"), ToQString(workspace.m_workspaceId));
            object.insert(QStringLiteral("DisplayName"), ToQString(workspace.m_displayName));
            object.insert(QStringLiteral("RootPath"), ToQString(workspace.m_rootPath));
            object.insert(QStringLiteral("OutputPath"), ToQString(workspace.m_outputPath));
            object.insert(QStringLiteral("StagingPath"), ToQString(workspace.m_stagingPath));
            object.insert(QStringLiteral("DeploymentPath"), ToQString(workspace.m_deploymentPath));
            object.insert(
                QStringLiteral("ActiveGameProfileId"),
                ToQString(workspace.m_activeGameProfileId));
            QJsonArray profiles;
            for (const GameProfile& profile : workspace.m_gameProfiles)
            {
                profiles.push_back(WriteProfile(profile));
            }
            object.insert(QStringLiteral("GameProfiles"), profiles);
            return object;
        }
    } // namespace

    AZ::Outcome<void, AZStd::string> WorkspacePersistenceService::Save(
        const WorkspaceModel& workspace,
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Workspace file path is required."));
        }

        WorkspaceSchemaService schema;
        const auto validation = schema.Validate(workspace);
        if (!validation.IsSuccess())
        {
            return AZ::Failure(AZStd::string(validation.GetError()));
        }

        QSaveFile file(ToQString(filePath));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return AZ::Failure(
                AZStd::string("Unable to open workspace file for writing: ") + filePath);
        }
        const QByteArray json = QJsonDocument(WriteWorkspace(workspace)).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            return AZ::Failure(AZStd::string("Unable to commit workspace file: ") + filePath);
        }
        return AZ::Success();
    }

    AZ::Outcome<WorkspaceModel, AZStd::string> WorkspacePersistenceService::Load(
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Workspace file path is required."));
        }

        QFile file(ToQString(filePath));
        if (!file.open(QIODevice::ReadOnly))
        {
            return AZ::Failure(AZStd::string("Unable to open workspace file: ") + filePath);
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            return AZ::Failure(
                AZStd::string("Workspace is not valid UTF-8 JSON: ")
                + filePath + ": " + ToAzString(parseError.errorString()));
        }

        const QJsonObject object = document.object();
        auto detected = DetectSchemaVersion(object);
        if (!detected.IsSuccess())
        {
            return AZ::Failure(AZStd::string(detected.GetError()));
        }
        const AZ::u32 schemaVersion = detected.GetValue();

        WorkspaceModel workspace;
        if (object.contains(QStringLiteral("Type")))
        {
            if (schemaVersion != WorkspaceSchemaService::LegacySchemaVersion)
            {
                return AZ::Failure(AZStd::string(
                    "Workspace schema 1 must use the plain durable JSON document, not a legacy O3DE serialization envelope."));
            }

            const auto legacyLoad = PersistenceJsonUtils::LoadObjectFromFile(workspace, filePath);
            if (!legacyLoad.IsSuccess())
            {
                return AZ::Failure(
                    AZStd::string("Legacy workspace schema 0 cannot be migrated safely: ")
                    + legacyLoad.GetError());
            }
        }
        else
        {
            auto plain = ReadPlainWorkspace(object);
            if (!plain.IsSuccess())
            {
                const char* prefix = schemaVersion == WorkspaceSchemaService::LegacySchemaVersion
                    ? "Legacy workspace schema 0 cannot be migrated safely: "
                    : "Workspace schema 1 parsing failed: ";
                return AZ::Failure(AZStd::string(prefix) + plain.GetError());
            }
            workspace = plain.TakeValue();
        }

        WorkspaceSchemaService schema;
        return schema.MigrateAndValidate(AZStd::move(workspace), schemaVersion);
    }
} // namespace TaintedGrailModdingSDK
