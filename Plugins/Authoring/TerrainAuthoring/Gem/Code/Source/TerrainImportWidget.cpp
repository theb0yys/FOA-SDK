/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "TerrainImportWidget.h"
#include "TerrainAuthoringContracts.h"
#include <ExtensionRequestBus.h>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
namespace TerrainAuthoring
{
    TerrainImportWidget::TerrainImportWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName("FoaHeightmapImporter");
        setWindowTitle(tr("Heightmap Importer"));
        setMinimumWidth(340);
        setMaximumWidth(520);
        setAcceptDrops(true);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("Heightmap Importer"), this));
        m_context = new QLabel(this);
        m_context->setWordWrap(true);
        layout->addWidget(m_context);
        auto* explanation =
            new QLabel(tr("Import a local heightmap for O3DE editing. Campaign map import and return to the game are unavailable."), this);
        explanation->setWordWrap(true);
        layout->addWidget(explanation);
        auto* actions = new QHBoxLayout();
        m_vanilla = new QPushButton(tr("Campaign import unavailable"), this);
        m_vanilla->setObjectName("TerrainEditVanillaMap");
        m_vanilla->setEnabled(false);
        m_vanilla->setToolTip(tr("Faithful campaign source preservation and game round-trip support have not been verified."));
        connect(
            m_vanilla,
            &QPushButton::clicked,
            this,
            [this]()
            {
                auto* dialog = new QDialog(this);
                dialog->setObjectName("TerrainCampaignChooser");
                dialog->setWindowTitle(tr("Choose a campaign map"));
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->setWindowModality(Qt::WindowModal);
                dialog->setMinimumWidth(420);
                auto* choices = new QVBoxLayout(dialog);
                choices->addWidget(new QLabel(tr("Create a local heightmap from a campaign."), dialog));
                auto* maps = new QComboBox(dialog);
                maps->setObjectName("TerrainCampaignSelection");
                for (const auto& value : m_campaigns)
                {
                    const auto row = value.toObject();
                    maps->addItem(row.value("name").toString(), row.value("key").toString());
                }
                choices->addWidget(maps);
                auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
                choices->addWidget(buttons);
                connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
                connect(
                    dialog,
                    &QDialog::accepted,
                    this,
                    [this, maps]()
                    {
                        if (!m_busy && maps->currentIndex() >= 0)
                        {
                            Send({ { "action", "import-campaign" }, { "campaign", maps->currentData().toString() } });
                        }
                    });
                dialog->open();
            });
        m_import = new QPushButton(tr("Import New Map"), this);
        m_import->setObjectName("TerrainImportNewMap");
        actions->addWidget(m_vanilla);
        actions->addWidget(m_import);
        actions->addStretch();
        layout->addLayout(actions);
        connect(
            m_import,
            &QPushButton::clicked,
            this,
            [this]()
            {
                const auto source = QFileDialog::getOpenFileName(
                    this, tr("Choose a heightmap"), {}, tr("16-bit heightmaps (*.png *.tif *.tiff *.raw *.u16 *.r16)"));
                if (!source.isEmpty())
                {
                    ImportFile(source);
                }
            });
        m_status = new QLabel(this);
        m_status->setObjectName("TerrainImportStatus");
        m_status->setWordWrap(true);
        layout->addWidget(m_status);
        m_progress = new QProgressBar(this);
        m_progress->setVisible(false);
        layout->addWidget(m_progress);
        m_cancel = new QPushButton(tr("Cancel"), this);
        m_cancel->setObjectName("TerrainImportCancel");
        m_cancel->setVisible(false);
        connect(
            m_cancel,
            &QPushButton::clicked,
            this,
            [this]()
            {
                Send({ { "action", "cancel" } });
            });
        layout->addWidget(m_cancel);
        layout->addWidget(new QLabel(tr("Imported maps"), this));
        m_recent = new QListWidget(this);
        m_recent->setObjectName("TerrainSavedRevisions");
        layout->addWidget(m_recent, 1);
        m_preview = new QLabel(this);
        m_preview->setObjectName("TerrainHeightPreview");
        m_preview->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_preview);
        m_details = new QLabel(this);
        m_details->setObjectName("TerrainHeightDetails");
        m_details->setWordWrap(true);
        layout->addWidget(m_details);
        auto* saved = new QHBoxLayout();
        m_refresh = new QPushButton(tr("Refresh"), this);
        m_refresh->setObjectName("TerrainRefresh");
        m_open = new QPushButton(tr("Open in Editor"), this);
        m_open->setObjectName("TerrainOpenRevision");
        auto* preview = new QPushButton(tr("Preview"), this);
        preview->setObjectName("TerrainPreviewRevision");
        connect(
            preview,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (const auto* item = m_recent->currentItem(); item && !m_busy)
                {
                    Send({ { "action", "open" }, { "revision", item->data(Qt::UserRole).toString() } });
                }
            });
        saved->addWidget(preview);
        saved->addWidget(m_refresh);
        saved->addWidget(m_open);
        layout->addLayout(saved);
        connect(
            m_refresh,
            &QPushButton::clicked,
            this,
            [this]()
            {
                Send({ { "action", "refresh" } });
            });
        connect(
            m_open,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (const auto* item = m_recent->currentItem(); item && !m_busy)
                {
                    Send({ { "action", "open-editor" }, { "revision", item->data(Qt::UserRole).toString() } });
                }
            });
        connect(m_recent, &QListWidget::itemDoubleClicked, m_open, &QPushButton::click);
        auto* help = new QLabel(
            tr("In O3DE: select Paint Terrain Heights, then Image Gradient > Paint. Use Asset Browser and entity tools for objects. Finish "
               "painting, then save the level with Ctrl+S."),
            this);
        help->setWordWrap(true);
        layout->addWidget(help);
        m_timer = new QTimer(this);
        connect(
            m_timer,
            &QTimer::timeout,
            this,
            [this]()
            {
                Send({ { "action", "poll" } });
            });
        TaintedGrailModdingSDK::FoundationNotificationBus::Handler::BusConnect();
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                Send({ { "action", "refresh" } });
            });
    }
    TerrainImportWidget::~TerrainImportWidget()
    {
        TaintedGrailModdingSDK::FoundationNotificationBus::Handler::BusDisconnect();
        m_timer->stop();
        if (m_busy)
        {
            Send({ { "action", "cancel" } });
        }
    }
    void TerrainImportWidget::OnFoundationChanged()
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                m_preview->clear();
                m_details->clear();
                m_recent->clear();
                Send({ { "action", "refresh" } });
            });
    }
    void TerrainImportWidget::ImportFile(const QString& source)
    {
        if (!m_busy)
        {
            Send({ { "action", "import" }, { "source", source } });
        }
    }
    void TerrainImportWidget::Send(const QJsonObject& command)
    {
        using namespace TaintedGrailModdingSDK;
        const QString action = command.value("action").toString();
        const auto bytes = QJsonDocument(command).toJson(QJsonDocument::Compact);
        AZStd::string response, error;
        bool accepted = false;
        ExtensionRequestBus::BroadcastResult(
            accepted,
            &ExtensionRequests::TerrainImportCommand,
            AZStd::string(TerrainAuthoringExtensionId),
            AZStd::string(bytes.constData(), static_cast<size_t>(bytes.size())),
            response,
            &error);
        if (!accepted)
        {
            m_vanilla->setEnabled(false);
            m_status->setText(
                error.empty() ? tr("The terrain importer is unavailable. Check SDK setup.") : QString::fromUtf8(error.c_str()));
            m_timer->stop();
            m_busy = false;
            m_import->setEnabled(false);
            m_refresh->setEnabled(true);
            m_open->setEnabled(false);
            setAcceptDrops(false);
            m_context->clear();
            m_preview->clear();
            m_details->clear();
            m_cancel->setEnabled(false);
            m_cancel->setVisible(false);
            m_progress->setVisible(false);
            return;
        }
        const auto snapshot = QJsonDocument::fromJson(QByteArray(response.data(), static_cast<int>(response.size()))).object();
        m_busy = snapshot.value("busy").toBool();
        if (snapshot.contains("campaigns"))
        {
            m_campaigns = snapshot.value("campaigns").toArray();
        }
        m_vanilla->setEnabled(!m_busy && !m_campaigns.isEmpty());
        m_vanilla->setToolTip(
            m_campaigns.isEmpty() ? tr("Faithful campaign source preservation and game round-trip support have not been verified.")
                                  : tr("Create a local heightmap from the selected campaign ground surface."));
        setAcceptDrops(!m_busy);
        m_context->setText(tr("%1 | %2").arg(snapshot.value("workspace_name").toString(), snapshot.value("profile_name").toString()));
        if (command.value("action").toString() == "import" || command.value("action").toString() == "import-campaign" ||
            command.value("action").toString() == "open" || command.value("action").toString() == "open-editor")
        {
            m_preview->clear();
            m_details->clear();
        }
        m_status->setText(snapshot.value("message").toString());
        m_import->setEnabled(!m_busy);
        m_refresh->setEnabled(!m_busy);
        m_open->setEnabled(!m_busy);
        m_cancel->setEnabled(m_busy);
        m_cancel->setVisible(m_busy);
        m_progress->setVisible(m_busy);
        const int progress = snapshot.value("progress").toInt();
        m_progress->setRange(0, m_busy && progress == 0 ? 0 : 100);
        m_progress->setValue(progress);
        if (m_busy)
        {
            m_timer->setInterval(100);
            m_timer->start();
        }
        else
        {
            m_timer->stop();
        }
        if (snapshot.contains("rows"))
        {
            m_recent->clear();
            for (const auto& value : snapshot.value("rows").toArray())
            {
                const auto row = value.toObject();
                auto* item = new QListWidgetItem(row.value("name").toString(), m_recent);
                item->setToolTip(row.value("created").toString() + "\n" + row.value("revision").toString());
                item->setData(Qt::UserRole, row.value("revision").toString());
            }
        }
        if (snapshot.contains("created") && snapshot.contains("revision"))
        {
            QListWidgetItem* item = nullptr;
            for (int i = 0; i < m_recent->count(); ++i)
            {
                if (m_recent->item(i)->data(Qt::UserRole).toString() == snapshot.value("revision").toString())
                {
                    item = m_recent->item(i);
                    break;
                }
            }
            if (!item)
            {
                item = new QListWidgetItem(snapshot.value("name").toString(), m_recent);
            }
            item->setToolTip(snapshot.value("created").toString() + "\n" + snapshot.value("revision").toString());
            item->setData(Qt::UserRole, snapshot.value("revision").toString());
            m_recent->setCurrentItem(item);
        }
        if (snapshot.contains("preview"))
        {
            QPixmap preview;
            const auto png = QByteArray::fromBase64(snapshot.value("preview").toString().toLatin1());
            if (png.size() <= 1024 * 1024 && preview.loadFromData(png, "PNG"))
            {
                m_preview->setPixmap(preview.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_details->setText(
                    tr("%1\n%2 x %3 samples")
                        .arg(snapshot.value("name").toString())
                        .arg(snapshot.value("width").toInt())
                        .arg(snapshot.value("height").toInt()) +
                    "\n" + snapshot.value("note").toString());
            }
        }
    }
    void TerrainImportWidget::dragEnterEvent(QDragEnterEvent* event)
    {
        if (!m_busy && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
            event->mimeData()->urls().front().isLocalFile())
        {
            event->acceptProposedAction();
        }
    }
    void TerrainImportWidget::dropEvent(QDropEvent* event)
    {
        if (!m_busy && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
            event->mimeData()->urls().front().isLocalFile())
        {
            ImportFile(event->mimeData()->urls().front().toLocalFile());
            event->acceptProposedAction();
        }
    }
} // namespace TerrainAuthoring
