                : (!activePack
                    ? tr("Select one active editor-owned pack.")
                    : (!activePackAuthoringReady
                        ? tr("The active pack is invalid, runtime-enabled, or incompatible with the active profile.")
                    : (!recordOwnershipMatches
                        ? tr("The synthetic record belongs to a different pack.")
                        : (!authoringEvidenceReady
                            ? tr("Select valid exact-subject evidence for every current authored row.")
                            : (hasTypedProfile
                                ? tr("The existing typed profile can be updated through Foundation.")
                                : tr("A new typed profile can be created through Foundation.")))))) });
        lanes.push_back({
            tr("compose_troop"),
            !isTroop
                ? tr("not applicable")
                : (profileAuthoringReady ? tr("available") : tr("blocked")),
            !isTroop
                ? tr("Troop composition applies only to canonical troop records.")
                : (!authoringEvidenceReady
                    ? tr("Stage members and select exact evidence for every authored row.")
                    : tr("The profile and staged member updates are submitted "
                         "atomically; persisted omissions are preserved.")) });
        lanes.push_back({
            tr("planning"),
            hasTypedProfile ? tr("available") : tr("blocked"),
            hasTypedProfile
                ? tr("Typed descriptive data is available for non-executable planning.")
                : tr("Create and persist the typed profile first.") });

        QString spawnStatus = tr("not authorised");
        QString spawnReason = tr("No explicit spawn_candidate permission is recorded.");
        if (Contains(record->m_forbiddenUsages, "spawn_candidate"))
        {
            spawnStatus = tr("forbidden");
            spawnReason = tr("The canonical record explicitly forbids spawn candidacy.");
        }
        else if (Contains(record->m_allowedUsages, "spawn_candidate"))
        {
            if (!hasTypedProfile || !validatedCurrent || hasOpenBlockers)
            {
                spawnStatus = tr("blocked");
                spawnReason = tr(
                    "The permission exists, but typed data, current validation, "
                    "or blockers prevent candidacy.");
            }
            else
            {
                spawnStatus = tr("allowed");
                spawnReason = tr("Governed candidacy is recorded; no runtime execution occurs in this pane.");
            }
        }
        lanes.push_back({ tr("spawn_candidate"), spawnStatus, spawnReason });
        lanes.push_back({
            tr("runtime_spawn"),
            tr("unavailable"),
            tr("This SDK slice has no runtime adapter or actor-spawn authority.") });
        lanes.push_back({
            tr("save_mutation"),
            tr("unavailable"),
            tr("This editor never mutates FoA save data.") });

        laneTable->setRowCount(static_cast<int>(lanes.size()));
        for (int row = 0; row < static_cast<int>(lanes.size()); ++row)
        {
            const Lane& lane = lanes[static_cast<size_t>(row)];
            SetCell(laneTable, row, 0, lane.m_name);
            SetCell(laneTable, row, 1, lane.m_status);
            SetCell(laneTable, row, 2, lane.m_reason);
        }
    }

    void ActorTroopEditorWidget::SetStatus(
        StatusKind kind,
        const QString& message)
    {
        QString prefix;
        QString style;
        switch (kind)
        {
        case StatusKind::Success:
            prefix = tr("Saved — ");
            style = QStringLiteral("color: #2e7d32;");
            break;
        case StatusKind::Warning:
            prefix = tr("Attention — ");
            style = QStringLiteral("color: #a15c00;");
            break;
        case StatusKind::Error:
            prefix = tr("Error — ");
            style = QStringLiteral("color: #b3261e;");
            break;
        case StatusKind::Neutral:
        default:
            prefix = tr("Status — ");
            break;
        }
        m_status->setText(prefix + message);
        m_status->setStyleSheet(style);
        m_status->setAccessibleDescription(prefix + message);
    }

    void ActorTroopEditorWidget::MarkActorDirty()
    {
        if (m_refreshing || m_actorRecord->currentData().toString().isEmpty())
        {
            return;
        }
        m_actorDirty = true;
        m_actorRevert->setEnabled(true);
        m_actorState->setText(
            tr("Unsaved actor changes. Save validates exact evidence and persists the complete schema-2 catalog."));
        const AZStd::string recordId =
            ToAzString(m_actorRecord->currentData().toString());
        RefreshReviewTables(
            recordId,
            FoundationService::Get().GetCatalog()
                    .FindPopulationActorProfile(recordId) != nullptr,
            false,
            IsActorEvidenceReady(),
            m_actorValidation,
            m_actorGovernance,
            m_actorBlockers,
            m_actorRelationships,
            m_actorLanes);
    }

    void ActorTroopEditorWidget::MarkTroopDirty()
    {
        if (m_refreshing || m_troopRecord->currentData().toString().isEmpty())
        {
            return;
        }
        m_troopDirty = true;
        m_troopRevert->setEnabled(true);
        m_troopState->setText(
            tr("Unsaved troop changes. Member rows are staged locally until the complete definition is saved."));
        const AZStd::string recordId =
            ToAzString(m_troopRecord->currentData().toString());
        RefreshReviewTables(
            recordId,
            FoundationService::Get().GetCatalog()
                    .FindPopulationTroopProfile(recordId) != nullptr,
            true,
            IsTroopEvidenceReady(),
            m_troopValidation,
            m_troopGovernance,
            m_troopBlockers,
            m_troopRelationships,
            m_troopLanes);
    }
} // namespace TaintedGrailModdingSDK
