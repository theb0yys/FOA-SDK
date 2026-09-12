/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "GameConnectionWidget.h"
#include "GameConnectionService.h"

#include <AzCore/Utils/Utils.h>
#include <AzToolsFramework/API/PythonLoader.h>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    GameConnectionWidget::GameConnectionWidget(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("TgeGameConnectionPane"));
        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scroll);
        auto* layout = new QVBoxLayout(content);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        scroll->setWidget(content);
        outerLayout->addWidget(scroll);
        auto* title = new QLabel(tr("Connect to Game"), this);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 3);
        title->setFont(titleFont);
        layout->addWidget(title);
        auto* help = new QLabel(tr("Start the game with Tainted Grail Extender's SDK listener enabled, "
            "then enter the port from its log and the matching connection key."), this);
        help->setWordWrap(true);
        layout->addWidget(help);

        auto* form = new QFormLayout();
        m_port = new QSpinBox(this);
        m_port->setObjectName(QStringLiteral("TgeConnectionPort"));
        m_port->setRange(0, 65535);
        m_port->setSpecialValueText(tr("Enter port"));
        form->addRow(tr("Local port"), m_port);
        m_version = new QLineEdit(QStringLiteral("0.1.0"), this);
        m_version->setObjectName(QStringLiteral("TgeConnectionVersion"));
        m_version->setMaxLength(256);
        form->addRow(tr("Extender version"), m_version);
        m_key = new QLineEdit(this);
        m_key->setObjectName(QStringLiteral("TgeConnectionKey"));
        m_key->setEchoMode(QLineEdit::Password);
        m_key->setMaxLength(64);
        m_key->setPlaceholderText(tr("Uses TGE_SDK_KEY when empty"));
        m_key->setToolTip(tr("The same 64-character hexadecimal key used by the game. Kept only for this connection and never saved."));
        form->addRow(tr("Connection key"), m_key);
        layout->addLayout(form);

        auto* buttons = new QHBoxLayout();
        m_connect = new QPushButton(tr("Connect"), this);
        m_connect->setObjectName(QStringLiteral("TgeConnect"));
        m_refresh = new QPushButton(tr("Refresh"), this);
        m_refresh->setObjectName(QStringLiteral("TgeRefresh"));
        m_disconnect = new QPushButton(tr("Disconnect"), this);
        m_disconnect->setObjectName(QStringLiteral("TgeDisconnect"));
        buttons->addWidget(m_connect);
        buttons->addWidget(m_refresh);
        buttons->addWidget(m_disconnect);
        layout->addLayout(buttons);
        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("TgeConnectionStatus"));
        m_status->setWordWrap(true);
        m_status->setTextFormat(Qt::PlainText);
        layout->addWidget(m_status);
        m_details = new QLabel(this);
        m_details->setObjectName(QStringLiteral("TgeConnectionDetails"));
        m_details->setWordWrap(true);
        m_details->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        m_details->setTextFormat(Qt::PlainText);
        m_details->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        layout->addWidget(m_details);
        m_getPosition = new QPushButton(tr("Get Player Position"), this);
        m_getPosition->setObjectName(QStringLiteral("TgeGetPlayerPosition"));
        layout->addWidget(m_getPosition);
        m_position = new QLabel(this);
        m_position->setObjectName(QStringLiteral("TgePlayerPosition"));
        m_position->setWordWrap(true);
        m_position->setTextFormat(Qt::PlainText);
        m_position->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        m_position->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        layout->addWidget(m_position);
        m_getVitals = new QPushButton(tr("Get Player Vitals"), this);
        m_getVitals->setObjectName(QStringLiteral("TgeGetPlayerVitals"));
        layout->addWidget(m_getVitals);
        m_vitals = new QLabel(this);
        m_vitals->setObjectName(QStringLiteral("TgePlayerVitals"));
        m_vitals->setWordWrap(true);
        m_vitals->setTextFormat(Qt::PlainText);
        m_vitals->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        m_vitals->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        m_vitals->setToolTip(tr("Current / maximum values reported by the game. The HUD may round these values."));
        layout->addWidget(m_vitals);
        auto* compositionRow = new QHBoxLayout();
        m_composition = new QLineEdit(this);
        m_composition->setObjectName(QStringLiteral("TgeEncounterComposition"));
        m_composition->setMaxLength(4096);
        m_composition->setPlaceholderText(tr("Encounter composition JSON file"));
        m_composition->setAccessibleName(tr("Encounter composition file"));
        m_browseComposition = new QPushButton(tr("Browse..."), this);
        m_browseComposition->setObjectName(QStringLiteral("TgeBrowseComposition"));
        compositionRow->addWidget(m_composition, 1);
        compositionRow->addWidget(m_browseComposition);
        layout->addLayout(compositionRow);
        m_previewEncounter = new QPushButton(tr("Preview Encounter"), this);
        m_previewEncounter->setObjectName(QStringLiteral("TgePreviewEncounter"));
        layout->addWidget(m_previewEncounter);
        m_encounter = new QLabel(this);
        m_encounter->setObjectName(QStringLiteral("TgeEncounterPreview"));
        m_encounter->setWordWrap(true);
        m_encounter->setTextFormat(Qt::PlainText);
        m_encounter->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        m_encounter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        layout->addWidget(m_encounter);
        m_services = new QTableWidget(0, 3, this);
        m_services->setObjectName(QStringLiteral("TgeConnectionServices"));
        m_services->setMinimumHeight(180);
        m_services->setHorizontalHeaderLabels({ tr("Service"), tr("Version"), tr("Owner") });
        m_services->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_services->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_services->verticalHeader()->setVisible(false);
        m_services->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        layout->addWidget(m_services, 1);
        auto* freshness = new QLabel(tr("Results describe the last check. Refresh after changes; reconnect after a game restart. "
            "Hiding this pane disconnects it."), this);
        freshness->setToolTip(tr("Extender identity does not verify the installed game build."));
        freshness->setWordWrap(true);
        layout->addWidget(freshness);

        const auto engine = AZ::Utils::GetEnginePath();
        const QString enginePath = QString::fromUtf8(engine.c_str());
        QString worker = QDir(enginePath).filePath(QStringLiteral("scripts/foa-sdk/tge_editor_connection.py"));
        if (!QFileInfo(worker).isFile()) { worker = QString::fromUtf8(TG_SDK_GAME_CONNECTION_SOURCE); }
        const auto pythonHome = AzToolsFramework::EmbeddedPython::PythonLoader::GetPythonExecutablePath(engine.c_str());
#if defined(Q_OS_WIN)
        const QString python = QDir(QString::fromUtf8(pythonHome.c_str())).filePath(QStringLiteral("python.exe"));
#else
        const QString python = QDir(QString::fromUtf8(pythonHome.c_str())).filePath(QStringLiteral("bin/python3"));
#endif
        m_service = new GameConnectionService(python, worker, this);
        m_service->m_changed = [this]() { RefreshView(); };
        connect(m_connect, &QPushButton::clicked, this, [this]()
        {
            const QString key = m_key->text();
            m_key->clear();
            m_service->Connect(m_port->value(), m_version->text().trimmed(), key);
        });
        connect(m_refresh, &QPushButton::clicked, m_service, &GameConnectionService::Refresh);
        connect(m_getPosition, &QPushButton::clicked, m_service, &GameConnectionService::GetPlayerPosition);
        connect(m_getVitals, &QPushButton::clicked, m_service, &GameConnectionService::GetPlayerVitals);
        connect(m_browseComposition, &QPushButton::clicked, this, [this]()
        {
            auto* dialog = new QFileDialog(this, tr("Choose encounter composition"), QString(), tr("Encounter compositions (*.json)"));
            dialog->setObjectName(QStringLiteral("TgeCompositionDialog"));
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setFileMode(QFileDialog::ExistingFile);
            dialog->setAcceptMode(QFileDialog::AcceptOpen);
            dialog->setOption(QFileDialog::DontUseNativeDialog);
            dialog->setWindowModality(Qt::WindowModal);
            connect(dialog, &QFileDialog::fileSelected, m_composition, &QLineEdit::setText);
            dialog->open();
        });
        connect(m_composition, &QLineEdit::textChanged, this, [this]() { m_service->ClearEncounterPreview(); });
        connect(m_previewEncounter, &QPushButton::clicked, this, [this]() { m_service->PreviewEncounter(m_composition->text()); });
        connect(m_disconnect, &QPushButton::clicked, this, [this]() { m_key->clear(); m_service->Disconnect(); });
        RefreshView();
    }

    GameConnectionWidget::~GameConnectionWidget()
    {
        m_service->m_changed = {};
        m_service->Disconnect();
    }

    void GameConnectionWidget::hideEvent(QHideEvent* event)
    {
        // O3DE may retain a hidden dock for later reuse instead of deleting it.
        if (m_service)
        {
            m_key->clear();
            m_service->Disconnect();
        }
        QWidget::hideEvent(event);
    }

    void GameConnectionWidget::RefreshView()
    {
        using State = GameConnectionSnapshot::State;
        const auto& snapshot = m_service->GetSnapshot();
        const bool checking = m_service->IsBusy();
        const bool bound = snapshot.m_state == State::Connected || snapshot.m_state == State::Stale;
        m_connect->setEnabled(!checking && !bound);
        m_refresh->setEnabled(bound && !checking);
        m_disconnect->setEnabled(checking || bound);
        m_disconnect->setText(checking ? tr("Cancel") : tr("Disconnect"));
        m_port->setEnabled(!checking && !bound);
        m_version->setEnabled(!checking && !bound);
        m_key->setEnabled(!checking && !bound);
        m_status->setText(snapshot.m_state == State::Disconnected ? tr("Disconnected")
            : snapshot.m_state == State::Checking ? tr("Checking connection...")
            : snapshot.m_state == State::Connected ? tr("Connected: last check succeeded") : snapshot.m_message);
        m_details->setText(bound ? tr("Tainted Grail Extender %1\nLast checked: %2\nSession: %3")
            .arg(snapshot.m_hostVersion, snapshot.m_checkedAt.toLocalTime().toString(Qt::ISODate), snapshot.m_session) : QString());
        m_details->setMinimumHeight(bound ? m_details->sizeHint().height() : 0);
        using PlayerState = GameConnectionSnapshot::PlayerPosition::State;
        const auto& player = snapshot.m_player;
        const bool available = player.m_state == PlayerState::Available || player.m_state == PlayerState::Stale;
        m_getPosition->setEnabled(m_service->CanGetPlayerPosition());
        m_getPosition->setText(available ? tr("Refresh Position") : tr("Get Player Position"));
        m_position->setText(available
            ? tr("%1\nX: %2   Y: %3   Z: %4\nActive Unity scene: %5\nLast read: %6")
                .arg(player.m_state == PlayerState::Stale ? tr("Position needs a fresh read") : tr("Player world coordinates"),
                    QString::number(player.m_x, 'g', 9), QString::number(player.m_y, 'g', 9),
                    QString::number(player.m_z, 'g', 9), player.m_scene,
                    player.m_checkedAt.toLocalTime().toString(Qt::ISODate))
            : player.m_state == PlayerState::Checking ? tr("Reading player position...")
            : player.m_state == PlayerState::Unavailable ? player.m_message
            : !bound ? tr("Connect to the game to read the player position.")
            : checking ? tr("Wait for the current check to finish.")
            : m_service->CanGetPlayerPosition() ? tr("Choose Get Player Position to take a snapshot. The active Unity scene is not a logical game location.")
            : tr("This extender does not provide the supported player-position service."));
        m_position->setMinimumHeight(m_position->sizeHint().height());
        using VitalsState = GameConnectionSnapshot::PlayerVitals::State;
        const auto& vitals = snapshot.m_vitals;
        const bool vitalsAvailable = vitals.m_state == VitalsState::Available || vitals.m_state == VitalsState::Stale;
        m_getVitals->setEnabled(m_service->CanGetPlayerVitals());
        m_getVitals->setText(vitalsAvailable ? tr("Refresh Vitals") : tr("Get Player Vitals"));
        auto metricText = [](const GameConnectionSnapshot::PlayerVitals::Metric& metric)
        {
            return QStringLiteral("%1 / %2").arg(QString::number(metric.m_current, 'g', 9), QString::number(metric.m_maximum, 'g', 9));
        };
        m_vitals->setText(vitalsAvailable
            ? tr("%1\nHealth: %2\nStamina: %3\nMana: %4\nLast read: %5")
                .arg(vitals.m_state == VitalsState::Stale ? tr("Vitals need a fresh read (current / maximum)")
                    : tr("Player vitals (current / maximum)"), metricText(vitals.m_health), metricText(vitals.m_stamina),
                    metricText(vitals.m_mana), vitals.m_checkedAt.toLocalTime().toString(Qt::ISODate))
            : vitals.m_state == VitalsState::Checking ? tr("Reading player vitals...")
            : vitals.m_state == VitalsState::Unavailable ? vitals.m_message
            : !bound ? tr("Connect to the game to read player vitals.")
            : checking ? tr("Wait for the current check to finish.")
            : m_service->CanGetPlayerVitals() ? tr("Choose Get Player Vitals to read health, stamina and mana. Older extenders may not support this operation.")
            : tr("This extender does not provide the supported player service."));
        m_vitals->setMinimumHeight(m_vitals->sizeHint().height());
        using PreviewState = GameConnectionSnapshot::EncounterPreview::State;
        const auto& encounter = snapshot.m_encounter;
        const bool previewed = encounter.m_state == PreviewState::Available || encounter.m_state == PreviewState::Expired;
        m_composition->setEnabled(!checking);
        m_browseComposition->setEnabled(!checking);
        m_previewEncounter->setEnabled(m_service->CanPreviewEncounter() && !m_composition->text().trimmed().isEmpty());
        m_encounter->setText(previewed
            ? tr("%1\nComposition: %2\nActors (%3): %4\nPlacement: %5\nFile SHA-256: %6\nLast preview: %7\n"
                 "A preview creates a temporary plan. It does not spawn actors. Moving or another preview may invalidate it sooner.")
                .arg(encounter.m_state == PreviewState::Expired ? tr("Preview expired. Preview again for a fresh plan.")
                    : tr("Preview accepted; expires within 30 seconds."), encounter.m_name,
                    QString::number(encounter.m_templates.size()), encounter.m_templates.join(QStringLiteral(", ")),
                    encounter.m_placement, encounter.m_compositionSha256, encounter.m_checkedAt.toLocalTime().toString(Qt::ISODate))
            : encounter.m_state == PreviewState::Checking ? tr("Previewing encounter...")
            : encounter.m_state == PreviewState::InputError ? tr("Choose a valid encounter composition JSON file (at most 64 KiB).")
            : !bound ? tr("Connect to the game to preview an encounter composition.")
            : checking ? tr("Wait for the current check to finish.")
            : m_service->CanPreviewEncounter() ? tr("Choose an exported encounter composition, then Preview Encounter. Preview creates a temporary plan without spawning actors.")
            : tr("This extender does not provide the supported encounter service."));
        m_encounter->setMinimumHeight(m_encounter->sizeHint().height());
        m_services->setRowCount(snapshot.m_services.size());
        for (int index = 0; index < snapshot.m_services.size(); ++index)
        {
            const auto& service = snapshot.m_services[index];
            m_services->setItem(index, 0, new QTableWidgetItem(service.m_id));
            m_services->setItem(index, 1, new QTableWidgetItem(service.m_version));
            m_services->setItem(index, 2, new QTableWidgetItem(service.m_owner));
        }
    }
}
