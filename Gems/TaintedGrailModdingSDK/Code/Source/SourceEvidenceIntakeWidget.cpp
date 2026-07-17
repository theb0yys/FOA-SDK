/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "SourceEvidenceIntakeWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }
    } // namespace

    SourceEvidenceIntakeWidget::SourceEvidenceIntakeWidget(QWidget* parent)
        : QWidget(parent)
    {
        FoundationNotificationBus::Handler::BusConnect();

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Source and Evidence Intake"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Every artifact is SHA-256 fingerprinted and bound to the active exact FoA profile before it enters the registry. Structured JSON and CSV may extract evidence; other inputs remain source-only until reviewed."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* profileGroup = new QGroupBox(tr("Active Intake Binding"), this);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_profileValue = new QLabel(profileGroup);
        m_buildValue = new QLabel(profileGroup);
        m_runtimeValue = new QLabel(profileGroup);
        profileLayout->addRow(tr("Profile"), m_profileValue);
        profileLayout->addRow(tr("Game build"), m_buildValue);
        profileLayout->addRow(tr("Runtime target"), m_runtimeValue);
        rootLayout->addWidget(profileGroup);

        auto* intakeGroup = new QGroupBox(tr("Import Artifact"), this);
        auto* intakeLayout = new QFormLayout(intakeGroup);

        auto* inputRow = new QWidget(intakeGroup);
        auto* inputLayout = new QHBoxLayout(inputRow);
        inputLayout->setContentsMargins(0, 0, 0, 0);
        m_inputPathEdit = new QLineEdit(inputRow);
        auto* browseButton = new QPushButton(tr("Browse…"), inputRow);
        inputLayout->addWidget(m_inputPathEdit, 1);
        inputLayout->addWidget(browseButton);
        intakeLayout->addRow(tr("Input file"), inputRow);

        m_sourceKindCombo = new QComboBox(intakeGroup);
        m_sourceKindCombo->addItems({
            "template-diagnostics",
            "item-recipe-dump",
            "scene-world-observation",
            "decompilation-note",
            "runtime-log",
            "screenshot",
            "csv-register",
            "json-register",
            "avalon-core-source-set"
        });
        m_importerCombo = new QComboBox(intakeGroup);
        m_titleEdit = new QLineEdit(intakeGroup);
        m_toolNameEdit = new QLineEdit(intakeGroup);
        m_toolVersionEdit = new QLineEdit(intakeGroup);
        m_capturedAtEdit = new QLineEdit(intakeGroup);
        m_capturedAtEdit->setText(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        m_limitationsEdit = new QPlainTextEdit(intakeGroup);
        m_limitationsEdit->setMaximumHeight(72);
        m_limitationsEdit->setPlaceholderText(tr("Known omissions, capture limitations, or interpretation boundaries"));

        intakeLayout->addRow(tr("Source kind"), m_sourceKindCombo);
        intakeLayout->addRow(tr("Importer contract"), m_importerCombo);
        intakeLayout->addRow(tr("Title"), m_titleEdit);
        intakeLayout->addRow(tr("Capture tool"), m_toolNameEdit);
        intakeLayout->addRow(tr("Tool version"), m_toolVersionEdit);
        intakeLayout->addRow(tr("Captured at"), m_capturedAtEdit);
        intakeLayout->addRow(tr("Limitations"), m_limitationsEdit);
        rootLayout->addWidget(intakeGroup);

        auto* actionLayout = new QHBoxLayout();
        auto* importButton = new QPushButton(tr("Import and Persist"), this);
        auto* reloadButton = new QPushButton(tr("Reload Workspace Sources"), this);
        actionLayout->addWidget(importButton);
        actionLayout->addWidget(reloadButton);
        actionLayout->addStretch(1);
        rootLayout->addLayout(actionLayout);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        rootLayout->addWidget(m_statusLabel);

        auto* splitter = new QSplitter(Qt::Vertical, this);

        auto* sourcesGroup = new QGroupBox(tr("Registered Sources"), splitter);
        auto* sourcesLayout = new QVBoxLayout(sourcesGroup);
        m_sourcesTable = new QTableWidget(0, 7, sourcesGroup);
        m_sourcesTable->setHorizontalHeaderLabels({
            tr("Source ID"), tr("Kind"), tr("Title"), tr("Fingerprint"), tr("Profile"), tr("Build"), tr("Status")
        });
        ConfigureTable(m_sourcesTable);
        sourcesLayout->addWidget(m_sourcesTable);
        splitter->addWidget(sourcesGroup);

        auto* evidenceGroup = new QGroupBox(tr("Extracted Evidence"), splitter);
        auto* evidenceLayout = new QVBoxLayout(evidenceGroup);
        m_evidenceTable = new QTableWidget(0, 5, evidenceGroup);
        m_evidenceTable->setHorizontalHeaderLabels({
            tr("Evidence ID"), tr("Source ID"), tr("Subject"), tr("Claim"), tr("Confidence")
        });
        ConfigureTable(m_evidenceTable);
        evidenceLayout->addWidget(m_evidenceTable);
        splitter->addWidget(evidenceGroup);

        auto* issuesGroup = new QGroupBox(tr("Import and Schema Issues"), splitter);
        auto* issuesLayout = new QVBoxLayout(issuesGroup);
        m_issuesTable = new QTableWidget(0, 4, issuesGroup);
        m_issuesTable->setHorizontalHeaderLabels({ tr("Severity"), tr("Code"), tr("Message"), tr("Locator") });
        ConfigureTable(m_issuesTable);
        issuesLayout->addWidget(m_issuesTable);
        splitter->addWidget(issuesGroup);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);
        splitter->setStretchFactor(2, 1);
        rootLayout->addWidget(splitter, 1);

        connect(browseButton, &QPushButton::clicked, this, [this]() { BrowseForSource(); });
        connect(importButton, &QPushButton::clicked, this, [this]() { ImportSource(); });
        connect(reloadButton, &QPushButton::clicked, this, [this]() { ReloadWorkspaceSources(); });
        connect(m_inputPathEdit, &QLineEdit::textChanged, this, [this](const QString& path)
        {
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (suffix == "json")
            {
                const int index = m_importerCombo->findData(QStringLiteral("tg.structured-json"));
                if (index >= 0)
                {
                    m_importerCombo->setCurrentIndex(index);
                }
            }
            else if (suffix == "csv")
            {
                const int index = m_importerCombo->findData(QStringLiteral("tg.structured-csv"));
                if (index >= 0)
                {
                    m_importerCombo->setCurrentIndex(index);
                }
            }
        });

        PopulateImporterContracts();
        Refresh();
    }

    SourceEvidenceIntakeWidget::~SourceEvidenceIntakeWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void SourceEvidenceIntakeWidget::OnFoundationChanged()
    {
        Refresh();
    }

    void SourceEvidenceIntakeWidget::Refresh()
    {
        const FoundationService& service = FoundationService::Get();
        const WorkspaceModel& workspace = service.GetWorkspace();
        if (const GameProfile* profile = workspace.FindActiveGameProfile())
        {
            m_profileValue->setText(ToQString(profile->m_displayName) + QStringLiteral(" [") + ToQString(profile->m_profileId) + ']');
            m_buildValue->setText(ToQString(profile->m_gameVersion) + QStringLiteral(" / ") + ToQString(profile->m_branch));
            m_runtimeValue->setText(ToQString(profile->m_runtimeTarget));
        }
        else
        {
            m_profileValue->setText(tr("Not configured"));
            m_buildValue->setText(tr("Unknown"));
            m_runtimeValue->setText(tr("Unknown"));
        }

        const SourceEvidenceRegistry& registry = service.GetSourceRegistry();
        const AZStd::vector<SourceRecord>& sources = registry.GetSources();
        m_sourcesTable->setRowCount(static_cast<int>(sources.size()));
        for (int row = 0; row < static_cast<int>(sources.size()); ++row)
        {
            const SourceRecord& source = sources[static_cast<size_t>(row)];
            SetCell(m_sourcesTable, row, 0, ToQString(source.m_sourceId));
            SetCell(m_sourcesTable, row, 1, ToQString(source.m_sourceKind));
            SetCell(m_sourcesTable, row, 2, ToQString(source.m_title));
            SetCell(m_sourcesTable, row, 3, ToQString(source.m_fingerprint));
            SetCell(m_sourcesTable, row, 4, ToQString(source.m_profileId));
            SetCell(m_sourcesTable, row, 5, ToQString(source.m_gameVersion) + QStringLiteral(" / ") + ToQString(source.m_branch));
            SetCell(m_sourcesTable, row, 6, ToQString(source.m_importStatus));
        }
        m_sourcesTable->resizeRowsToContents();

        const AZStd::vector<EvidenceRecord>& evidenceRecords = registry.GetEvidence();
        m_evidenceTable->setRowCount(static_cast<int>(evidenceRecords.size()));
        for (int row = 0; row < static_cast<int>(evidenceRecords.size()); ++row)
        {
            const EvidenceRecord& evidence = evidenceRecords[static_cast<size_t>(row)];
            SetCell(m_evidenceTable, row, 0, ToQString(evidence.m_evidenceId));
            SetCell(m_evidenceTable, row, 1, ToQString(evidence.m_sourceId));
            SetCell(m_evidenceTable, row, 2, ToQString(evidence.m_subjectRef));
            SetCell(m_evidenceTable, row, 3, ToQString(evidence.m_claim));
            SetCell(m_evidenceTable, row, 4, ToQString(evidence.m_confidence));
        }
        m_evidenceTable->resizeRowsToContents();

        const AZStd::vector<ImportIssue>& issues = service.GetImportIssues();
        m_issuesTable->setRowCount(static_cast<int>(issues.size()));
        for (int row = 0; row < static_cast<int>(issues.size()); ++row)
        {
            const ImportIssue& issue = issues[static_cast<size_t>(row)];
            SetCell(m_issuesTable, row, 0, ToQString(issue.m_severity));
            SetCell(m_issuesTable, row, 1, ToQString(issue.m_code));
            SetCell(m_issuesTable, row, 2, ToQString(issue.m_message));
            SetCell(m_issuesTable, row, 3, ToQString(issue.m_locator));
        }
        m_issuesTable->resizeRowsToContents();
    }

    void SourceEvidenceIntakeWidget::BrowseForSource()
    {
        const WorkspaceModel& workspace = FoundationService::Get().GetWorkspace();
        QString startDirectory;
        if (const GameProfile* profile = workspace.FindActiveGameProfile())
        {
            startDirectory = ToQString(
                profile->m_diagnosticsPath.empty() ? profile->m_extractedDataPath : profile->m_diagnosticsPath);
        }

        const QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("Select TG source artifact"),
            startDirectory,
            tr("Supported inputs (*.json *.csv *.log *.txt *.md *.png *.jpg *.jpeg);;All files (*)"));
        if (filePath.isEmpty())
        {
            return;
        }
        m_inputPathEdit->setText(filePath);
        if (m_titleEdit->text().trimmed().isEmpty())
        {
            m_titleEdit->setText(QFileInfo(filePath).fileName());
        }
    }

    void SourceEvidenceIntakeWidget::ImportSource()
    {
        SourceImportRequest request;
        request.m_inputPath = ToAzString(m_inputPathEdit->text());
        request.m_sourceKind = ToAzString(m_sourceKindCombo->currentText());
        request.m_title = ToAzString(m_titleEdit->text());
        request.m_toolName = ToAzString(m_toolNameEdit->text());
        request.m_toolVersion = ToAzString(m_toolVersionEdit->text());
        request.m_capturedAt = ToAzString(m_capturedAtEdit->text());
        request.m_limitations = ToAzString(m_limitationsEdit->toPlainText());
        request.m_preferredImporterId = ToAzString(m_importerCombo->currentData().toString());

        SourceEvidenceDocumentPaths paths;
        AZStd::string error;
        if (!FoundationService::Get().ImportSource(request, &paths, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }

        SetStatus(
            tr("Source and evidence documents saved:\n%1\n%2")
                .arg(ToQString(paths.m_sourceDocumentPath), ToQString(paths.m_evidenceDocumentPath)));
        m_capturedAtEdit->setText(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }

    void SourceEvidenceIntakeWidget::ReloadWorkspaceSources()
    {
        AZStd::string error;
        if (!FoundationService::Get().ReloadSourceEvidence(&error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Workspace source and evidence documents reloaded."));
    }

    void SourceEvidenceIntakeWidget::PopulateImporterContracts()
    {
        m_importerCombo->clear();
        const AZStd::vector<SourceImporterContract> contracts = FoundationService::Get().GetSourceImporterContracts();
        for (const SourceImporterContract& contract : contracts)
        {
            m_importerCombo->addItem(ToQString(contract.m_displayName), ToQString(contract.m_importerId));
        }
    }

    void SourceEvidenceIntakeWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }
} // namespace TaintedGrailModdingSDK
