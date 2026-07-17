/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogBrowserWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
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
            const QStringList parts = value.split(',', Qt::SkipEmptyParts);
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

        QString JoinValues(const AZStd::vector<AZStd::string>& values, const QString& separator = QStringLiteral(", "))
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(separator);
        }

        void ConfigureReadOnlyTable(QTableWidget* table)
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

        void PopulateTextCombo(QComboBox* combo, const QStringList& values, const QString& anyLabel)
        {
            const QString previous = combo->currentText();
            QSignalBlocker blocker(combo);
            combo->clear();
            combo->addItem(anyLabel, QString());
            QStringList unique = values;
            unique.removeDuplicates();
            unique.sort(Qt::CaseInsensitive);
            for (const QString& value : unique)
            {
                if (!value.isEmpty())
                {
                    combo->addItem(value, value);
                }
            }
            const int previousIndex = combo->findText(previous);
            combo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
        }

        QPlainTextEdit* CreateReadOnlyText(QWidget* parent, int maximumHeight)
        {
            auto* view = new QPlainTextEdit(parent);
            view->setReadOnly(true);
            view->setMaximumHeight(maximumHeight);
            return view;
        }
    } // namespace

    CatalogBrowserWidget::CatalogBrowserWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);

        auto* heading = new QLabel(tr("Tainted Grail Canonical Catalog"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Search exact identities and evidence-backed records. Promotion creates reviewed-but-unvalidated records and never grants runtime permission."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        auto* pathGroup = new QGroupBox(tr("Catalog Document"), this);
        auto* pathLayout = new QFormLayout(pathGroup);
        m_catalogPathValue = new QLabel(pathGroup);
        m_catalogPathValue->setWordWrap(true);
        m_catalogPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pathLayout->addRow(tr("Path"), m_catalogPathValue);
        rootLayout->addWidget(pathGroup);

        auto* filterGroup = new QGroupBox(tr("Search and Filters"), this);
        auto* filterLayout = new QGridLayout(filterGroup);
        m_searchEdit = new QLineEdit(filterGroup);
        m_searchEdit->setPlaceholderText(tr("Display name, alias, GUID, exact ref, subject, tag…"));
        m_recordIdFilter = new QLineEdit(filterGroup);
        m_exactRefFilter = new QLineEdit(filterGroup);
        m_subjectFilter = new QLineEdit(filterGroup);
        m_evidenceFilter = new QLineEdit(filterGroup);
        m_packFilter = new QComboBox(filterGroup);
        m_domainFilter = new QComboBox(filterGroup);
        m_kindFilter = new QComboBox(filterGroup);
        m_identityFilter = new QComboBox(filterGroup);
        m_confidenceFilter = new QComboBox(filterGroup);
        m_validationFilter = new QComboBox(filterGroup);
        m_permissionFilter = new QLineEdit(filterGroup);
        m_blockedOnly = new QCheckBox(tr("Blocked only"), filterGroup);
        m_includeSuperseded = new QCheckBox(tr("Include superseded"), filterGroup);
        auto* searchButton = new QPushButton(tr("Search"), filterGroup);
        auto* reloadButton = new QPushButton(tr("Reload Catalog"), filterGroup);
        auto* saveButton = new QPushButton(tr("Save Catalog"), filterGroup);

        filterLayout->addWidget(new QLabel(tr("Text"), filterGroup), 0, 0);
        filterLayout->addWidget(m_searchEdit, 0, 1, 1, 3);
        filterLayout->addWidget(new QLabel(tr("Record ID"), filterGroup), 1, 0);
        filterLayout->addWidget(m_recordIdFilter, 1, 1);
        filterLayout->addWidget(new QLabel(tr("Exact native ref / GUID"), filterGroup), 1, 2);
        filterLayout->addWidget(m_exactRefFilter, 1, 3);
        filterLayout->addWidget(new QLabel(tr("Subject ref"), filterGroup), 2, 0);
        filterLayout->addWidget(m_subjectFilter, 2, 1);
        filterLayout->addWidget(new QLabel(tr("Evidence ID"), filterGroup), 2, 2);
        filterLayout->addWidget(m_evidenceFilter, 2, 3);
        filterLayout->addWidget(new QLabel(tr("Pack"), filterGroup), 3, 0);
        filterLayout->addWidget(m_packFilter, 3, 1);
        filterLayout->addWidget(new QLabel(tr("Domain"), filterGroup), 3, 2);
        filterLayout->addWidget(m_domainFilter, 3, 3);
        filterLayout->addWidget(new QLabel(tr("Kind"), filterGroup), 4, 0);
        filterLayout->addWidget(m_kindFilter, 4, 1);
        filterLayout->addWidget(new QLabel(tr("Identity"), filterGroup), 4, 2);
        filterLayout->addWidget(m_identityFilter, 4, 3);
        filterLayout->addWidget(new QLabel(tr("Confidence"), filterGroup), 5, 0);
        filterLayout->addWidget(m_confidenceFilter, 5, 1);
        filterLayout->addWidget(new QLabel(tr("Validation"), filterGroup), 5, 2);
        filterLayout->addWidget(m_validationFilter, 5, 3);
        filterLayout->addWidget(new QLabel(tr("Permission / prohibition"), filterGroup), 6, 0);
        filterLayout->addWidget(m_permissionFilter, 6, 1);
        filterLayout->addWidget(m_blockedOnly, 6, 2);
        filterLayout->addWidget(m_includeSuperseded, 6, 3);
        filterLayout->addWidget(searchButton, 7, 1);
        filterLayout->addWidget(reloadButton, 7, 2);
        filterLayout->addWidget(saveButton, 7, 3);
        rootLayout->addWidget(filterGroup);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        m_resultsTable = new QTableWidget(0, 7, splitter);
        m_resultsTable->setHorizontalHeaderLabels({
            tr("Record ID"), tr("Display name"), tr("Domain"), tr("Kind"),
            tr("Exact ref"), tr("Validation"), tr("Blocked") });
        ConfigureReadOnlyTable(m_resultsTable);
        splitter->addWidget(m_resultsTable);

        auto* inspectorScroll = new QScrollArea(splitter);
        inspectorScroll->setWidgetResizable(true);
        auto* inspectorContent = new QWidget(inspectorScroll);
        auto* inspectorLayout = new QVBoxLayout(inspectorContent);

        auto* identityGroup = new QGroupBox(tr("Record Inspector"), inspectorContent);
        auto* identityLayout = new QFormLayout(identityGroup);
        m_recordIdentityValue = new QLabel(identityGroup);
        m_recordOwnerValue = new QLabel(identityGroup);
        m_recordSubjectValue = new QLabel(identityGroup);
        m_recordNativeRefValue = new QLabel(identityGroup);
        m_recordStateValue = new QLabel(identityGroup);
        for (QLabel* label : { m_recordIdentityValue, m_recordOwnerValue, m_recordSubjectValue, m_recordNativeRefValue, m_recordStateValue })
        {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
        m_recordAliasesView = CreateReadOnlyText(identityGroup, 90);
        identityLayout->addRow(tr("Identity"), m_recordIdentityValue);
        identityLayout->addRow(tr("Owner pack"), m_recordOwnerValue);
        identityLayout->addRow(tr("Subject"), m_recordSubjectValue);
        identityLayout->addRow(tr("Exact native ref"), m_recordNativeRefValue);
        identityLayout->addRow(tr("State"), m_recordStateValue);
        identityLayout->addRow(tr("Aliases / scoped refs / tags"), m_recordAliasesView);
        inspectorLayout->addWidget(identityGroup);

        auto* evidenceGroup = new QGroupBox(tr("Linked Evidence"), inspectorContent);
        auto* evidenceLayout = new QVBoxLayout(evidenceGroup);
        m_recordEvidenceView = CreateReadOnlyText(evidenceGroup, 180);
        evidenceLayout->addWidget(m_recordEvidenceView);
        inspectorLayout->addWidget(evidenceGroup);

        auto* relationshipsGroup = new QGroupBox(tr("Relationships"), inspectorContent);
        auto* relationshipsLayout = new QVBoxLayout(relationshipsGroup);
        m_recordRelationshipsView = CreateReadOnlyText(relationshipsGroup, 150);
        relationshipsLayout->addWidget(m_recordRelationshipsView);
        inspectorLayout->addWidget(relationshipsGroup);

        auto* validationGroup = new QGroupBox(tr("Validation History"), inspectorContent);
        auto* validationLayout = new QVBoxLayout(validationGroup);
        m_recordValidationView = CreateReadOnlyText(validationGroup, 150);
        validationLayout->addWidget(m_recordValidationView);
        inspectorLayout->addWidget(validationGroup);

        auto* blockersGroup = new QGroupBox(tr("Record Blockers"), inspectorContent);
        auto* blockersLayout = new QVBoxLayout(blockersGroup);
        m_recordBlockersView = CreateReadOnlyText(blockersGroup, 130);
        blockersLayout->addWidget(m_recordBlockersView);
        inspectorLayout->addWidget(blockersGroup);

        auto* promotionGroup = new QGroupBox(tr("Promote Evidence to Reviewed Record"), inspectorContent);
        auto* promotionLayout = new QFormLayout(promotionGroup);
        m_promotionEvidence = new QComboBox(promotionGroup);
        m_promotionRecordId = new QLineEdit(promotionGroup);
        m_promotionOwnerPack = new QComboBox(promotionGroup);
        m_promotionDomain = new QLineEdit(promotionGroup);
        m_promotionKind = new QLineEdit(promotionGroup);
        m_promotionSubject = new QLineEdit(promotionGroup);
        m_promotionNativeRef = new QLineEdit(promotionGroup);
        m_promotionIdentityKind = new QComboBox(promotionGroup);
        m_promotionIdentityKind->addItems({ "native", "synthetic", "composite", "source_scoped" });
        m_promotionDisplayName = new QLineEdit(promotionGroup);
        m_promotionAliases = new QLineEdit(promotionGroup);
        m_promotionStage = new QComboBox(promotionGroup);
        m_promotionStage->addItems({ "reviewed", "reconciled" });
        m_promotionConfidence = new QComboBox(promotionGroup);
        m_promotionConfidence->addItems({ "unknown", "hypothesis", "inferred", "documented", "runtime_observed", "validated" });
        m_promotionRisk = new QComboBox(promotionGroup);
        m_promotionRisk->addItems({ "unknown", "low", "medium", "high", "critical" });
        m_promotionForbidden = new QLineEdit(promotionGroup);
        m_promotionForbidden->setText(QStringLiteral("no_unvalidated_runtime_use"));
        m_promotionTags = new QLineEdit(promotionGroup);
        auto* promoteButton = new QPushButton(tr("Promote Reviewed Record"), promotionGroup);

        promotionLayout->addRow(tr("Evidence"), m_promotionEvidence);
        promotionLayout->addRow(tr("Record ID"), m_promotionRecordId);
        promotionLayout->addRow(tr("Owner pack"), m_promotionOwnerPack);
        promotionLayout->addRow(tr("Domain"), m_promotionDomain);
        promotionLayout->addRow(tr("Record kind"), m_promotionKind);
        promotionLayout->addRow(tr("Subject ref"), m_promotionSubject);
        promotionLayout->addRow(tr("Exact native ref / GUID"), m_promotionNativeRef);
        promotionLayout->addRow(tr("Identity kind"), m_promotionIdentityKind);
        promotionLayout->addRow(tr("Display name"), m_promotionDisplayName);
        promotionLayout->addRow(tr("Aliases (comma separated)"), m_promotionAliases);
        promotionLayout->addRow(tr("Research stage"), m_promotionStage);
        promotionLayout->addRow(tr("Confidence"), m_promotionConfidence);
        promotionLayout->addRow(tr("Operational risk"), m_promotionRisk);
        promotionLayout->addRow(tr("Prohibitions (comma separated)"), m_promotionForbidden);
        promotionLayout->addRow(tr("Tags (comma separated)"), m_promotionTags);
        promotionLayout->addRow(promoteButton);
        inspectorLayout->addWidget(promotionGroup);
        inspectorLayout->addStretch(1);

        inspectorScroll->setWidget(inspectorContent);
        splitter->addWidget(inspectorScroll);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);
        rootLayout->addWidget(splitter, 1);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        rootLayout->addWidget(m_statusLabel);

        connect(searchButton, &QPushButton::clicked, this, [this]() { RunSearch(); });
        connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { RunSearch(); });
        connect(reloadButton, &QPushButton::clicked, this, [this]()
        {
            AZStd::string error;
            if (!FoundationService::Get().ReloadCatalog(&error))
            {
                SetStatus(ToQString(error), true);
                return;
            }
            RefreshChoices();
            RunSearch();
            SetStatus(tr("Canonical catalog reloaded from the active workspace."));
        });
        connect(saveButton, &QPushButton::clicked, this, [this]()
        {
            AZStd::string error;
            if (!FoundationService::Get().SaveCatalog(&error))
            {
                SetStatus(ToQString(error), true);
                return;
            }
            SetStatus(tr("Canonical catalog saved."));
        });
        connect(m_resultsTable, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { InspectCurrentRow(); });
        connect(promoteButton, &QPushButton::clicked, this, [this]() { PromoteEvidence(); });
        connect(m_promotionEvidence, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int)
        {
            const AZStd::string evidenceId = ToAzString(m_promotionEvidence->currentData().toString());
            const EvidenceRecord* evidence = FoundationService::Get().GetSourceRegistry().FindEvidence(evidenceId);
            if (evidence)
            {
                m_promotionSubject->setText(ToQString(evidence->m_subjectRef));
                const int confidenceIndex = m_promotionConfidence->findText(ToQString(evidence->m_confidence));
                if (confidenceIndex >= 0)
                {
                    m_promotionConfidence->setCurrentIndex(confidenceIndex);
                }
            }
        });

        FoundationNotificationBus::Handler::BusConnect();
        ReloadForWorkspaceIfNeeded();
        RefreshChoices();
        RunSearch();
    }

    CatalogBrowserWidget::~CatalogBrowserWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void CatalogBrowserWidget::OnFoundationChanged()
    {
        if (m_reloading)
        {
            return;
        }
        ReloadForWorkspaceIfNeeded();
        RefreshChoices();
        RunSearch();
    }

    void CatalogBrowserWidget::ReloadForWorkspaceIfNeeded()
    {
        FoundationService& service = FoundationService::Get();
        const WorkspaceModel& workspace = service.GetWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        const AZStd::string profileId = profile ? profile->m_profileId : AZStd::string();
        if (workspace.m_rootPath == m_lastWorkspaceRoot && profileId == m_lastProfileId)
        {
            return;
        }

        m_lastWorkspaceRoot = workspace.m_rootPath;
        m_lastProfileId = profileId;
        m_reloading = true;
        AZStd::string error;
        const bool loaded = service.ReloadCatalog(&error);
        m_reloading = false;
        if (!loaded)
        {
            SetStatus(ToQString(error), true);
        }
    }

    void CatalogBrowserWidget::RefreshChoices()
    {
        const FoundationService& service = FoundationService::Get();
        const CatalogDatabase& catalog = service.GetCatalog();

        QStringList packs;
        for (const PackManifest& pack : service.GetPacks())
        {
            packs.push_back(ToQString(pack.m_packId));
        }
        PopulateTextCombo(m_packFilter, packs, tr("Any pack"));

        const QString previousOwner = m_promotionOwnerPack->currentData().toString();
        {
            QSignalBlocker blocker(m_promotionOwnerPack);
            m_promotionOwnerPack->clear();
            m_promotionOwnerPack->addItem(tr("No pack owner"), QString());
            for (const QString& pack : packs)
            {
                m_promotionOwnerPack->addItem(pack, pack);
            }
            const int previousIndex = m_promotionOwnerPack->findData(previousOwner);
            m_promotionOwnerPack->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
        }

        QStringList domains;
        QStringList kinds;
        QStringList identities;
        QStringList confidence;
        QStringList validation;
        for (const CatalogRecord& record : catalog.GetRecords())
        {
            domains.push_back(ToQString(record.m_domain));
            kinds.push_back(ToQString(record.m_recordKind));
            identities.push_back(ToQString(record.m_identityKind));
            confidence.push_back(ToQString(record.m_confidence));
            validation.push_back(ToQString(record.m_validationState));
        }
        PopulateTextCombo(m_domainFilter, domains, tr("Any domain"));
        PopulateTextCombo(m_kindFilter, kinds, tr("Any kind"));
        PopulateTextCombo(m_identityFilter, identities, tr("Any identity"));
        PopulateTextCombo(m_confidenceFilter, confidence, tr("Any confidence"));
        PopulateTextCombo(m_validationFilter, validation, tr("Any validation"));

        const QString previousEvidence = m_promotionEvidence->currentData().toString();
        {
            QSignalBlocker blocker(m_promotionEvidence);
            m_promotionEvidence->clear();
            m_promotionEvidence->addItem(tr("Select evidence…"), QString());
            for (const EvidenceRecord& evidence : service.GetSourceRegistry().GetEvidence())
            {
                QString label = ToQString(evidence.m_evidenceId);
                label += QStringLiteral(" — ");
                label += ToQString(evidence.m_subjectRef);
                label += QStringLiteral(" — ");
                label += ToQString(evidence.m_claim).left(80);
                m_promotionEvidence->addItem(label, ToQString(evidence.m_evidenceId));
            }
            const int previousIndex = m_promotionEvidence->findData(previousEvidence);
            m_promotionEvidence->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
        }

        m_catalogPathValue->setText(
            service.GetCatalogFilePath().empty() ? tr("Not saved") : ToQString(service.GetCatalogFilePath()));
    }

    CatalogQuery CatalogBrowserWidget::BuildQuery() const
    {
        CatalogQuery query;
        query.m_searchText = ToAzString(m_searchEdit->text());
        query.m_recordId = ToAzString(m_recordIdFilter->text());
        query.m_nativeRefExact = ToAzString(m_exactRefFilter->text());
        query.m_subjectRef = ToAzString(m_subjectFilter->text());
        query.m_evidenceId = ToAzString(m_evidenceFilter->text());
        query.m_ownerPackId = ToAzString(m_packFilter->currentData().toString());
        query.m_domain = ToAzString(m_domainFilter->currentData().toString());
        query.m_recordKind = ToAzString(m_kindFilter->currentData().toString());
        query.m_identityKind = ToAzString(m_identityFilter->currentData().toString());
        query.m_confidence = ToAzString(m_confidenceFilter->currentData().toString());
        query.m_validationState = ToAzString(m_validationFilter->currentData().toString());
        query.m_permission = ToAzString(m_permissionFilter->text());
        query.m_blockedOnly = m_blockedOnly->isChecked();
        query.m_includeSuperseded = m_includeSuperseded->isChecked();
        return query;
    }

    void CatalogBrowserWidget::RunSearch()
    {
        const FoundationService& service = FoundationService::Get();
        m_results = service.GetCatalog().Query(BuildQuery());
        m_resultsTable->setRowCount(static_cast<int>(m_results.size()));
        for (int row = 0; row < static_cast<int>(m_results.size()); ++row)
        {
            const CatalogRecord& record = m_results[static_cast<size_t>(row)];
            SetCell(m_resultsTable, row, 0, ToQString(record.m_recordId));
            SetCell(m_resultsTable, row, 1, ToQString(record.m_displayName));
            SetCell(m_resultsTable, row, 2, ToQString(record.m_domain));
            SetCell(m_resultsTable, row, 3, ToQString(record.m_recordKind));
            SetCell(m_resultsTable, row, 4, ToQString(record.m_nativeRefExact));
            SetCell(m_resultsTable, row, 5, ToQString(record.m_validationState));
            SetCell(m_resultsTable, row, 6, record.IsBlocked() ? tr("Yes") : tr("No"));
        }
        m_resultsTable->resizeRowsToContents();

        if (!m_results.empty())
        {
            m_resultsTable->setCurrentCell(0, 0);
        }
        else
        {
            InspectRecord(nullptr);
        }
        SetStatus(tr("%1 catalog record(s) matched.").arg(static_cast<qulonglong>(m_results.size())));
    }

    void CatalogBrowserWidget::InspectCurrentRow()
    {
        const int row = m_resultsTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_results.size()))
        {
            InspectRecord(nullptr);
            return;
        }
        InspectRecord(&m_results[static_cast<size_t>(row)]);
    }

    void CatalogBrowserWidget::InspectRecord(const CatalogRecord* record)
    {
        if (!record)
        {
            m_recordIdentityValue->setText(tr("No record selected"));
            m_recordOwnerValue->clear();
            m_recordSubjectValue->clear();
            m_recordNativeRefValue->clear();
            m_recordStateValue->clear();
            m_recordAliasesView->clear();
            m_recordEvidenceView->clear();
            m_recordRelationshipsView->clear();
            m_recordValidationView->clear();
            m_recordBlockersView->clear();
            return;
        }

        m_recordIdentityValue->setText(
            tr("%1 | %2 | %3 | %4")
                .arg(ToQString(record->m_recordId))
                .arg(ToQString(record->m_identityKind))
                .arg(ToQString(record->m_domain))
                .arg(ToQString(record->m_recordKind)));
        m_recordOwnerValue->setText(record->m_ownerPackId.empty() ? tr("Native / no pack owner") : ToQString(record->m_ownerPackId));
        m_recordSubjectValue->setText(ToQString(record->m_subjectRef));
        m_recordNativeRefValue->setText(record->m_nativeRefExact.empty() ? tr("Not applicable") : ToQString(record->m_nativeRefExact));
        m_recordStateValue->setText(
            tr("Stage: %1 | Confidence: %2 | Risk: %3 | Validation: %4 | Superseded by: %5")
                .arg(ToQString(record->m_researchStage))
                .arg(ToQString(record->m_confidence))
                .arg(ToQString(record->m_operationalRisk))
                .arg(ToQString(record->m_validationState))
                .arg(record->m_supersededByRecordId.empty() ? tr("No") : ToQString(record->m_supersededByRecordId)));

        QStringList aliases;
        aliases << tr("Aliases: %1").arg(JoinValues(record->m_aliases));
        aliases << tr("Source-scoped refs: %1").arg(JoinValues(record->m_sourceScopedRefs));
        aliases << tr("Tags: %1").arg(JoinValues(record->m_tags));
        aliases << tr("Allowed usages: %1").arg(JoinValues(record->m_allowedUsages));
        aliases << tr("Prohibitions: %1").arg(JoinValues(record->m_forbiddenUsages));
        aliases << tr("Missing refs: %1").arg(JoinValues(record->m_missingRefs));
        aliases << tr("Conflicts: %1").arg(JoinValues(record->m_conflictRefs));
        m_recordAliasesView->setPlainText(aliases.join('\n'));

        const FoundationService& service = FoundationService::Get();
        QStringList evidenceLines;
        for (const AZStd::string& evidenceId : record->m_evidenceIds)
        {
            const EvidenceRecord* evidence = service.GetSourceRegistry().FindEvidence(evidenceId);
            if (!evidence)
            {
                evidenceLines << tr("%1 — missing from active evidence registry").arg(ToQString(evidenceId));
                continue;
            }
            evidenceLines << tr("%1\n  Source: %2\n  Claim: %3\n  Confidence: %4\n  Locator: %5")
                .arg(ToQString(evidence->m_evidenceId))
                .arg(ToQString(evidence->m_sourceId))
                .arg(ToQString(evidence->m_claim))
                .arg(ToQString(evidence->m_confidence))
                .arg(ToQString(evidence->m_locator));
        }
        m_recordEvidenceView->setPlainText(evidenceLines.join(QStringLiteral("\n\n")));

        QStringList relationshipLines;
        for (const CatalogRelationship& relationship : service.GetCatalog().FindRelationshipsForRecord(record->m_recordId))
        {
            const QString target = relationship.m_toRecordId.empty()
                ? ToQString(relationship.m_targetSubjectRef)
                : ToQString(relationship.m_toRecordId);
            relationshipLines << tr("%1 | %2 | %3 → %4 | validation: %5 | evidence: %6")
                .arg(ToQString(relationship.m_relationshipId))
                .arg(ToQString(relationship.m_relationshipKind))
                .arg(ToQString(relationship.m_fromRecordId))
                .arg(target)
                .arg(ToQString(relationship.m_validationState))
                .arg(JoinValues(relationship.m_evidenceIds));
        }
        m_recordRelationshipsView->setPlainText(relationshipLines.join('\n'));

        QStringList validationLines;
        for (const CatalogValidationEvent& validation : service.GetCatalog().FindValidationForRecord(record->m_recordId))
        {
            validationLines << tr("%1 | %2 | %3 | %4 | %5\n  %6")
                .arg(ToQString(validation.m_checkedAt))
                .arg(ToQString(validation.m_state))
                .arg(ToQString(validation.m_method))
                .arg(ToQString(validation.m_validator))
                .arg(ToQString(validation.m_gameVersion))
                .arg(ToQString(validation.m_notes));
        }
        m_recordValidationView->setPlainText(validationLines.join(QStringLiteral("\n\n")));

        QStringList blockerLines;
        for (const BlockerRecord& blocker : service.GetSnapshot().m_blockers)
        {
            if (blocker.m_subjectRef == record->m_subjectRef || blocker.m_subjectRef == record->m_recordId)
            {
                blockerLines << tr("%1 | %2 | %3\n  Affected: %4")
                    .arg(ToQString(blocker.m_severity))
                    .arg(ToQString(blocker.m_area))
                    .arg(ToQString(blocker.m_reason))
                    .arg(JoinValues(blocker.m_affectedUsages));
            }
        }
        m_recordBlockersView->setPlainText(blockerLines.join(QStringLiteral("\n\n")));
    }

    CatalogPromotionRequest CatalogBrowserWidget::BuildPromotionRequest() const
    {
        CatalogPromotionRequest request;
        request.m_evidenceId = ToAzString(m_promotionEvidence->currentData().toString());
        request.m_recordId = ToAzString(m_promotionRecordId->text());
        request.m_ownerPackId = ToAzString(m_promotionOwnerPack->currentData().toString());
        request.m_domain = ToAzString(m_promotionDomain->text());
        request.m_recordKind = ToAzString(m_promotionKind->text());
        request.m_subjectRef = ToAzString(m_promotionSubject->text());
        request.m_nativeRefExact = ToAzString(m_promotionNativeRef->text());
        request.m_identityKind = ToAzString(m_promotionIdentityKind->currentText());
        request.m_displayName = ToAzString(m_promotionDisplayName->text());
        request.m_aliases = ParseCommaSeparated(m_promotionAliases->text());
        request.m_researchStage = ToAzString(m_promotionStage->currentText());
        request.m_confidence = ToAzString(m_promotionConfidence->currentText());
        request.m_operationalRisk = ToAzString(m_promotionRisk->currentText());
        request.m_forbiddenUsages = ParseCommaSeparated(m_promotionForbidden->text());
        request.m_tags = ParseCommaSeparated(m_promotionTags->text());
        return request;
    }

    void CatalogBrowserWidget::PromoteEvidence()
    {
        AZStd::string error;
        if (!FoundationService::Get().PromoteEvidenceToCatalog(BuildPromotionRequest(), &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        RefreshChoices();
        RunSearch();
        SetStatus(tr("Evidence promoted to a reviewed, unvalidated catalog record. No usage permission was granted."));
    }

    void CatalogBrowserWidget::SetStatus(const QString& message, bool error)
    {
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }
} // namespace TaintedGrailModdingSDK
