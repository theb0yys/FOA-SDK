        RefreshMemberTable();
        MarkTroopDirty();
        SetStatus(
            StatusKind::Warning,
            replaced
                ? tr("The staged member was updated. Save Troop Definition to persist it.")
                : tr("The member was added to the staged set. Save Troop Definition to persist it."));
    }

    void ActorTroopEditorWidget::RemoveSelectedMember()
    {
        if (m_memberFormDirty)
        {
            SetStatus(
                StatusKind::Error,
                tr("Apply or clear the unapplied member-form changes before removing a staged row."));
            return;
        }
        const int row = m_memberTable->currentRow();
        if (row < 0 || !m_memberTable->item(row, 0))
        {
            SetStatus(
                StatusKind::Error,
                tr("Select a staged member row to remove."));
            return;
        }
        const AZStd::string linkId =
            ToAzString(m_memberTable->item(row, 0)->text());
        const AZStd::string troopRecordId =
            ToAzString(m_troopRecord->currentData().toString());
        const AZStd::vector<PopulationTroopMember> persistedMembers =
            FoundationService::Get().GetCatalog()
                .FindPopulationMembersForTroop(troopRecordId);
        if (AZStd::any_of(
                persistedMembers.begin(),
                persistedMembers.end(),
                [&linkId](const PopulationTroopMember& member)
                {
                    return member.m_linkId == linkId;
                }))
        {
            SetStatus(
                StatusKind::Error,
                tr("Persisted troop-member removal is not authorised in this SDK slice. "
                   "Only newly staged, never-published rows can be discarded."));
            return;
        }
        m_stagedMembers.erase(
            AZStd::remove_if(
                m_stagedMembers.begin(),
                m_stagedMembers.end(),
                [&linkId](const PopulationTroopMember& member)
                {
                    return member.m_linkId == linkId;
                }),
            m_stagedMembers.end());
        RefreshMemberTable();
        ClearMemberEditor();
        MarkTroopDirty();
        SetStatus(
            StatusKind::Warning,
            tr("Discarded newly staged member %1. No persisted membership was removed.")
                .arg(ToQString(linkId)));
    }

    void ActorTroopEditorWidget::SaveTroopDefinition()
    {
        if (m_memberFormDirty)
        {
            m_troopState->setText(
                tr("Invalid state — the member form has unapplied changes."));
            SetStatus(
                StatusKind::Error,
                tr("Apply the current member form to the staged set, or clear it, before saving."));
            return;
        }

        PopulationTroopDefinition definition;
        definition.m_profile.m_recordId =
            ToAzString(m_troopRecord->currentData().toString());
        definition.m_profile.m_troopKind =
            ToAzString(m_troopKind->currentText());
        definition.m_profile.m_leaderActorRecordId =
            ToAzString(m_troopLeaderRecord->currentData().toString());
        definition.m_profile.m_leaderActorSubjectRef =
            ToAzString(m_troopLeaderSubject->text());
        definition.m_profile.m_minimumSize =
            static_cast<AZ::u32>(m_troopMinimumSize->value());
        definition.m_profile.m_maximumSize =
            static_cast<AZ::u32>(m_troopMaximumSize->value());
        definition.m_profile.m_formation =
            ToAzString(m_troopFormation->text());
        definition.m_profile.m_tags = ParseCommaSeparated(m_troopTags->text());
        definition.m_profile.m_evidenceIds =
            SelectedEvidenceIds(m_troopEvidence);
        definition.m_members = m_stagedMembers;
        for (PopulationTroopMember& member : definition.m_members)
        {
            member.m_troopRecordId = definition.m_profile.m_recordId;
        }

        if (definition.m_members.empty())
        {
            m_troopState->setText(
                tr("Invalid state — a complete troop definition requires at least one staged member."));
            SetStatus(
                StatusKind::Error,
                tr("Stage at least one member before saving the troop definition."));
            return;
        }

        m_troopDirty = false;
        m_memberFormDirty = false;
        AZStd::string error;
        if (!FoundationService::Get().UpsertPopulationTroopDefinition(
                definition,
                &error))
        {
            m_troopDirty = true;
            m_memberFormDirty = false;
            m_troopState->setText(
                tr("Save failed. The previously published troop definition remains unchanged."));
            SetStatus(
                StatusKind::Error,
                tr("Troop definition save failed: %1").arg(ToQString(error)));
            return;
        }

        m_troopDirty = false;
        m_memberFormDirty = false;
        SetStatus(
            StatusKind::Success,
            tr("The troop profile and staged member updates were persisted "
               "atomically; omitted persisted rows were preserved."));
    }

    void ActorTroopEditorWidget::RevertTroopDefinition()
    {
        const bool previousRefreshing = m_refreshing;
        m_refreshing = true;
        LoadCurrentTroop();
        m_refreshing = previousRefreshing;
        m_troopDirty = false;
        m_memberFormDirty = false;
        SetStatus(
            StatusKind::Neutral,
            tr("Unsaved troop and member changes were discarded; persisted state was reloaded."));
    }
} // namespace TaintedGrailModdingSDK
