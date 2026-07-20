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
#include <AzCore/std/sort.h>
#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        QString RecordLabel(const CatalogRecord& record)
        {
            QString label = QString::fromUtf8(record.m_recordId.c_str());
            if (!record.m_displayName.empty())
            {
                label += QStringLiteral(" — ")
                    + QString::fromUtf8(record.m_displayName.c_str());
            }
            if (!record.m_ownerPackId.empty())
            {
                label += QStringLiteral(" [")
                    + QString::fromUtf8(record.m_ownerPackId.c_str())
                    + QStringLiteral("]");
            }
            return label;
        }

        bool RecordMatchesFilter(
            const CatalogRecord& record,
            const QString& filter)
        {
            if (filter.isEmpty())
            {
                return true;
            }
            const QString haystack = QStringLiteral("%1 %2 %3 %4 %5 %6")
                .arg(QString::fromUtf8(record.m_recordId.c_str()))
                .arg(QString::fromUtf8(record.m_displayName.c_str()))
                .arg(QString::fromUtf8(record.m_subjectRef.c_str()))
                .arg(QString::fromUtf8(record.m_nativeRefExact.c_str()))
                .arg(QString::fromUtf8(record.m_ownerPackId.c_str()))
                .arg(QString::fromUtf8(record.m_identityKind.c_str()));
            return haystack.contains(filter, Qt::CaseInsensitive);
        }

        bool BlockerMatchesRecord(
            const BlockerRecord& blocker,
            const CatalogRecord& record)
        {
            return blocker.m_subjectRef == record.m_subjectRef
                || blocker.m_subjectRef == record.m_recordId
                || blocker.m_subjectRef == "record:" + record.m_recordId;
        }

        QWidget* WrapScrollable(QWidget* content, QWidget* parent)
        {
            auto* scroll = new QScrollArea(parent);
            scroll->setWidgetResizable(true);
            scroll->setWidget(content);
            return scroll;
        }

        QString DecisionValue(const CatalogGovernanceEvent& event)
        {
            QString value = QString::fromUtf8(event.m_newValue.c_str());
            if (!event.m_usage.empty())
            {
                value += QStringLiteral(" / ")
                    + QString::fromUtf8(event.m_usage.c_str());
            }
            return value;
        }
    } // namespace

    ActorTroopEditorWidget::ActorTroopEditorWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            tr("Tainted Grail Actor and Troop Editor"),
            this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        heading->setAccessibleName(tr("Actor and Troop Editor heading"));
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Author evidence-bound actor profiles and atomic troop definitions on existing canonical records. "
               "This pane does not create canonical identities, grant permissions, spawn actors, invoke FoA, "
               "deploy content, or mutate saves."),
            this);
        description->setWordWrap(true);
        description->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rootLayout->addWidget(description);

        m_tabs = new QTabWidget(this);
        m_tabs->setAccessibleName(tr("Actor and troop authoring tabs"));
        m_tabs->addTab(
            WrapScrollable(BuildActorTab(m_tabs), m_tabs),
            tr("Actors"));
        m_tabs->addTab(
            WrapScrollable(BuildTroopTab(m_tabs), m_tabs),
            tr("Troops"));
        rootLayout->addWidget(m_tabs, 1);

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_status->setAccessibleName(tr("Actor and Troop Editor status"));
        rootLayout->addWidget(m_status);

        ConfigureAccessibility();
        FoundationNotificationBus::Handler::BusConnect();
        RefreshAll();
        SetStatus(
            StatusKind::Neutral,
            tr("Select a canonical actor or troop record to begin."));
    }

    ActorTroopEditorWidget::~ActorTroopEditorWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void ActorTroopEditorWidget::OnFoundationChanged()
    {
        if (!m_actorDirty && !m_troopDirty && !m_memberFormDirty)
        {
            RefreshAll();
            return;
        }

        const bool previousRefreshing = m_refreshing;
        m_refreshing = true;
        RefreshRecordChoices();
        if (m_actorDirty)
        {
            RefreshActorEvidenceChoices();
            const AZStd::string actorRecordId =
                ToAzString(m_actorRecord->currentData().toString());
            RefreshReviewTables(
                actorRecordId,
                FoundationService::Get().GetCatalog()
                        .FindPopulationActorProfile(actorRecordId) != nullptr,
                false,
                IsActorEvidenceReady(),
                m_actorValidation,
                m_actorGovernance,
                m_actorBlockers,
                m_actorRelationships,
                m_actorLanes);
        }
        else
        {
            LoadCurrentActor();
        }

        if (m_troopDirty || m_memberFormDirty)
        {
            RefreshTroopEvidenceChoices();
            RefreshMemberEvidenceChoices();
            const AZStd::string troopRecordId =
                ToAzString(m_troopRecord->currentData().toString());
            RefreshReviewTables(
                troopRecordId,
                FoundationService::Get().GetCatalog()
                        .FindPopulationTroopProfile(troopRecordId) != nullptr,
                true,
                IsTroopEvidenceReady(),
                m_troopValidation,
                m_troopGovernance,
                m_troopBlockers,
                m_troopRelationships,
                m_troopLanes);
        }
        else
        {
            LoadCurrentTroop();
        }
        m_refreshing = previousRefreshing;
        SetStatus(
            StatusKind::Warning,
            tr("Foundation state changed while unsaved authoring input was present. "
               "The unsaved form and staged troop members were retained; review "
               "the refreshed evidence and blocker surfaces before saving."));
    }

    AZStd::string ActorTroopEditorWidget::ToAzString(const QString& value)
    {
        const QByteArray utf8 = value.trimmed().toUtf8();
        return AZStd::string(
            utf8.constData(),
            static_cast<size_t>(utf8.size()));
    }

    QString ActorTroopEditorWidget::ToQString(const AZStd::string& value)
    {
        return QString::fromUtf8(
            value.c_str(),
            static_cast<int>(value.size()));
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::ParseCommaSeparated(const QString& value)
    {
        AZStd::vector<AZStd::string> values;
        for (const QString& part : value.split(',', Qt::SkipEmptyParts))
        {
            const AZStd::string converted = ToAzString(part);
            if (!converted.empty() && !Contains(values, converted))
            {
                values.push_back(converted);
            }
        }
        return values;
    }

    QString ActorTroopEditorWidget::JoinValues(
        const AZStd::vector<AZStd::string>& values)
    {
        QStringList result;
        for (const AZStd::string& value : values)
        {
            result.push_back(ToQString(value));
        }
        return result.join(QStringLiteral(", "));
    }

    void ActorTroopEditorWidget::ConfigureTable(
        QTableWidget* table,
        const QString& accessibleName)
    {
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setWordWrap(true);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionsMovable(false);
        table->setAccessibleName(accessibleName);
    }

    void ActorTroopEditorWidget::SetCell(
        QTableWidget* table,
        int row,
        int column,
        const QString& value)
    {
        auto* item = new QTableWidgetItem(value);
        item->setToolTip(value);
        table->setItem(row, column, item);
    }

    void ActorTroopEditorWidget::ConfigureAccessibility()
    {
        m_actorFilter->setAccessibleName(tr("Canonical actor filter"));
        m_actorRecord->setAccessibleName(tr("Canonical actor record"));
        m_actorEvidence->setAccessibleName(tr("Actor exact evidence selection"));
        m_actorSave->setAccessibleDescription(
