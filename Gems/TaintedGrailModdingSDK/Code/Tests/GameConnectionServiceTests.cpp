/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "GameConnectionService.h"
#include <AzTest/AzTest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <memory>

namespace TaintedGrailModdingSDK
{
    class GameConnectionTests : public ::testing::Test
    {
    protected:
        using State = GameConnectionSnapshot::State;
        void SetUp() override
        {
            if (!QCoreApplication::instance()) { m_app = std::make_unique<QCoreApplication>(m_argc, m_argv); }
            ASSERT_TRUE(m_directory.isValid());
            m_worker = m_directory.filePath(QStringLiteral("worker.py"));
            QFile file(m_worker);
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"PY(import argparse, json, os, sys, time
p = argparse.ArgumentParser()
p.add_argument('--port', type=int)
p.add_argument('--version')
p.add_argument('--session')
p.add_argument('--operation', default='connection')
p.add_argument('--composition', default='')
a = p.parse_args()
if os.environ.get('TGE_SDK_KEY') != 'ab' * 32 or any('ab' * 32 in value for value in sys.argv):
    sys.exit(9)
if a.port == 12346:
    time.sleep(60)
if a.operation == 'encounter-preview':
    if a.port == 12602:
        time.sleep(60)
    if a.composition == 'invalid':
        print(json.dumps({'schema': 1, 'previewInputError': 'Safe input error'}))
        sys.exit(0)
    encounter = {'name': '<b>Patrol</b>', 'compositionSha256': 'c' * 64,
        'templates': ['wyrdspirit'], 'placement': 'fixture|12,0,0', 'planId': 'a' * 32, 'fingerprint': 'b' * 64}
    changes = {12610: {'templates': []}, 12611: {'templates': ['unknown']},
        12612: {'templates': ['wyrdspirit'] * 9}, 12613: {'planId': 'bad'},
        12614: {'fingerprint': 'bad'}, 12615: {'compositionSha256': 'bad'},
        12616: {'name': ''}, 12617: {'placement': ''}, 12618: {'placement': 'x' * 4097},
        12619: {'extra': True}, 12620: {'templates': [1]}}
    encounter.update(changes.get(a.port, {}))
    print(json.dumps({'schema': 1, 'hostId': 'kane.tgfoa.tainted-grail-extender',
        'hostVersion': a.version, 'sessionId': 'changed-session' if a.port == 12621 else a.session, 'encounter': encounter}))
elif a.operation == 'vitals':
    if a.port == 12522:
        time.sleep(60)
    vitals = {'available': True, 'health': 120.25, 'healthMax': 100.5,
        'stamina': -2.5, 'staminaMax': 80.75, 'mana': 10.75, 'manaMax': 0}
    if a.port in (12501, 12502):
        vitals = {'available': False, 'code': 'player_unavailable' if a.port == 12501 else 'vitals_unavailable'}
    if a.port == 12503:
        print(json.dumps({'schema': 1, 'error': 'This extender may not support vitals. Check its version and reconnect.'}))
        sys.exit(1)
    if 12510 <= a.port <= 12515:
        vitals[list(vitals)[a.port - 12510 + 1]] = 'invalid'
    if a.port == 12516:
        vitals = {'available': False, 'code': 'unknown_operation'}
    if a.port == 12517:
        del vitals['healthMax']
    if a.port == 12518:
        vitals['extra'] = 1
    if a.port == 12519:
        vitals['health'] = 1e-100
    if a.port == 12520:
        vitals['available'] = False
        vitals['code'] = 'vitals_unavailable'
    print(json.dumps({'schema': 1, 'hostId': 'kane.tgfoa.tainted-grail-extender',
        'hostVersion': a.version, 'sessionId': 'changed-session' if a.port == 12521 else a.session, 'vitals': vitals}))
elif a.operation == 'position':
    if a.port == 12404:
        time.sleep(60)
    player = {'available': True, 'sceneName': '<b>synthetic-scene</b>', 'x': 1.25, 'y': -2.5, 'z': 3.75}
    if a.port == 12401:
        player = {'available': False, 'code': 'player_unavailable'}
    if a.port == 12402:
        player['x'] = 'not a coordinate'
    print(json.dumps({'schema': 1, 'hostId': 'kane.tgfoa.tainted-grail-extender',
        'hostVersion': a.version, 'sessionId': 'changed-session' if a.port == 12403 else a.session, 'player': player}))
elif a.port == 12347:
    print('{}')
elif a.port == 12348:
    print('x' * (2 * 1024 * 1024 + 1))
elif a.port == 12349:
    print(json.dumps({'schema': 1, 'error': 'The extender restarted. Choose Connect to use its new session.'}))
    sys.exit(1)
else:
    services = [{'id': 'tge.core.identity', 'version': '0.1', 'owner': 'tge.core'}]
    if a.port >= 12400:
        services.append({'id': 'tge.foa.player', 'version': '0.1',
            'owner': 'wrong-owner' if a.port == 12405 else 'tge.foa.player'})
    if a.port >= 12600:
        services.append({'id': 'tge.foa.encounters', 'version': '0.1',
            'owner': 'wrong-owner' if a.port == 12601 else 'tge.foa.encounters'})
    print(json.dumps({'schema': 1, 'hostId': 'kane.tgfoa.tainted-grail-extender',
      'hostVersion': a.version, 'sessionId': 'test-session', 'services': services}))
)PY");
        }
        bool Wait(GameConnectionService& service, State state, int milliseconds = 6000)
        {
            QElapsedTimer timer;
            timer.start();
            while (service.GetSnapshot().m_state != state && timer.elapsed() < milliseconds)
            {
                QCoreApplication::processEvents();
                QThread::msleep(1);
            }
            return service.GetSnapshot().m_state == state;
        }
        void TearDown() override
        {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();
        }
        bool WaitIdle(GameConnectionService& service)
        {
            QElapsedTimer timer;
            timer.start();
            while (service.IsBusy() && timer.elapsed() < 6000)
            {
                QCoreApplication::processEvents();
                QThread::msleep(1);
            }
            return !service.IsBusy();
        }
        bool WaitUntil(const std::function<bool()>& predicate, int milliseconds = 6000)
        {
            QElapsedTimer timer;
            timer.start();
            while (!predicate() && timer.elapsed() < milliseconds)
            {
                QCoreApplication::processEvents();
                QThread::msleep(1);
            }
            return predicate();
        }
        QString Python() { return QString::fromUtf8(TG_SDK_TEST_PYTHON); }
        QString Key() { return QStringLiteral("ab").repeated(32); }
        int m_argc = 1;
        char m_name[16] = "connection-test";
        char* m_argv[2] = { m_name, nullptr };
        std::unique_ptr<QCoreApplication> m_app;
        QTemporaryDir m_directory;
        QString m_worker;
    };

    TEST_F(GameConnectionTests, ConnectRefreshDisconnectPublishesTransientSnapshots)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12345, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_EQ(service.GetSnapshot().m_services.size(), 1);
        EXPECT_EQ(service.GetSnapshot().m_session, QStringLiteral("test-session"));
        EXPECT_TRUE(service.GetSnapshot().m_checkedAt.isValid());
        service.Refresh();
        ASSERT_TRUE(Wait(service, State::Connected));
        service.Disconnect();
        EXPECT_EQ(service.GetSnapshot().m_state, State::Disconnected);
        EXPECT_TRUE(service.GetSnapshot().m_services.isEmpty());
        EXPECT_TRUE(service.GetSnapshot().m_session.isEmpty());
    }

    TEST_F(GameConnectionTests, InvalidKeyAndMissingWorkerFailBeforeExecution)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12345, QStringLiteral("0.1.0"), QStringLiteral("bad"));
        EXPECT_EQ(service.GetSnapshot().m_state, State::Error);
        EXPECT_TRUE(service.GetSnapshot().m_message.contains(QStringLiteral("key")));
        GameConnectionService missing(Python(), m_worker + QStringLiteral(".missing"));
        missing.Connect(12345, QStringLiteral("0.1.0"), Key());
        EXPECT_EQ(missing.GetSnapshot().m_state, State::Error);
    }

    TEST_F(GameConnectionTests, InvalidOversizedAndRejectedResultsClearServices)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port : {12347, 12348, 12349})
        {
            service.Connect(12345, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Error));
            EXPECT_TRUE(service.GetSnapshot().m_services.isEmpty());
            EXPECT_TRUE(service.GetSnapshot().m_session.isEmpty());
        }
    }

    TEST_F(GameConnectionTests, AnExistingNonExecutableWorkerHostFailsCleanly)
    {
        GameConnectionService service(m_worker, m_worker);
        service.Connect(12345, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Error));
        EXPECT_TRUE(service.GetSnapshot().m_message.contains(QStringLiteral("could not start")));
        EXPECT_TRUE(service.GetSnapshot().m_services.isEmpty());
    }

    TEST_F(GameConnectionTests, CancelAndDestructionDoNotWaitForWorker)
    {
        QElapsedTimer timer;
        auto service = std::make_unique<GameConnectionService>(Python(), m_worker);
        service->Connect(12346, QStringLiteral("0.1.0"), Key());
        QCoreApplication::processEvents();
        timer.start();
        service->Disconnect();
        EXPECT_LT(timer.elapsed(), 250);
        service->Connect(12346, QStringLiteral("0.1.0"), Key());
        QCoreApplication::processEvents();
        timer.restart();
        service.reset();
        EXPECT_LT(timer.elapsed(), 250);
    }

    TEST_F(GameConnectionTests, LateCancelledWorkerCannotOverwriteNewConnection)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12346, QStringLiteral("0.1.0"), Key());
        QCoreApplication::processEvents();
        service.Connect(12345, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 200) { QCoreApplication::processEvents(); QThread::msleep(1); }
        EXPECT_EQ(service.GetSnapshot().m_state, State::Connected);
    }

    TEST_F(GameConnectionTests, WaitingConnectionKeepsEventLoopResponsiveAndHasDeadline)
    {
        GameConnectionService service(Python(), m_worker);
        QTimer heartbeat;
        QElapsedTimer gap;
        gap.start();
        qint64 maximumGap = 0;
        int ticks = 0;
        QObject::connect(&heartbeat, &QTimer::timeout, [&]()
        {
            maximumGap = qMax(maximumGap, gap.restart());
            ++ticks;
        });
        heartbeat.start(10);
        service.Connect(12346, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Error, 24000));
        EXPECT_TRUE(service.GetSnapshot().m_message.contains(QStringLiteral("timed out")));
        EXPECT_GT(ticks, 100);
        EXPECT_LT(maximumGap, 500);
        RecordProperty("maximum_ui_timer_gap_ms", static_cast<int>(maximumGap));
    }

    TEST_F(GameConnectionTests, AnOldObservationBecomesStaleWithoutNetworkPolling)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12600, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.PreviewEncounter(QStringLiteral("composition.json"));
        ASSERT_TRUE(WaitIdle(service));
        service.GetPlayerPosition();
        ASSERT_TRUE(WaitIdle(service));
        QTimer::singleShot(3000, &service, [&service]() { service.GetPlayerVitals(); });
        ASSERT_TRUE(WaitUntil([&service]()
        {
            return service.GetSnapshot().m_player.m_state == GameConnectionSnapshot::PlayerPosition::State::Stale;
        }, 33000));
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Available);
        ASSERT_TRUE(WaitUntil([&service]()
        {
            return service.GetSnapshot().m_vitals.m_state == GameConnectionSnapshot::PlayerVitals::State::Stale;
        }));
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Stale);
        EXPECT_EQ(service.GetSnapshot().m_services.size(), 3);
        EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, GameConnectionSnapshot::EncounterPreview::State::Expired);
        service.Refresh();
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Empty);
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Empty);
        EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, GameConnectionSnapshot::EncounterPreview::State::Empty);
    }

    TEST_F(GameConnectionTests, EncounterPreviewRequiresTheExactAdvertisedService)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port : { 12345, 12400, 12601 })
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            EXPECT_FALSE(service.CanPreviewEncounter());
            service.PreviewEncounter(QStringLiteral("composition.json"));
            EXPECT_FALSE(service.IsBusy());
        }
        service.Connect(12600, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_TRUE(service.CanPreviewEncounter());
        service.PreviewEncounter(QStringLiteral("composition with spaces.json"));
        EXPECT_FALSE(service.CanGetPlayerPosition());
        ASSERT_TRUE(WaitIdle(service));
        const auto& preview = service.GetSnapshot().m_encounter;
        EXPECT_EQ(preview.m_state, GameConnectionSnapshot::EncounterPreview::State::Available);
        EXPECT_EQ(preview.m_name, QStringLiteral("<b>Patrol</b>"));
        EXPECT_EQ(preview.m_templates, QStringList{ QStringLiteral("wyrdspirit") });
        EXPECT_EQ(preview.m_compositionSha256, QStringLiteral("c").repeated(64));
        EXPECT_TRUE(preview.m_checkedAt.isValid());
    }

    TEST_F(GameConnectionTests, PreviewInputErrorsPreserveTheConnectionAndOtherObservations)
    {
        using PreviewState = GameConnectionSnapshot::EncounterPreview::State;
        GameConnectionService service(Python(), m_worker);
        service.Connect(12600, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerPosition();
        ASSERT_TRUE(WaitIdle(service));
        service.GetPlayerVitals();
        ASSERT_TRUE(WaitIdle(service));
        const auto checkedAt = service.GetSnapshot().m_checkedAt;
        for (const auto& path : { QString(), QStringLiteral("invalid") })
        {
            service.PreviewEncounter(path);
            ASSERT_TRUE(WaitIdle(service));
            EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, PreviewState::InputError);
            EXPECT_TRUE(service.GetSnapshot().m_encounter.m_name.isEmpty());
            EXPECT_EQ(service.GetSnapshot().m_state, State::Connected);
            EXPECT_EQ(service.GetSnapshot().m_checkedAt, checkedAt);
            EXPECT_TRUE(service.CanPreviewEncounter());
            EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Available);
            EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Available);
        }
        service.PreviewEncounter(QStringLiteral("composition.json"));
        ASSERT_TRUE(WaitIdle(service));
        service.ClearEncounterPreview();
        EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, PreviewState::Empty);
        EXPECT_TRUE(service.CanPreviewEncounter());
    }

    TEST_F(GameConnectionTests, MalformedOrChangedSessionPreviewsInvalidateTheConnection)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port = 12610; port <= 12621; ++port)
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            service.PreviewEncounter(QStringLiteral("composition.json"));
            ASSERT_TRUE(Wait(service, State::Error));
            EXPECT_TRUE(service.GetSnapshot().m_encounter.m_name.isEmpty());
            EXPECT_FALSE(service.CanPreviewEncounter());
        }
    }

    TEST_F(GameConnectionTests, CancelledEncounterPreviewCannotPublishIntoNewConnection)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12602, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.PreviewEncounter(QStringLiteral("composition.json"));
        ASSERT_TRUE(service.IsBusy());
        service.Disconnect();
        service.Connect(12600, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.PreviewEncounter(QStringLiteral("new composition.json"));
        ASSERT_TRUE(WaitIdle(service));
        EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, GameConnectionSnapshot::EncounterPreview::State::Available);
        service.Disconnect();
        EXPECT_EQ(service.GetSnapshot().m_encounter.m_state, GameConnectionSnapshot::EncounterPreview::State::Empty);
    }

    TEST_F(GameConnectionTests, PlayerPositionRequiresTheExactAdvertisedService)
    {
        GameConnectionService service(Python(), m_worker);
        EXPECT_FALSE(service.CanGetPlayerPosition());
        service.GetPlayerPosition();
        EXPECT_FALSE(service.IsBusy());
        for (int port : {12345, 12405})
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            EXPECT_FALSE(service.CanGetPlayerPosition());
            service.GetPlayerPosition();
            EXPECT_FALSE(service.IsBusy());
        }
    }

    TEST_F(GameConnectionTests, PlayerCoordinatesRefreshAndDisconnectClearOldObservations)
    {
        using PlayerState = GameConnectionSnapshot::PlayerPosition::State;
        GameConnectionService service(Python(), m_worker);
        service.Connect(12400, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        ASSERT_TRUE(service.CanGetPlayerPosition());
        for (int read = 0; read < 2; ++read)
        {
            service.GetPlayerPosition();
            EXPECT_FALSE(service.CanGetPlayerPosition());
            EXPECT_EQ(service.GetSnapshot().m_player.m_state, PlayerState::Checking);
            EXPECT_TRUE(service.GetSnapshot().m_player.m_scene.isEmpty());
            ASSERT_TRUE(WaitIdle(service));
            const auto& player = service.GetSnapshot().m_player;
            EXPECT_EQ(player.m_state, PlayerState::Available);
            EXPECT_DOUBLE_EQ(player.m_x, 1.25);
            EXPECT_DOUBLE_EQ(player.m_y, -2.5);
            EXPECT_DOUBLE_EQ(player.m_z, 3.75);
            EXPECT_EQ(player.m_scene, QStringLiteral("<b>synthetic-scene</b>"));
            EXPECT_TRUE(player.m_checkedAt.isValid());
        }
        service.Disconnect();
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, PlayerState::Empty);
        EXPECT_TRUE(service.GetSnapshot().m_player.m_scene.isEmpty());
    }

    TEST_F(GameConnectionTests, MissingPlayerKeepsConnectionWithoutCoordinates)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12401, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerPosition();
        ASSERT_TRUE(WaitIdle(service));
        EXPECT_EQ(service.GetSnapshot().m_state, State::Connected);
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Unavailable);
        EXPECT_TRUE(service.GetSnapshot().m_player.m_scene.isEmpty());
        EXPECT_TRUE(service.GetSnapshot().m_player.m_message.contains(QStringLiteral("No player")));
        EXPECT_TRUE(service.CanGetPlayerPosition());
    }

    TEST_F(GameConnectionTests, MalformedPositionAndChangedSessionInvalidateConnection)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port : {12402, 12403})
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            service.GetPlayerPosition();
            ASSERT_TRUE(WaitIdle(service));
            EXPECT_EQ(service.GetSnapshot().m_state, State::Error);
            EXPECT_TRUE(service.GetSnapshot().m_player.m_scene.isEmpty());
            EXPECT_TRUE(service.GetSnapshot().m_session.isEmpty());
        }
    }

    TEST_F(GameConnectionTests, CancelledPositionCannotPublishIntoNewConnection)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12404, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerPosition();
        QCoreApplication::processEvents();
        QElapsedTimer timer;
        timer.start();
        service.Disconnect();
        EXPECT_LT(timer.elapsed(), 250);
        service.Connect(12400, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_TRUE(service.GetSnapshot().m_player.m_scene.isEmpty());
        service.GetPlayerPosition();
        ASSERT_TRUE(WaitIdle(service));
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Available);
    }

    TEST_F(GameConnectionTests, VitalsRequireTheExactServiceAndPreserveNativeRanges)
    {
        GameConnectionService service(Python(), m_worker);
        EXPECT_FALSE(service.CanGetPlayerVitals());
        service.GetPlayerVitals();
        EXPECT_FALSE(service.IsBusy());
        service.Connect(12405, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_FALSE(service.CanGetPlayerVitals());
        service.Connect(12500, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerVitals();
        EXPECT_FALSE(service.CanGetPlayerPosition());
        EXPECT_FALSE(service.CanGetPlayerVitals());
        ASSERT_TRUE(WaitIdle(service));
        const auto& vitals = service.GetSnapshot().m_vitals;
        EXPECT_EQ(vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Available);
        EXPECT_DOUBLE_EQ(vitals.m_health.m_current, 120.25);
        EXPECT_DOUBLE_EQ(vitals.m_health.m_maximum, 100.5);
        EXPECT_DOUBLE_EQ(vitals.m_stamina.m_current, -2.5);
        EXPECT_DOUBLE_EQ(vitals.m_stamina.m_maximum, 80.75);
        EXPECT_DOUBLE_EQ(vitals.m_mana.m_current, 10.75);
        EXPECT_DOUBLE_EQ(vitals.m_mana.m_maximum, 0);
        EXPECT_TRUE(vitals.m_checkedAt.isValid());
    }

    TEST_F(GameConnectionTests, ReadingPositionAndVitalsPreservesTheOtherObservation)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12500, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerPosition();
        ASSERT_TRUE(WaitIdle(service));
        service.GetPlayerVitals();
        EXPECT_EQ(service.GetSnapshot().m_player.m_state, GameConnectionSnapshot::PlayerPosition::State::Available);
        ASSERT_TRUE(WaitIdle(service));
        service.GetPlayerPosition();
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Available);
        ASSERT_TRUE(WaitIdle(service));
        service.GetPlayerVitals();
        EXPECT_FALSE(service.GetSnapshot().m_vitals.m_checkedAt.isValid());
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Checking);
        ASSERT_TRUE(WaitIdle(service));
        service.Disconnect();
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Empty);
    }

    TEST_F(GameConnectionTests, UnavailableVitalsKeepConnectionWithoutPartialValues)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port : {12501, 12502})
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            service.GetPlayerVitals();
            ASSERT_TRUE(WaitIdle(service));
            EXPECT_EQ(service.GetSnapshot().m_state, State::Connected);
            EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Unavailable);
            EXPECT_FALSE(service.GetSnapshot().m_vitals.m_message.isEmpty());
            EXPECT_TRUE(service.CanGetPlayerVitals());
        }
    }

    TEST_F(GameConnectionTests, InvalidVitalsAndOlderHostInvalidateTheConnection)
    {
        GameConnectionService service(Python(), m_worker);
        for (int port : {12503, 12510, 12511, 12512, 12513, 12514, 12515, 12516, 12517, 12518, 12519, 12520, 12521})
        {
            service.Connect(port, QStringLiteral("0.1.0"), Key());
            ASSERT_TRUE(Wait(service, State::Connected));
            service.GetPlayerVitals();
            ASSERT_TRUE(WaitIdle(service));
            EXPECT_EQ(service.GetSnapshot().m_state, State::Error);
            EXPECT_TRUE(service.GetSnapshot().m_session.isEmpty());
            EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Empty);
        }
    }

    TEST_F(GameConnectionTests, CancelledVitalsCannotPublishIntoNewConnection)
    {
        GameConnectionService service(Python(), m_worker);
        service.Connect(12522, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        service.GetPlayerVitals();
        QCoreApplication::processEvents();
        QElapsedTimer timer;
        timer.start();
        service.Disconnect();
        EXPECT_LT(timer.elapsed(), 250);
        service.Connect(12500, QStringLiteral("0.1.0"), Key());
        ASSERT_TRUE(Wait(service, State::Connected));
        EXPECT_FALSE(service.GetSnapshot().m_vitals.m_checkedAt.isValid());
        service.GetPlayerVitals();
        ASSERT_TRUE(WaitIdle(service));
        EXPECT_EQ(service.GetSnapshot().m_vitals.m_state, GameConnectionSnapshot::PlayerVitals::State::Available);
    }
}
