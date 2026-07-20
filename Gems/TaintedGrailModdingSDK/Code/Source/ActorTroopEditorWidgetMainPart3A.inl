            {
                label.prepend(tr("[subject mismatch] "));
            }
            auto* item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, ToQString(evidence.m_evidenceId));
            item->setToolTip(
                tr("Source: %1\nClaim: %2\nLocator: %3\nRecord path: %4")
                    .arg(ToQString(evidence.m_sourceId))
                    .arg(ToQString(evidence.m_claim))
                    .arg(ToQString(evidence.m_locator))
                    .arg(ToQString(evidence.m_recordPath)));
            item->setSelected(wasSelected);
            seenEvidenceIds.push_back(evidence.m_evidenceId);
        }

        for (const AZStd::string& selectedEvidenceId : selectedEvidenceIds)
        {
            if (Contains(seenEvidenceIds, selectedEvidenceId))
            {
                continue;
            }
            auto* missing = new QListWidgetItem(
                tr("[missing from active registry] %1")
                    .arg(ToQString(selectedEvidenceId)),
                list);
            missing->setData(Qt::UserRole, ToQString(selectedEvidenceId));
            missing->setToolTip(
                tr("This previously selected evidence ID is no longer registered. "
                   "Saving will fail closed until it is restored or deselected."));
            missing->setSelected(true);
        }

        if (list->count() == 0)
        {
            auto* empty = new QListWidgetItem(
                tr("No active-profile evidence proves the currently represented exact subjects."),
                list);
            empty->setFlags(Qt::NoItemFlags);
        }
    }

    void ActorTroopEditorWidget::RefreshEvidenceDetails(
        const QListWidget* primary,
        const QListWidget* secondary,
        QTableWidget* table) const
    {
        AZStd::vector<AZStd::string> evidenceIds =
            SelectedEvidenceIds(primary);
        for (const AZStd::string& evidenceId : SelectedEvidenceIds(secondary))
        {
            if (!Contains(evidenceIds, evidenceId))
            {
                evidenceIds.push_back(evidenceId);
            }
        }
        AZStd::sort(evidenceIds.begin(), evidenceIds.end());

        const SourceEvidenceRegistry& registry =
            FoundationService::Get().GetSourceRegistry();
        table->setRowCount(static_cast<int>(evidenceIds.size()));
        for (int row = 0; row < static_cast<int>(evidenceIds.size()); ++row)
        {
            const EvidenceRecord* evidence = registry.FindEvidence(
                evidenceIds[static_cast<size_t>(row)]);
            if (!evidence)
            {
                SetCell(table, row, 0, ToQString(evidenceIds[static_cast<size_t>(row)]));
                SetCell(table, row, 1, tr("Missing from active registry"));
                continue;
            }
            const SourceRecord* source = registry.FindSource(evidence->m_sourceId);
            SetCell(table, row, 0, ToQString(evidence->m_evidenceId));
            SetCell(table, row, 1, ToQString(evidence->m_subjectRef));
            SetCell(table, row, 2, ToQString(evidence->m_claim));
            SetCell(table, row, 3, ToQString(evidence->m_evidenceKind));
            SetCell(table, row, 4, ToQString(evidence->m_confidence));
            SetCell(table, row, 5, ToQString(evidence->m_sourceId));
            SetCell(
                table,
                row,
                6,
                tr("%1 | %2")
                    .arg(ToQString(evidence->m_locator))
                    .arg(ToQString(evidence->m_recordPath)));
            SetCell(
                table,
                row,
                7,
                tr("extracted %1 | source %2")
                    .arg(ToQString(evidence->m_extractedAt))
                    .arg(source ? ToQString(source->m_importStatus) : tr("missing")));
        }
    }

    AZStd::string ActorTroopEditorWidget::ResolveActorSubject(
        const AZStd::string& recordId,
        const AZStd::string& unresolvedSubject) const
    {
        const CatalogRecord* record =
            FoundationService::Get().GetCatalog().FindByRecordId(recordId);
        return record ? record->m_subjectRef : unresolvedSubject;
    }

    void ActorTroopEditorWidget::SynchronizeResolvedSubject(
        QComboBox* recordCombo,
        QLineEdit* subjectEdit)
    {
        const CatalogRecord* record =
            FoundationService::Get().GetCatalog().FindByRecordId(
                ToAzString(recordCombo->currentData().toString()));
        if (record)
        {
            subjectEdit->setText(ToQString(record->m_subjectRef));
        }
    }

    void ActorTroopEditorWidget::RefreshReviewTables(
        const AZStd::string& recordId,
        bool hasTypedProfile,
        bool isTroop,
        bool authoringEvidenceReady,
        QTableWidget* validationTable,
        QTableWidget* governanceTable,
        QTableWidget* blockerTable,
        QTableWidget* relationshipTable,
        QTableWidget* laneTable) const
    {
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* record = catalog.FindByRecordId(recordId);
        validationTable->setRowCount(0);
        governanceTable->setRowCount(0);
        blockerTable->setRowCount(0);
        relationshipTable->setRowCount(0);
        laneTable->setRowCount(0);
        if (!record)
        {
            return;
        }

        const AZStd::vector<CatalogValidationEvent> validations =
            catalog.FindValidationForRecord(recordId);
        validationTable->setRowCount(static_cast<int>(validations.size()));
        for (int row = 0; row < static_cast<int>(validations.size()); ++row)
        {
            const CatalogValidationEvent& validation =
                validations[static_cast<size_t>(row)];
            SetCell(validationTable, row, 0, ToQString(validation.m_checkedAt));
            SetCell(validationTable, row, 1, ToQString(validation.m_state));
            SetCell(validationTable, row, 2, ToQString(validation.m_method));
