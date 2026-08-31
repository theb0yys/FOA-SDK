/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationStatusWidget.h"

#include "FoundationService.h"
#include "LocalSetupDetectionService.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        void ConfigureReadOnlyTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QString StatusText(bool ready, const QString& readyText, const QString& pendingText)
        {
            return ready ? readyText : pendingText;
        }
    } // namespace

    FoundationStatusWidget::FoundationStatusWidget(QWidget* parent)
        : QWidget(parent)
    {
        FoundationNotificationBus::Handler::BusConnect();

        auto* rootLayout = new QVBoxLayout(this);
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);
        scrollArea->setWidget(content);
        rootLayout->addWidget(scrollArea);

        auto* heading = new QLabel(tr("FOA-SDK System Details"), content);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        contentLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("FOA-SDK checks the local game and authoring workspace automatically. "
               "Most users should only need this screen when setup needs attention."),
            content);
        description->setWordWrap(true);
        contentLayout->addWidget(description);

        m_overallStatus = new QLabel(content);
        QFont overallFont = m_overallStatus->font();
        overallFont.setPointSize(overallFont.pointSize() + 2);
        overallFont.setBold(true);
        m_overallStatus->setFont(overallFont);
        contentLayout->addWidget(m_overallStatus);

        auto* systemGroup = new QGroupBox(tr("System status"), content);
        auto* systemLayout = new QFormLayout(systemGroup);
        m_sdkStatus = new QLabel(systemGroup);
        m_gameStatus = new QLabel(systemGroup);
        m_versionValue = new QLabel(systemGroup);
        m_runtimeTargetValue = new QLabel(systemGroup);
        m_workspaceValue = new QLabel(systemGroup);
        m_authoringStatus = new QLabel(systemGroup);
        systemLayout->addRow(tr("FOA-SDK"), m_sdkStatus);
        systemLayout->addRow(tr("Fall of Avalon"), m_gameStatus);
        systemLayout->addRow(tr("Game version"), m_versionValue);
        systemLayout->addRow(tr("Runtime"), m_runtimeTargetValue);
        systemLayout->addRow(tr("Workspace"), m_workspaceValue);
        systemLayout->addRow(tr("Authoring"), m_authoringStatus);
        contentLayout->addWidget(systemGroup);

        auto* actionRow = new QWidget(content);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        auto* recheckButton = new QPushButton(tr("Check again"), actionRow);
        m_locateGameButton = new QPushButton(tr("Locate Fall of Avalon..."), actionRow);
        m_advancedToggleButton = new QPushButton(tr("Show advanced details"), actionRow);
        actionLayout->addWidget(recheckButton);
        actionLayout->addWidget(m_locateGameButton);
        actionLayout->addStretch(1);
        actionLayout->addWidget(m_advancedToggleButton);
        contentLayout->addWidget(actionRow);

        m_persistenceStatus = new QLabel(content);
        m_persistenceStatus->setWordWrap(true);
        contentLayout->addWidget(m_persistenceStatus);

        m_advancedGroup = new QGroupBox(tr("Advanced details"), content);
        auto* advancedLayout = new QVBoxLayout(m_advancedGroup);
        auto* advancedDescription = new QLabel(
            tr("These values are detected or generated by FOA-SDK. They are shown for diagnostics and are not normal setup fields."),
            m_advancedGroup);
        advancedDescription->setWordWrap(true);
        advancedLayout->addWidget(advancedDescription);

        m_boundaryValue = new QLabel(
            tr("Authoring tools do not grant runtime, deployment, save, signing, or publication authority."),
            m_advancedGroup);
        m_boundaryValue->setWordWrap(true);
        advancedLayout->addWidget(m_boundaryValue);

        m_advancedDetails = new QPlainTextEdit(m_advancedGroup);
        m_advancedDetails->setReadOnly(true);
        m_advancedDetails->setMinimumHeight(190);
        advancedLayout->addWidget(m_advancedDetails);

        auto* openWorkspaceButton = new QPushButton(tr("Open existing workspace..."), m_advancedGroup);
        openWorkspaceButton->setToolTip(
            tr("Compatibility option for an existing FOA-SDK workspace. New installations use the automatic workspace."));
        advancedLayout->addWidget(openWorkspaceButton);

        auto* countsGroup = new QGroupBox(tr("Foundation and governance"), m_advancedGroup);
        auto* countsLayout = new QVBoxLayout(countsGroup);
        m_countsTable = new QTableWidget(12, 2, countsGroup);
        m_countsTable->setHorizontalHeaderLabels({ tr("Area"), tr("Count") });
        ConfigureReadOnlyTable(m_countsTable);
        countsLayout->addWidget(m_countsTable);
        advancedLayout->addWidget(countsGroup);

        auto* coverageGroup = new QGroupBox(tr("Catalog coverage by domain"), m_advancedGroup);
        auto* coverageLayout = new QVBoxLayout(coverageGroup);
        m_domainTable = new QTableWidget(0, 3, coverageGroup);
        m_domainTable->setHorizontalHeaderLabels({ tr("Domain"), tr("Records"), tr("Blocked") });
        ConfigureReadOnlyTable(m_domainTable);
        coverageLayout->addWidget(m_domainTable);
        advancedLayout->addWidget(coverageGroup);

        auto* blockersGroup = new QGroupBox(tr("Open blockers"), m_advancedGroup);
        auto* blockersLayout = new QVBoxLayout(blockersGroup);
        m_blockerTable = new QTableWidget(0, 3, blockersGroup);
        m_blockerTable->setHorizontalHeaderLabels({ tr("Severity"), tr("Area"), tr("Reason") });
        ConfigureReadOnlyTable(m_blockerTable);
        blockersLayout->addWidget(m_blockerTable);
        advancedLayout->addWidget(blockersGroup);

        m_advancedGroup->setVisible(false);
        contentLayout->addWidget(m_advancedGroup);

        connect(recheckButton, &QPushButton::clicked, this, [this]() { DetectAndApply(); });
        connect(m_locateGameButton, &QPushButton::clicked, this, [this]() { LocateGame(); });
        connect(openWorkspaceButton, &QPushButton::clicked, this, [this]() { OpenWorkspace(); });
        connect(m_advancedToggleButton, &QPushButton::clicked, this, [this]()
        {
            const bool show = !m_advancedGroup->isVisible();
            m_advancedGroup->setVisible(show);
            m_advancedToggleButton->setText(
                show ? tr("Hide advanced details") : tr("Show advanced details"));
        });

        DetectAndApply();
    }

    FoundationStatusWidget::~FoundationStatusWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void FoundationStatusWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void FoundationStatusWidget::DetectAndApply(const AZStd::string& explicitInstallPath)
    {
        const FoundationLocalSetupResult result =
            FoundationService::Get().RefreshLocalSetup(explicitInstallPath);
        m_detectionNotes = result.m_notes;

        if (!result.m_error.empty())
        {
            m_persistenceStatus->setText(
                tr("Setup could not be completed: %1").arg(ToQString(result.m_error)));
        }
        else if (result.IsReady())
        {
            m_persistenceStatus->setText(
                tr("Setup is ready. FOA-SDK manages this configuration automatically."));
        }
        else if (!result.m_gameInstallDetected)
        {
            m_persistenceStatus->setText(
                tr("Fall of Avalon was not found automatically. Locate the game once and FOA-SDK will derive everything else from that folder."));
        }
        else
        {
            m_persistenceStatus->setText(
                tr("Fall of Avalon was found, but setup is still resolving the local profile. Check again after the game files are available."));
        }

        Refresh();
    }

    void FoundationStatusWidget::LocateGame()
    {
        QString startDirectory;
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        if (const GameProfile* profile = workspace.FindActiveGameProfile())
        {
            startDirectory = ToQString(profile->m_installPath);
        }

        const QString selectedPath = QFileDialog::getExistingDirectory(
            this,
            tr("Locate Fall of Avalon"),
            startDirectory);
        if (selectedPath.isEmpty())
        {
            return;
        }
        if (!LocalSetupDetectionService::LooksLikeTaintedGrailInstall(ToAzString(selectedPath)))
        {
            QMessageBox::warning(
                this,
                tr("Fall of Avalon not found"),
                tr("That folder does not look like a Fall of Avalon installation. Select the folder containing Fall of Avalon.exe."));
            return;
        }

        DetectAndApply(ToAzString(selectedPath));
    }

    void FoundationStatusWidget::OpenWorkspace()
    {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("Open FOA-SDK workspace"),
            QString(),
            tr("FOA-SDK workspace (*.tgworkspace.json *.json);;JSON files (*.json)"));
        if (filePath.isEmpty())
        {
            return;
        }

        AZStd::string error;
        if (!FoundationService::Get().LoadWorkspace(ToAzString(filePath), &error))
        {
            QMessageBox::critical(this, tr("Unable to open workspace"), ToQString(error));
            return;
        }

        m_persistenceStatus->setText(tr("Workspace opened. Local setup was rechecked automatically."));
        DetectAndApply();
    }

    void FoundationStatusWidget::UpdateAdvancedDetails()
    {
        const FoundationService& service = FoundationService::Get();
        const WorkspaceModel& workspace = service.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();

        QStringList lines;
        lines.push_back(tr("Workspace file: %1").arg(
            service.GetWorkspaceFilePath().empty()
                ? tr("Not saved")
                : ToQString(service.GetWorkspaceFilePath())));
        lines.push_back(tr("Workspace root: %1").arg(ToQString(workspace.m_rootPath)));
        lines.push_back(tr("Output: %1").arg(ToQString(workspace.m_outputPath)));
        lines.push_back(tr("Staging: %1").arg(ToQString(workspace.m_stagingPath)));
        lines.push_back(tr("Deployment: %1").arg(ToQString(workspace.m_deploymentPath)));

        if (profile)
        {
            lines.push_back(QString());
            lines.push_back(tr("Profile ID: %1").arg(ToQString(profile->m_profileId)));
            lines.push_back(tr("Game install: %1").arg(ToQString(profile->m_installPath)));
            lines.push_back(tr("Branch: %1").arg(ToQString(profile->m_branch)));
            lines.push_back(tr("Unity: %1").arg(ToQString(profile->m_unityVersion)));
            lines.push_back(tr("BepInEx: %1").arg(ToQString(profile->m_bepInExVersion)));
            lines.push_back(tr("Managed/interop: %1").arg(ToQString(profile->m_managedAssembliesPath)));
            lines.push_back(tr("Plugins: %1").arg(
                profile->m_pluginPath.empty() ? tr("Not applicable") : ToQString(profile->m_pluginPath)));
            lines.push_back(tr("Diagnostics: %1").arg(ToQString(profile->m_diagnosticsPath)));
            lines.push_back(tr("Extracted data: %1").arg(ToQString(profile->m_extractedDataPath)));
        }

        if (!m_detectionNotes.empty())
        {
            lines.push_back(QString());
            lines.push_back(tr("Detection notes:"));
            for (const AZStd::string& note : m_detectionNotes)
            {
                lines.push_back(tr("- %1").arg(ToQString(note)));
            }
        }
        m_advancedDetails->setPlainText(lines.join('\n'));
    }

    void FoundationStatusWidget::Refresh()
    {
        const FoundationService& service = FoundationService::Get();
        const WorkspaceModel& workspace = service.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        const bool workspaceReady =
            !workspace.m_rootPath.empty()
            && !workspace.m_outputPath.empty()
            && !workspace.m_stagingPath.empty()
            && !workspace.m_deploymentPath.empty();
        const bool gameFound = profile
            && LocalSetupDetectionService::LooksLikeTaintedGrailInstall(profile->m_installPath);
        const bool profileReady = profile && profile->IsConfigured();
        const bool ready = workspaceReady && gameFound && profileReady;

        m_overallStatus->setText(
            ready ? tr("Ready to author") : tr("Setup needs attention"));
        m_sdkStatus->setText(tr("Ready"));
        m_gameStatus->setText(
            gameFound ? tr("Found") : tr("Not found"));
        m_versionValue->setText(
            profile && !profile->m_gameVersion.empty()
                ? ToQString(profile->m_gameVersion)
                : tr("Detecting"));
        m_runtimeTargetValue->setText(
            profile && !profile->m_runtimeTarget.empty()
                ? ToQString(profile->m_runtimeTarget)
                : tr("Detecting"));
        m_workspaceValue->setText(
            StatusText(workspaceReady, tr("Ready"), tr("Preparing")));
        m_authoringStatus->setText(
            profileReady ? tr("Ready") : (gameFound ? tr("Resolving profile") : tr("Waiting for game")));
        m_locateGameButton->setVisible(!gameFound);

        UpdateAdvancedDetails();

        const FoundationSnapshot& snapshot = service.GetSnapshot();
        const struct CountRow
        {
            const char* m_name;
            AZ::u64 m_count;
        } countRows[] = {
            { "Game profiles", snapshot.m_gameProfileCount },
            { "Pack manifests", snapshot.m_packCount },
            { "Registered sources", snapshot.m_sourceCount },
            { "Evidence records", snapshot.m_evidenceCount },
            { "Catalog records", snapshot.m_catalogRecordCount },
            { "Catalog relationships", snapshot.m_catalogRelationshipCount },
            { "Validation events", snapshot.m_catalogValidationCount },
            { "Governance decisions", snapshot.m_catalogGovernanceCount },
            { "Stale catalog subjects", snapshot.m_staleCatalogSubjectCount },
            { "Allowed usage lanes", snapshot.m_allowedUsageCount },
            { "Prohibited usage lanes", snapshot.m_forbiddenUsageCount },
            { "Open blockers", snapshot.m_openBlockerCount },
        };

        const int countRowCount = static_cast<int>(sizeof(countRows) / sizeof(countRows[0]));
        m_countsTable->setRowCount(countRowCount);
        for (int row = 0; row < countRowCount; ++row)
        {
            SetCell(m_countsTable, row, 0, tr(countRows[row].m_name));
            SetCell(m_countsTable, row, 1, QString::number(static_cast<qulonglong>(countRows[row].m_count)));
        }
        m_countsTable->resizeRowsToContents();

        m_domainTable->setRowCount(static_cast<int>(snapshot.m_domainCoverage.size()));
        for (int row = 0; row < static_cast<int>(snapshot.m_domainCoverage.size()); ++row)
        {
            const DomainCoverage& coverage = snapshot.m_domainCoverage[static_cast<size_t>(row)];
            SetCell(m_domainTable, row, 0, ToQString(coverage.m_domain));
            SetCell(m_domainTable, row, 1, QString::number(static_cast<qulonglong>(coverage.m_recordCount)));
            SetCell(m_domainTable, row, 2, QString::number(static_cast<qulonglong>(coverage.m_blockedRecordCount)));
        }
        m_domainTable->resizeRowsToContents();

        m_blockerTable->setRowCount(static_cast<int>(snapshot.m_blockers.size()));
        for (int row = 0; row < static_cast<int>(snapshot.m_blockers.size()); ++row)
        {
            const BlockerRecord& blocker = snapshot.m_blockers[static_cast<size_t>(row)];
            SetCell(m_blockerTable, row, 0, ToQString(blocker.m_severity));
            SetCell(m_blockerTable, row, 1, ToQString(blocker.m_area));
            SetCell(m_blockerTable, row, 2, ToQString(blocker.m_reason));
        }
        m_blockerTable->resizeRowsToContents();
    }
} // namespace TaintedGrailModdingSDK
