/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopEditorWidget.h"

#include "FoundationService.h"

#include <AzCore/std/algorithm.h>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
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
    } // namespace

    QWidget* ActorTroopEditorWidget::BuildActorTab(QWidget* parent)
    {
        auto* content = new QWidget(parent);
        auto* layout = new QVBoxLayout(content);

        auto* recordGroup = new QGroupBox(tr("Canonical Actor"), content);
        auto* recordLayout = new QFormLayout(recordGroup);
        m_actorFilter = new QLineEdit(recordGroup);
        m_actorFilter->setPlaceholderText(
            tr("Filter by ID, name, subject, exact native ref, or pack"));
        m_actorRecord = new QComboBox(recordGroup);
        m_actorIdentity = new QLabel(recordGroup);
        m_actorIdentity->setWordWrap(true);
        m_actorIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_actorState = new QLabel(recordGroup);
        m_actorState->setWordWrap(true);
        m_actorState->setTextInteractionFlags(Qt::TextSelectableByMouse);
        recordLayout->addRow(tr("&Actor filter"), m_actorFilter);
        recordLayout->addRow(tr("Actor &record"), m_actorRecord);
        recordLayout->addRow(tr("Exact identity and governance"), m_actorIdentity);
        recordLayout->addRow(tr("Authoring state"), m_actorState);
        layout->addWidget(recordGroup);

        auto* profileGroup = new QGroupBox(tr("Typed Actor Profile"), content);
        auto* profileLayout = new QFormLayout(profileGroup);
        m_actorKind = new QComboBox(profileGroup);
        m_actorKind->addItems({
            QStringLiteral("npc"),
            QStringLiteral("creature"),
            QStringLiteral("animal"),
            QStringLiteral("construct"),
            QStringLiteral("other"),
        });
        m_actorArchetype = new QLineEdit(profileGroup);
        m_actorArchetype->setPlaceholderText(tr("Evidence-backed bounded classification"));
        m_actorTemplateRecord = new QComboBox(profileGroup);
        m_actorTemplateSubject = new QLineEdit(profileGroup);
        m_actorTemplateSubject->setPlaceholderText(
            tr("Exact unresolved template subject, preserving case"));
        m_actorMinimumLevel = new QSpinBox(profileGroup);
        m_actorMinimumLevel->setRange(0, 1000);
        m_actorMaximumLevel = new QSpinBox(profileGroup);
        m_actorMaximumLevel->setRange(0, 1000);
        m_actorUnique = new QCheckBox(
            tr("Unique actor planning claim"),
            profileGroup);
        m_actorEssential = new QCheckBox(
            tr("Essential-state planning claim"),
            profileGroup);
        m_actorPersistent = new QCheckBox(
            tr("Persistence planning claim"),
            profileGroup);
        m_actorNameRef = new QLineEdit(profileGroup);
        m_actorDescriptionRef = new QLineEdit(profileGroup);
        m_actorPortraitRef = new QLineEdit(profileGroup);
        m_actorModelRef = new QLineEdit(profileGroup);
        m_actorTags = new QLineEdit(profileGroup);
        m_actorTags->setPlaceholderText(tr("companion, merchant, quest-sensitive"));

        profileLayout->addRow(tr("Actor &kind"), m_actorKind);
        profileLayout->addRow(tr("&Archetype"), m_actorArchetype);
        profileLayout->addRow(tr("Resolved &template record"), m_actorTemplateRecord);
        profileLayout->addRow(tr("Exact template subject"), m_actorTemplateSubject);
        profileLayout->addRow(tr("Minimum level (0 = unspecified)"), m_actorMinimumLevel);
        profileLayout->addRow(tr("Maximum level (0 = unspecified)"), m_actorMaximumLevel);
        profileLayout->addRow(m_actorUnique);
        profileLayout->addRow(m_actorEssential);
        profileLayout->addRow(m_actorPersistent);
        profileLayout->addRow(tr("Name localisation ref"), m_actorNameRef);
        profileLayout->addRow(tr("Description localisation ref"), m_actorDescriptionRef);
        profileLayout->addRow(tr("Portrait asset ref"), m_actorPortraitRef);
        profileLayout->addRow(tr("Model asset ref"), m_actorModelRef);
        profileLayout->addRow(tr("Tags"), m_actorTags);
        layout->addWidget(profileGroup);

        auto* evidenceGroup = new QGroupBox(
            tr("Exact Actor / Template Evidence"),
            content);
        auto* evidenceLayout = new QVBoxLayout(evidenceGroup);
        auto* evidenceDescription = new QLabel(
            tr("Only active-profile evidence for the selected actor and current template binding is offered. "
               "Selected evidence remains subject to full source, fingerprint, provenance, and chronology checks."),
            evidenceGroup);
        evidenceDescription->setWordWrap(true);
        evidenceLayout->addWidget(evidenceDescription);
        m_actorEvidenceFilter = new QLineEdit(evidenceGroup);
        m_actorEvidenceFilter->setPlaceholderText(
            tr("Filter matching evidence by ID, subject, claim, source, or locator"));
        evidenceLayout->addWidget(m_actorEvidenceFilter);
        m_actorEvidence = new QListWidget(evidenceGroup);
        m_actorEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_actorEvidence->setMinimumHeight(140);
        evidenceLayout->addWidget(m_actorEvidence);
        m_actorEvidenceDetails = new QTableWidget(0, 8, evidenceGroup);
        m_actorEvidenceDetails->setHorizontalHeaderLabels({
            tr("Evidence ID"),
            tr("Exact subject"),
            tr("Claim"),
            tr("Kind"),
            tr("Confidence"),
            tr("Source"),
            tr("Locator / record path"),
            tr("Provenance state"),
        });
