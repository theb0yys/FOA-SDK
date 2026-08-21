/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "QuestStateInspectorWidget.h"

#include <AzCore/std/string/string.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr qint64 MaximumQuestDocumentBytes = 4 * 1024 * 1024;

        AZStd::string ToAzString(const QByteArray& value)
        {
            return AZStd::string(value.constData(), static_cast<size_t>(value.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        QString BoolText(bool value)
        {
            return value ? QStringLiteral("true") : QStringLiteral("false");
        }

        QString SeverityText(QuestDefinitionIssueSeverityV1 severity)
        {
            return severity == QuestDefinitionIssueSeverityV1::Blocker
                ? QStringLiteral("blocker")
                : QStringLiteral("error");
        }

        void ConfigureValueLabel(QLabel* label)
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
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

        QWidget* CreatePathRow(
            QWidget* parent,
            QLineEdit* pathEdit,
            QPushButton* browseButton,
            QPushButton* loadButton)
        {
            auto* row = new QWidget(parent);
            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(pathEdit, 1);
            layout->addWidget(browseButton);
            layout->addWidget(loadButton);
            return row;
        }
    } // namespace

    QuestStateInspectorWidget::QuestStateInspectorWidget(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumSize(720, 680);

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Quest and State Inspector"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Load and inspect local QuestDefinition V1 documents. This pane validates shape, references, fingerprint state, and disabled authority flags; it does not write files, mutate editor state, execute quests, deploy content, or touch FoA saves."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* inputGroup = new QGroupBox(tr("QuestDefinition input"), this);
        auto* inputLayout = new QFormLayout(inputGroup);
        m_questPathEdit = new QLineEdit(inputGroup);
        m_questPathEdit->setClearButtonEnabled(true);
        m_questPathEdit->setPlaceholderText(tr("Select a *.tgquest.json file"));
        auto* browseButton = new QPushButton(tr("Browse..."), inputGroup);
        auto* loadButton = new QPushButton(tr("Load"), inputGroup);
        inputLayout->addRow(
            tr("Quest document"),
            CreatePathRow(inputGroup, m_questPathEdit, browseButton, loadButton));
        rootLayout->addWidget(inputGroup);

        auto* summaryGroup = new QGroupBox(tr("Inspection summary"), this);
        auto* summaryLayout = new QFormLayout(summaryGroup);
        m_summary = new QLabel(summaryGroup);
        ConfigureValueLabel(m_summary);
        summaryLayout->addRow(tr("Quest"), m_summary);
        rootLayout->addWidget(summaryGroup);

        auto* issueGroup = new QGroupBox(tr("Validation issues"), this);
        auto* issueLayout = new QVBoxLayout(issueGroup);
        m_issueTable = new QTableWidget(0, 4, issueGroup);
        m_issueTable->setHorizontalHeaderLabels({
            tr("Severity"),
            tr("Code"),
            tr("Subject"),
            tr("Property"),
        });
        ConfigureTable(m_issueTable);
        issueLayout->addWidget(m_issueTable);
        rootLayout->addWidget(issueGroup, 1);

        auto* bindingGroup = new QGroupBox(tr("Binding requirements"), this);
        auto* bindingLayout = new QVBoxLayout(bindingGroup);
        m_bindingTable = new QTableWidget(0, 4, bindingGroup);
        m_bindingTable->setHorizontalHeaderLabels({
            tr("Requirement"),
            tr("Role"),
            tr("Subject kind"),
            tr("Usage"),
        });
        ConfigureTable(m_bindingTable);
        bindingLayout->addWidget(m_bindingTable);
        rootLayout->addWidget(bindingGroup, 1);

        m_status = new QLabel(this);
        ConfigureValueLabel(m_status);
        rootLayout->addWidget(m_status);

        connect(browseButton, &QPushButton::clicked, this, [this]() { BrowseForQuestDefinition(); });
        connect(loadButton, &QPushButton::clicked, this, [this]() { LoadQuestDefinition(); });
        connect(m_questPathEdit, &QLineEdit::returnPressed, this, [this]() { LoadQuestDefinition(); });

        ClearInspection();
    }

    void QuestStateInspectorWidget::BrowseForQuestDefinition()
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Open QuestDefinition"),
            QString(),
            tr("QuestDefinition (*.tgquest.json);;JSON files (*.json);;All files (*)"));
        if (!path.isEmpty())
        {
            m_questPathEdit->setText(path);
            LoadQuestDefinition();
        }
    }

    void QuestStateInspectorWidget::LoadQuestDefinition()
    {
        ClearInspection();
        const QString path = m_questPathEdit->text().trimmed();
        if (path.isEmpty())
        {
            SetStatus(tr("Select a QuestDefinition document to inspect."), true);
            return;
        }

        const QFileInfo info(path);
        if (!info.isFile())
        {
            SetStatus(tr("QuestDefinition path does not resolve to a file."), true);
            return;
        }
        if (info.size() < 0 || info.size() > MaximumQuestDocumentBytes)
        {
            SetStatus(tr("QuestDefinition document is outside the bounded inspection size."), true);
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            SetStatus(tr("QuestDefinition document could not be opened for read-only inspection."), true);
            return;
        }

        const QByteArray bytes = file.read(MaximumQuestDocumentBytes + 1);
        if (bytes.size() > MaximumQuestDocumentBytes)
        {
            SetStatus(tr("QuestDefinition document exceeded the bounded inspection size."), true);
            return;
        }

        QuestDefinitionV1 definition;
        const AZStd::string json = ToAzString(bytes);
        const QuestDefinitionValidationResultV1 result =
            ParseQuestDefinitionJsonV1(json, definition);
        PopulateSummary(definition, result);
        PopulateIssues(result);
        PopulateBindingRequirements(definition);

        if (result.IsValid())
        {
            SetStatus(tr("QuestDefinition inspected successfully. Runtime, editor mutation, save mutation, deployment, and asset extraction authority remain disabled."));
        }
        else
        {
            SetStatus(tr("QuestDefinition inspection found blocking or invalid contract issues."), true);
        }
    }

    void QuestStateInspectorWidget::ClearInspection()
    {
        m_summary->setText(tr("No QuestDefinition loaded."));
        m_issueTable->setRowCount(0);
        m_bindingTable->setRowCount(0);
        SetStatus(tr("No QuestDefinition inspection has run."));
    }

    void QuestStateInspectorWidget::PopulateSummary(
        const QuestDefinitionV1& definition,
        const QuestDefinitionValidationResultV1& result)
    {
        const QString fingerprint = definition.m_questFingerprint.empty()
            ? tr("not declared")
            : ToQString(definition.m_questFingerprint);
        const QString calculated = definition.m_questId.empty()
            ? tr("unavailable")
            : ToQString(CalculateQuestDefinitionFingerprintV1(definition));
        m_summary->setText(
            tr("id=%1 | owner pack=%2 | module=%3 | lifecycle=%4 | valid=%5 | blocked=%6 | declared fingerprint=%7 | calculated fingerprint=%8 | roles=%9 phases=%10 objectives=%11 transitions=%12 conditions=%13 actions=%14 outcomes=%15")
                .arg(definition.m_questId.empty() ? tr("unavailable") : ToQString(definition.m_questId))
                .arg(definition.m_ownerPackId.empty() ? tr("unavailable") : ToQString(definition.m_ownerPackId))
                .arg(definition.m_ownerModuleId.empty() ? tr("unavailable") : ToQString(definition.m_ownerModuleId))
                .arg(definition.m_lifecycle.empty() ? tr("unavailable") : ToQString(definition.m_lifecycle))
                .arg(BoolText(result.IsValid()))
                .arg(BoolText(result.IsBlocked()))
                .arg(fingerprint)
                .arg(calculated)
                .arg(static_cast<qulonglong>(definition.m_roles.size()))
                .arg(static_cast<qulonglong>(definition.m_phases.size()))
                .arg(static_cast<qulonglong>(definition.m_objectives.size()))
                .arg(static_cast<qulonglong>(definition.m_transitions.size()))
                .arg(static_cast<qulonglong>(definition.m_conditions.size()))
                .arg(static_cast<qulonglong>(definition.m_actions.size()))
                .arg(static_cast<qulonglong>(definition.m_outcomes.size())));
    }

    void QuestStateInspectorWidget::PopulateIssues(const QuestDefinitionValidationResultV1& result)
    {
        m_issueTable->setRowCount(static_cast<int>(result.m_issues.size()));
        for (int row = 0; row < static_cast<int>(result.m_issues.size()); ++row)
        {
            const QuestDefinitionIssueV1& issue = result.m_issues[static_cast<size_t>(row)];
            SetCell(m_issueTable, row, 0, SeverityText(issue.m_severity));
            SetCell(m_issueTable, row, 1, ToQString(issue.m_code));
            SetCell(m_issueTable, row, 2, ToQString(issue.m_subjectId));
            SetCell(m_issueTable, row, 3, ToQString(issue.m_propertyPath));
        }
    }

    void QuestStateInspectorWidget::PopulateBindingRequirements(const QuestDefinitionV1& definition)
    {
        m_bindingTable->setRowCount(static_cast<int>(definition.m_bindingRequirements.size()));
        for (int row = 0; row < static_cast<int>(definition.m_bindingRequirements.size()); ++row)
        {
            const QuestDefinitionBindingRequirementV1& requirement =
                definition.m_bindingRequirements[static_cast<size_t>(row)];
            SetCell(m_bindingTable, row, 0, ToQString(requirement.m_requirementId));
            SetCell(m_bindingTable, row, 1, ToQString(requirement.m_roleId));
            SetCell(m_bindingTable, row, 2, ToQString(requirement.m_subjectKind));
            SetCell(m_bindingTable, row, 3, ToQString(requirement.m_usage));
        }
    }

    void QuestStateInspectorWidget::SetStatus(const QString& message, bool error)
    {
        m_status->setText(message);
        m_status->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString());
    }
} // namespace TaintedGrailModdingSDK
