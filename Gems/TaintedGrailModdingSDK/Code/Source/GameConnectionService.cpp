/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "GameConnectionService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <utility>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        double ReadNativeNumber(const QJsonObject& object, const QString& field, bool& valid)
        {
            const auto value = object.value(field);
            const double number = value.toDouble();
            valid = valid && value.isDouble() && std::isfinite(number) && std::abs(number) <= 3.4028235e38
                && (number == 0 || static_cast<float>(number) != 0);
            return number;
        }
    }

    GameConnectionService::GameConnectionService(QString python, QString worker, QObject* parent)
        : QObject(parent), m_python(std::move(python)), m_worker(std::move(worker))
    {
        m_timeout.setSingleShot(true);
        connect(&m_timeout, &QTimer::timeout, this, [this]()
        {
            Fail(tr("The connection check timed out. Check the game and choose Connect again."));
        });
        m_freshness.setSingleShot(true);
        connect(&m_freshness, &QTimer::timeout, this, [this]()
        {
            m_snapshot.m_state = GameConnectionSnapshot::State::Stale;
            m_snapshot.m_message = tr("Connection needs a fresh check. Choose Refresh to check the current game session.");
            Notify();
        });
        m_positionFreshness.setSingleShot(true);
        m_positionFreshness.setTimerType(Qt::PreciseTimer);
        connect(&m_positionFreshness, &QTimer::timeout, this, [this]()
        {
            if (m_snapshot.m_player.m_state == GameConnectionSnapshot::PlayerPosition::State::Available)
            {
                m_snapshot.m_player.m_state = GameConnectionSnapshot::PlayerPosition::State::Stale;
            }
            Notify();
        });
        m_vitalsFreshness.setSingleShot(true);
        m_vitalsFreshness.setTimerType(Qt::PreciseTimer);
        connect(&m_vitalsFreshness, &QTimer::timeout, this, [this]()
        {
            if (m_snapshot.m_vitals.m_state == GameConnectionSnapshot::PlayerVitals::State::Available)
            {
                m_snapshot.m_vitals.m_state = GameConnectionSnapshot::PlayerVitals::State::Stale;
            }
            Notify();
        });
        m_previewExpiry.setSingleShot(true);
        m_previewExpiry.setTimerType(Qt::PreciseTimer);
        connect(&m_previewExpiry, &QTimer::timeout, this, [this]()
        {
            m_snapshot.m_encounter.m_state = GameConnectionSnapshot::EncounterPreview::State::Expired;
            Notify();
        });
    }

    GameConnectionService::~GameConnectionService()
    {
        m_changed = {};
        Disconnect();
    }

    void GameConnectionService::Notify()
    {
        if (m_changed) { m_changed(); }
    }

    void GameConnectionService::Connect(int port, const QString& version, const QString& key)
    {
        Disconnect();
        m_port = port;
        m_version = version;
        m_key = key.isEmpty() ? qEnvironmentVariable("TGE_SDK_KEY") : key;
        static const QRegularExpression keyPattern(QStringLiteral("\\A[0-9a-fA-F]{64}\\z"));
        if (port < 1 || port > 65535 || version.trimmed().isEmpty() || version.size() > 256)
        {
            Fail(tr("Enter the port from the extender log and its exact version."));
            return;
        }
        if (!keyPattern.match(m_key).hasMatch())
        {
            Fail(tr("Enter the same 64-character hexadecimal key used by the game, or set TGE_SDK_KEY before starting the Editor."));
            return;
        }
        Start();
    }

    void GameConnectionService::Refresh()
    {
        if (!m_session.isEmpty() && !m_process) { Start(); }
    }

    bool GameConnectionService::CanGetPlayerPosition() const
    {
        if (m_session.isEmpty() || m_process) { return false; }
        for (const auto& service : m_snapshot.m_services)
        {
            if (service.m_id == QStringLiteral("tge.foa.player") && service.m_version == QStringLiteral("0.1")
                && service.m_owner == QStringLiteral("tge.foa.player")) { return true; }
        }
        return false;
    }

    void GameConnectionService::GetPlayerPosition()
    {
        if (CanGetPlayerPosition()) { Start(Operation::Position); }
    }

    void GameConnectionService::GetPlayerVitals()
    {
        if (CanGetPlayerVitals()) { Start(Operation::Vitals); }
    }

    bool GameConnectionService::CanPreviewEncounter() const
    {
        if (m_session.isEmpty() || m_process) { return false; }
        for (const auto& service : m_snapshot.m_services)
        {
            if (service.m_id == QStringLiteral("tge.foa.encounters") && service.m_version == QStringLiteral("0.1")
                && service.m_owner == QStringLiteral("tge.foa.encounters")) { return true; }
        }
        return false;
    }

    void GameConnectionService::ClearEncounterPreview()
    {
        if (m_process) { return; }
        m_previewExpiry.stop();
        m_snapshot.m_encounter = {};
        m_compositionPath.clear();
        Notify();
    }

    void GameConnectionService::PreviewEncounter(const QString& compositionPath)
    {
        if (!CanPreviewEncounter()) { return; }
        ClearEncounterPreview();
        if (compositionPath.trimmed().isEmpty() || compositionPath.size() > 4096)
        {
            m_snapshot.m_encounter.m_state = GameConnectionSnapshot::EncounterPreview::State::InputError;
            Notify();
            return;
        }
        m_compositionPath = compositionPath;
        Start(Operation::EncounterPreview);
    }

    void GameConnectionService::Disconnect()
    {
        StopWorker();
        m_freshness.stop();
        m_positionFreshness.stop();
        m_vitalsFreshness.stop();
        m_previewExpiry.stop();
        m_compositionPath.clear();
        m_key.clear();
        m_session.clear();
        m_snapshot = {};
        Notify();
    }

    void GameConnectionService::StopWorker()
    {
        m_timeout.stop();
        m_output.clear();
        if (m_process)
        {
            QProcess* process = m_process;
            m_process = nullptr;
            process->disconnect(this);
            process->setProcessEnvironment(QProcessEnvironment());
            // No wait on the Editor thread, including pane destruction. The
            // detached child reaps itself and cannot publish a late result.
            if (process->state() == QProcess::NotRunning) { process->deleteLater(); }
            else
            {
                connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater);
                connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error)
                {
                    if (error == QProcess::FailedToStart) { process->deleteLater(); }
                });
                process->kill();
            }
        }
    }

    void GameConnectionService::Fail(const QString& message)
    {
        Disconnect();
        m_snapshot.m_state = GameConnectionSnapshot::State::Error;
        m_snapshot.m_message = message;
        Notify();
    }

    void GameConnectionService::Start(Operation operation)
    {
        if (operation != Operation::EncounterPreview) { m_freshness.stop(); }
        if (!QFileInfo(m_python).isFile() || !QFileInfo(m_worker).isFile())
        {
            Fail(tr("The SDK connection worker is missing. Rebuild or repair the SDK installation."));
            return;
        }
        m_operation = operation;
        if (operation == Operation::Position)
        {
            m_positionFreshness.stop();
            m_snapshot.m_player = {};
            m_snapshot.m_player.m_state = GameConnectionSnapshot::PlayerPosition::State::Checking;
        }
        else if (operation == Operation::Vitals)
        {
            m_vitalsFreshness.stop();
            m_snapshot.m_vitals = {};
            m_snapshot.m_vitals.m_state = GameConnectionSnapshot::PlayerVitals::State::Checking;
        }
        else if (operation == Operation::EncounterPreview)
        {
            m_previewExpiry.stop();
            m_previewElapsed.start();
            m_snapshot.m_encounter = {};
            m_snapshot.m_encounter.m_state = GameConnectionSnapshot::EncounterPreview::State::Checking;
        }
        else
        {
            m_positionFreshness.stop();
            m_vitalsFreshness.stop();
            m_previewExpiry.stop();
            m_snapshot = {};
            m_snapshot.m_state = GameConnectionSnapshot::State::Checking;
        }
        m_output.clear();
        m_outputBytes = 0;
        m_process = new QProcess();
        QProcess* process = m_process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("PYTHONHOME"));
        environment.remove(QStringLiteral("PYTHONPATH"));
        environment.insert(QStringLiteral("TGE_SDK_KEY"), m_key);
        environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
        environment.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
        process->setProcessEnvironment(environment);
        process->setWorkingDirectory(QFileInfo(m_worker).absolutePath());
        process->setProgram(m_python);
        QStringList arguments{ QStringLiteral("-B"), QStringLiteral("-s"), m_worker,
            QStringLiteral("--port"), QString::number(m_port), QStringLiteral("--version"), m_version,
            QStringLiteral("--session"), m_session,
            QStringLiteral("--operation"), operation == Operation::Position ? QStringLiteral("position")
                : operation == Operation::Vitals ? QStringLiteral("vitals")
                : operation == Operation::EncounterPreview ? QStringLiteral("encounter-preview") : QStringLiteral("connection") };
        if (operation == Operation::EncounterPreview) { arguments << QStringLiteral("--composition") << m_compositionPath; }
        process->setArguments(arguments);
        connect(process, &QProcess::readyReadStandardOutput, this, &GameConnectionService::ReadOutput);
        connect(process, &QProcess::readyReadStandardError, this, &GameConnectionService::ReadOutput);
        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart) { Fail(tr("The SDK connection worker could not start. Repair the SDK installation.")); }
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &GameConnectionService::Finish);
        m_timeout.start(21000);
        process->start();
        Notify();
    }

    void GameConnectionService::ReadOutput()
    {
        if (!m_process) { return; }
        const auto output = m_process->readAllStandardOutput();
        const auto diagnostics = m_process->readAllStandardError();
        m_outputBytes += output.size() + diagnostics.size();
        // 256 service rows with escaped bounded identities fit below 2 MiB.
        if (m_outputBytes > 2 * 1024 * 1024)
        {
            Fail(tr("The connection worker exceeded its output limit."));
            return;
        }
        m_output += output;
    }

    void GameConnectionService::Finish(int exitCode, QProcess::ExitStatus exitStatus)
    {
        ReadOutput();
        if (!m_process) { return; }
        const QByteArray output = m_output;
        StopWorker();
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(output, &parseError);
        const auto root = document.object();
        if (exitStatus != QProcess::NormalExit || parseError.error != QJsonParseError::NoError
            || !document.isObject() || root.value(QStringLiteral("schema")).toInt() != 1)
        {
            Fail(tr("The connection worker returned an invalid result. Rebuild or repair the SDK installation."));
            return;
        }
        if (root.contains(QStringLiteral("error")))
        {
            Fail(root.value(QStringLiteral("error")).toString().left(500));
            return;
        }
        if (m_operation == Operation::EncounterPreview && exitCode == 0 && root.size() == 2
            && root.value(QStringLiteral("previewInputError")).isString())
        {
            m_snapshot.m_encounter = {};
            m_snapshot.m_encounter.m_state = GameConnectionSnapshot::EncounterPreview::State::InputError;
            Notify();
            return;
        }
        const QString session = root.value(QStringLiteral("sessionId")).toString();
        if (exitCode != 0 || root.value(QStringLiteral("hostId")).toString() != QStringLiteral("kane.tgfoa.tainted-grail-extender")
            || root.value(QStringLiteral("hostVersion")).toString() != m_version
            || session.isEmpty() || session.size() > 256 || (!m_session.isEmpty() && session != m_session)
            || (m_operation == Operation::Connection && !root.value(QStringLiteral("services")).isArray()))
        {
            Fail(tr("The extender identity or session changed. Check its version and reconnect."));
            return;
        }
        if (m_operation == Operation::Position)
        {
            FinishPlayer(root);
            return;
        }
        if (m_operation == Operation::Vitals)
        {
            FinishVitals(root);
            return;
        }
        if (m_operation == Operation::EncounterPreview)
        {
            FinishEncounterPreview(root);
            return;
        }
        const auto services = root.value(QStringLiteral("services")).toArray();
        if (services.isEmpty() || services.size() > 256)
        {
            Fail(tr("The extender returned an invalid service list."));
            return;
        }
        GameConnectionSnapshot candidate;
        QSet<QPair<QString, QString>> seen;
        for (const auto& value : services)
        {
            const auto row = value.toObject();
            GameConnectionSnapshot::Service service{ row.value(QStringLiteral("id")).toString(),
                row.value(QStringLiteral("version")).toString(), row.value(QStringLiteral("owner")).toString() };
            const auto pair = qMakePair(service.m_id, service.m_version);
            if (service.m_id.isEmpty() || service.m_version.isEmpty() || service.m_owner.isEmpty()
                || service.m_id.size() > 256 || service.m_version.size() > 256 || service.m_owner.size() > 256 || seen.contains(pair))
            {
                Fail(tr("The extender returned an invalid service identity."));
                return;
            }
            seen.insert(pair);
            candidate.m_services.push_back(service);
        }
        if (!seen.contains(qMakePair(QStringLiteral("tge.core.identity"), QStringLiteral("0.1"))))
        {
            Fail(tr("The extender did not report its identity service."));
            return;
        }
        m_session = session;
        candidate.m_session = session;
        candidate.m_hostVersion = m_version;
        candidate.m_state = GameConnectionSnapshot::State::Connected;
        candidate.m_checkedAt = QDateTime::currentDateTimeUtc();
        m_snapshot = std::move(candidate);
        m_freshness.start(30000);
        Notify();
    }

    void GameConnectionService::FinishPlayer(const QJsonObject& root)
    {
        using PlayerState = GameConnectionSnapshot::PlayerPosition::State;
        const auto player = root.value(QStringLiteral("player")).toObject();
        GameConnectionSnapshot::PlayerPosition candidate;
        bool valid = player.value(QStringLiteral("available")).isBool();
        if (valid && player.value(QStringLiteral("available")).toBool())
        {
            candidate.m_scene = player.value(QStringLiteral("sceneName")).toString();
            valid = player.size() == 5 && !candidate.m_scene.trimmed().isEmpty() && candidate.m_scene.size() <= 65536;
            candidate.m_x = ReadNativeNumber(player, QStringLiteral("x"), valid);
            candidate.m_y = ReadNativeNumber(player, QStringLiteral("y"), valid);
            candidate.m_z = ReadNativeNumber(player, QStringLiteral("z"), valid);
            candidate.m_state = PlayerState::Available;
        }
        else if (valid)
        {
            const auto code = player.value(QStringLiteral("code")).toString();
            valid = player.size() == 2;
            if (code == QStringLiteral("player_unavailable"))
            {
                candidate.m_message = tr("No player is loaded. Load a game, then choose Get Player Position.");
            }
            else if (code == QStringLiteral("scene_unavailable"))
            {
                candidate.m_message = tr("The active scene is unavailable. Wait for loading to finish, then try again.");
            }
            else if (code == QStringLiteral("position_unavailable"))
            {
                candidate.m_message = tr("The player position is unavailable. Try again when the player is ready.");
            }
            else { valid = false; }
            candidate.m_state = PlayerState::Unavailable;
        }
        if (!valid)
        {
            Fail(tr("The player-position worker returned invalid data. Reconnect and try again."));
            return;
        }
        candidate.m_checkedAt = QDateTime::currentDateTimeUtc();
        m_snapshot.m_checkedAt = candidate.m_checkedAt;
        m_snapshot.m_state = GameConnectionSnapshot::State::Connected;
        m_snapshot.m_message.clear();
        m_snapshot.m_player = std::move(candidate);
        m_positionFreshness.start(30000);
        m_freshness.start(30000);
        Notify();
    }

    void GameConnectionService::FinishEncounterPreview(const QJsonObject& root)
    {
        using PreviewState = GameConnectionSnapshot::EncounterPreview::State;
        const auto preview = root.value(QStringLiteral("encounter")).toObject();
        GameConnectionSnapshot::EncounterPreview candidate;
        candidate.m_name = preview.value(QStringLiteral("name")).toString();
        candidate.m_compositionSha256 = preview.value(QStringLiteral("compositionSha256")).toString();
        candidate.m_placement = preview.value(QStringLiteral("placement")).toString();
        const auto templates = preview.value(QStringLiteral("templates")).toArray();
        static const QRegularExpression digest(QStringLiteral("\\A[0-9a-f]{64}\\z"));
        static const QRegularExpression planId(QStringLiteral("\\A[0-9a-f]{32}\\z"));
        bool valid = preview.size() == 6 && !candidate.m_name.trimmed().isEmpty() && candidate.m_name.size() <= 256
            && digest.match(candidate.m_compositionSha256).hasMatch()
            && digest.match(preview.value(QStringLiteral("fingerprint")).toString()).hasMatch()
            && planId.match(preview.value(QStringLiteral("planId")).toString()).hasMatch()
            && !candidate.m_placement.trimmed().isEmpty() && candidate.m_placement.size() <= 4096
            && templates.size() >= 1 && templates.size() <= 8;
        for (const auto& entry : templates)
        {
            const auto name = entry.toString();
            valid = valid && (name == QStringLiteral("wyrdspirit") || name == QStringLiteral("outlaw-1h"));
            candidate.m_templates.push_back(name);
        }
        if (!valid)
        {
            Fail(tr("The encounter-preview worker returned invalid data. Reconnect and try again."));
            return;
        }
        candidate.m_checkedAt = QDateTime::currentDateTimeUtc();
        // Start before launching the worker, so latency cannot extend the host's
        // 30-second lifetime. This display is an observation, not spawn authority.
        const int remaining = static_cast<int>(qMax<qint64>(0, 30000 - m_previewElapsed.elapsed()));
        candidate.m_state = remaining > 0 ? PreviewState::Available : PreviewState::Expired;
        m_snapshot.m_encounter = std::move(candidate);
        m_snapshot.m_checkedAt = QDateTime::currentDateTimeUtc();
        m_snapshot.m_state = GameConnectionSnapshot::State::Connected;
        m_snapshot.m_message.clear();
        if (remaining > 0) { m_previewExpiry.start(remaining); }
        m_freshness.start(30000);
        Notify();
    }

    void GameConnectionService::FinishVitals(const QJsonObject& root)
    {
        using VitalsState = GameConnectionSnapshot::PlayerVitals::State;
        const auto vitals = root.value(QStringLiteral("vitals")).toObject();
        GameConnectionSnapshot::PlayerVitals candidate;
        bool valid = vitals.value(QStringLiteral("available")).isBool();
        if (valid && vitals.value(QStringLiteral("available")).toBool())
        {
            valid = vitals.size() == 7;
            auto metric = [&vitals, &valid](const QString& name)
            {
                return GameConnectionSnapshot::PlayerVitals::Metric{
                    ReadNativeNumber(vitals, name, valid), ReadNativeNumber(vitals, name + QStringLiteral("Max"), valid) };
            };
            candidate.m_health = metric(QStringLiteral("health"));
            candidate.m_stamina = metric(QStringLiteral("stamina"));
            candidate.m_mana = metric(QStringLiteral("mana"));
            candidate.m_state = VitalsState::Available;
        }
        else if (valid)
        {
            const auto code = vitals.value(QStringLiteral("code")).toString();
            valid = vitals.size() == 2;
            if (code == QStringLiteral("player_unavailable"))
            {
                candidate.m_message = tr("No player is loaded. Load a game, then choose Get Player Vitals.");
            }
            else if (code == QStringLiteral("vitals_unavailable"))
            {
                candidate.m_message = tr("Player vitals are unavailable. Wait for the player to be ready, then try again.");
            }
            else { valid = false; }
            candidate.m_state = VitalsState::Unavailable;
        }
        if (!valid)
        {
            Fail(tr("The player-vitals worker returned invalid data. Reconnect and try again."));
            return;
        }
        candidate.m_checkedAt = QDateTime::currentDateTimeUtc();
        m_snapshot.m_checkedAt = candidate.m_checkedAt;
        m_snapshot.m_state = GameConnectionSnapshot::State::Connected;
        m_snapshot.m_message.clear();
        m_snapshot.m_vitals = std::move(candidate);
        m_vitalsFreshness.start(30000);
        m_freshness.start(30000);
        Notify();
    }
}
