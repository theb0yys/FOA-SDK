/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>

namespace TaintedGrailModdingSDK
{
    //! Supervises the read-only native icon provider. No Unity parsing occurs on the UI thread.
    class NativeItemPreviewService final : public QObject
    {
    public:
        using Progress = std::function<void(const QString&)>;
        using Completion = std::function<void(const QString& manifest, const QString& error)>;

        explicit NativeItemPreviewService(QObject* parent = nullptr);
        ~NativeItemPreviewService() override;
        void Start(const QString& workspacePath, Progress progress, Completion completion);
        void Cancel();
        bool IsRunning() const;

    private:
        void ReadOutput();
        void Complete(const QString& error);

        QProcess m_process;
        QTimer m_timeout;
        QByteArray m_pendingOutput;
        QString m_manifest;
        QString m_error;
        Progress m_progress;
        Completion m_completion;
        qint64 m_outputBytes = 0;
    };
} // namespace TaintedGrailModdingSDK
