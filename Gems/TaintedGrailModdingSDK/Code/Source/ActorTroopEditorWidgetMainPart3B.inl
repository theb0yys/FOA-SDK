            SetCell(validationTable, row, 3, ToQString(validation.m_validator));
            SetCell(validationTable, row, 4, JoinValues(validation.m_evidenceIds));
            SetCell(validationTable, row, 5, ToQString(validation.m_notes));
        }

        const AZStd::vector<CatalogGovernanceEvent> governance =
            catalog.FindGovernanceForSubject("record", recordId);
        governanceTable->setRowCount(static_cast<int>(governance.size()));
        for (int row = 0; row < static_cast<int>(governance.size()); ++row)
        {
            const CatalogGovernanceEvent& event =
                governance[static_cast<size_t>(row)];
            SetCell(governanceTable, row, 0, ToQString(event.m_decidedAt));
            SetCell(governanceTable, row, 1, ToQString(event.m_axis));
            SetCell(governanceTable, row, 2, DecisionValue(event));
            SetCell(governanceTable, row, 3, ToQString(event.m_reviewer));
            SetCell(governanceTable, row, 4, JoinValues(event.m_evidenceIds));
            SetCell(governanceTable, row, 5, JoinValues(event.m_validationIds));
            SetCell(governanceTable, row, 6, ToQString(event.m_notes));
        }

        AZStd::vector<BlockerRecord> blockers;
        for (const BlockerRecord& blocker : foundation.GetSnapshot().m_blockers)
        {
            if (BlockerMatchesRecord(blocker, *record))
            {
                blockers.push_back(blocker);
            }
        }
        blockerTable->setRowCount(static_cast<int>(blockers.size()));
        for (int row = 0; row < static_cast<int>(blockers.size()); ++row)
        {
            const BlockerRecord& blocker = blockers[static_cast<size_t>(row)];
            SetCell(blockerTable, row, 0, ToQString(blocker.m_blockerId));
            SetCell(blockerTable, row, 1, ToQString(blocker.m_severity));
            SetCell(blockerTable, row, 2, ToQString(blocker.m_area));
            SetCell(blockerTable, row, 3, ToQString(blocker.m_status));
            SetCell(blockerTable, row, 4, ToQString(blocker.m_reason));
            SetCell(blockerTable, row, 5, JoinValues(blocker.m_affectedUsages));
        }

        const AZStd::vector<CatalogRelationship> relationships =
            catalog.FindRelationshipsForRecord(recordId);
        relationshipTable->setRowCount(static_cast<int>(relationships.size()));
        for (int row = 0; row < static_cast<int>(relationships.size()); ++row)
        {
            const CatalogRelationship& relationship =
                relationships[static_cast<size_t>(row)];
            const bool outgoing = relationship.m_fromRecordId == recordId;
            const QString counterpart = outgoing
                ? (relationship.m_toRecordId.empty()
                    ? ToQString(relationship.m_targetSubjectRef)
                    : ToQString(relationship.m_toRecordId))
                : ToQString(relationship.m_fromRecordId);
            SetCell(
                relationshipTable,
                row,
                0,
                ToQString(relationship.m_relationshipId));
            SetCell(
                relationshipTable,
                row,
                1,
                outgoing ? tr("outgoing") : tr("incoming"));
            SetCell(
                relationshipTable,
                row,
                2,
                ToQString(relationship.m_relationshipKind));
            SetCell(relationshipTable, row, 3, counterpart);
            SetCell(
                relationshipTable,
                row,
                4,
                ToQString(relationship.m_validationState));
            SetCell(
                relationshipTable,
                row,
                5,
                ToQString(relationship.m_stalenessState));
            SetCell(
                relationshipTable,
                row,
                6,
                JoinValues(relationship.m_evidenceIds));
        }

        struct Lane
        {
            QString m_name;
            QString m_status;
            QString m_reason;
        };
        AZStd::vector<Lane> lanes;
        const PackManifest* activePack = foundation.GetActivePack();
        const GameProfile* activeProfile =
            foundation.GetWorkspace().FindActiveGameProfile();
        const bool recordOwnershipMatches = activePack
            && (record->m_ownerPackId.empty()
                || record->m_ownerPackId == activePack->m_packId);
        const bool packTargetsProfile = activePack
            && activeProfile
            && !activePack->m_targetGameVersion.empty()
            && !activePack->m_targetBranch.empty()
            && (activePack->m_targetGameVersion
                    == activeProfile->m_gameVersion
                || Contains(
                    activePack->m_compatibleGameVersions,
                    activeProfile->m_gameVersion))
            && activePack->m_targetBranch == activeProfile->m_branch;
        const bool workspaceAuthoringReady =
            IsStableContractId(foundation.GetWorkspace().m_workspaceId)
            && !foundation.GetWorkspaceRootPath().empty();
        const bool activePackAuthoringReady = workspaceAuthoringReady
            && activePack
            && activeProfile
            && activeProfile->IsConfigured()
            && activePack->HasStableIdentity()
            && activePack->UsesSupportedSchema()
            && !activePack->m_runtimeActionsEnabled
            && packTargetsProfile;
        const bool packCompatible =
            activePackAuthoringReady && recordOwnershipMatches;
        const bool validatedCurrent =
            record->m_validationState == "validated"
            && record->m_stalenessState == "current"
            && record->m_missingRefs.empty()
            && record->m_conflictRefs.empty()
            && record->m_supersededByRecordId.empty();
        const bool hasOpenBlockers = AZStd::any_of(
            blockers.begin(),
            blockers.end(),
            [](const BlockerRecord& blocker)
            {
                return blocker.m_status != "resolved"
                    && blocker.m_status != "closed"
                    && blocker.m_status != "dismissed";
            });

        lanes.push_back({
            tr("display"),
            tr("available"),
            tr("Canonical identity and review evidence may be inspected in the editor.") });
        const bool profileAuthoringReady =
            packCompatible && authoringEvidenceReady;
        lanes.push_back({
            tr("author_profile"),
            profileAuthoringReady ? tr("available") : tr("blocked"),
            !workspaceAuthoringReady
                ? tr("Save or load the workspace so Foundation has a path-policy-validated root.")
