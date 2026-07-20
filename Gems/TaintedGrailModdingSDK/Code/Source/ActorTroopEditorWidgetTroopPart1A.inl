/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopEditorWidget.h"

#include "FoundationService.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void SetComboByData(QComboBox* combo, const QString& value)
        {
            const int index = combo->findData(value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
        }

        void SetComboByText(QComboBox* combo, const QString& value)
        {
            const int index = combo->findText(value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
        }

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }
    } // namespace

    QWidget* ActorTroopEditorWidget::BuildTroopTab(QWidget* parent)
    {
        auto* content = new QWidget(parent);
        auto* layout = new QVBoxLayout(content);

        auto* recordGroup = new QGroupBox(tr("Canonical Troop"), content);
        auto* recordLayout = new QFormLayout(recordGroup);
        m_troopFilter = new QLineEdit(recordGroup);
        m_troopFilter->setPlaceholderText(
            tr("Filter by ID, name, subject, exact native ref, or pack"));
        m_troopRecord = new QComboBox(recordGroup);
        m_troopIdentity = new QLabel(recordGroup);
        m_troopIdentity->setWordWrap(true);
        m_troopIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_troopState = new QLabel(recordGroup);
        m_troopState->setWordWrap(true);
        m_troopState->setTextInteractionFlags(Qt::TextSelectableByMouse);
        recordLayout->addRow(tr("&Troop filter"), m_troopFilter);
        recordLayout->addRow(tr("Troop &record"), m_troopRecord);
        recordLayout->addRow(tr("Exact identity and governance"), m_troopIdentity);
        recordLayout->addRow(tr("Authoring state"), m_troopState);
        layout->addWidget(recordGroup);

        auto* profileGroup = new QGroupBox(tr("Typed Troop Profile"), content);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_troopKind = new QComboBox(profileGroup);
        m_troopKind->addItems({
            QStringLiteral("party"),
            QStringLiteral("patrol"),
            QStringLiteral("encounter_group"),
            QStringLiteral("reinforcement"),
            QStringLiteral("other"),
        });
        m_troopLeaderRecord = new QComboBox(profileGroup);
        m_troopLeaderSubject = new QLineEdit(profileGroup);
        m_troopLeaderSubject->setPlaceholderText(
            tr("Exact unresolved leader subject, preserving case"));
        m_troopMinimumSize = new QSpinBox(profileGroup);
        m_troopMinimumSize->setRange(1, 1000);
        m_troopMaximumSize = new QSpinBox(profileGroup);
        m_troopMaximumSize->setRange(1, 1000);
        m_troopFormation = new QLineEdit(profileGroup);
        m_troopFormation->setPlaceholderText(tr("Optional bounded planning label"));
        m_troopTags = new QLineEdit(profileGroup);
        m_troopTags->setPlaceholderText(tr("patrol, night, faction-neutral"));
        profileLayout->addRow(tr("Troop &kind"), m_troopKind);
        profileLayout->addRow(tr("Resolved &leader actor"), m_troopLeaderRecord);
        profileLayout->addRow(tr("Exact leader subject"), m_troopLeaderSubject);
        profileLayout->addRow(tr("Minimum troop size"), m_troopMinimumSize);
        profileLayout->addRow(tr("Maximum troop size"), m_troopMaximumSize);
        profileLayout->addRow(tr("Formation"), m_troopFormation);
        profileLayout->addRow(tr("Tags"), m_troopTags);
        layout->addWidget(profileGroup);

        auto* troopEvidenceGroup = new QGroupBox(
            tr("Exact Troop / Leader Evidence"),
            content);
        auto* troopEvidenceLayout = new QVBoxLayout(troopEvidenceGroup);
        auto* troopEvidenceDescription = new QLabel(
            tr("Select active-profile evidence for the canonical troop and optional leader binding. "
               "A display-name match is never accepted as an identity match."),
            troopEvidenceGroup);
        troopEvidenceDescription->setWordWrap(true);
        troopEvidenceLayout->addWidget(troopEvidenceDescription);
        m_troopEvidenceFilter = new QLineEdit(troopEvidenceGroup);
        m_troopEvidenceFilter->setPlaceholderText(
            tr("Filter matching troop evidence"));
        troopEvidenceLayout->addWidget(m_troopEvidenceFilter);
        m_troopEvidence = new QListWidget(troopEvidenceGroup);
        m_troopEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_troopEvidence->setMinimumHeight(130);
        troopEvidenceLayout->addWidget(m_troopEvidence);
        layout->addWidget(troopEvidenceGroup);

        auto* memberGroup = new QGroupBox(
            tr("Atomic Troop Member Staging"),
            content);
        auto* memberLayout = new QVBoxLayout(memberGroup);
        auto* memberDescription = new QLabel(
            tr("Add or update rows in this staged member set. Newly staged rows may be discarded before "
               "their first save, but removal of persisted membership remains deferred. Save Troop Definition "
               "submits the profile and staged row updates atomically while preserving omitted persisted rows."),
            memberGroup);
        memberDescription->setWordWrap(true);
        memberLayout->addWidget(memberDescription);

        m_memberTable = new QTableWidget(0, 8, memberGroup);
        m_memberTable->setHorizontalHeaderLabels({
            tr("Link ID"),
            tr("Actor / unresolved subject"),
            tr("Role"),
            tr("Count range"),
            tr("Weight"),
            tr("Required"),
