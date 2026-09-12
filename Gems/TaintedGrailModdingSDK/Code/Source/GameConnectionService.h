/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <functional>

class QJsonObject;

namespace TaintedGrailModdingSDK
{
    struct GameConnectionSnapshot
    {
        enum class State { Disconnected, Checking, Connected, Stale, Error };
        struct Service { QString m_id; QString m_version; QString m_owner; };
        struct PlayerPosition
        {
            enum class State { Empty, Checking, Available, Unavailable, Stale };
            State m_state = State::Empty;
            QString m_scene;
            QString m_message;
            double m_x = 0;
            double m_y = 0;
            double m_z = 0;
            QDateTime m_checkedAt;
        };
        struct PlayerVitals
        {
            enum class State { Empty, Checking, Available, Unavailable, Stale };
            struct Metric { double m_current = 0; double m_maximum = 0; };
            State m_state = State::Empty;
            Metric m_health;
            Metric m_stamina;
            Metric m_mana;
            QString m_message;
            QDateTime m_checkedAt;
        };
        struct EncounterPreview
        {
            enum class State { Empty, Checking, Available, Expired, InputError };
            State m_state = State::Empty;
            QString m_name;
            QString m_compositionSha256;
            QStringList m_templates;
            QString m_placement;
            QDateTime m_checkedAt;
        };
        State m_state = State::Disconnected;
        QString m_message;
        QString m_hostVersion;
        QString m_session;
        QDateTime m_checkedAt;
        QVector<Service> m_services;
        PlayerPosition m_player;
        PlayerVitals m_vitals;
        EncounterPreview m_encounter;
    };

    //! Owns transient identity, player-position and native-vitals observations.
    class GameConnectionService final : public QObject
    {
    public:
        GameConnectionService(QString python, QString worker, QObject* parent = nullptr);
        ~GameConnectionService() override;
        void Connect(int port, const QString& version, const QString& key);
        void Refresh();
        void GetPlayerPosition();
        bool CanGetPlayerPosition() const;
        void GetPlayerVitals();
        bool CanGetPlayerVitals() const { return CanGetPlayerPosition(); }
        void PreviewEncounter(const QString& compositionPath);
        bool CanPreviewEncounter() const;
        void ClearEncounterPreview();
        bool IsBusy() const { return m_process != nullptr; }
        void Disconnect();
        const GameConnectionSnapshot& GetSnapshot() const { return m_snapshot; }
        std::function<void()> m_changed;

    private:
        enum class Operation { Connection, Position, Vitals, EncounterPreview };
        void Start(Operation operation = Operation::Connection);
        void FinishPlayer(const QJsonObject& root);
        void FinishVitals(const QJsonObject& root);
        void FinishEncounterPreview(const QJsonObject& root);
        void StopWorker();
        void ReadOutput();
        void Finish(int exitCode, QProcess::ExitStatus exitStatus);
        void Fail(const QString& message);
        void Notify();
        QString m_python;
        QString m_worker;
        QString m_key;
        QString m_version;
        QString m_session;
        QString m_compositionPath;
        int m_port = 0;
        Operation m_operation = Operation::Connection;
        QProcess* m_process = nullptr;
        QTimer m_timeout;
        QTimer m_freshness;
        QTimer m_positionFreshness;
        QTimer m_vitalsFreshness;
        QTimer m_previewExpiry;
        QElapsedTimer m_previewElapsed;
        QByteArray m_output;
        qint64 m_outputBytes = 0;
        GameConnectionSnapshot m_snapshot;
    };
}
