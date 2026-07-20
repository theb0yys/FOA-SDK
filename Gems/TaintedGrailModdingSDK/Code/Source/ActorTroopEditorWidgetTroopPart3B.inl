            m_troopLeaderSubject->clear();
            m_troopMinimumSize->setValue(1);
            m_troopMaximumSize->setValue(1);
            m_troopFormation->clear();
            m_troopTags->clear();
            if (foundation.GetWorkspaceRootPath().empty())
            {
                m_troopState->setText(
                    tr("Blocked state — save or load the workspace before authoring this troop."));
            }
            else
            {
                m_troopState->setText(
                    foundation.GetActivePack()
                        ? tr("Empty typed-profile state — stage at least one member "
                             "and save the definition atomically.")
                        : tr("Blocked state — select an active editor-owned pack before authoring this troop."));
            }
        }

        m_troopRevert->setEnabled(profile != nullptr);
        RefreshMemberTable();
        ClearMemberEditor();
        RefreshTroopEvidenceChoices(evidenceIds);
        RefreshReviewTables(
            recordId,
            profile != nullptr,
            true,
            IsTroopEvidenceReady(),
            m_troopValidation,
            m_troopGovernance,
            m_troopBlockers,
            m_troopRelationships,
            m_troopLanes);
    }

    void ActorTroopEditorWidget::RefreshMemberTable()
    {
        AZStd::sort(
            m_stagedMembers.begin(),
            m_stagedMembers.end(),
            [](const PopulationTroopMember& left,
               const PopulationTroopMember& right)
            {
                return left.m_linkId < right.m_linkId;
            });
        m_memberTable->setRowCount(static_cast<int>(m_stagedMembers.size()));
        for (int row = 0; row < static_cast<int>(m_stagedMembers.size()); ++row)
        {
            const PopulationTroopMember& member =
                m_stagedMembers[static_cast<size_t>(row)];
            const QString actor = member.m_actorRecordId.empty()
                ? ToQString(member.m_actorSubjectRef)
                : ToQString(member.m_actorRecordId);
            SetCell(m_memberTable, row, 0, ToQString(member.m_linkId));
            SetCell(m_memberTable, row, 1, actor);
            SetCell(m_memberTable, row, 2, ToQString(member.m_role));
            SetCell(
                m_memberTable,
                row,
                3,
                tr("%1–%2")
                    .arg(member.m_minimumCount)
                    .arg(member.m_maximumCount));
            SetCell(
                m_memberTable,
                row,
                4,
                QString::number(member.m_weight, 'f', 4));
            SetCell(
                m_memberTable,
                row,
                5,
                member.m_required ? tr("Yes") : tr("No"));
            SetCell(m_memberTable, row, 6, JoinValues(member.m_conditions));
            SetCell(m_memberTable, row, 7, JoinValues(member.m_evidenceIds));
        }
    }

    void ActorTroopEditorWidget::ClearMemberEditor()
    {
        const bool previousRefreshing = m_refreshing;
        m_refreshing = true;
        m_memberTable->clearSelection();
        m_loadedMemberLinkId.clear();
        m_memberLinkId->clear();
        m_memberActorRecord->setCurrentIndex(0);
        m_memberActorSubject->clear();
        m_memberRole->setCurrentIndex(0);
        m_memberMinimumCount->setValue(1);
        m_memberMaximumCount->setValue(1);
        m_memberWeight->setValue(1.0);
        m_memberRequired->setChecked(false);
        m_memberConditions->clear();
        RefreshMemberEvidenceChoices({});
        m_memberFormDirty = false;
        const AZStd::string troopRecordId =
            ToAzString(m_troopRecord->currentData().toString());
        const bool hasPersistedProfile = FoundationService::Get().GetCatalog()
            .FindPopulationTroopProfile(troopRecordId) != nullptr;
        m_troopRevert->setEnabled(m_troopDirty || hasPersistedProfile);
        m_refreshing = previousRefreshing;
    }

    void ActorTroopEditorWidget::LoadSelectedMember()
    {
        const int row = m_memberTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_stagedMembers.size()))
        {
            return;
        }
        const AZStd::string linkId = ToAzString(
            m_memberTable->item(row, 0)->text());
        if (m_memberFormDirty && linkId != m_loadedMemberLinkId)
        {
            QSignalBlocker blocker(m_memberTable);
            m_memberTable->clearSelection();
            for (int stagedRow = 0;
                 stagedRow < static_cast<int>(m_stagedMembers.size());
                 ++stagedRow)
            {
                if (m_stagedMembers[static_cast<size_t>(stagedRow)].m_linkId
                    == m_loadedMemberLinkId)
                {
                    m_memberTable->selectRow(stagedRow);
                    break;
                }
            }
            SetStatus(
                StatusKind::Warning,
                tr("Apply or clear the unapplied member-form changes before selecting another staged row."));
            return;
        }
        const auto found = AZStd::find_if(
            m_stagedMembers.begin(),
            m_stagedMembers.end(),
            [&linkId](const PopulationTroopMember& member)
            {
                return member.m_linkId == linkId;
            });
        if (found == m_stagedMembers.end())
        {
            return;
        }

        const bool previousRefreshing = m_refreshing;
        m_refreshing = true;
        m_loadedMemberLinkId = found->m_linkId;
        m_memberLinkId->setText(ToQString(found->m_linkId));
        SetComboByData(
