/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "NativeItemPreviewService.h"

#include <AzCore/Utils/Utils.h>
#include <AzToolsFramework/API/PythonLoader.h>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <utility>

namespace TaintedGrailModdingSDK
{
    NativeItemPreviewService::NativeItemPreviewService(QObject* parent)
        : QObject(parent)
    {
        m_timeout.setSingleShot(true);
        connect(&m_timeout, &QTimer::timeout, this, [this]()
        {
            m_error = tr("Item preview extraction timed out after 180 seconds. Previous previews are preserved.");
            m_process.kill();
        });
        connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() { ReadOutput(); });
        connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart)
            {
                Complete(tr("The packaged item preview reader could not start: %1").arg(m_process.errorString()));
            }
        });
        connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status)
        {
            ReadOutput();
            if (!m_error.isEmpty())
            {
                Complete(m_error);
            }
            else if (code != 0 || status != QProcess::NormalExit || !QFileInfo(m_manifest).isFile())
            {
                Complete(tr("The item preview reader failed without a completed result (exit %1).").arg(code));
            }
            else
            {
                Complete({});
            }
        });
    }

    NativeItemPreviewService::~NativeItemPreviewService()
    {
        m_completion = {};
        m_progress = {};
        m_process.disconnect(this);
        if (IsRunning())
        {
            m_process.kill();
            m_process.waitForFinished(1000);
        }
    }

    bool NativeItemPreviewService::IsRunning() const
    {
        return m_process.state() != QProcess::NotRunning;
    }

    void NativeItemPreviewService::Start(const QString& workspacePath, Progress progress, Completion completion, bool economy)
    {
        StartMode(workspacePath, std::move(progress), std::move(completion), economy ? QStringLiteral("--economy") : QString());
    }

    void NativeItemPreviewService::StartPopulation(const QString& workspacePath, Progress progress, Completion completion)
    {
        StartMode(workspacePath, std::move(progress), std::move(completion), QStringLiteral("--population"));
    }

    void NativeItemPreviewService::StartMode(const QString& workspacePath, Progress progress, Completion completion, const QString& mode)
    {
        if (IsRunning())
        {
            return;
        }
        m_progress = std::move(progress);
        m_completion = std::move(completion);
        m_manifest.clear();
        m_error.clear();
        m_pendingOutput.clear();
        m_outputBytes = 0;

        const auto engine = AZ::Utils::GetEnginePath();
        const QString enginePath = QString::fromUtf8(engine.c_str());
        QString script = QDir(enginePath).filePath(QStringLiteral("scripts/foa-sdk/foa_native_item_preview.py"));
        QString vendor = QDir(enginePath).filePath(QStringLiteral("scripts/foa-sdk/item-preview-vendor"));
        if (!QFileInfo(script).isFile())
        {
            script = QString::fromUtf8(TG_SDK_NATIVE_ITEM_PREVIEW_SOURCE);
            vendor = QString::fromUtf8(TG_SDK_NATIVE_ITEM_PREVIEW_VENDOR);
        }
        const auto pythonHome = AzToolsFramework::EmbeddedPython::PythonLoader::GetPythonExecutablePath(engine.c_str());
        const QString python = QDir(QString::fromUtf8(pythonHome.c_str())).filePath(QStringLiteral("python.exe"));
        if (!QFileInfo(script).isFile() || !QFileInfo(python).isFile()
            || !QFileInfo(QDir(vendor).filePath(QStringLiteral("UnityPy/__init__.py"))).isFile())
        {
            Complete(tr("The SDK item preview reader is missing. Rebuild or repair this SDK installation."));
            return;
        }
        if (!QFileInfo(workspacePath).isFile())
        {
            Complete(tr("Choose your game folder in System Details before refreshing item previews."));
            return;
        }

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("PYTHONHOME"));
        environment.remove(QStringLiteral("PYTHONPATH"));
        environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        environment.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
        m_process.setProcessEnvironment(environment);
        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.setWorkingDirectory(QFileInfo(workspacePath).absolutePath());
        m_process.setProgram(python);
        QStringList arguments{ QStringLiteral("-B"), QStringLiteral("-s"), script,
            QStringLiteral("--workspace"), workspacePath, QStringLiteral("--vendor"), vendor,
            QStringLiteral("--forbid-root"), enginePath };
        if (!mode.isEmpty())
        {
            arguments.append(mode);
        }
        m_process.setArguments(arguments);
        m_timeout.start(180000);
        m_process.start();
    }

    void NativeItemPreviewService::Cancel()
    {
        if (IsRunning())
        {
            m_error = tr("Item preview refresh cancelled. Previous previews are preserved.");
            m_process.kill();
        }
    }

    void NativeItemPreviewService::ReadOutput()
    {
        const QByteArray output = m_process.readAllStandardOutput();
        m_outputBytes += output.size();
        if (m_outputBytes > 1024 * 1024)
        {
            m_error = tr("The item preview reader exceeded its diagnostic output limit.");
            m_process.kill();
            return;
        }
        m_pendingOutput += output;
        qsizetype newline = -1;
        while ((newline = m_pendingOutput.indexOf('\n')) >= 0)
        {
            const QByteArray line = m_pendingOutput.left(newline);
            m_pendingOutput.remove(0, newline + 1);
            const QJsonObject object = QJsonDocument::fromJson(line).object();
            if (object.contains(QStringLiteral("error")))
            {
                m_error = object.value(QStringLiteral("error")).toString().left(500);
            }
            if (object.contains(QStringLiteral("manifest")))
            {
                m_manifest = object.value(QStringLiteral("manifest")).toString();
            }
            if (m_progress && object.contains(QStringLiteral("progress")))
            {
                m_progress(object.value(QStringLiteral("progress")).toString().left(500));
            }
        }
    }

    void NativeItemPreviewService::Complete(const QString& error)
    {
        m_timeout.stop();
        auto completion = std::move(m_completion);
        m_completion = {};
        m_progress = {};
        if (completion)
        {
            completion(error.isEmpty() ? m_manifest : QString(), error);
        }
    }
} // namespace TaintedGrailModdingSDK
