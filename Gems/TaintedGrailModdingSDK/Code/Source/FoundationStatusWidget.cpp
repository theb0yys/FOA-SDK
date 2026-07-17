/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationStatusWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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

        AZStd::vector<AZStd::string> ParseCommaSeparatedValues(const QString& value)
        {
            AZStd::vector<AZStd::string> values;
            const QStringList parts = value.split(',', Qt::SkipEmptyParts);
            values.reserve(static_cast<size_t>(parts.size()));
            for (const QString& part : parts)
            {
                const QString trimmed = part.trimmed();
                if (!trimmed.isEmpty())
                {
                    values.push_back(ToAzString(trimmed));
                }
            }
            return values;
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            result.reserve(static_cast<int>(values.size()));
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(", ");
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

        QWidget* CreateDirectoryEditor(
            QWidget* parent,
            QLineEdit*& lineEdit,
            const QString& dialogCaption)
        {
            auto* container = new QWidget(parent);
            auto* layout = new QHBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);

            lineEdit = new QLineEdit(container);
            auto* browseButton = new QPushButton(QObject::tr("Browse…"), container);
            layout->addWidget(lineEdit, 1);
            layout->addWidget(browseButton);

            QObject::connect(browseButton, &QPushButton::clicked, container, [container, lineEdit, dialogCaption]()
            {
                const QString selectedPath = QFileDialog::getExistingDirectory(
                    container,
                    dialogCaption,
                    lineEdit->text());
                if (!selectedPath.isEmpty())
                {
                    lineEdit->setText(selectedPath);
                }
            });

            return container;
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

        auto* heading = new QLabel(tr("Tainted Grail Modding SDK — Foundation Status"), content);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        contentLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Configure an exact FoA build, persist the workspace, and review ownership, evidence, catalog coverage, and blockers. Displayed status never grants runtime permission."),
            content);
        description->setWordWrap(true);
        contentLayout->addWidget(description);

        auto* configurationGroup = new QGroupBox(tr("Workspace Configuration"), content);
        auto* configurationLayout = new QFormLayout(configurationGroup);
        m_workspaceIdEdit = new QLineEdit(configurationGroup);
        m_workspaceNameEdit = new QLineEdit(configurationGroup);
        configurationLayout->addRow(tr("Workspace ID"), m_workspaceIdEdit);
        configurationLayout->addRow(tr("Display name"), m_workspaceNameEdit);
        configurationLayout->addRow(
            tr("Workspace root"),
            CreateDirectoryEditor(configurationGroup, m_workspaceRootEdit, tr("Choose TG workspace root")));
        configurationLayout->addRow(
            tr("Output directory"),
            CreateDirectoryEditor(configurationGroup, m_outputPathEdit, tr("Choose build output directory")));
        configurationLayout->addRow(
            tr("Staging directory"),
            CreateDirectoryEditor(configurationGroup, m_stagingPathEdit, tr("Choose package staging directory")));
        configurationLayout->addRow(
            tr("Deployment directory"),
            CreateDirectoryEditor(configurationGroup, m_deploymentPathEdit, tr("Choose deployment directory")));
        contentLayout->addWidget(configurationGroup);

        auto* profileGroup = new QGroupBox(tr("Active FoA Game Profile"), content);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_profileIdEdit = new QLineEdit(profileGroup);
        m_profileNameEdit = new QLineEdit(profileGroup);
        m_gameVersionEdit = new QLineEdit(profileGroup);
        m_branchEdit = new QLineEdit(profileGroup);
        m_runtimeTargetEdit = new QComboBox(profileGroup);
        m_runtimeTargetEdit->addItems({ "Mono", "IL2CPP" });
        m_unityVersionEdit = new QLineEdit(profileGroup);
        m_bepInExVersionEdit = new QLineEdit(profileGroup);
        m_dlcScopesEdit = new QLineEdit(profileGroup);
        m_dlcScopesEdit->setPlaceholderText(tr("base-game, sanctuary-of-sarras"));

        profileLayout->addRow(tr("Profile ID"), m_profileIdEdit);
        profileLayout->addRow(tr("Display name"), m_profileNameEdit);
        profileLayout->addRow(
            tr("FoA installation"),
            CreateDirectoryEditor(profileGroup, m_installPathEdit, tr("Choose Tainted Grail installation")));
        profileLayout->addRow(tr("Game version"), m_gameVersionEdit);
        profileLayout->addRow(tr("Game branch"), m_branchEdit);
        profileLayout->addRow(tr("Runtime target"), m_runtimeTargetEdit);
        profileLayout->addRow(tr("Unity version"), m_unityVersionEdit);
        profileLayout->addRow(tr("BepInEx version"), m_bepInExVersionEdit);
        profileLayout->addRow(
            tr("Managed assemblies"),
            CreateDirectoryEditor(profileGroup, m_managedAssembliesPathEdit, tr("Choose managed assembly directory")));
        profileLayout->addRow(
            tr("BepInEx plugins"),
            CreateDirectoryEditor(profileGroup, m_pluginPathEdit, tr("Choose BepInEx plugin directory")));
        profileLayout->addRow(
            tr("Diagnostics"),
            CreateDirectoryEditor(profileGroup, m_diagnosticsPathEdit, tr("Choose diagnostic capture directory")));
        profileLayout->addRow(
            tr("Extracted data"),
            CreateDirectoryEditor(profileGroup, m_extractedDataPathEdit, tr("Choose extracted data directory")));
        profileLayout->addRow(tr("DLC/content scopes"), m_dlcScopesEdit);
        contentLayout->addWidget(profileGroup);

        auto* actionRow = new QWidget(content);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        auto* applyButton = new QPushButton(tr("Apply Configuration"), actionRow);
        auto* openButton = new QPushButton(tr("Open Workspace…"), actionRow);
        auto* saveButton = new QPushButton(tr("Save Workspace"), actionRow);
        auto* saveAsButton = new QPushButton(tr("Save Workspace As…"), actionRow);
        actionLayout->addWidget(applyButton);
        actionLayout->addWidget(openButton);
        actionLayout->addWidget(saveButton);
        actionLayout->addWidget(saveAsButton);
        contentLayout->addWidget(actionRow);

        m_persistenceStatus = new QLabel(content);
        m_persistenceStatus->setWordWrap(true);
        contentLayout->addWidget(m_persistenceStatus);

        connect(applyButton, &QPushButton::clicked, this, [this]() { ApplyConfiguration(); });
        connect(openButton, &QPushButton::clicked, this, [this]() { OpenWorkspace(); });
        connect(saveButton, &QPushButton::clicked, this, [this]() { SaveWorkspace(); });
        connect(saveAsButton, &QPushButton::clicked, this, [this]() { SaveWorkspaceAs(); });

        auto* workspaceGroup = new QGroupBox(tr("Current Workspace Status"), content);
        auto* workspaceLayout = new QFormLayout(workspaceGroup);
        m_workspaceValue = new QLabel(workspaceGroup);
        m_workspaceFileValue = new QLabel(workspaceGroup);
        m_profileValue = new QLabel(workspaceGroup);
        m_versionValue = new QLabel(workspaceGroup);
        m_branchValue = new QLabel(workspaceGroup);
        m_runtimeTargetValue = new QLabel(workspaceGroup);
        m_unityVersionValue = new QLabel(workspaceGroup);
        m_bepInExVersionValue = new QLabel(workspaceGroup);
        m_boundaryValue = new QLabel(tr("Editor authoring only — FoA runtime execution disabled"), workspaceGroup);
        m_workspaceFileValue->setWordWrap(true);
        m_boundaryValue->setWordWrap(true);
        workspaceLayout->addRow(tr("Workspace"), m_workspaceValue);
        workspaceLayout->addRow(tr("Workspace file"), m_workspaceFileValue);
        workspaceLayout->addRow(tr("Active profile"), m_profileValue);
        workspaceLayout->addRow(tr("Game version"), m_versionValue);
        workspaceLayout->addRow(tr("Branch"), m_branchValue);
        workspaceLayout->addRow(tr("Runtime target"), m_runtimeTargetValue);
        workspaceLayout->addRow(tr("Unity version"), m_unityVersionValue);
        workspaceLayout->addRow(tr("BepInEx version"), m_bepInExVersionValue);
        workspaceLayout->addRow(tr("Runtime boundary"), m_boundaryValue);
        contentLayout->addWidget(workspaceGroup);

        auto* countsGroup = new QGroupBox(tr("Foundation Counts"), content);
        auto* countsLayout = new QVBoxLayout(countsGroup);
        m_countsTable = new QTableWidget(6, 2, countsGroup);
        m_countsTable->setHorizontalHeaderLabels({ tr("Area"), tr("Count") });
        ConfigureReadOnlyTable(m_countsTable);
        countsLayout->addWidget(m_countsTable);
        contentLayout->addWidget(countsGroup);

        auto* coverageGroup = new QGroupBox(tr("Catalog Coverage by Domain"), content);
        auto* coverageLayout = new QVBoxLayout(coverageGroup);
        m_domainTable = new QTableWidget(0, 3, coverageGroup);
        m_domainTable->setHorizontalHeaderLabels({ tr("Domain"), tr("Records"), tr("Blocked") });
        ConfigureReadOnlyTable(m_domainTable);
        coverageLayout->addWidget(m_domainTable);
        contentLayout->addWidget(coverageGroup);

        auto* blockersGroup = new QGroupBox(tr("Open Blockers"), content);
        auto* blockersLayout = new QVBoxLayout(blockersGroup);
        m_blockerTable = new QTableWidget(0, 3, blockersGroup);
        m_blockerTable->setHorizontalHeaderLabels({ tr("Severity"), tr("Area"), tr("Reason") });
        ConfigureReadOnlyTable(m_blockerTable);
        blockersLayout->addWidget(m_blockerTable);
        contentLayout->addWidget(blockersGroup, 1);

        auto* refreshButton = new QPushButton(tr("Refresh Foundation Status"), content);
        connect(refreshButton, &QPushButton::clicked, this, []()
        {
            FoundationService::Get().RefreshSnapshot();
        });
        contentLayout->addWidget(refreshButton);

        LoadFormFromWorkspace();
        Refresh();
    }

    FoundationStatusWidget::~FoundationStatusWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void FoundationStatusWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void FoundationStatusWidget::LoadFormFromWorkspace()
    {
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        m_workspaceIdEdit->setText(ToQString(workspace.m_workspaceId));
        m_workspaceNameEdit->setText(ToQString(workspace.m_displayName));
        m_workspaceRootEdit->setText(ToQString(workspace.m_rootPath));
        m_outputPathEdit->setText(ToQString(workspace.m_outputPath));
        m_stagingPathEdit->setText(ToQString(workspace.m_stagingPath));
        m_deploymentPathEdit->setText(ToQString(workspace.m_deploymentPath));

        if (const GameProfile* profile = workspace.FindActiveGameProfile())
        {
            m_profileIdEdit->setText(ToQString(profile->m_profileId));
            m_profileNameEdit->setText(ToQString(profile->m_displayName));
            m_installPathEdit->setText(ToQString(profile->m_installPath));
            m_gameVersionEdit->setText(ToQString(profile->m_gameVersion));
            m_branchEdit->setText(ToQString(profile->m_branch));
            const int targetIndex = m_runtimeTargetEdit->findText(ToQString(profile->m_runtimeTarget));
            m_runtimeTargetEdit->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
            m_unityVersionEdit->setText(ToQString(profile->m_unityVersion));
            m_bepInExVersionEdit->setText(ToQString(profile->m_bepInExVersion));
            m_managedAssembliesPathEdit->setText(ToQString(profile->m_managedAssembliesPath));
            m_pluginPathEdit->setText(ToQString(profile->m_pluginPath));
            m_diagnosticsPathEdit->setText(ToQString(profile->m_diagnosticsPath));
            m_extractedDataPathEdit->setText(ToQString(profile->m_extractedDataPath));
            m_dlcScopesEdit->setText(JoinValues(profile->m_dlcScopes));
        }
        else
        {
            m_profileIdEdit->clear();
            m_profileNameEdit->clear();
            m_installPathEdit->clear();
            m_gameVersionEdit->clear();
            m_branchEdit->setText("mono");
            m_runtimeTargetEdit->setCurrentText("Mono");
            m_unityVersionEdit->clear();
            m_bepInExVersionEdit->clear();
            m_managedAssembliesPathEdit->clear();
            m_pluginPathEdit->clear();
            m_diagnosticsPathEdit->clear();
            m_extractedDataPathEdit->clear();
            m_dlcScopesEdit->clear();
        }
    }

    void FoundationStatusWidget::ApplyConfiguration()
    {
        FoundationService& service = FoundationService::Get();
        WorkspaceModel workspace = service.GetWorkspace();
        const AZStd::string previousActiveProfileId = workspace.m_activeGameProfileId;

        workspace.m_workspaceId = ToAzString(m_workspaceIdEdit->text().trimmed());
        workspace.m_displayName = ToAzString(m_workspaceNameEdit->text().trimmed());
        workspace.m_rootPath = ToAzString(m_workspaceRootEdit->text().trimmed());
        workspace.m_outputPath = ToAzString(m_outputPathEdit->text().trimmed());
        workspace.m_stagingPath = ToAzString(m_stagingPathEdit->text().trimmed());
        workspace.m_deploymentPath = ToAzString(m_deploymentPathEdit->text().trimmed());

        GameProfile profile;
        profile.m_profileId = ToAzString(m_profileIdEdit->text().trimmed());
        profile.m_displayName = ToAzString(m_profileNameEdit->text().trimmed());
        profile.m_installPath = ToAzString(m_installPathEdit->text().trimmed());
        profile.m_gameVersion = ToAzString(m_gameVersionEdit->text().trimmed());
        profile.m_branch = ToAzString(m_branchEdit->text().trimmed());
        profile.m_runtimeTarget = ToAzString(m_runtimeTargetEdit->currentText());
        profile.m_unityVersion = ToAzString(m_unityVersionEdit->text().trimmed());
        profile.m_bepInExVersion = ToAzString(m_bepInExVersionEdit->text().trimmed());
        profile.m_managedAssembliesPath = ToAzString(m_managedAssembliesPathEdit->text().trimmed());
        profile.m_pluginPath = ToAzString(m_pluginPathEdit->text().trimmed());
        profile.m_diagnosticsPath = ToAzString(m_diagnosticsPathEdit->text().trimmed());
        profile.m_extractedDataPath = ToAzString(m_extractedDataPathEdit->text().trimmed());
        profile.m_dlcScopes = ParseCommaSeparatedValues(m_dlcScopesEdit->text());

        bool replaced = false;
        for (GameProfile& existing : workspace.m_gameProfiles)
        {
            if (existing.m_profileId == previousActiveProfileId
                || (!profile.m_profileId.empty() && existing.m_profileId == profile.m_profileId))
            {
                existing = profile;
                replaced = true;
                break;
            }
        }
        if (!replaced)
        {
            workspace.m_gameProfiles.push_back(profile);
        }
        workspace.m_activeGameProfileId = profile.m_profileId;

        service.SetWorkspace(workspace);
        m_persistenceStatus->setText(tr("Configuration applied in memory. Review blockers, then save the workspace."));
    }

    void FoundationStatusWidget::OpenWorkspace()
    {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("Open TG workspace"),
            QString(),
            tr("TG workspace (*.tgworkspace.json *.json);;JSON files (*.json)"));
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

        LoadFormFromWorkspace();
        m_persistenceStatus->setText(tr("Workspace opened from %1").arg(filePath));
    }

    void FoundationStatusWidget::SaveWorkspace()
    {
        ApplyConfiguration();
        FoundationService& service = FoundationService::Get();
        if (service.GetWorkspaceFilePath().empty())
        {
            SaveWorkspaceAs();
            return;
        }

        AZStd::string error;
        if (!service.SaveWorkspace(&error))
        {
            QMessageBox::critical(this, tr("Unable to save workspace"), ToQString(error));
            return;
        }

        m_persistenceStatus->setText(
            tr("Workspace saved to %1").arg(ToQString(service.GetWorkspaceFilePath())));
    }

    void FoundationStatusWidget::SaveWorkspaceAs()
    {
        ApplyConfiguration();
        QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("Save TG workspace"),
            QString(),
            tr("TG workspace (*.tgworkspace.json);;JSON files (*.json)"));
        if (filePath.isEmpty())
        {
            return;
        }
        if (!filePath.endsWith(".json", Qt::CaseInsensitive))
        {
            filePath += ".tgworkspace.json";
        }

        AZStd::string error;
        if (!FoundationService::Get().SaveWorkspace(ToAzString(filePath), &error))
        {
            QMessageBox::critical(this, tr("Unable to save workspace"), ToQString(error));
            return;
        }

        m_persistenceStatus->setText(tr("Workspace saved to %1").arg(filePath));
    }

    void FoundationStatusWidget::Refresh()
    {
        const FoundationSnapshot& snapshot = FoundationService::Get().GetSnapshot();

        m_workspaceValue->setText(ToQString(snapshot.m_workspaceName));
        m_workspaceFileValue->setText(
            snapshot.m_workspaceFilePath.empty() ? tr("Not saved") : ToQString(snapshot.m_workspaceFilePath));
        m_profileValue->setText(ToQString(snapshot.m_activeGameProfile));
        m_versionValue->setText(ToQString(snapshot.m_gameVersion));
        m_branchValue->setText(ToQString(snapshot.m_branch));
        m_runtimeTargetValue->setText(ToQString(snapshot.m_runtimeTarget));
        m_unityVersionValue->setText(ToQString(snapshot.m_unityVersion));
        m_bepInExVersionValue->setText(ToQString(snapshot.m_bepInExVersion));

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
            { "Open blockers", snapshot.m_openBlockerCount },
        };

        for (int row = 0; row < 6; ++row)
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
