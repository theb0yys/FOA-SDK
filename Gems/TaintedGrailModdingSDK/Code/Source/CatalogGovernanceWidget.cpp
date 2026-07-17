/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

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

        AZStd::vector<AZStd::string> ParseCommaSeparated(const QString& value)
        {
            AZStd::vector<AZStd::string> values;
            for (const QString& part : value.split(',', Qt::SkipEmptyParts))
            {
                const QString trimmed = part.trimmed();
                if (!trimmed.isEmpty())
                {
                    const AZStd::string converted = ToAzString(trimmed);
                    if (AZStd::find(values.begin(), values.end(), converted) == values.end())
                    {
                        values.push_back(converted);
                    }
                }
            }
            return values;
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList items;
            for (const AZStd::string& value : values)
            {
                items.push_back(ToQString(value));
            }
            return items.join(QStringLiteral(", "));
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->setMinimumHeight(150);
        }

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }
    } // namespace

    CatalogGovernanceWidget::CatalogGovernanceWidget(QWidget* parent)
        : QWidget(parent)
    {
        FoundationNotificationBus::Handler::BusConnect();

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Catalog Governance"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* boundary = new QLabel(
            tr("Maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession are independent reviewed decisions. Permission never follows automatically from evidence or validation."),
            this);
        boundary->setWordWrap(true);
        rootLayout->addWidget(boundary);

        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        auto* content = new QWidget(scrollArea);
        auto* contentLayout = new QVBoxLayout(content);

        auto* subjectGroup = new QGroupBox(tr("Catalog Subject"), content);
        auto* subjectLayout = new QFormLayout(subjectGroup);
        m_subjectKind = new QComboBox(subjectGroup);
        m_subjectKind->addItems({ QStringLiteral("record"), QStringLiteral("relationship") });
        m_subjectId = new QComboBox(subjectGroup);
        subjectLayout->addRow(tr("Subject kind"), m_subjectKind);
        subjectLayout->addRow(tr("Subject ID"), m_subjectId);
        contentLayout->addWidget(subjectGroup);

        auto* stateGroup = new QGroupBox(tr("Current Independent States"), content);
        auto* stateLayout = new QFormLayout(stateGroup);
        m_identityValue = new QLabel(stateGroup);
        m_maturityValue = new QLabel(stateGroup);
        m_confidenceValue = new QLabel(stateGroup);
        m_riskValue = new QLabel(stateGroup);
        m_validationValue = new QLabel(stateGroup);
        m_stalenessValue = new QLabel(stateGroup);
        m_allowedValue = new QLabel(stateGroup);
        m_forbiddenValue = new QLabel(stateGroup);
        m_supersessionValue = new QLabel(stateGroup);
        for (QLabel* label : { m_identityValue, m_allowedValue, m_forbiddenValue, m_supersessionValue })
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
        stateLayout->addRow(tr("Identity / relationship"), m_identityValue);
        stateLayout->addRow(tr("Maturity"), m_maturityValue);
        stateLayout->addRow(tr("Confidence"), m_confidenceValue);
        stateLayout->addRow(tr("Operational risk"), m_riskValue);
        stateLayout->addRow(tr("Validation"), m_validationValue);
        stateLayout->addRow(tr("Staleness"), m_stalenessValue);
        stateLayout->addRow(tr("Allowed usages"), m_allowedValue);
        stateLayout->addRow(tr("Prohibitions"), m_forbiddenValue);
        stateLayout->addRow(tr("Superseded by"), m_supersessionValue);
        contentLayout->addWidget(stateGroup);

        auto* decisionGroup = new QGroupBox(tr("Governance Decision"), content);
        auto* decisionLayout = new QFormLayout(decisionGroup);
        m_axis = new QComboBox(decisionGroup);
        m_axis->addItems({
            QStringLiteral("maturity"),
            QStringLiteral("confidence"),
            QStringLiteral("operational_risk"),
            QStringLiteral("staleness"),
            QStringLiteral("permission"),
            QStringLiteral("supersession")
        });
        m_axisValue = new QComboBox(decisionGroup);
        m_usage = new QLineEdit(decisionGroup);
        m_usage->setPlaceholderText(tr("Named usage, for example display_only or spawn_static"));
        m_evidenceIds = new QLineEdit(decisionGroup);
        m_evidenceIds->setPlaceholderText(tr("Comma-separated evidence IDs"));
        m_validationIds = new QLineEdit(decisionGroup);
        m_validationIds->setPlaceholderText(tr("Required when allowing usage"));
        m_reviewer = new QLineEdit(decisionGroup);
        m_notes = new QPlainTextEdit(decisionGroup);
        m_notes->setMaximumHeight(75);
        auto* applyDecision = new QPushButton(tr("Apply Reviewed Decision"), decisionGroup);
        decisionLayout->addRow(tr("Axis"), m_axis);
        decisionLayout->addRow(tr("New value / outcome"), m_axisValue);
        decisionLayout->addRow(tr("Usage"), m_usage);
        decisionLayout->addRow(tr("Evidence IDs"), m_evidenceIds);
        decisionLayout->addRow(tr("Validation proof IDs"), m_validationIds);
        decisionLayout->addRow(tr("Reviewer"), m_reviewer);
        decisionLayout->addRow(tr("Notes"), m_notes);
        decisionLayout->addRow(applyDecision);
        contentLayout->addWidget(decisionGroup);

        auto* validationGroup = new QGroupBox(tr("Validation Decision"), content);
        auto* validationLayout = new QFormLayout(validationGroup);
        m_validationState = new QComboBox(validationGroup);
        m_validationState->addItems({
            QStringLiteral("pending"),
            QStringLiteral("validated"),
            QStringLiteral("failed"),
            QStringLiteral("stale"),
            QStringLiteral("blocked"),
            QStringLiteral("unvalidated")
        });
        m_validationMethod = new QLineEdit(validationGroup);
        m_validationMethod->setPlaceholderText(tr("Method or test protocol"));
        m_validationEvidenceIds = new QLineEdit(validationGroup);
        m_validationEvidenceIds->setPlaceholderText(tr("Comma-separated evidence IDs"));
        m_validator = new QLineEdit(validationGroup);
        m_validationNotes = new QPlainTextEdit(validationGroup);
        m_validationNotes->setMaximumHeight(75);
        auto* applyValidation = new QPushButton(tr("Record Validation Decision"), validationGroup);
        validationLayout->addRow(tr("Validation state"), m_validationState);
        validationLayout->addRow(tr("Method"), m_validationMethod);
        validationLayout->addRow(tr("Evidence IDs"), m_validationEvidenceIds);
        validationLayout->addRow(tr("Validator"), m_validator);
        validationLayout->addRow(tr("Notes"), m_validationNotes);
        validationLayout->addRow(applyValidation);
        contentLayout->addWidget(validationGroup);

        auto* governanceHistoryGroup = new QGroupBox(tr("Governance History"), content);
        auto* governanceHistoryLayout = new QVBoxLayout(governanceHistoryGroup);
        m_governanceHistory = new QTableWidget(0, 6, governanceHistoryGroup);
        m_governanceHistory->setHorizontalHeaderLabels({
            tr("Time"), tr("Axis"), tr("Previous"), tr("New"), tr("Usage"), tr("Reviewer")
        });
        ConfigureTable(m_governanceHistory);
        governanceHistoryLayout->addWidget(m_governanceHistory);
        contentLayout->addWidget(governanceHistoryGroup);

        auto* validationHistoryGroup = new QGroupBox(tr("Validation History"), content);
        auto* validationHistoryLayout = new QVBoxLayout(validationHistoryGroup);
        m_validationHistory = new QTableWidget(0, 5, validationHistoryGroup);
        m_validationHistory->setHorizontalHeaderLabels({
            tr("Time"), tr("State"), tr("Method"), tr("Validator"), tr("Evidence")
        });
        ConfigureTable(m_validationHistory);
        validationHistoryLayout->addWidget(m_validationHistory);
        contentLayout->addWidget(validationHistoryGroup);

        m_status = new QLabel(content);
        m_status->setWordWrap(true);
        contentLayout->addWidget(m_status);
        contentLayout->addStretch(1);

        scrollArea->setWidget(content);
        rootLayout->addWidget(scrollArea, 1);

        connect(m_subjectKind, &QComboBox::currentTextChanged, this, [this]() { RefreshSubjects(); });
        connect(m_subjectId, &QComboBox::currentTextChanged, this, [this]() { RefreshCurrentSubject(); });
        connect(m_axis, &QComboBox::currentTextChanged, this, [this]() { RefreshAxisValues(); });
        connect(applyDecision, &QPushButton::clicked, this, [this]() { ApplyGovernanceDecision(); });
        connect(applyValidation, &QPushButton::clicked, this, [this]() { ApplyValidationDecision(); });

        RefreshAxisValues();
        RefreshSubjects();
    }

    CatalogGovernanceWidget::~CatalogGovernanceWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void CatalogGovernanceWidget::OnFoundationChanged()
    {
        RefreshSubjects();
    }

    void CatalogGovernanceWidget::RefreshSubjects()
    {
        if (m_refreshing)
        {
            return;
        }
        m_refreshing = true;
        const QSignalBlocker blocker(m_subjectId);
        const QString previous = m_subjectId->currentText();
        m_subjectId->clear();

        QStringList subjects;
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        if (m_subjectKind->currentText() == QStringLiteral("record"))
        {
            for (const CatalogRecord& record : catalog.GetRecords())
            {
                subjects.push_back(ToQString(record.m_recordId));
            }
        }
        else
        {
            for (const CatalogRelationship& relationship : catalog.GetRelationships())
            {
                subjects.push_back(ToQString(relationship.m_relationshipId));
            }
        }
        subjects.sort(Qt::CaseSensitive);
        m_subjectId->addItems(subjects);
        const int previousIndex = m_subjectId->findText(previous);
        if (previousIndex >= 0)
        {
            m_subjectId->setCurrentIndex(previousIndex);
        }
        m_refreshing = false;
        RefreshAxisValues();
        RefreshCurrentSubject();
    }

    void CatalogGovernanceWidget::RefreshCurrentSubject()
    {
        if (m_refreshing)
        {
            return;
        }

        const AZStd::string subjectKind = ToAzString(m_subjectKind->currentText());
        const AZStd::string subjectId = ToAzString(m_subjectId->currentText());
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();

        AZStd::string maturity;
        AZStd::string confidence;
        AZStd::string risk;
        AZStd::string validation;
        AZStd::string staleness;
        AZStd::vector<AZStd::string> allowed;
        AZStd::vector<AZStd::string> forbidden;
        AZStd::string supersession;
        QString identity;

        if (subjectKind == "record")
        {
            if (const CatalogRecord* record = catalog.FindByRecordId(subjectId))
            {
                identity = tr("%1 · %2 · %3")
                    .arg(ToQString(record->m_identityKind), ToQString(record->m_domain), ToQString(record->m_subjectRef));
                maturity = record->m_researchStage;
                confidence = record->m_confidence;
                risk = record->m_operationalRisk;
                validation = record->m_validationState;
                staleness = record->m_stalenessState;
                allowed = record->m_allowedUsages;
                forbidden = record->m_forbiddenUsages;
                supersession = record->m_supersededByRecordId;
            }
        }
        else if (const CatalogRelationship* relationship = catalog.FindRelationshipById(subjectId))
        {
            identity = tr("%1 → %2 · %3")
                .arg(
                    ToQString(relationship->m_fromRecordId),
                    relationship->m_toRecordId.empty()
                        ? ToQString(relationship->m_targetSubjectRef)
                        : ToQString(relationship->m_toRecordId),
                    ToQString(relationship->m_relationshipKind));
            maturity = relationship->m_researchStage;
            confidence = relationship->m_confidence;
            risk = relationship->m_operationalRisk;
            validation = relationship->m_validationState;
            staleness = relationship->m_stalenessState;
            allowed = relationship->m_allowedUsages;
            forbidden = relationship->m_forbiddenUsages;
            supersession = relationship->m_supersededByRelationshipId;
        }

        m_identityValue->setText(identity.isEmpty() ? tr("No subject selected") : identity);
        m_maturityValue->setText(maturity.empty() ? tr("unknown") : ToQString(maturity));
        m_confidenceValue->setText(confidence.empty() ? tr("unknown") : ToQString(confidence));
        m_riskValue->setText(risk.empty() ? tr("unknown") : ToQString(risk));
        m_validationValue->setText(validation.empty() ? tr("unvalidated") : ToQString(validation));
        m_stalenessValue->setText(staleness.empty() ? tr("unknown") : ToQString(staleness));
        m_allowedValue->setText(allowed.empty() ? tr("None") : JoinValues(allowed));
        m_forbiddenValue->setText(forbidden.empty() ? tr("None") : JoinValues(forbidden));
        m_supersessionValue->setText(supersession.empty() ? tr("Not superseded") : ToQString(supersession));

        const AZStd::vector<CatalogGovernanceEvent> governance = catalog.FindGovernanceForSubject(subjectKind, subjectId);
        m_governanceHistory->setRowCount(static_cast<int>(governance.size()));
        for (int row = 0; row < static_cast<int>(governance.size()); ++row)
        {
            const CatalogGovernanceEvent& event = governance[static_cast<size_t>(row)];
            SetCell(m_governanceHistory, row, 0, ToQString(event.m_decidedAt));
            SetCell(m_governanceHistory, row, 1, ToQString(event.m_axis));
            SetCell(m_governanceHistory, row, 2, ToQString(event.m_previousValue));
            SetCell(m_governanceHistory, row, 3, ToQString(event.m_newValue));
            SetCell(m_governanceHistory, row, 4, ToQString(event.m_usage));
            SetCell(m_governanceHistory, row, 5, ToQString(event.m_reviewer));
        }
        m_governanceHistory->resizeRowsToContents();

        const AZStd::vector<CatalogValidationEvent> validations = catalog.FindValidationForSubject(subjectKind, subjectId);
        m_validationHistory->setRowCount(static_cast<int>(validations.size()));
        for (int row = 0; row < static_cast<int>(validations.size()); ++row)
        {
            const CatalogValidationEvent& event = validations[static_cast<size_t>(row)];
            SetCell(m_validationHistory, row, 0, ToQString(event.m_checkedAt));
            SetCell(m_validationHistory, row, 1, ToQString(event.m_state));
            SetCell(m_validationHistory, row, 2, ToQString(event.m_method));
            SetCell(m_validationHistory, row, 3, ToQString(event.m_validator));
            SetCell(m_validationHistory, row, 4, JoinValues(event.m_evidenceIds));
        }
        m_validationHistory->resizeRowsToContents();
    }

    void CatalogGovernanceWidget::RefreshAxisValues()
    {
        const QSignalBlocker blocker(m_axisValue);
        m_axisValue->clear();
        m_axisValue->setEditable(false);
        m_usage->setEnabled(false);
        m_validationIds->setEnabled(false);

        const QString axis = m_axis->currentText();
        if (axis == QStringLiteral("maturity"))
        {
            m_axisValue->addItems({ "S0", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8" });
        }
        else if (axis == QStringLiteral("confidence"))
        {
            m_axisValue->addItems({ "unknown", "hypothesis", "inferred", "documented", "runtime_observed", "validated" });
        }
        else if (axis == QStringLiteral("operational_risk"))
        {
            m_axisValue->addItems({ "unknown", "low", "medium", "high", "critical" });
        }
        else if (axis == QStringLiteral("staleness"))
        {
            m_axisValue->addItems({ "unknown", "current", "potentially_stale", "stale" });
        }
        else if (axis == QStringLiteral("permission"))
        {
            m_axisValue->addItems({ "allow", "forbid", "clear" });
            m_usage->setEnabled(true);
            m_validationIds->setEnabled(true);
        }
        else
        {
            m_axisValue->setEditable(true);
            const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
            QStringList replacements;
            if (m_subjectKind->currentText() == QStringLiteral("record"))
            {
                for (const CatalogRecord& record : catalog.GetRecords())
                {
                    replacements.push_back(ToQString(record.m_recordId));
                }
            }
            else
            {
                for (const CatalogRelationship& relationship : catalog.GetRelationships())
                {
                    replacements.push_back(ToQString(relationship.m_relationshipId));
                }
            }
            replacements.sort(Qt::CaseSensitive);
            m_axisValue->addItems(replacements);
        }
    }

    void CatalogGovernanceWidget::ApplyGovernanceDecision()
    {
        CatalogGovernanceRequest request;
        request.m_subjectKind = ToAzString(m_subjectKind->currentText());
        request.m_subjectId = ToAzString(m_subjectId->currentText());
        request.m_axis = ToAzString(m_axis->currentText());
        request.m_value = ToAzString(m_axisValue->currentText());
        request.m_usage = ToAzString(m_usage->text());
        request.m_evidenceIds = ParseCommaSeparated(m_evidenceIds->text());
        request.m_validationIds = ParseCommaSeparated(m_validationIds->text());
        request.m_reviewer = ToAzString(m_reviewer->text());
        request.m_notes = ToAzString(m_notes->toPlainText());

        AZStd::string error;
        if (!FoundationService::Get().ApplyCatalogGovernanceDecision(request, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Governance decision saved to the canonical catalog."));
    }

    void CatalogGovernanceWidget::ApplyValidationDecision()
    {
        CatalogValidationRequest request;
        request.m_subjectKind = ToAzString(m_subjectKind->currentText());
        request.m_subjectId = ToAzString(m_subjectId->currentText());
        request.m_state = ToAzString(m_validationState->currentText());
        request.m_method = ToAzString(m_validationMethod->text());
        request.m_evidenceIds = ParseCommaSeparated(m_validationEvidenceIds->text());
        request.m_validator = ToAzString(m_validator->text());
        request.m_notes = ToAzString(m_validationNotes->toPlainText());

        AZStd::string error;
        if (!FoundationService::Get().ApplyCatalogValidationDecision(request, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Validation decision saved. No usage permission was granted automatically."));
    }

    void CatalogGovernanceWidget::SetStatus(const QString& message, bool error)
    {
        m_status->setText(message);
        m_status->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }
} // namespace TaintedGrailModdingSDK
