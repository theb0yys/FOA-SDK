/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopEditorWidget.h"

#include "FoundationService.h"
#include "PopulationActionLaneService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <cmath>
#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(
                utf8.constData(),
                static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        AZStd::vector<AZStd::string> ParseCommaSeparated(const QString& value)
        {
            AZStd::vector<AZStd::string> values;
            for (const QString& part : value.split(',', Qt::SkipEmptyParts))
            {
                const AZStd::string converted = ToAzString(part);
                if (!converted.empty())
                {
                    // Preserve duplicate user input so Core can reject it with
                    // the authoritative validation error rather than silently
                    // changing the draft.
                    values.push_back(converted);
                }
            }
            return values;
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(QStringLiteral(", "));
        }

        QString ValueOrNone(const AZStd::vector<AZStd::string>& values)
        {
            return values.empty() ? QObject::tr("None") : JoinValues(values);
        }

        QString RecordLabel(const CatalogRecord& record)
        {
            QString label = ToQString(record.m_recordId);
            if (!record.m_displayName.empty())
            {
                label += QStringLiteral(" - ")
                    + ToQString(record.m_displayName);
            }
            return label;
        }

        bool MatchesFilter(const CatalogRecord& record, const QString& filter)
        {
            if (filter.trimmed().isEmpty())
            {
                return true;
            }
            const QString needle = filter.trimmed();
            return ToQString(record.m_recordId).contains(
                       needle,
                       Qt::CaseInsensitive)
                || ToQString(record.m_displayName).contains(
                       needle,
                       Qt::CaseInsensitive)
                || ToQString(record.m_subjectRef).contains(
                       needle,
                       Qt::CaseInsensitive);
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->setAlternatingRowColors(true);
        }

        void SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QLabel* AddFormRow(
            QFormLayout* form,
            const QString& text,
            QWidget* control,
            const QString& accessibleDescription = {})
        {
            auto* label = new QLabel(text, control->parentWidget());
            label->setBuddy(control);
            control->setAccessibleName(text);
            if (!accessibleDescription.isEmpty())
            {
                control->setAccessibleDescription(accessibleDescription);
            }
            form->addRow(label, control);
            return label;
        }

        QPlainTextEdit* CreateSummary(
            const QString& accessibleName,
            QWidget* parent)
        {
            auto* summary = new QPlainTextEdit(parent);
            summary->setReadOnly(true);
            summary->setMinimumHeight(120);
            summary->setAccessibleName(accessibleName);
            summary->setTabChangesFocus(true);
            return summary;
        }

        QWidget* WrapScrollable(QWidget* content, QWidget* parent)
        {
            auto* scroll = new QScrollArea(parent);
            scroll->setWidgetResizable(true);
            scroll->setWidget(content);
            return scroll;
        }

        AZStd::vector<AZStd::string> SelectedEvidenceIds(
            const QListWidget* selector)
        {
            AZStd::vector<AZStd::string> result;
            for (const QListWidgetItem* item : selector->selectedItems())
            {
                AddUnique(result, ToAzString(item->data(Qt::UserRole).toString()));
            }
            AZStd::sort(result.begin(), result.end());
            return result;
        }

        void PopulateEvidenceSelector(
            QListWidget* selector,
            const AZStd::vector<AZStd::string>& allowedSubjects,
            const AZStd::vector<AZStd::string>& selectedEvidenceIds,
            const SourceEvidenceRegistry& registry)
        {
            const QSignalBlocker blocker(selector);
            selector->clear();
            for (const EvidenceRecord& evidence : registry.GetEvidence())
            {
                const bool subjectMatches = Contains(
                    allowedSubjects,
                    evidence.m_subjectRef);
                const bool selected = Contains(
                    selectedEvidenceIds,
                    evidence.m_evidenceId);
                if (!subjectMatches && !selected)
                {
                    continue;
                }

                QString label = QStringLiteral("%1 | %2 | %3 | %4")
                    .arg(ToQString(evidence.m_evidenceId))
                    .arg(ToQString(evidence.m_subjectRef))
                    .arg(ToQString(evidence.m_evidenceKind))
                    .arg(ToQString(evidence.m_confidence));
                if (!subjectMatches)
                {
                    label += QObject::tr(" | selected subject mismatch");
                }
                auto* item = new QListWidgetItem(label, selector);
                item->setData(Qt::UserRole, ToQString(evidence.m_evidenceId));
                item->setToolTip(
                    QObject::tr("Claim: %1\nSource: %2\nLocator: %3")
                        .arg(ToQString(evidence.m_claim))
                        .arg(ToQString(evidence.m_sourceId))
                        .arg(ToQString(evidence.m_locator)));
                item->setSelected(selected);
            }
            if (selector->count() == 0)
            {
                auto* emptyItem = new QListWidgetItem(
                    QObject::tr(
                        "No evidence for the current exact subject bindings."),
                    selector);
                emptyItem->setFlags(Qt::NoItemFlags);
            }
        }

        AZStd::vector<AZStd::string> RecordSubjects(
            const CatalogRecord* primary,
            const CatalogRecord* referenced,
            const AZStd::string& unresolvedSubject = {})
        {
            AZStd::vector<AZStd::string> subjects;
            if (primary)
            {
                AddUnique(subjects, primary->m_subjectRef);
            }
            if (referenced)
            {
                AddUnique(subjects, referenced->m_subjectRef);
            }
            AddUnique(subjects, unresolvedSubject);
            return subjects;
        }

        QString IdentitySummary(const CatalogRecord& record)
        {
            return QObject::tr(
                       "Record: %1 | subject: %2 | identity: %3 | owner: %4 | "
                       "validation: %5 | staleness: %6 | record evidence: %7")
                .arg(ToQString(record.m_recordId))
                .arg(ToQString(record.m_subjectRef))
                .arg(ToQString(record.m_identityKind))
                .arg(
                    record.m_ownerPackId.empty()
                        ? QObject::tr("native / no pack owner")
                        : ToQString(record.m_ownerPackId))
                .arg(ToQString(record.m_validationState))
                .arg(ToQString(record.m_stalenessState))
                .arg(ValueOrNone(record.m_evidenceIds));
        }

        void FillReviewContext(
            const CatalogRecord* record,
            QPlainTextEdit* validationSummary,
            QPlainTextEdit* governanceSummary,
            QPlainTextEdit* blockerSummary,
            QPlainTextEdit* relationshipSummary)
        {
            if (!record)
            {
                const QString empty = QObject::tr(
                    "No canonical population record is selected.");
                validationSummary->setPlainText(empty);
                governanceSummary->setPlainText(empty);
                blockerSummary->setPlainText(empty);
                relationshipSummary->setPlainText(empty);
                return;
            }

            const FoundationService& foundation = FoundationService::Get();
            const CatalogDatabase& catalog = foundation.GetCatalog();

            QStringList validationLines;
            for (const CatalogValidationEvent& event :
                 catalog.FindValidationForRecord(record->m_recordId))
            {
                validationLines << QObject::tr(
                                       "%1 | %2 | %3 | validator: %4\n"
                                       "Evidence: %5\n%6")
                                       .arg(ToQString(event.m_checkedAt))
                                       .arg(ToQString(event.m_state))
                                       .arg(ToQString(event.m_method))
                                       .arg(ToQString(event.m_validator))
                                       .arg(ValueOrNone(event.m_evidenceIds))
                                       .arg(ToQString(event.m_notes));
            }
            validationSummary->setPlainText(
                validationLines.isEmpty()
                    ? QObject::tr("No validation history exists for this record.")
                    : validationLines.join(QStringLiteral("\n\n")));

            QStringList governanceLines;
            for (const CatalogGovernanceEvent& event :
                 catalog.FindGovernanceForSubject("record", record->m_recordId))
            {
                governanceLines << QObject::tr(
                                       "%1 | %2 | %3 -> %4 | usage: %5\n"
                                       "Reviewer: %6 | evidence: %7\n%8")
                                       .arg(ToQString(event.m_decidedAt))
                                       .arg(ToQString(event.m_axis))
                                       .arg(ToQString(event.m_previousValue))
                                       .arg(ToQString(event.m_newValue))
                                       .arg(ToQString(event.m_usage))
                                       .arg(ToQString(event.m_reviewer))
                                       .arg(ValueOrNone(event.m_evidenceIds))
                                       .arg(ToQString(event.m_notes));
            }
            QString governanceHeader = QObject::tr(
                "Allowed usages: %1\nForbidden usages: %2")
                    .arg(ValueOrNone(record->m_allowedUsages))
                    .arg(ValueOrNone(record->m_forbiddenUsages));
            governanceSummary->setPlainText(
                governanceLines.isEmpty()
                    ? governanceHeader
                        + QObject::tr("\n\nNo governance history exists for this record.")
                    : governanceHeader + QStringLiteral("\n\n")
                        + governanceLines.join(QStringLiteral("\n\n")));

            QStringList blockerLines;
            for (const BlockerRecord& blocker : foundation.GetSnapshot().m_blockers)
            {
                if (blocker.m_subjectRef == record->m_subjectRef
                    || blocker.m_subjectRef == record->m_recordId
                    || blocker.m_subjectRef
                        == "record:" + record->m_recordId)
                {
                    blockerLines << QObject::tr(
                                        "%1 | %2 | %3 | %4\nAffected lanes: %5")
                                        .arg(ToQString(blocker.m_blockerId))
                                        .arg(ToQString(blocker.m_severity))
                                        .arg(ToQString(blocker.m_status))
                                        .arg(ToQString(blocker.m_reason))
                                        .arg(ValueOrNone(blocker.m_affectedUsages));
                }
            }
            blockerSummary->setPlainText(
                blockerLines.isEmpty()
                    ? QObject::tr("No matching blockers are currently reported.")
                    : blockerLines.join(QStringLiteral("\n\n")));

            QStringList relationshipLines;
            for (const CatalogRelationship& relationship :
                 catalog.FindRelationshipsForRecord(record->m_recordId))
            {
                const QString target = relationship.m_toRecordId.empty()
                    ? ToQString(relationship.m_targetSubjectRef)
                    : ToQString(relationship.m_toRecordId);
                relationshipLines << QObject::tr(
                                           "%1 | %2 | %3 -> %4\n"
                                           "Validation: %5 | staleness: %6 | evidence: %7")
                                           .arg(ToQString(relationship.m_relationshipId))
                                           .arg(ToQString(relationship.m_relationshipKind))
                                           .arg(ToQString(relationship.m_fromRecordId))
                                           .arg(target)
                                           .arg(ToQString(relationship.m_validationState))
                                           .arg(ToQString(relationship.m_stalenessState))
                                           .arg(ValueOrNone(relationship.m_evidenceIds));
            }
            relationshipSummary->setPlainText(
                relationshipLines.isEmpty()
                    ? QObject::tr("No relevant catalog relationships exist.")
                    : relationshipLines.join(QStringLiteral("\n\n")));
        }
    } // namespace

    ActorTroopEditorWidget::ActorTroopEditorWidget(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("TaintedGrailActorTroopEditor"));
        setAccessibleName(tr("Tainted Grail Actor and Troop Editor"));

        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Actor and Troop Editor"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr(
                "Author evidence-bound population profiles on existing canonical "
                "records. This pane cannot create identities, grant permissions, "
                "spawn actors, mutate saves, invoke adapters, or deploy content."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        m_tabs = new QTabWidget(this);
        rootLayout->addWidget(m_tabs, 1);

        auto* actorContent = new QWidget(m_tabs);
        auto* actorLayout = new QVBoxLayout(actorContent);
        auto* actorSelection = new QGroupBox(
            tr("Canonical Actor Record"),
            actorContent);
        auto* actorSelectionLayout = new QFormLayout(actorSelection);
        m_actorFilter = new QLineEdit(actorSelection);
        m_actorFilter->setPlaceholderText(
            tr("Filter by record ID, display name, or exact subject"));
        m_actorRecord = new QComboBox(actorSelection);
        m_actorIdentity = new QLabel(actorSelection);
        m_actorIdentity->setWordWrap(true);
        m_actorIdentity->setTextInteractionFlags(
            Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        m_actorState = new QLabel(actorSelection);
        m_actorState->setWordWrap(true);
        m_actorState->setAccessibleName(tr("Actor editor state"));
        AddFormRow(actorSelectionLayout, tr("Actor filter"), m_actorFilter);
        AddFormRow(actorSelectionLayout, tr("Actor record"), m_actorRecord);
        actorSelectionLayout->addRow(tr("Exact identity"), m_actorIdentity);
        actorSelectionLayout->addRow(tr("Editor state"), m_actorState);
        actorLayout->addWidget(actorSelection);

        auto* actorProfile = new QGroupBox(
            tr("Typed Actor Profile"),
            actorContent);
        auto* actorForm = new QFormLayout(actorProfile);
        m_actorKind = new QComboBox(actorProfile);
        m_actorKind->addItems(
            { "npc", "creature", "animal", "construct", "other" });
        m_actorArchetype = new QLineEdit(actorProfile);
        m_actorTemplateRecord = new QComboBox(actorProfile);
        m_actorTemplateSubject = new QLineEdit(actorProfile);
        m_actorTemplateSubject->setPlaceholderText(
            tr("Exact unresolved template subject, or matching resolved subject"));
        m_actorMinimumLevel = new QSpinBox(actorProfile);
        m_actorMinimumLevel->setRange(0, 1000);
        m_actorMinimumLevel->setSpecialValueText(tr("No range"));
        m_actorMaximumLevel = new QSpinBox(actorProfile);
        m_actorMaximumLevel->setRange(0, 1000);
        m_actorMaximumLevel->setSpecialValueText(tr("No range"));
        m_actorUnique = new QCheckBox(
            tr("Unique actor planning claim"),
            actorProfile);
        m_actorEssential = new QCheckBox(
            tr("Essential-state planning claim"),
            actorProfile);
        m_actorPersistent = new QCheckBox(
            tr("Persistence planning claim"),
            actorProfile);
        auto* actorFlags = new QWidget(actorProfile);
        auto* actorFlagsLayout = new QVBoxLayout(actorFlags);
        actorFlagsLayout->setContentsMargins(0, 0, 0, 0);
        actorFlagsLayout->addWidget(m_actorUnique);
        actorFlagsLayout->addWidget(m_actorEssential);
        actorFlagsLayout->addWidget(m_actorPersistent);
        m_actorNameRef = new QLineEdit(actorProfile);
        m_actorDescriptionRef = new QLineEdit(actorProfile);
        m_actorPortraitRef = new QLineEdit(actorProfile);
        m_actorModelRef = new QLineEdit(actorProfile);
        m_actorTags = new QLineEdit(actorProfile);
        m_actorTags->setPlaceholderText(tr("Comma-separated unique tags"));
        AddFormRow(actorForm, tr("Actor kind"), m_actorKind);
        AddFormRow(actorForm, tr("Archetype"), m_actorArchetype);
        AddFormRow(
            actorForm,
            tr("Resolved template record"),
            m_actorTemplateRecord);
        AddFormRow(
            actorForm,
            tr("Template subject reference"),
            m_actorTemplateSubject);
        AddFormRow(actorForm, tr("Minimum level"), m_actorMinimumLevel);
        AddFormRow(actorForm, tr("Maximum level"), m_actorMaximumLevel);
        actorForm->addRow(tr("Planning flags"), actorFlags);
        AddFormRow(actorForm, tr("Localisation name reference"), m_actorNameRef);
        AddFormRow(
            actorForm,
            tr("Localisation description reference"),
            m_actorDescriptionRef);
        AddFormRow(actorForm, tr("Portrait asset reference"), m_actorPortraitRef);
        AddFormRow(actorForm, tr("Model asset reference"), m_actorModelRef);
        AddFormRow(actorForm, tr("Tags"), m_actorTags);
        actorLayout->addWidget(actorProfile);

        auto* actorEvidenceGroup = new QGroupBox(
            tr("Exact-subject Actor Evidence"),
            actorContent);
        auto* actorEvidenceLayout = new QVBoxLayout(actorEvidenceGroup);
        auto* actorEvidenceHelp = new QLabel(
            tr(
                "Select one or more evidence records bound to the actor or its "
                "template. Details remain visible without hovering."),
            actorEvidenceGroup);
        actorEvidenceHelp->setWordWrap(true);
        actorEvidenceLayout->addWidget(actorEvidenceHelp);
        m_actorEvidence = new QListWidget(actorEvidenceGroup);
        m_actorEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_actorEvidence->setAccessibleName(tr("Actor profile evidence selector"));
        m_actorEvidence->setMinimumHeight(120);
        actorEvidenceHelp->setBuddy(m_actorEvidence);
        actorEvidenceLayout->addWidget(m_actorEvidence);
        m_actorEvidenceDetails = new QTableWidget(0, 6, actorEvidenceGroup);
        m_actorEvidenceDetails->setHorizontalHeaderLabels(
            { tr("Evidence ID"), tr("Subject"), tr("Claim"), tr("Kind"),
              tr("Confidence"), tr("Source") });
        m_actorEvidenceDetails->setAccessibleName(tr("Actor evidence details"));
        ConfigureTable(m_actorEvidenceDetails);
        actorEvidenceLayout->addWidget(m_actorEvidenceDetails);
        actorLayout->addWidget(actorEvidenceGroup);

        auto* actorReview = new QGroupBox(
            tr("Read-only Actor Review Context"),
            actorContent);
        auto* actorReviewLayout = new QVBoxLayout(actorReview);
        auto* actorReviewTabs = new QTabWidget(actorReview);
        m_actorValidationSummary = CreateSummary(
            tr("Actor validation summary"),
            actorReviewTabs);
        m_actorGovernanceSummary = CreateSummary(
            tr("Actor governance summary"),
            actorReviewTabs);
        m_actorBlockerSummary = CreateSummary(
            tr("Actor blocker summary"),
            actorReviewTabs);
        m_actorRelationshipSummary = CreateSummary(
            tr("Actor relationship summary"),
            actorReviewTabs);
        actorReviewTabs->addTab(m_actorValidationSummary, tr("Validation"));
        actorReviewTabs->addTab(m_actorGovernanceSummary, tr("Governance"));
        actorReviewTabs->addTab(m_actorBlockerSummary, tr("Blockers"));
        actorReviewTabs->addTab(m_actorRelationshipSummary, tr("Relationships"));
        actorReviewLayout->addWidget(actorReviewTabs);
        actorLayout->addWidget(actorReview);

        auto* actorLaneGroup = new QGroupBox(
            tr("Actor Action Lanes - Governed Read-only Status"),
            actorContent);
        auto* actorLaneLayout = new QVBoxLayout(actorLaneGroup);
        m_actorActionLanes = new QTableWidget(0, 4, actorLaneGroup);
        m_actorActionLanes->setHorizontalHeaderLabels(
            { tr("Lane"), tr("State"), tr("Blockers"), tr("Reasons") });
        m_actorActionLanes->setAccessibleName(tr("Actor action lane matrix"));
        ConfigureTable(m_actorActionLanes);
        actorLaneLayout->addWidget(m_actorActionLanes);
        actorLayout->addWidget(actorLaneGroup);

        auto* actorButtons = new QWidget(actorContent);
        auto* actorButtonLayout = new QGridLayout(actorButtons);
        m_revertActor = new QPushButton(tr("Revert Actor Draft"), actorButtons);
        m_saveActor = new QPushButton(tr("Save Actor Profile"), actorButtons);
        actorButtonLayout->addWidget(m_revertActor, 0, 0);
        actorButtonLayout->addWidget(m_saveActor, 0, 1);
        actorLayout->addWidget(actorButtons);
        actorLayout->addStretch(1);
        m_tabs->addTab(WrapScrollable(actorContent, m_tabs), tr("Actors"));

        auto* troopContent = new QWidget(m_tabs);
        auto* troopLayout = new QVBoxLayout(troopContent);
        auto* troopSelection = new QGroupBox(
            tr("Canonical Troop Record"),
            troopContent);
        auto* troopSelectionLayout = new QFormLayout(troopSelection);
        m_troopFilter = new QLineEdit(troopSelection);
        m_troopFilter->setPlaceholderText(
            tr("Filter by record ID, display name, or exact subject"));
        m_troopRecord = new QComboBox(troopSelection);
        m_troopIdentity = new QLabel(troopSelection);
        m_troopIdentity->setWordWrap(true);
        m_troopIdentity->setTextInteractionFlags(
            Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        m_troopState = new QLabel(troopSelection);
        m_troopState->setWordWrap(true);
        m_troopState->setAccessibleName(tr("Troop editor state"));
        AddFormRow(troopSelectionLayout, tr("Troop filter"), m_troopFilter);
        AddFormRow(troopSelectionLayout, tr("Troop record"), m_troopRecord);
        troopSelectionLayout->addRow(tr("Exact identity"), m_troopIdentity);
        troopSelectionLayout->addRow(tr("Editor state"), m_troopState);
        troopLayout->addWidget(troopSelection);

        auto* troopProfile = new QGroupBox(
            tr("Typed Troop Profile"),
            troopContent);
        auto* troopForm = new QFormLayout(troopProfile);
        m_troopKind = new QComboBox(troopProfile);
        m_troopKind->addItems(
            { "party", "patrol", "encounter_group", "reinforcement", "other" });
        m_troopLeaderRecord = new QComboBox(troopProfile);
        m_troopLeaderSubject = new QLineEdit(troopProfile);
        m_troopLeaderSubject->setPlaceholderText(
            tr("Exact unresolved leader subject, or matching resolved subject"));
        m_troopMinimumSize = new QSpinBox(troopProfile);
        m_troopMinimumSize->setRange(1, 1000);
        m_troopMaximumSize = new QSpinBox(troopProfile);
        m_troopMaximumSize->setRange(1, 1000);
        m_troopFormation = new QLineEdit(troopProfile);
        m_troopTags = new QLineEdit(troopProfile);
        m_troopTags->setPlaceholderText(tr("Comma-separated unique tags"));
        AddFormRow(troopForm, tr("Troop kind"), m_troopKind);
        AddFormRow(
            troopForm,
            tr("Resolved leader actor"),
            m_troopLeaderRecord);
        AddFormRow(
            troopForm,
            tr("Leader subject reference"),
            m_troopLeaderSubject);
        AddFormRow(troopForm, tr("Minimum size"), m_troopMinimumSize);
        AddFormRow(troopForm, tr("Maximum size"), m_troopMaximumSize);
        AddFormRow(troopForm, tr("Formation"), m_troopFormation);
        AddFormRow(troopForm, tr("Tags"), m_troopTags);
        troopLayout->addWidget(troopProfile);

        auto* troopEvidenceGroup = new QGroupBox(
            tr("Exact-subject Troop Evidence"),
            troopContent);
        auto* troopEvidenceLayout = new QVBoxLayout(troopEvidenceGroup);
        auto* troopEvidenceLabel = new QLabel(
            tr("Select evidence bound to the troop or its leader."),
            troopEvidenceGroup);
        troopEvidenceLabel->setWordWrap(true);
        troopEvidenceLayout->addWidget(troopEvidenceLabel);
        m_troopEvidence = new QListWidget(troopEvidenceGroup);
        m_troopEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_troopEvidence->setAccessibleName(tr("Troop profile evidence selector"));
        m_troopEvidence->setMinimumHeight(120);
        troopEvidenceLabel->setBuddy(m_troopEvidence);
        troopEvidenceLayout->addWidget(m_troopEvidence);
        m_troopEvidenceDetails = new QTableWidget(0, 6, troopEvidenceGroup);
        m_troopEvidenceDetails->setHorizontalHeaderLabels(
            { tr("Evidence ID"), tr("Subject"), tr("Claim"), tr("Kind"),
              tr("Confidence"), tr("Source") });
        m_troopEvidenceDetails->setAccessibleName(tr("Troop evidence details"));
        ConfigureTable(m_troopEvidenceDetails);
        troopEvidenceLayout->addWidget(m_troopEvidenceDetails);
        troopLayout->addWidget(troopEvidenceGroup);

        auto* memberGroup = new QGroupBox(
            tr("Atomic Troop Composition Draft"),
            troopContent);
        auto* memberLayout = new QVBoxLayout(memberGroup);
        auto* memberHelp = new QLabel(
            tr(
                "Stage member additions or updates locally, then save the whole "
                "troop definition once. Existing omitted members are preserved; "
                "this pane has no removal or link-move authority."),
            memberGroup);
        memberHelp->setWordWrap(true);
        memberLayout->addWidget(memberHelp);
        m_memberTable = new QTableWidget(0, 8, memberGroup);
        m_memberTable->setHorizontalHeaderLabels(
            { tr("Link ID"), tr("Actor / subject"), tr("Role"), tr("Minimum"),
              tr("Maximum"), tr("Weight"), tr("Required"), tr("Evidence") });
        m_memberTable->setAccessibleName(tr("Atomic troop member draft"));
        ConfigureTable(m_memberTable);
        memberLayout->addWidget(m_memberTable);

        auto* memberForm = new QFormLayout();
        m_memberLinkId = new QLineEdit(memberGroup);
        m_memberActorRecord = new QComboBox(memberGroup);
        m_memberActorSubject = new QLineEdit(memberGroup);
        m_memberActorSubject->setPlaceholderText(
            tr("Exact unresolved actor subject, or matching resolved subject"));
        m_memberRole = new QComboBox(memberGroup);
        m_memberRole->addItems(
            { "leader", "melee", "ranged", "support", "specialist", "other" });
        m_memberMinimumCount = new QSpinBox(memberGroup);
        m_memberMinimumCount->setRange(1, 1000);
        m_memberMaximumCount = new QSpinBox(memberGroup);
        m_memberMaximumCount->setRange(1, 1000);
        m_memberWeight = new QLineEdit(memberGroup);
        m_memberWeight->setText(QStringLiteral("1"));
        m_memberWeight->setPlaceholderText(
            tr("Any finite non-negative number"));
        m_memberRequired = new QCheckBox(
            tr("Structurally required"),
            memberGroup);
        m_memberConditions = new QLineEdit(memberGroup);
        m_memberConditions->setPlaceholderText(
            tr("Comma-separated unique conditions"));
        AddFormRow(memberForm, tr("Stable link ID"), m_memberLinkId);
        AddFormRow(memberForm, tr("Resolved actor record"), m_memberActorRecord);
        AddFormRow(
            memberForm,
            tr("Actor subject reference"),
            m_memberActorSubject);
        AddFormRow(memberForm, tr("Member role"), m_memberRole);
        AddFormRow(memberForm, tr("Minimum count"), m_memberMinimumCount);
        AddFormRow(memberForm, tr("Maximum count"), m_memberMaximumCount);
        AddFormRow(
            memberForm,
            tr("Planning weight"),
            m_memberWeight,
            tr("The Core contract accepts any finite non-negative number."));
        memberForm->addRow(tr("Requirement"), m_memberRequired);
        AddFormRow(memberForm, tr("Conditions"), m_memberConditions);
        memberLayout->addLayout(memberForm);

        auto* memberEvidenceLabel = new QLabel(
            tr("Member exact-subject evidence"),
            memberGroup);
        memberEvidenceLabel->setWordWrap(true);
        memberLayout->addWidget(memberEvidenceLabel);
        m_memberEvidence = new QListWidget(memberGroup);
        m_memberEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_memberEvidence->setAccessibleName(tr("Troop member evidence selector"));
        m_memberEvidence->setMinimumHeight(120);
        memberEvidenceLabel->setBuddy(m_memberEvidence);
        memberLayout->addWidget(m_memberEvidence);
        m_memberEvidenceDetails = new QTableWidget(0, 6, memberGroup);
        m_memberEvidenceDetails->setHorizontalHeaderLabels(
            { tr("Evidence ID"), tr("Subject"), tr("Claim"), tr("Kind"),
              tr("Confidence"), tr("Source") });
        m_memberEvidenceDetails->setAccessibleName(tr("Troop member evidence details"));
        ConfigureTable(m_memberEvidenceDetails);
        memberLayout->addWidget(m_memberEvidenceDetails);
        auto* memberButtons = new QWidget(memberGroup);
        auto* memberButtonLayout = new QGridLayout(memberButtons);
        m_clearMember = new QPushButton(tr("Clear Member Editor"), memberButtons);
        m_stageMember = new QPushButton(
            tr("Stage Member in Definition"),
            memberButtons);
        memberButtonLayout->addWidget(m_clearMember, 0, 0);
        memberButtonLayout->addWidget(m_stageMember, 0, 1);
        memberLayout->addWidget(memberButtons);
        troopLayout->addWidget(memberGroup);

        auto* troopReview = new QGroupBox(
            tr("Read-only Troop Review Context"),
            troopContent);
        auto* troopReviewLayout = new QVBoxLayout(troopReview);
        auto* troopReviewTabs = new QTabWidget(troopReview);
        m_troopValidationSummary = CreateSummary(
            tr("Troop validation summary"),
            troopReviewTabs);
        m_troopGovernanceSummary = CreateSummary(
            tr("Troop governance summary"),
            troopReviewTabs);
        m_troopBlockerSummary = CreateSummary(
            tr("Troop blocker summary"),
            troopReviewTabs);
        m_troopRelationshipSummary = CreateSummary(
            tr("Troop relationship summary"),
            troopReviewTabs);
        troopReviewTabs->addTab(m_troopValidationSummary, tr("Validation"));
        troopReviewTabs->addTab(m_troopGovernanceSummary, tr("Governance"));
        troopReviewTabs->addTab(m_troopBlockerSummary, tr("Blockers"));
        troopReviewTabs->addTab(m_troopRelationshipSummary, tr("Relationships"));
        troopReviewLayout->addWidget(troopReviewTabs);
        troopLayout->addWidget(troopReview);

        auto* troopLaneGroup = new QGroupBox(
            tr("Troop Action Lanes - Governed Read-only Status"),
            troopContent);
        auto* troopLaneLayout = new QVBoxLayout(troopLaneGroup);
        m_troopActionLanes = new QTableWidget(0, 4, troopLaneGroup);
        m_troopActionLanes->setHorizontalHeaderLabels(
            { tr("Lane"), tr("State"), tr("Blockers"), tr("Reasons") });
        m_troopActionLanes->setAccessibleName(tr("Troop action lane matrix"));
        ConfigureTable(m_troopActionLanes);
        troopLaneLayout->addWidget(m_troopActionLanes);
        troopLayout->addWidget(troopLaneGroup);

        auto* troopButtons = new QWidget(troopContent);
        auto* troopButtonLayout = new QGridLayout(troopButtons);
        m_revertTroop = new QPushButton(
            tr("Revert Troop Definition Draft"),
            troopButtons);
        m_saveTroop = new QPushButton(
            tr("Save Atomic Troop Definition"),
            troopButtons);
        troopButtonLayout->addWidget(m_revertTroop, 0, 0);
        troopButtonLayout->addWidget(m_saveTroop, 0, 1);
        troopLayout->addWidget(troopButtons);
        troopLayout->addStretch(1);
        m_tabs->addTab(WrapScrollable(troopContent, m_tabs), tr("Troops"));

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setAccessibleName(tr("Actor and troop editor status"));
        m_status->setTextInteractionFlags(
            Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
        rootLayout->addWidget(m_status);

        connect(
            m_actorFilter,
            &QLineEdit::textChanged,
            this,
            [this]()
            {
                if (HasDirtyDrafts())
                {
                    SetStatus(
                        tr(
                            "Actor filter refresh is deferred because an actor, "
                            "troop, or member draft is unsaved. Save or revert it first."));
                    return;
                }
                RefreshRecordChoices();
                LoadCurrentActor();
            });
        connect(
            m_troopFilter,
            &QLineEdit::textChanged,
            this,
            [this]()
            {
                if (HasDirtyDrafts())
                {
                    SetStatus(
                        tr(
                            "Troop filter refresh is deferred because an actor, "
                            "troop, or member draft is unsaved. Save or revert it first."));
                    return;
                }
                RefreshRecordChoices();
                LoadCurrentTroop();
            });
        connect(
            m_actorRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { HandleActorRecordChange(); });
        connect(
            m_troopRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { HandleTroopRecordChange(); });
        connect(
            m_actorTemplateRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                MarkActorDirty();
                const CatalogRecord* record = FoundationService::Get()
                    .GetCatalog()
                    .FindByRecordId(ToAzString(
                        m_actorTemplateRecord->currentData().toString()));
                if (record)
                {
                    m_actorTemplateSubject->setText(
                        ToQString(record->m_subjectRef));
                }
                RefreshActorEvidenceChoices();
            });
        connect(
            m_actorTemplateSubject,
            &QLineEdit::editingFinished,
            this,
            [this]() { RefreshActorEvidenceChoices(); });
        connect(
            m_troopLeaderRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                MarkTroopDirty();
                const CatalogRecord* record = FoundationService::Get()
                    .GetCatalog()
                    .FindByRecordId(ToAzString(
                        m_troopLeaderRecord->currentData().toString()));
                if (record)
                {
                    m_troopLeaderSubject->setText(
                        ToQString(record->m_subjectRef));
                }
                RefreshTroopEvidenceChoices();
            });
        connect(
            m_troopLeaderSubject,
            &QLineEdit::editingFinished,
            this,
            [this]() { RefreshTroopEvidenceChoices(); });
        connect(
            m_memberActorRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                MarkMemberEditorDirty();
                const CatalogRecord* record = FoundationService::Get()
                    .GetCatalog()
                    .FindByRecordId(ToAzString(
                        m_memberActorRecord->currentData().toString()));
                if (record)
                {
                    m_memberActorSubject->setText(
                        ToQString(record->m_subjectRef));
                }
                RefreshMemberEvidenceChoices();
            });
        connect(
            m_memberActorSubject,
            &QLineEdit::editingFinished,
            this,
            [this]() { RefreshMemberEvidenceChoices(); });
        connect(
            m_memberLinkId,
            &QLineEdit::editingFinished,
            this,
            [this]() { RefreshMemberEvidenceChoices(); });
        connect(
            m_actorEvidence,
            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                MarkActorDirty();
                RefreshEvidenceDetails(
                    m_actorEvidence,
                    m_actorEvidenceDetails);
            });
        connect(
            m_troopEvidence,
            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                MarkTroopDirty();
                RefreshEvidenceDetails(
                    m_troopEvidence,
                    m_troopEvidenceDetails);
            });
        connect(
            m_memberEvidence,
            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                MarkMemberEditorDirty();
                RefreshEvidenceDetails(
                    m_memberEvidence,
                    m_memberEvidenceDetails);
            });
        connect(
            m_memberTable,
            &QTableWidget::itemSelectionChanged,
            this,
            [this]() { HandleMemberSelectionChange(); });
        connect(
            m_actorKind,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkActorDirty(); });
        connect(
            m_actorArchetype,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkActorDirty(); });
        connect(
            m_actorTemplateSubject,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkActorDirty(); });
        connect(
            m_actorMinimumLevel,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkActorDirty(); });
        connect(
            m_actorMaximumLevel,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkActorDirty(); });
        connect(
            m_actorUnique,
            &QCheckBox::toggled,
            this,
            [this](bool) { MarkActorDirty(); });
        connect(
            m_actorEssential,
            &QCheckBox::toggled,
            this,
            [this](bool) { MarkActorDirty(); });
        connect(
            m_actorPersistent,
            &QCheckBox::toggled,
            this,
            [this](bool) { MarkActorDirty(); });
        for (QLineEdit* field : {
                 m_actorNameRef,
                 m_actorDescriptionRef,
                 m_actorPortraitRef,
                 m_actorModelRef,
                 m_actorTags })
        {
            connect(
                field,
                &QLineEdit::textEdited,
                this,
                [this](const QString&) { MarkActorDirty(); });
        }

        connect(
            m_troopKind,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(
            m_troopLeaderSubject,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkTroopDirty(); });
        connect(
            m_troopMinimumSize,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(
            m_troopMaximumSize,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(
            m_troopFormation,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkTroopDirty(); });
        connect(
            m_troopTags,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkTroopDirty(); });

        connect(
            m_memberLinkId,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkMemberEditorDirty(); });
        connect(
            m_memberActorSubject,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkMemberEditorDirty(); });
        connect(
            m_memberRole,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkMemberEditorDirty(); });
        connect(
            m_memberMinimumCount,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkMemberEditorDirty(); });
        connect(
            m_memberMaximumCount,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkMemberEditorDirty(); });
        connect(
            m_memberWeight,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkMemberEditorDirty(); });
        connect(
            m_memberRequired,
            &QCheckBox::toggled,
            this,
            [this](bool) { MarkMemberEditorDirty(); });
        connect(
            m_memberConditions,
            &QLineEdit::textEdited,
            this,
            [this](const QString&) { MarkMemberEditorDirty(); });
        connect(
            m_stageMember,
            &QPushButton::clicked,
            this,
            [this]() { StageMember(); });
        connect(
            m_clearMember,
            &QPushButton::clicked,
            this,
            [this]() { ClearMemberEditor(); });
        connect(
            m_saveActor,
            &QPushButton::clicked,
            this,
            [this]() { SaveActorProfile(); });
        connect(
            m_revertActor,
            &QPushButton::clicked,
            this,
            [this]() { RevertActorProfile(); });
        connect(
            m_saveTroop,
            &QPushButton::clicked,
            this,
            [this]() { SaveTroopDefinition(); });
        connect(
            m_revertTroop,
            &QPushButton::clicked,
            this,
            [this]() { RevertTroopDefinition(); });

        auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
        connect(
            saveShortcut,
            &QShortcut::activated,
            this,
            [this]()
            {
                if (m_tabs->currentIndex() == 0)
                {
                    SaveActorProfile();
                }
                else
                {
                    SaveTroopDefinition();
                }
            });
        auto* revertShortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+R")),
            this);
        connect(
            revertShortcut,
            &QShortcut::activated,
            this,
            [this]()
            {
                if (m_tabs->currentIndex() == 0)
                {
                    RevertActorProfile();
                }
                else
                {
                    RevertTroopDefinition();
                }
            });

        QWidget::setTabOrder(m_actorFilter, m_actorRecord);
        QWidget::setTabOrder(m_actorRecord, m_actorKind);
        QWidget::setTabOrder(m_actorKind, m_actorArchetype);
        QWidget::setTabOrder(m_actorArchetype, m_actorTemplateRecord);
        QWidget::setTabOrder(m_actorTemplateRecord, m_actorTemplateSubject);
        QWidget::setTabOrder(m_actorTemplateSubject, m_actorMinimumLevel);
        QWidget::setTabOrder(m_actorMinimumLevel, m_actorMaximumLevel);
        QWidget::setTabOrder(m_actorMaximumLevel, m_actorEvidence);
        QWidget::setTabOrder(m_actorEvidence, m_revertActor);
        QWidget::setTabOrder(m_revertActor, m_saveActor);
        QWidget::setTabOrder(m_troopFilter, m_troopRecord);
        QWidget::setTabOrder(m_troopRecord, m_troopKind);
        QWidget::setTabOrder(m_troopKind, m_troopLeaderRecord);
        QWidget::setTabOrder(m_troopLeaderRecord, m_troopLeaderSubject);
        QWidget::setTabOrder(m_troopLeaderSubject, m_troopEvidence);
        QWidget::setTabOrder(m_troopEvidence, m_memberTable);
        QWidget::setTabOrder(m_memberTable, m_memberLinkId);
        QWidget::setTabOrder(m_memberLinkId, m_memberActorRecord);
        QWidget::setTabOrder(m_memberActorRecord, m_memberActorSubject);
        QWidget::setTabOrder(m_memberActorSubject, m_memberRole);
        QWidget::setTabOrder(m_memberRole, m_memberEvidence);
        QWidget::setTabOrder(m_memberEvidence, m_stageMember);
        QWidget::setTabOrder(m_stageMember, m_revertTroop);
        QWidget::setTabOrder(m_revertTroop, m_saveTroop);

        FoundationNotificationBus::Handler::BusConnect();
        RefreshAll();
    }

    ActorTroopEditorWidget::~ActorTroopEditorWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void ActorTroopEditorWidget::closeEvent(QCloseEvent* event)
    {
        if (!HasDirtyDrafts())
        {
            QWidget::closeEvent(event);
            return;
        }

        const QMessageBox::StandardButton choice = QMessageBox::warning(
            this,
            tr("Discard unsaved population drafts?"),
            tr(
                "The Actor/Troop Editor contains unsaved actor, troop, or member "
                "changes. Close the pane and discard those local drafts?"),
            QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (choice == QMessageBox::Discard)
        {
            QWidget::closeEvent(event);
        }
        else
        {
            event->ignore();
        }
    }

    void ActorTroopEditorWidget::OnFoundationChanged()
    {
        if (HasDirtyDrafts())
        {
            m_foundationRefreshPending = true;
            SetStatus(
                tr(
                    "Published Foundation state changed while local drafts were "
                    "unsaved. The drafts were preserved; save or revert them to "
                    "apply the pending refresh."));
            return;
        }
        RefreshAll();
    }

    void ActorTroopEditorWidget::RefreshAll()
    {
        if (m_refreshing)
        {
            return;
        }

        m_refreshing = true;
        RefreshRecordChoices();
        RefreshReferenceChoices();
        LoadCurrentActor();
        LoadCurrentTroop();
        m_refreshing = false;
    }

    void ActorTroopEditorWidget::ApplyPendingFoundationRefresh()
    {
        if (HasDirtyDrafts() || !m_foundationRefreshPending)
        {
            return;
        }
        m_foundationRefreshPending = false;
        RefreshAll();
    }

    void ActorTroopEditorWidget::MarkActorDirty()
    {
        if (m_refreshing || m_loadingActor)
        {
            return;
        }
        m_actorDirty = true;
        SetActorState(
            tr("Unsaved draft: save or revert before selecting another actor."));
    }

    void ActorTroopEditorWidget::MarkTroopDirty()
    {
        if (m_refreshing || m_loadingTroop)
        {
            return;
        }
        m_troopDirty = true;
        SetTroopState(
            tr("Unsaved draft: save or revert before selecting another troop."));
    }

    void ActorTroopEditorWidget::MarkMemberEditorDirty()
    {
        if (m_refreshing || m_loadingTroop || m_loadingMember)
        {
            return;
        }
        m_memberEditorDirty = true;
        SetTroopState(
            tr(
                "Unsaved member editor: stage the member, clear the member "
                "editor, or revert the troop before changing selection."));
    }

    bool ActorTroopEditorWidget::HasDirtyDrafts() const
    {
        return m_actorDirty || m_troopDirty || m_memberEditorDirty;
    }

    void ActorTroopEditorWidget::HandleActorRecordChange()
    {
        if (m_refreshing || m_loadingActor)
        {
            return;
        }
        if (m_actorDirty)
        {
            const QSignalBlocker blocker(m_actorRecord);
            const int loadedIndex = m_actorRecord->findData(
                ToQString(m_loadedActorRecordId));
            m_actorRecord->setCurrentIndex(
                loadedIndex >= 0 ? loadedIndex : 0);
            SetStatus(
                tr(
                    "Actor selection was not changed because the current actor "
                    "draft is unsaved. Save or revert it first."));
            return;
        }
        LoadCurrentActor();
    }

    void ActorTroopEditorWidget::HandleTroopRecordChange()
    {
        if (m_refreshing || m_loadingTroop)
        {
            return;
        }
        if (m_troopDirty || m_memberEditorDirty)
        {
            const QSignalBlocker blocker(m_troopRecord);
            const int loadedIndex = m_troopRecord->findData(
                ToQString(m_loadedTroopRecordId));
            m_troopRecord->setCurrentIndex(
                loadedIndex >= 0 ? loadedIndex : 0);
            SetStatus(
                tr(
                    "Troop selection was not changed because the current troop "
                    "or member draft is unsaved. Save or revert it first."));
            return;
        }
        LoadCurrentTroop();
    }

    void ActorTroopEditorWidget::HandleMemberSelectionChange()
    {
        if (m_refreshing || m_loadingTroop || m_loadingMember)
        {
            return;
        }
        if (m_memberEditorDirty)
        {
            RefreshMemberTable();
            SetStatus(
                tr(
                    "Member selection was not changed because the member editor "
                    "contains unstaged changes. Stage or clear them first."));
            return;
        }
        LoadSelectedMember();
    }

    void ActorTroopEditorWidget::RefreshRecordChoices()
    {
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const QString previousActor = m_actorRecord->currentData().toString();
        const QString previousTroop = m_troopRecord->currentData().toString();
        const QSignalBlocker actorBlocker(m_actorRecord);
        const QSignalBlocker troopBlocker(m_troopRecord);

        m_actorRecord->clear();
        m_troopRecord->clear();
        m_actorRecord->addItem(tr("Select canonical population actor..."), QString());
        m_troopRecord->addItem(tr("Select canonical population troop..."), QString());

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (record.m_domain != "population")
            {
                continue;
            }
            if (record.m_recordKind == "actor"
                && MatchesFilter(record, m_actorFilter->text()))
            {
                m_actorRecord->addItem(
                    RecordLabel(record),
                    ToQString(record.m_recordId));
            }
            else if (record.m_recordKind == "troop"
                && MatchesFilter(record, m_troopFilter->text()))
            {
                m_troopRecord->addItem(
                    RecordLabel(record),
                    ToQString(record.m_recordId));
            }
        }

        const int actorIndex = m_actorRecord->findData(previousActor);
        m_actorRecord->setCurrentIndex(actorIndex >= 0 ? actorIndex : 0);
        const int troopIndex = m_troopRecord->findData(previousTroop);
        m_troopRecord->setCurrentIndex(troopIndex >= 0 ? troopIndex : 0);
    }

    void ActorTroopEditorWidget::RefreshReferenceChoices()
    {
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const QString previousTemplate =
            m_actorTemplateRecord->currentData().toString();
        const QString previousLeader =
            m_troopLeaderRecord->currentData().toString();
        const QString previousMember =
            m_memberActorRecord->currentData().toString();
        const QSignalBlocker templateBlocker(m_actorTemplateRecord);
        const QSignalBlocker leaderBlocker(m_troopLeaderRecord);
        const QSignalBlocker memberBlocker(m_memberActorRecord);

        m_actorTemplateRecord->clear();
        m_troopLeaderRecord->clear();
        m_memberActorRecord->clear();
        m_actorTemplateRecord->addItem(
            tr("Use no template or an unresolved subject"),
            QString());
        m_troopLeaderRecord->addItem(
            tr("Use no leader or an unresolved subject"),
            QString());
        m_memberActorRecord->addItem(
            tr("Use an unresolved actor subject"),
            QString());

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (record.m_domain != "population")
            {
                continue;
            }
            if (record.m_recordKind == "template"
                || record.m_recordKind == "actor_template")
            {
                m_actorTemplateRecord->addItem(
                    RecordLabel(record),
                    ToQString(record.m_recordId));
            }
            if (record.m_recordKind == "actor")
            {
                const QString id = ToQString(record.m_recordId);
                const QString label = RecordLabel(record);
                m_troopLeaderRecord->addItem(label, id);
                m_memberActorRecord->addItem(label, id);
            }
        }

        auto restore = [](QComboBox* combo, const QString& value)
        {
            const int index = combo->findData(value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
        };
        restore(m_actorTemplateRecord, previousTemplate);
        restore(m_troopLeaderRecord, previousLeader);
        restore(m_memberActorRecord, previousMember);
    }

    void ActorTroopEditorWidget::RefreshActorEvidenceChoices()
    {
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* actor = catalog.FindByRecordId(
            ToAzString(m_actorRecord->currentData().toString()));
        const CatalogRecord* actorTemplate = catalog.FindByRecordId(
            ToAzString(m_actorTemplateRecord->currentData().toString()));
        const AZStd::vector<AZStd::string> selected =
            SelectedEvidenceIds(m_actorEvidence);
        PopulateEvidenceSelector(
            m_actorEvidence,
            RecordSubjects(
                actor,
                actorTemplate,
                ToAzString(m_actorTemplateSubject->text())),
            selected,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_actorEvidence, m_actorEvidenceDetails);
    }

    void ActorTroopEditorWidget::RefreshTroopEvidenceChoices()
    {
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* troop = catalog.FindByRecordId(
            ToAzString(m_troopRecord->currentData().toString()));
        const CatalogRecord* leader = catalog.FindByRecordId(
            ToAzString(m_troopLeaderRecord->currentData().toString()));
        const AZStd::vector<AZStd::string> selected =
            SelectedEvidenceIds(m_troopEvidence);
        PopulateEvidenceSelector(
            m_troopEvidence,
            RecordSubjects(
                troop,
                leader,
                ToAzString(m_troopLeaderSubject->text())),
            selected,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_troopEvidence, m_troopEvidenceDetails);
    }

    void ActorTroopEditorWidget::RefreshMemberEvidenceChoices()
    {
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* troop = catalog.FindByRecordId(
            ToAzString(m_troopRecord->currentData().toString()));
        const CatalogRecord* actor = catalog.FindByRecordId(
            ToAzString(m_memberActorRecord->currentData().toString()));
        AZStd::vector<AZStd::string> subjects = RecordSubjects(
            troop,
            actor,
            ToAzString(m_memberActorSubject->text()));
        const AZStd::string linkId = ToAzString(m_memberLinkId->text());
        if (!linkId.empty())
        {
            AddUnique(subjects, "population-troop-member:" + linkId);
        }
        const AZStd::vector<AZStd::string> selected =
            SelectedEvidenceIds(m_memberEvidence);
        PopulateEvidenceSelector(
            m_memberEvidence,
            subjects,
            selected,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_memberEvidence, m_memberEvidenceDetails);
    }

    void ActorTroopEditorWidget::RefreshEvidenceDetails(
        QListWidget* selector,
        QTableWidget* details)
    {
        const SourceEvidenceRegistry& registry =
            FoundationService::Get().GetSourceRegistry();
        const AZStd::vector<AZStd::string> selected =
            SelectedEvidenceIds(selector);
        details->setRowCount(static_cast<int>(selected.size()));
        for (int row = 0; row < static_cast<int>(selected.size()); ++row)
        {
            const EvidenceRecord* evidence = registry.FindEvidence(
                selected[static_cast<size_t>(row)]);
            if (!evidence)
            {
                SetCell(details, row, 0, ToQString(selected[static_cast<size_t>(row)]));
                SetCell(details, row, 1, tr("Missing from active registry"));
                continue;
            }
            SetCell(details, row, 0, ToQString(evidence->m_evidenceId));
            SetCell(details, row, 1, ToQString(evidence->m_subjectRef));
            SetCell(details, row, 2, ToQString(evidence->m_claim));
            SetCell(details, row, 3, ToQString(evidence->m_evidenceKind));
            SetCell(details, row, 4, ToQString(evidence->m_confidence));
            SetCell(details, row, 5, ToQString(evidence->m_sourceId));
        }
        details->resizeRowsToContents();
    }

    void ActorTroopEditorWidget::RefreshActorReviewContext(
        const CatalogRecord* record)
    {
        FillReviewContext(
            record,
            m_actorValidationSummary,
            m_actorGovernanceSummary,
            m_actorBlockerSummary,
            m_actorRelationshipSummary);
    }

    void ActorTroopEditorWidget::RefreshTroopReviewContext(
        const CatalogRecord* record)
    {
        FillReviewContext(
            record,
            m_troopValidationSummary,
            m_troopGovernanceSummary,
            m_troopBlockerSummary,
            m_troopRelationshipSummary);
    }

    void ActorTroopEditorWidget::RefreshActorActionLanes(
        const CatalogRecord* record)
    {
        m_actorActionLanes->setRowCount(0);
        if (!record)
        {
            return;
        }

        const FoundationService& foundation = FoundationService::Get();
        const PopulationActionLaneService laneService;
        const auto result = laneService.BuildActionLaneMatrix(
            record->m_recordId,
            foundation.GetWorkspace(),
            foundation.GetWorkspaceRootPath(),
            foundation.GetActivePack(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);
        if (!result.IsSuccess())
        {
            m_actorActionLanes->setRowCount(1);
            SetCell(m_actorActionLanes, 0, 0, tr("matrix"));
            SetCell(m_actorActionLanes, 0, 1, tr("unavailable"));
            SetCell(m_actorActionLanes, 0, 3, ToQString(result.GetError()));
            return;
        }

        const auto& decisions = result.GetValue();
        m_actorActionLanes->setRowCount(static_cast<int>(decisions.size()));
        for (int row = 0; row < static_cast<int>(decisions.size()); ++row)
        {
            const PopulationActionLaneDecision& decision =
                decisions[static_cast<size_t>(row)];
            SetCell(m_actorActionLanes, row, 0, ToQString(ToString(decision.m_lane)));
            SetCell(m_actorActionLanes, row, 1, ToQString(ToString(decision.m_state)));
            SetCell(m_actorActionLanes, row, 2, ValueOrNone(decision.m_blockerIds));
            SetCell(m_actorActionLanes, row, 3, ValueOrNone(decision.m_reasons));
        }
        m_actorActionLanes->resizeRowsToContents();
    }

    void ActorTroopEditorWidget::RefreshTroopActionLanes(
        const CatalogRecord* record)
    {
        m_troopActionLanes->setRowCount(0);
        if (!record)
        {
            return;
        }

        const FoundationService& foundation = FoundationService::Get();
        const PopulationActionLaneService laneService;
        const auto result = laneService.BuildActionLaneMatrix(
            record->m_recordId,
            foundation.GetWorkspace(),
            foundation.GetWorkspaceRootPath(),
            foundation.GetActivePack(),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);
        if (!result.IsSuccess())
        {
            m_troopActionLanes->setRowCount(1);
            SetCell(m_troopActionLanes, 0, 0, tr("matrix"));
            SetCell(m_troopActionLanes, 0, 1, tr("unavailable"));
            SetCell(m_troopActionLanes, 0, 3, ToQString(result.GetError()));
            return;
        }

        const auto& decisions = result.GetValue();
        m_troopActionLanes->setRowCount(static_cast<int>(decisions.size()));
        for (int row = 0; row < static_cast<int>(decisions.size()); ++row)
        {
            const PopulationActionLaneDecision& decision =
                decisions[static_cast<size_t>(row)];
            SetCell(m_troopActionLanes, row, 0, ToQString(ToString(decision.m_lane)));
            SetCell(m_troopActionLanes, row, 1, ToQString(ToString(decision.m_state)));
            SetCell(m_troopActionLanes, row, 2, ValueOrNone(decision.m_blockerIds));
            SetCell(m_troopActionLanes, row, 3, ValueOrNone(decision.m_reasons));
        }
        m_troopActionLanes->resizeRowsToContents();
    }

    void ActorTroopEditorWidget::LoadCurrentActor()
    {
        m_loadingActor = true;
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const AZStd::string recordId = ToAzString(
            m_actorRecord->currentData().toString());
        const CatalogRecord* record = catalog.FindByRecordId(recordId);

        AZStd::vector<AZStd::string> selectedEvidence;
        {
            const QSignalBlocker templateBlocker(m_actorTemplateRecord);
            const QSignalBlocker subjectBlocker(m_actorTemplateSubject);
            if (!record)
            {
                m_actorIdentity->setText(
                    tr("Empty state: no canonical population actor is selected."));
                m_actorKind->setCurrentIndex(0);
                m_actorArchetype->clear();
                m_actorTemplateRecord->setCurrentIndex(0);
                m_actorTemplateSubject->clear();
                m_actorMinimumLevel->setValue(0);
                m_actorMaximumLevel->setValue(0);
                m_actorUnique->setChecked(false);
                m_actorEssential->setChecked(false);
                m_actorPersistent->setChecked(false);
                m_actorNameRef->clear();
                m_actorDescriptionRef->clear();
                m_actorPortraitRef->clear();
                m_actorModelRef->clear();
                m_actorTags->clear();
            }
            else
            {
                m_actorIdentity->setText(IdentitySummary(*record));
                const PopulationActorProfile* profile =
                    catalog.FindPopulationActorProfile(recordId);
                if (profile)
                {
                    const int kindIndex = m_actorKind->findText(
                        ToQString(profile->m_actorKind));
                    m_actorKind->setCurrentIndex(kindIndex >= 0 ? kindIndex : 0);
                    m_actorArchetype->setText(ToQString(profile->m_archetype));
                    const int templateIndex = m_actorTemplateRecord->findData(
                        ToQString(profile->m_templateRecordId));
                    m_actorTemplateRecord->setCurrentIndex(
                        templateIndex >= 0 ? templateIndex : 0);
                    m_actorTemplateSubject->setText(
                        ToQString(profile->m_templateSubjectRef));
                    m_actorMinimumLevel->setValue(
                        static_cast<int>(profile->m_minimumLevel));
                    m_actorMaximumLevel->setValue(
                        static_cast<int>(profile->m_maximumLevel));
                    m_actorUnique->setChecked(profile->m_uniqueActor);
                    m_actorEssential->setChecked(profile->m_essentialActor);
                    m_actorPersistent->setChecked(profile->m_persistentActor);
                    m_actorNameRef->setText(
                        ToQString(profile->m_localisationNameRef));
                    m_actorDescriptionRef->setText(
                        ToQString(profile->m_localisationDescriptionRef));
                    m_actorPortraitRef->setText(
                        ToQString(profile->m_portraitAssetRef));
                    m_actorModelRef->setText(
                        ToQString(profile->m_modelAssetRef));
                    m_actorTags->setText(JoinValues(profile->m_tags));
                    selectedEvidence = profile->m_evidenceIds;
                    SetActorState(
                        tr("Loaded state: the persisted typed actor profile is ready for review or editing."));
                }
                else
                {
                    m_actorKind->setCurrentIndex(0);
                    m_actorArchetype->clear();
                    m_actorTemplateRecord->setCurrentIndex(0);
                    m_actorTemplateSubject->clear();
                    m_actorMinimumLevel->setValue(0);
                    m_actorMaximumLevel->setValue(0);
                    m_actorUnique->setChecked(false);
                    m_actorEssential->setChecked(false);
                    m_actorPersistent->setChecked(false);
                    m_actorNameRef->clear();
                    m_actorDescriptionRef->clear();
                    m_actorPortraitRef->clear();
                    m_actorModelRef->clear();
                    m_actorTags->clear();
                    selectedEvidence = record->m_evidenceIds;
                    SetActorState(
                        tr(
                            "Draft state: this canonical actor has no typed "
                            "profile. Complete the form and select exact-subject "
                            "evidence."));
                }
            }
        }

        const CatalogRecord* templateRecord = record
            ? catalog.FindByRecordId(ToAzString(
                  m_actorTemplateRecord->currentData().toString()))
            : nullptr;
        PopulateEvidenceSelector(
            m_actorEvidence,
            RecordSubjects(
                record,
                templateRecord,
                ToAzString(m_actorTemplateSubject->text())),
            selectedEvidence,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_actorEvidence, m_actorEvidenceDetails);
        RefreshActorReviewContext(record);
        RefreshActorActionLanes(record);
        UpdateEnabledStates();
        m_loadedActorRecordId = recordId;
        m_actorDirty = false;
        m_loadingActor = false;

        if (!record)
        {
            SetActorState(
                m_actorRecord->count() <= 1
                    ? tr("Empty state: no canonical population/actor records match the current catalog and filter.")
                    : tr("Empty state: select a canonical population/actor record."));
            return;
        }

        if (record->IsBlocked())
        {
            SetActorState(
                tr(
                    "Blocked state: the canonical actor has unresolved or "
                    "conflicting record references. Review Validation, Blockers, "
                    "and Action Lanes before saving."));
        }

        for (const BlockerRecord& blocker : foundation.GetSnapshot().m_blockers)
        {
            if (blocker.m_status == "open"
                && (blocker.m_subjectRef == record->m_recordId
                    || blocker.m_subjectRef == record->m_subjectRef
                    || blocker.m_subjectRef
                        == "record:" + record->m_recordId))
            {
                SetActorState(
                    tr(
                        "Blocked state: one or more open blockers affect this "
                        "actor. Review the Blockers and Action Lanes tabs before "
                        "saving."));
                break;
            }
        }
    }

    void ActorTroopEditorWidget::LoadCurrentTroop()
    {
        m_loadingTroop = true;
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const AZStd::string recordId = ToAzString(
            m_troopRecord->currentData().toString());
        const CatalogRecord* record = catalog.FindByRecordId(recordId);

        AZStd::vector<AZStd::string> selectedEvidence;
        m_draftMembers.clear();
        m_selectedMemberLinkId.clear();
        {
            const QSignalBlocker leaderBlocker(m_troopLeaderRecord);
            const QSignalBlocker subjectBlocker(m_troopLeaderSubject);
            if (!record)
            {
                m_troopIdentity->setText(
                    tr("Empty state: no canonical population troop is selected."));
                m_troopKind->setCurrentIndex(0);
                m_troopLeaderRecord->setCurrentIndex(0);
                m_troopLeaderSubject->clear();
                m_troopMinimumSize->setValue(1);
                m_troopMaximumSize->setValue(1);
                m_troopFormation->clear();
                m_troopTags->clear();
            }
            else
            {
                m_troopIdentity->setText(IdentitySummary(*record));
                const PopulationTroopProfile* profile =
                    catalog.FindPopulationTroopProfile(recordId);
                if (profile)
                {
                    const int kindIndex = m_troopKind->findText(
                        ToQString(profile->m_troopKind));
                    m_troopKind->setCurrentIndex(kindIndex >= 0 ? kindIndex : 0);
                    const int leaderIndex = m_troopLeaderRecord->findData(
                        ToQString(profile->m_leaderActorRecordId));
                    m_troopLeaderRecord->setCurrentIndex(
                        leaderIndex >= 0 ? leaderIndex : 0);
                    m_troopLeaderSubject->setText(
                        ToQString(profile->m_leaderActorSubjectRef));
                    m_troopMinimumSize->setValue(
                        static_cast<int>(profile->m_minimumSize));
                    m_troopMaximumSize->setValue(
                        static_cast<int>(profile->m_maximumSize));
                    m_troopFormation->setText(ToQString(profile->m_formation));
                    m_troopTags->setText(JoinValues(profile->m_tags));
                    selectedEvidence = profile->m_evidenceIds;
                    m_draftMembers = catalog.FindPopulationMembersForTroop(recordId);
                    SetTroopState(
                        tr(
                            "Loaded state: the complete persisted troop definition "
                            "is copied into an atomic local draft."));
                }
                else
                {
                    m_troopKind->setCurrentIndex(0);
                    m_troopLeaderRecord->setCurrentIndex(0);
                    m_troopLeaderSubject->clear();
                    m_troopMinimumSize->setValue(1);
                    m_troopMaximumSize->setValue(1);
                    m_troopFormation->clear();
                    m_troopTags->clear();
                    selectedEvidence = record->m_evidenceIds;
                    SetTroopState(
                        tr(
                            "Draft state: this canonical troop has no typed "
                            "definition. At least one member must be staged before "
                            "the first save."));
                }
            }
        }

        const CatalogRecord* leader = record
            ? catalog.FindByRecordId(ToAzString(
                  m_troopLeaderRecord->currentData().toString()))
            : nullptr;
        PopulateEvidenceSelector(
            m_troopEvidence,
            RecordSubjects(
                record,
                leader,
                ToAzString(m_troopLeaderSubject->text())),
            selectedEvidence,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_troopEvidence, m_troopEvidenceDetails);
        RefreshMemberTable();
        ClearMemberEditor();
        RefreshTroopReviewContext(record);
        RefreshTroopActionLanes(record);
        UpdateEnabledStates();
        m_loadedTroopRecordId = recordId;
        m_troopDirty = false;
        m_memberEditorDirty = false;
        m_loadingTroop = false;

        if (!record)
        {
            SetTroopState(
                m_troopRecord->count() <= 1
                    ? tr("Empty state: no canonical population/troop records match the current catalog and filter.")
                    : tr("Empty state: select a canonical population/troop record."));
            return;
        }

        if (record->IsBlocked())
        {
            SetTroopState(
                tr(
                    "Blocked state: the canonical troop has unresolved or "
                    "conflicting record references. Review Validation, Blockers, "
                    "and Action Lanes before saving."));
        }

        for (const BlockerRecord& blocker : foundation.GetSnapshot().m_blockers)
        {
            if (blocker.m_status == "open"
                && (blocker.m_subjectRef == record->m_recordId
                    || blocker.m_subjectRef == record->m_subjectRef
                    || blocker.m_subjectRef
                        == "record:" + record->m_recordId))
            {
                SetTroopState(
                    tr(
                        "Blocked state: one or more open blockers affect this "
                        "troop. Review the Blockers and Action Lanes tabs before "
                        "saving."));
                break;
            }
        }
    }

    void ActorTroopEditorWidget::RefreshMemberTable()
    {
        const QSignalBlocker blocker(m_memberTable);
        m_memberTable->setRowCount(static_cast<int>(m_draftMembers.size()));
        int selectedRow = -1;
        for (int row = 0; row < static_cast<int>(m_draftMembers.size()); ++row)
        {
            const PopulationTroopMember& member =
                m_draftMembers[static_cast<size_t>(row)];
            const QString actor = member.m_actorRecordId.empty()
                ? ToQString(member.m_actorSubjectRef)
                : ToQString(member.m_actorRecordId);
            SetCell(m_memberTable, row, 0, ToQString(member.m_linkId));
            SetCell(m_memberTable, row, 1, actor);
            SetCell(m_memberTable, row, 2, ToQString(member.m_role));
            SetCell(m_memberTable, row, 3, QString::number(member.m_minimumCount));
            SetCell(m_memberTable, row, 4, QString::number(member.m_maximumCount));
            SetCell(
                m_memberTable,
                row,
                5,
                QString::number(member.m_weight, 'g', 17));
            SetCell(m_memberTable, row, 6, member.m_required ? tr("Yes") : tr("No"));
            SetCell(m_memberTable, row, 7, ValueOrNone(member.m_evidenceIds));
            if (member.m_linkId == m_selectedMemberLinkId)
            {
                selectedRow = row;
            }
        }
        if (selectedRow >= 0)
        {
            m_memberTable->setCurrentCell(selectedRow, 0);
            m_memberTable->selectRow(selectedRow);
        }
        m_memberTable->resizeRowsToContents();
    }

    void ActorTroopEditorWidget::LoadSelectedMember()
    {
        const int row = m_memberTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_draftMembers.size()))
        {
            return;
        }

        m_loadingMember = true;
        const PopulationTroopMember& member =
            m_draftMembers[static_cast<size_t>(row)];
        m_selectedMemberLinkId = member.m_linkId;
        {
            const QSignalBlocker linkBlocker(m_memberLinkId);
            const QSignalBlocker actorBlocker(m_memberActorRecord);
            const QSignalBlocker subjectBlocker(m_memberActorSubject);
            m_memberLinkId->setText(ToQString(member.m_linkId));
            m_memberLinkId->setReadOnly(true);
            const int actorIndex = m_memberActorRecord->findData(
                ToQString(member.m_actorRecordId));
            m_memberActorRecord->setCurrentIndex(
                actorIndex >= 0 ? actorIndex : 0);
            m_memberActorSubject->setText(ToQString(member.m_actorSubjectRef));
            const int roleIndex = m_memberRole->findText(ToQString(member.m_role));
            m_memberRole->setCurrentIndex(roleIndex >= 0 ? roleIndex : 0);
            m_memberMinimumCount->setValue(
                static_cast<int>(member.m_minimumCount));
            m_memberMaximumCount->setValue(
                static_cast<int>(member.m_maximumCount));
            m_memberWeight->setText(QString::number(member.m_weight, 'g', 17));
            m_memberRequired->setChecked(member.m_required);
            m_memberConditions->setText(JoinValues(member.m_conditions));
        }

        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* troop = catalog.FindByRecordId(
            ToAzString(m_troopRecord->currentData().toString()));
        const CatalogRecord* actor = catalog.FindByRecordId(member.m_actorRecordId);
        AZStd::vector<AZStd::string> subjects = RecordSubjects(
            troop,
            actor,
            member.m_actorSubjectRef);
        AddUnique(subjects, "population-troop-member:" + member.m_linkId);
        PopulateEvidenceSelector(
            m_memberEvidence,
            subjects,
            member.m_evidenceIds,
            foundation.GetSourceRegistry());
        RefreshEvidenceDetails(m_memberEvidence, m_memberEvidenceDetails);
        m_memberEditorDirty = false;
        m_loadingMember = false;
        SetTroopState(
            tr("Draft state: editing staged member %1. Its stable link ID cannot be changed or moved to another troop.")
                .arg(ToQString(member.m_linkId)));
    }

    void ActorTroopEditorWidget::ClearMemberEditor()
    {
        const bool userInitiated = !m_refreshing && !m_loadingTroop;
        const bool wasLoadingMember = m_loadingMember;
        m_loadingMember = true;
        {
            const QSignalBlocker tableBlocker(m_memberTable);
            m_memberTable->clearSelection();
            m_memberTable->setCurrentItem(nullptr);
        }
        m_selectedMemberLinkId.clear();
        {
            const QSignalBlocker linkBlocker(m_memberLinkId);
            const QSignalBlocker actorBlocker(m_memberActorRecord);
            const QSignalBlocker subjectBlocker(m_memberActorSubject);
            m_memberLinkId->setReadOnly(false);
            m_memberLinkId->clear();
            m_memberActorRecord->setCurrentIndex(0);
            m_memberActorSubject->clear();
            m_memberRole->setCurrentIndex(0);
            m_memberMinimumCount->setValue(1);
            m_memberMaximumCount->setValue(1);
            m_memberWeight->setText(QStringLiteral("1"));
            m_memberRequired->setChecked(false);
            m_memberConditions->clear();
        }
        PopulateEvidenceSelector(
            m_memberEvidence,
            {},
            {},
            FoundationService::Get().GetSourceRegistry());
        RefreshEvidenceDetails(m_memberEvidence, m_memberEvidenceDetails);
        m_memberEditorDirty = false;
        m_loadingMember = wasLoadingMember;
        if (userInitiated)
        {
            SetStatus(
                tr(
                    "Member editor cleared explicitly. Staged troop members were "
                    "not removed or changed."));
        }
    }

    PopulationActorProfile ActorTroopEditorWidget::BuildActorProfile() const
    {
        PopulationActorProfile profile;
        profile.m_recordId = ToAzString(
            m_actorRecord->currentData().toString());
        profile.m_actorKind = ToAzString(m_actorKind->currentText());
        profile.m_archetype = ToAzString(m_actorArchetype->text());
        profile.m_templateRecordId = ToAzString(
            m_actorTemplateRecord->currentData().toString());
        profile.m_templateSubjectRef = ToAzString(
            m_actorTemplateSubject->text());
        profile.m_minimumLevel = static_cast<AZ::u32>(
            m_actorMinimumLevel->value());
        profile.m_maximumLevel = static_cast<AZ::u32>(
            m_actorMaximumLevel->value());
        profile.m_uniqueActor = m_actorUnique->isChecked();
        profile.m_essentialActor = m_actorEssential->isChecked();
        profile.m_persistentActor = m_actorPersistent->isChecked();
        profile.m_localisationNameRef = ToAzString(m_actorNameRef->text());
        profile.m_localisationDescriptionRef = ToAzString(
            m_actorDescriptionRef->text());
        profile.m_portraitAssetRef = ToAzString(m_actorPortraitRef->text());
        profile.m_modelAssetRef = ToAzString(m_actorModelRef->text());
        profile.m_tags = ParseCommaSeparated(m_actorTags->text());
        profile.m_evidenceIds = SelectedEvidenceIds(m_actorEvidence);
        return profile;
    }

    PopulationTroopProfile ActorTroopEditorWidget::BuildTroopProfile() const
    {
        PopulationTroopProfile profile;
        profile.m_recordId = ToAzString(
            m_troopRecord->currentData().toString());
        profile.m_troopKind = ToAzString(m_troopKind->currentText());
        profile.m_leaderActorRecordId = ToAzString(
            m_troopLeaderRecord->currentData().toString());
        profile.m_leaderActorSubjectRef = ToAzString(
            m_troopLeaderSubject->text());
        profile.m_minimumSize = static_cast<AZ::u32>(
            m_troopMinimumSize->value());
        profile.m_maximumSize = static_cast<AZ::u32>(
            m_troopMaximumSize->value());
        profile.m_formation = ToAzString(m_troopFormation->text());
        profile.m_tags = ParseCommaSeparated(m_troopTags->text());
        profile.m_evidenceIds = SelectedEvidenceIds(m_troopEvidence);
        return profile;
    }

    PopulationTroopMember ActorTroopEditorWidget::BuildMember() const
    {
        PopulationTroopMember member;
        member.m_linkId = ToAzString(m_memberLinkId->text());
        member.m_troopRecordId = ToAzString(
            m_troopRecord->currentData().toString());
        member.m_actorRecordId = ToAzString(
            m_memberActorRecord->currentData().toString());
        member.m_actorSubjectRef = ToAzString(m_memberActorSubject->text());
        member.m_role = ToAzString(m_memberRole->currentText());
        member.m_minimumCount = static_cast<AZ::u32>(
            m_memberMinimumCount->value());
        member.m_maximumCount = static_cast<AZ::u32>(
            m_memberMaximumCount->value());
        member.m_weight = m_memberWeight->text().trimmed().toDouble();
        member.m_required = m_memberRequired->isChecked();
        member.m_conditions = ParseCommaSeparated(m_memberConditions->text());
        member.m_evidenceIds = SelectedEvidenceIds(m_memberEvidence);
        return member;
    }

    bool ActorTroopEditorWidget::StageMember()
    {
        if (m_troopRecord->currentData().toString().isEmpty())
        {
            SetTroopState(
                tr("Invalid draft: select a canonical population/troop record before staging a member."),
                true);
            return false;
        }

        bool weightParsed = false;
        const double weight = m_memberWeight->text().trimmed().toDouble(
            &weightParsed);
        if (!weightParsed || !std::isfinite(weight) || weight < 0.0)
        {
            SetTroopState(
                tr(
                    "Invalid member weight: enter any finite non-negative number. "
                    "The editor does not silently clamp the Core contract."),
                true);
            return false;
        }

        const PopulationTroopMember member = BuildMember();
        if (member.m_linkId.empty())
        {
            SetTroopState(
                tr("Invalid member draft: a stable membership link ID is required."),
                true);
            return false;
        }
        if (!m_selectedMemberLinkId.empty()
            && member.m_linkId != m_selectedMemberLinkId)
        {
            SetTroopState(
                tr(
                    "Invalid member draft: membership link identity is immutable. "
                    "Clear the member editor to stage a different link."),
                true);
            return false;
        }
        if (member.m_actorRecordId.empty()
            && member.m_actorSubjectRef.empty())
        {
            SetTroopState(
                tr("Invalid member draft: choose a resolved actor or enter one exact unresolved actor subject."),
                true);
            return false;
        }
        if (member.m_minimumCount > member.m_maximumCount)
        {
            SetTroopState(
                tr("Invalid member draft: minimum count cannot exceed maximum count."),
                true);
            return false;
        }
        if (member.m_evidenceIds.empty())
        {
            SetTroopState(
                tr("Invalid member draft: select at least one exact-subject evidence record."),
                true);
            return false;
        }

        const AZStd::string binding = member.m_actorRecordId.empty()
            ? member.m_actorSubjectRef
            : member.m_actorRecordId;
        for (const PopulationTroopMember& existing : m_draftMembers)
        {
            const AZStd::string existingBinding =
                existing.m_actorRecordId.empty()
                ? existing.m_actorSubjectRef
                : existing.m_actorRecordId;
            if (existing.m_linkId != member.m_linkId
                && existingBinding == binding)
            {
                SetTroopState(
                    tr("Invalid member draft: actor binding %1 already appears under membership link %2.")
                        .arg(ToQString(binding))
                        .arg(ToQString(existing.m_linkId)),
                    true);
                return false;
            }
        }

        bool replaced = false;
        for (PopulationTroopMember& existing : m_draftMembers)
        {
            if (existing.m_linkId == member.m_linkId)
            {
                existing = member;
                replaced = true;
                break;
            }
        }
        if (!replaced)
        {
            m_draftMembers.push_back(member);
        }
        AZStd::sort(
            m_draftMembers.begin(),
            m_draftMembers.end(),
            [](const PopulationTroopMember& left,
               const PopulationTroopMember& right)
            {
                return left.m_linkId < right.m_linkId;
            });
        m_selectedMemberLinkId = member.m_linkId;
        RefreshMemberTable();
        LoadSelectedMember();
        m_troopDirty = true;
        m_memberEditorDirty = false;
        SetTroopState(
            tr(
                "Draft state: member %1 is staged locally. Save Atomic Troop "
                "Definition to validate, persist, and publish the whole additive "
                "definition.")
                .arg(ToQString(member.m_linkId)));
        return true;
    }

    void ActorTroopEditorWidget::SaveActorProfile()
    {
        if (m_actorRecord->currentData().toString().isEmpty())
        {
            SetActorState(
                tr("Invalid draft: select a canonical population/actor record before saving."),
                true);
            return;
        }

        const PopulationActorProfile profile = BuildActorProfile();
        AZStd::string error;
        if (!FoundationService::Get().UpsertPopulationActorProfile(
                profile,
                &error))
        {
            SetActorState(
                tr("Save failed: %1").arg(ToQString(error)),
                true);
            SetStatus(
                tr("Actor profile was not published. Candidate validation or persistence failed: %1")
                    .arg(ToQString(error)),
                true);
            return;
        }
        m_actorDirty = false;
        m_foundationRefreshPending = true;
        ApplyPendingFoundationRefresh();
        SetActorState(
            tr("Saved state: the actor profile was validated, persisted, and then published."));
        SetStatus(
            tr("Actor profile saved through the evidence-bound Foundation transaction boundary."));
    }

    void ActorTroopEditorWidget::SaveTroopDefinition()
    {
        const AZStd::string troopRecordId = ToAzString(
            m_troopRecord->currentData().toString());
        if (troopRecordId.empty())
        {
            SetTroopState(
                tr("Invalid draft: select a canonical population/troop record before saving."),
                true);
            return;
        }

        // An edited member form is part of the current local draft. Stage it
        // before the one atomic Foundation call so no visible edit is ignored.
        if (m_memberEditorDirty && !StageMember())
        {
            return;
        }

        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const bool newTroop =
            catalog.FindPopulationTroopProfile(troopRecordId) == nullptr;
        if (m_draftMembers.empty())
        {
            SetTroopState(
                newTroop
                    ? tr("Invalid new troop: stage at least one evidence-bound member before the first atomic save.")
                    : tr("Invalid troop draft: the published troop definition has no members to preserve."),
                true);
            return;
        }

        PopulationTroopDefinition definition;
        definition.m_profile = BuildTroopProfile();
        definition.m_members = m_draftMembers;
        AZStd::string error;
        if (!FoundationService::Get().UpsertPopulationTroopDefinition(
                definition,
                &error))
        {
            SetTroopState(
                tr("Atomic save failed: %1").arg(ToQString(error)),
                true);
            SetStatus(
                tr("Troop definition was not published. The complete local draft remains available: %1")
                    .arg(ToQString(error)),
                true);
            return;
        }

        m_troopDirty = false;
        m_memberEditorDirty = false;
        m_foundationRefreshPending = true;
        ApplyPendingFoundationRefresh();
        SetTroopState(
            tr(
                "Saved state: the complete troop profile and additive member draft "
                "were validated, persisted, and then published atomically."));
        SetStatus(
            tr(
                "Atomic troop definition saved. Existing members were preserved; "
                "no removal or runtime authority was introduced."));
    }

    void ActorTroopEditorWidget::RevertActorProfile()
    {
        m_actorDirty = false;
        LoadCurrentActor();
        ApplyPendingFoundationRefresh();
        SetStatus(
            tr("Actor draft reverted to the currently published catalog state."));
    }

    void ActorTroopEditorWidget::RevertTroopDefinition()
    {
        m_troopDirty = false;
        m_memberEditorDirty = false;
        LoadCurrentTroop();
        ApplyPendingFoundationRefresh();
        SetStatus(
            tr("Troop profile and member draft reverted to the currently published catalog state."));
    }

    void ActorTroopEditorWidget::SetActorState(
        const QString& message,
        bool error)
    {
        m_actorState->setText(
            error ? tr("Error: %1").arg(message) : message);
        m_actorState->setStyleSheet(
            error ? QStringLiteral("color: #d9534f;") : QString());
    }

    void ActorTroopEditorWidget::SetTroopState(
        const QString& message,
        bool error)
    {
        m_troopState->setText(
            error ? tr("Error: %1").arg(message) : message);
        m_troopState->setStyleSheet(
            error ? QStringLiteral("color: #d9534f;") : QString());
    }

    void ActorTroopEditorWidget::SetStatus(
        const QString& message,
        bool error)
    {
        m_status->setText(
            error ? tr("Error: %1").arg(message)
                  : tr("Status: %1").arg(message));
        m_status->setStyleSheet(
            error ? QStringLiteral("color: #d9534f;") : QString());
    }

    void ActorTroopEditorWidget::UpdateEnabledStates()
    {
        const bool actorSelected =
            !m_actorRecord->currentData().toString().isEmpty();
        m_actorKind->setEnabled(actorSelected);
        m_actorArchetype->setEnabled(actorSelected);
        m_actorTemplateRecord->setEnabled(actorSelected);
        m_actorTemplateSubject->setEnabled(actorSelected);
        m_actorMinimumLevel->setEnabled(actorSelected);
        m_actorMaximumLevel->setEnabled(actorSelected);
        m_actorUnique->setEnabled(actorSelected);
        m_actorEssential->setEnabled(actorSelected);
        m_actorPersistent->setEnabled(actorSelected);
        m_actorNameRef->setEnabled(actorSelected);
        m_actorDescriptionRef->setEnabled(actorSelected);
        m_actorPortraitRef->setEnabled(actorSelected);
        m_actorModelRef->setEnabled(actorSelected);
        m_actorTags->setEnabled(actorSelected);
        m_actorEvidence->setEnabled(actorSelected);
        m_saveActor->setEnabled(actorSelected);
        m_revertActor->setEnabled(actorSelected);

        const bool troopSelected =
            !m_troopRecord->currentData().toString().isEmpty();
        m_troopKind->setEnabled(troopSelected);
        m_troopLeaderRecord->setEnabled(troopSelected);
        m_troopLeaderSubject->setEnabled(troopSelected);
        m_troopMinimumSize->setEnabled(troopSelected);
        m_troopMaximumSize->setEnabled(troopSelected);
        m_troopFormation->setEnabled(troopSelected);
        m_troopTags->setEnabled(troopSelected);
        m_troopEvidence->setEnabled(troopSelected);
        m_memberTable->setEnabled(troopSelected);
        m_memberLinkId->setEnabled(troopSelected);
        m_memberActorRecord->setEnabled(troopSelected);
        m_memberActorSubject->setEnabled(troopSelected);
        m_memberRole->setEnabled(troopSelected);
        m_memberMinimumCount->setEnabled(troopSelected);
        m_memberMaximumCount->setEnabled(troopSelected);
        m_memberWeight->setEnabled(troopSelected);
        m_memberRequired->setEnabled(troopSelected);
        m_memberConditions->setEnabled(troopSelected);
        m_memberEvidence->setEnabled(troopSelected);
        m_stageMember->setEnabled(troopSelected);
        m_clearMember->setEnabled(troopSelected);
        m_saveTroop->setEnabled(troopSelected);
        m_revertTroop->setEnabled(troopSelected);
    }
} // namespace TaintedGrailModdingSDK
