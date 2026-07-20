        return subjects;
    }

    bool ActorTroopEditorWidget::IsTroopEvidenceReady() const
    {
        if (m_memberFormDirty
            || m_stagedMembers.empty()
            || !EvidenceIdsReady(
                SelectedEvidenceIds(m_troopEvidence),
                ResolveTroopEvidenceSubjects()))
        {
            return false;
        }
        return AZStd::all_of(
            m_stagedMembers.begin(),
            m_stagedMembers.end(),
            [this](const PopulationTroopMember& member)
            {
                return EvidenceIdsReady(
                    member.m_evidenceIds,
                    ResolveMemberEvidenceSubjects(member));
            });
    }

    void ActorTroopEditorWidget::RefreshTroopEvidenceChoices()
    {
        RefreshTroopEvidenceChoices(SelectedEvidenceIds(m_troopEvidence));
    }

    void ActorTroopEditorWidget::RefreshTroopEvidenceChoices(
        const AZStd::vector<AZStd::string>& selectedEvidenceIds)
    {
        RefreshEvidenceList(
            m_troopEvidence,
            m_troopEvidenceFilter,
            ResolveTroopEvidenceSubjects(),
            selectedEvidenceIds);
        RefreshEvidenceDetails(
            m_troopEvidence,
            m_memberEvidence,
            m_troopEvidenceDetails);
    }

    void ActorTroopEditorWidget::RefreshMemberEvidenceChoices()
    {
        RefreshMemberEvidenceChoices(SelectedEvidenceIds(m_memberEvidence));
    }

    void ActorTroopEditorWidget::RefreshMemberEvidenceChoices(
        const AZStd::vector<AZStd::string>& selectedEvidenceIds)
    {
        RefreshEvidenceList(
            m_memberEvidence,
            m_memberEvidenceFilter,
            ResolveMemberEvidenceSubjects(),
            selectedEvidenceIds);
        RefreshEvidenceDetails(
            m_troopEvidence,
            m_memberEvidence,
            m_troopEvidenceDetails);
    }

    void ActorTroopEditorWidget::LoadCurrentTroop()
    {
        const AZStd::string recordId =
            ToAzString(m_troopRecord->currentData().toString());
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* record = catalog.FindByRecordId(recordId);

        m_loadedTroopRecordId = record ? recordId : AZStd::string{};
        m_troopDirty = false;
        m_memberFormDirty = false;
        m_troopSave->setEnabled(record != nullptr);
        m_stagedMembers.clear();
        if (!record)
        {
            m_troopIdentity->setText(tr("No canonical troop selected."));
            m_troopState->setText(
                tr("Empty state — use the troop filter and select an existing population/troop record."));
            m_troopKind->setCurrentIndex(0);
            m_troopLeaderRecord->setCurrentIndex(0);
            m_troopLeaderSubject->clear();
            m_troopMinimumSize->setValue(1);
            m_troopMaximumSize->setValue(1);
            m_troopFormation->clear();
            m_troopTags->clear();
            m_troopRevert->setEnabled(false);
            RefreshMemberTable();
            ClearMemberEditor();
            RefreshTroopEvidenceChoices({});
            RefreshReviewTables(
                {},
                false,
                true,
                false,
                m_troopValidation,
                m_troopGovernance,
                m_troopBlockers,
                m_troopRelationships,
                m_troopLanes);
            return;
        }

        const QString owner = record->m_ownerPackId.empty()
            ? tr("native / no pack owner")
            : ToQString(record->m_ownerPackId);
        m_troopIdentity->setText(
            tr("record: %1 | subject: %2 | identity: %3 | owner: %4 | exact native ref: %5 | "
               "validation: %6 | staleness: %7 | allowed: %8 | forbidden: %9")
                .arg(ToQString(record->m_recordId))
                .arg(ToQString(record->m_subjectRef))
                .arg(ToQString(record->m_identityKind))
                .arg(owner)
                .arg(record->m_nativeRefExact.empty()
                    ? tr("not applicable")
                    : ToQString(record->m_nativeRefExact))
                .arg(ToQString(record->m_validationState))
                .arg(ToQString(record->m_stalenessState))
                .arg(JoinValues(record->m_allowedUsages))
                .arg(JoinValues(record->m_forbiddenUsages)));

        const PopulationTroopProfile* profile =
            catalog.FindPopulationTroopProfile(recordId);
        AZStd::vector<AZStd::string> evidenceIds;
        if (profile)
        {
            SetComboByText(m_troopKind, ToQString(profile->m_troopKind));
            SetComboByData(
                m_troopLeaderRecord,
                ToQString(profile->m_leaderActorRecordId));
            m_troopLeaderSubject->setText(
                ToQString(profile->m_leaderActorSubjectRef));
            m_troopMinimumSize->setValue(
                static_cast<int>(profile->m_minimumSize));
            m_troopMaximumSize->setValue(
                static_cast<int>(profile->m_maximumSize));
            m_troopFormation->setText(ToQString(profile->m_formation));
            m_troopTags->setText(JoinValues(profile->m_tags));
            evidenceIds = profile->m_evidenceIds;
            m_stagedMembers = catalog.FindPopulationMembersForTroop(recordId);
            m_troopState->setText(
                tr("Loaded persisted troop definition with %1 member row(s). Changes remain staged until the "
                   "complete definition passes integrity and persistence.")
                    .arg(static_cast<int>(m_stagedMembers.size())));
        }
        else
        {
            m_troopKind->setCurrentIndex(0);
            m_troopLeaderRecord->setCurrentIndex(0);
