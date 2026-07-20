            m_memberActorRecord,
            ToQString(found->m_actorRecordId));
        m_memberActorSubject->setText(ToQString(found->m_actorSubjectRef));
        SetComboByText(m_memberRole, ToQString(found->m_role));
        m_memberMinimumCount->setValue(
            static_cast<int>(found->m_minimumCount));
        m_memberMaximumCount->setValue(
            static_cast<int>(found->m_maximumCount));
        m_memberWeight->setValue(found->m_weight);
        m_memberRequired->setChecked(found->m_required);
        m_memberConditions->setText(JoinValues(found->m_conditions));
        RefreshMemberEvidenceChoices(found->m_evidenceIds);
        m_memberFormDirty = false;
        m_refreshing = previousRefreshing;
        SetStatus(
            StatusKind::Neutral,
            tr("Loaded staged member %1 into the member form.")
                .arg(ToQString(found->m_linkId)));
    }

    void ActorTroopEditorWidget::MarkMemberFormDirty()
    {
        if (m_refreshing
            || m_troopRecord->currentData().toString().isEmpty())
        {
            return;
        }
        m_memberFormDirty = true;
        m_troopRevert->setEnabled(true);
        m_troopState->setText(
            tr("Unapplied member-form changes. Add / Update the staged member, or clear the form, before saving."));
        const AZStd::string recordId =
            ToAzString(m_troopRecord->currentData().toString());
        RefreshReviewTables(
            recordId,
            FoundationService::Get().GetCatalog()
                    .FindPopulationTroopProfile(recordId) != nullptr,
            true,
            false,
            m_troopValidation,
            m_troopGovernance,
            m_troopBlockers,
            m_troopRelationships,
            m_troopLanes);
    }

    void ActorTroopEditorWidget::AddOrUpdateMember()
    {
        const AZStd::string troopRecordId =
            ToAzString(m_troopRecord->currentData().toString());
        PopulationTroopMember member;
        member.m_linkId = ToAzString(m_memberLinkId->text());
        member.m_troopRecordId = troopRecordId;
        member.m_actorRecordId =
            ToAzString(m_memberActorRecord->currentData().toString());
        member.m_actorSubjectRef = ToAzString(m_memberActorSubject->text());
        member.m_role = ToAzString(m_memberRole->currentText());
        member.m_minimumCount =
            static_cast<AZ::u32>(m_memberMinimumCount->value());
        member.m_maximumCount =
            static_cast<AZ::u32>(m_memberMaximumCount->value());
        member.m_weight = m_memberWeight->value();
        member.m_required = m_memberRequired->isChecked();
        member.m_conditions = ParseCommaSeparated(m_memberConditions->text());
        member.m_evidenceIds = SelectedEvidenceIds(m_memberEvidence);

        if (troopRecordId.empty())
        {
            SetStatus(StatusKind::Error, tr("Select a canonical troop first."));
            return;
        }
        if (!IsStableContractId(member.m_linkId))
        {
            SetStatus(
                StatusKind::Error,
                tr("The member link ID must be a bounded namespaced stable identity."));
            return;
        }
        if (member.m_actorRecordId.empty()
            && member.m_actorSubjectRef.empty())
        {
            SetStatus(
                StatusKind::Error,
                tr("Select a canonical actor or enter an exact unresolved actor subject."));
            return;
        }
        if (member.m_minimumCount > member.m_maximumCount)
        {
            SetStatus(
                StatusKind::Error,
                tr("Member minimum count cannot exceed maximum count."));
            return;
        }
        if (!EvidenceIdsReady(
                member.m_evidenceIds,
                ResolveMemberEvidenceSubjects(member)))
        {
            SetStatus(
                StatusKind::Error,
                tr("Select valid active-profile evidence for an exact member, troop, or actor subject."));
            return;
        }

        const AZStd::string newSubject = ResolveActorSubject(
            member.m_actorRecordId,
            member.m_actorSubjectRef);
        for (const PopulationTroopMember& existing : m_stagedMembers)
        {
            if (existing.m_linkId == member.m_linkId)
            {
                continue;
            }
            const AZStd::string existingSubject = ResolveActorSubject(
                existing.m_actorRecordId,
                existing.m_actorSubjectRef);
            if (!newSubject.empty() && newSubject == existingSubject)
            {
                SetStatus(
                    StatusKind::Error,
                    tr("The staged troop already contains this exact actor subject."));
                return;
            }
            if (member.m_role == "leader" && existing.m_role == "leader")
            {
                SetStatus(
                    StatusKind::Error,
                    tr("A troop can stage at most one typed leader row."));
                return;
            }
        }

        auto found = AZStd::find_if(
            m_stagedMembers.begin(),
            m_stagedMembers.end(),
            [&member](const PopulationTroopMember& existing)
            {
                return existing.m_linkId == member.m_linkId;
            });
        const bool replaced = found != m_stagedMembers.end();
        if (replaced)
        {
            *found = member;
        }
        else
        {
            m_stagedMembers.push_back(member);
        }
        m_memberFormDirty = false;
        m_loadedMemberLinkId = member.m_linkId;
