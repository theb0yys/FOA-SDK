/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "TerrainCampaignExportHost.h"
#include <ExternalToolchain/ExternalToolchainBus.h>
#include <AzCore/IO/FileIO.h>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#if AZ_TRAIT_OS_PLATFORM_WINDOWS
#include <AzCore/PlatformIncl.h>
#endif

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* CampaignKeys[] = {"hos", "cuanacht", "forlorn", "sarras"};
        constexpr const char* CampaignNames[] = {"Horns of the South", "Cuanacht", "Forlorn Swords", "Sarras"};
        QString Qt(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        QJsonObject Failure(const QString& message) { return {{"status", "failed"}, {"message", message}}; }
        bool Write(const QString& path, const QByteArray& bytes)
        {
            QFile file(path);
            return file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(bytes) == bytes.size();
        }
        bool DirectFile(const QString& path)
        {
            const QFileInfo info(path);
            return info.isFile() && info.canonicalFilePath() == info.absoluteFilePath();
        }
        bool HashMatches(const QString& path, const QString& expected)
        {
            QFile file(path);
            if (!DirectFile(path) || !file.open(QIODevice::ReadOnly)) { return false; }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            return hash.addData(&file) && "sha256:" + QString::fromLatin1(hash.result().toHex()) == expected;
        }
    }
    TerrainCampaignProvider ResolveTerrainCampaignProvider()
    {
        TerrainCampaignProvider provider;
        ExternalToolchain::ExternalToolDiscoveryResult discovery;
        bool found = false;
        ExternalToolchain::ExternalToolchainRequestBus::BroadcastResult(found,
            &ExternalToolchain::ExternalToolchainRequests::GetDiscoveryResult,
            AZStd::string("foa.terrain-extraction"), discovery);
        if (!found || discovery.m_status != ExternalToolchain::DiscoveryStatus::Installed
            || discovery.m_selectedVersion != "1.24.2") { return provider; }
        provider.m_python = Qt(discovery.m_selectedPath);
        if (auto* io = AZ::IO::FileIOBase::GetDirectInstance())
        {
            char resolved[AZ_MAX_PATH_LEN] = {};
            if (io->ResolvePath("@engroot@/scripts/foa-sdk/foa_campaign_heightmap_export.py", resolved, AZ_MAX_PATH_LEN)
                && DirectFile(QString::fromUtf8(resolved))) { provider.m_script = QString::fromUtf8(resolved); }
        }
#ifdef TG_SDK_CAMPAIGN_EXPORT_TOOL_SOURCE
        if (provider.m_script.isEmpty() && DirectFile(QString::fromUtf8(TG_SDK_CAMPAIGN_EXPORT_TOOL_SOURCE)))
        {
            provider.m_script = QString::fromUtf8(TG_SDK_CAMPAIGN_EXPORT_TOOL_SOURCE);
        }
#endif
        if (!DirectFile(provider.m_python) || provider.m_script.isEmpty()) { return {}; }
        return provider;
    }

    QJsonArray AvailableTerrainCampaigns(const TerrainCampaignProvider& provider, const QString& gameRoot, const QString& unityVersion)
    {
        QJsonArray rows;
        if (provider.m_python.isEmpty() || provider.m_script.isEmpty() || unityVersion != "6000.0.64f1") { return rows; }
        const QDir root(gameRoot);
        if (!DirectFile(root.filePath("Fall of Avalon_Data/StreamingAssets/aa/catalog.json"))) { return rows; }
        for (int index = 0; index < 4; ++index)
        {
            const QString stem = "Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/scenes_scenes_campaignmap_" + QString::fromUtf8(CampaignKeys[index]);
            if (DirectFile(root.filePath(stem + ".bundle")) && DirectFile(root.filePath(stem + "_static.bundle")))
            {
                rows.append(QJsonObject{{"key", CampaignKeys[index]}, {"name", CampaignNames[index]}});
            }
        }
        return rows;
    }

    QJsonObject ExportTerrainCampaign(const TerrainCampaignProvider& provider, const QString& workspace,
        const QString& gameRoot, const QString& unityVersion, const TerrainHeightmap::ProfileBinding& profile,
        const QString& campaign, const QString& operation, const QString& createdAt,
        const TerrainHeightmap::ImportControl* control, int timeoutMs)
    {
        QString displayName;
        for (const auto& row : AvailableTerrainCampaigns(provider, gameRoot, unityVersion))
        {
            if (row.toObject().value("key").toString() == campaign) { displayName = row.toObject().value("name").toString(); }
        }
        if (displayName.isEmpty()) { return Failure("This campaign or its extraction tool is unavailable. Check External Tools diagnostics."); }
        if (!QFileInfo(workspace).isDir() || QFileInfo(workspace).canonicalFilePath() != QFileInfo(workspace).absoluteFilePath()
            || !operation.startsWith("terrain-import.") || operation.contains('/') || operation.contains('\\') || operation.contains(".."))
        { return Failure("Campaign export requires a direct saved workspace and valid operation."); }
        QString staging = workspace;
        for (const auto& component : {QString("Staging"), QString("TerrainCampaign")})
        {
            const QString next = QDir(staging).filePath(component);
            const QFileInfo info(next);
            if ((info.exists() && (!info.isDir() || info.canonicalFilePath() != info.absoluteFilePath()))
                || (!info.exists() && !QDir(staging).mkdir(component))) { return Failure("Campaign staging is not a direct workspace directory."); }
            staging = next;
        }
        QTemporaryDir temporary(QDir(staging).filePath("operation-XXXXXX"));
        if (!temporary.isValid()) { return Failure("Unable to create the campaign operation directory."); }
        const QJsonObject binding{{"profile_id", Qt(profile.m_profileId)}, {"game_version", Qt(profile.m_gameVersion)},
            {"branch", Qt(profile.m_branch)}, {"runtime_target", Qt(profile.m_runtimeTarget)}, {"profile_fingerprint", Qt(profile.m_profileFingerprint)}};
        const QJsonObject request{{"schema", "foa.campaign-heightmap-export.request"}, {"schema_version", 1},
            {"workspace_root", workspace}, {"game_root", gameRoot}, {"map", campaign}, {"resolution", 2049},
            {"operation_id", operation}, {"created_at_utc", createdAt}, {"unity_version", unityVersion}, {"profile_binding", binding}};
        const QString requestPath = temporary.filePath("request.json");
        if (!Write(requestPath, QJsonDocument(request).toJson(QJsonDocument::Compact))) { return Failure("Unable to save the campaign work order."); }
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove("PYTHONPATH");
        environment.remove("PYTHONHOME");
        process.setProcessEnvironment(environment);
        process.setWorkingDirectory(temporary.path());
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(provider.m_python, {"-I", "-B", provider.m_script, "--request", requestPath}, QIODevice::ReadOnly);
        if (!process.waitForStarted(5000)) { return Failure("The campaign extraction tool could not start."); }
#if AZ_TRAIT_OS_PLATFORM_WINDOWS
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        limits.ProcessMemoryLimit = static_cast<SIZE_T>(1536) * 1024 * 1024;
        HANDLE worker = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(process.processId()));
        const bool constrained = job && worker && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))
            && AssignProcessToJobObject(job, worker);
        if (worker) { CloseHandle(worker); }
        if (!constrained)
        {
            process.kill(); process.waitForFinished(5000);
            if (job) { CloseHandle(job); }
            return Failure("Unable to apply the campaign worker memory limit.");
        }
#endif
        QElapsedTimer elapsed; elapsed.start();
        QByteArray log;
        QString failure;
        while (process.state() != QProcess::NotRunning)
        {
            process.waitForFinished(50);
            log += process.readAll();
            if (control && control->IsCancelled())
            {
                Write(temporary.filePath("cancel.flag"), QByteArray("cancel"));
                if (!process.waitForFinished(2000)) { process.kill(); process.waitForFinished(5000); }
                failure = "Terrain import cancelled.";
                break;
            }
            if (elapsed.elapsed() > timeoutMs || log.size() > 1024 * 1024)
            {
                process.kill(); process.waitForFinished(5000);
                failure = "Campaign extraction exceeded its time or output limit.";
                break;
            }
        }
        log += process.readAll();
#if AZ_TRAIT_OS_PLATFORM_WINDOWS
        CloseHandle(job);
#endif
        // Keep a bounded diagnostic log in the workspace even when the operation fails.
        const QString logPath = QDir(staging).filePath(operation + ".log");
        Write(logPath, log.left(1024 * 1024));
        if (!failure.isEmpty()) { return Failure(failure); }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        { return Failure("Campaign extraction failed. Its workspace diagnostic log contains details."); }
        QFile resultFile(temporary.filePath("result.json"));
        if (!DirectFile(resultFile.fileName()) || !resultFile.open(QIODevice::ReadOnly) || resultFile.size() > 16384)
        { return Failure("Campaign extraction did not return a bounded result."); }
        const auto result = QJsonDocument::fromJson(resultFile.read(16385)).object();
        const QString relative = "SourceExports/Terrain/" + operation;
        if (result.value("schema").toString() != "foa.campaign-heightmap-export.result" || result.value("schema_version").toInt() != 1
            || result.value("status").toString() != "PASSED" || result.value("operation_id").toString() != operation
            || result.value("map").toString() != campaign || result.value("export_relative_path").toString() != relative
            || !result.value("coverage_fraction").isDouble() || result.value("coverage_fraction").toDouble() < .25
            || result.value("coverage_fraction").toDouble() > 1.0)
        { return Failure("Campaign extraction returned an invalid or mismatched result."); }
        const QString root = QDir(workspace).filePath(relative);
        const QString raw = QDir(root).filePath("heightmap.raw");
        const QString metadata = raw + ".json";
        if (QFileInfo(raw).size() != 2049LL * 2049 * 2 || QFileInfo(metadata).size() > 1024 * 1024
            || !HashMatches(raw, result.value("raw_sha256").toString()) || !HashMatches(metadata, result.value("sidecar_sha256").toString()))
        { return Failure("Campaign export payload verification failed."); }
        return {{"status", "complete"}, {"source", raw}, {"name", displayName + " Terrain"},
            {"coverage", result.value("coverage_fraction")}};
    }
}
