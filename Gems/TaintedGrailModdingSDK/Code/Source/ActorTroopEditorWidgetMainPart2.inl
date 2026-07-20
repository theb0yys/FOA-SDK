            tr("Validate, persist, and publish the complete actor profile candidate."));
        m_actorRevert->setAccessibleDescription(
            tr("Discard unsaved actor form changes and reload the persisted profile."));

        m_troopFilter->setAccessibleName(tr("Canonical troop filter"));
        m_troopRecord->setAccessibleName(tr("Canonical troop record"));
        m_troopEvidence->setAccessibleName(tr("Troop exact evidence selection"));
        m_memberEvidence->setAccessibleName(tr("Troop member exact evidence selection"));
        m_memberTable->setAccessibleDescription(
            tr("Staged troop members. Changes remain local until Save Troop Definition succeeds."));
        m_troopSave->setAccessibleDescription(
            tr("Validate and persist the troop profile and complete staged member set atomically."));
        m_troopRevert->setAccessibleDescription(
            tr("Discard all unsaved troop and member changes and reload persisted state."));

        QWidget::setTabOrder(m_actorFilter, m_actorRecord);
        QWidget::setTabOrder(m_actorRecord, m_actorKind);
        QWidget::setTabOrder(m_actorKind, m_actorArchetype);
        QWidget::setTabOrder(m_actorArchetype, m_actorTemplateRecord);
        QWidget::setTabOrder(m_actorTemplateRecord, m_actorTemplateSubject);
        QWidget::setTabOrder(m_actorTemplateSubject, m_actorMinimumLevel);
        QWidget::setTabOrder(m_actorMinimumLevel, m_actorMaximumLevel);
        QWidget::setTabOrder(m_actorMaximumLevel, m_actorEvidenceFilter);
        QWidget::setTabOrder(m_actorEvidenceFilter, m_actorEvidence);
        QWidget::setTabOrder(m_actorEvidence, m_actorSave);
        QWidget::setTabOrder(m_actorSave, m_actorRevert);

        QWidget::setTabOrder(m_troopFilter, m_troopRecord);
        QWidget::setTabOrder(m_troopRecord, m_troopKind);
        QWidget::setTabOrder(m_troopKind, m_troopLeaderRecord);
        QWidget::setTabOrder(m_troopLeaderRecord, m_troopLeaderSubject);
        QWidget::setTabOrder(m_troopLeaderSubject, m_troopMinimumSize);
        QWidget::setTabOrder(m_troopMinimumSize, m_troopMaximumSize);
        QWidget::setTabOrder(m_troopMaximumSize, m_troopEvidenceFilter);
        QWidget::setTabOrder(m_troopEvidenceFilter, m_troopEvidence);
        QWidget::setTabOrder(m_troopEvidence, m_memberTable);
        QWidget::setTabOrder(m_memberTable, m_memberLinkId);
        QWidget::setTabOrder(m_memberLinkId, m_memberActorRecord);
        QWidget::setTabOrder(m_memberActorRecord, m_memberActorSubject);
        QWidget::setTabOrder(m_memberActorSubject, m_memberRole);
        QWidget::setTabOrder(m_memberRole, m_memberEvidenceFilter);
        QWidget::setTabOrder(m_memberEvidenceFilter, m_memberEvidence);
        QWidget::setTabOrder(m_memberEvidence, m_memberApply);
        QWidget::setTabOrder(m_memberApply, m_troopSave);
        QWidget::setTabOrder(m_troopSave, m_troopRevert);
    }

    void ActorTroopEditorWidget::RefreshAll()
    {
        if (m_refreshing)
        {
            return;
        }

        m_refreshing = true;
        const int tabIndex = m_tabs->currentIndex();
        RefreshRecordChoices();
        LoadCurrentActor();
        LoadCurrentTroop();
        m_tabs->setCurrentIndex(tabIndex);
        m_refreshing = false;
    }

    void ActorTroopEditorWidget::RefreshRecordChoices()
    {
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const QString previousActor = m_actorRecord->currentData().toString();
        const QString previousTroop = m_troopRecord->currentData().toString();
        const QString previousTemplate =
            m_actorTemplateRecord->currentData().toString();
        const QString previousLeader =
            m_troopLeaderRecord->currentData().toString();
        const QString previousMemberActor =
            m_memberActorRecord->currentData().toString();
        const QString actorFilter = m_actorFilter->text().trimmed();
        const QString troopFilter = m_troopFilter->text().trimmed();

        QSignalBlocker actorBlocker(m_actorRecord);
        QSignalBlocker troopBlocker(m_troopRecord);
        QSignalBlocker templateBlocker(m_actorTemplateRecord);
        QSignalBlocker leaderBlocker(m_troopLeaderRecord);
        QSignalBlocker memberActorBlocker(m_memberActorRecord);

        m_actorRecord->clear();
        m_troopRecord->clear();
        m_actorTemplateRecord->clear();
        m_troopLeaderRecord->clear();
        m_memberActorRecord->clear();

        m_actorRecord->addItem(tr("Select canonical actor..."), QString());
        m_troopRecord->addItem(tr("Select canonical troop..."), QString());
        m_actorTemplateRecord->addItem(
            tr("Use no resolved template / enter exact subject"),
            QString());
        m_troopLeaderRecord->addItem(
            tr("Use no resolved leader / enter exact subject"),
            QString());
        m_memberActorRecord->addItem(
            tr("Use unresolved actor subject"),
            QString());

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            const QString id = ToQString(record.m_recordId);
            const QString label = RecordLabel(record);
            if (record.m_domain == "population"
                && record.m_recordKind == "actor")
            {
                if (RecordMatchesFilter(record, actorFilter)
                    || (m_actorDirty && id == previousActor))
                {
                    m_actorRecord->addItem(label, id);
                }
                m_troopLeaderRecord->addItem(label, id);
                m_memberActorRecord->addItem(label, id);
            }
            else if (record.m_domain == "population"
                && record.m_recordKind == "troop"
                && (RecordMatchesFilter(record, troopFilter)
                    || ((m_troopDirty || m_memberFormDirty)
                        && id == previousTroop)))
            {
                m_troopRecord->addItem(label, id);
            }
            else if (record.m_domain == "population"
                && (record.m_recordKind == "template"
                    || record.m_recordKind == "actor_template"))
            {
                m_actorTemplateRecord->addItem(label, id);
            }
        }

        if (m_actorDirty
            && !previousActor.isEmpty()
            && m_actorRecord->findData(previousActor) < 0)
        {
            m_actorRecord->addItem(
                tr("[missing canonical actor] %1").arg(previousActor),
                previousActor);
        }
        if ((m_troopDirty || m_memberFormDirty)
            && !previousTroop.isEmpty()
            && m_troopRecord->findData(previousTroop) < 0)
        {
            m_troopRecord->addItem(
                tr("[missing canonical troop] %1").arg(previousTroop),
                previousTroop);
        }

        auto restore = [](QComboBox* combo, const QString& value)
        {
            const int index = combo->findData(value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
        };
        restore(m_actorRecord, previousActor);
        restore(m_troopRecord, previousTroop);
        restore(m_actorTemplateRecord, previousTemplate);
        restore(m_troopLeaderRecord, previousLeader);
        restore(m_memberActorRecord, previousMemberActor);
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::SelectedEvidenceIds(const QListWidget* list) const
    {
        AZStd::vector<AZStd::string> result;
        if (!list)
        {
            return result;
        }
        for (const QListWidgetItem* item : list->selectedItems())
        {
            const AZStd::string evidenceId =
                ToAzString(item->data(Qt::UserRole).toString());
            if (!evidenceId.empty() && !Contains(result, evidenceId))
            {
                result.push_back(evidenceId);
            }
        }
        AZStd::sort(result.begin(), result.end());
        return result;
    }

    bool ActorTroopEditorWidget::EvidenceIdsReady(
        const AZStd::vector<AZStd::string>& evidenceIds,
        const AZStd::vector<AZStd::string>& allowedSubjects) const
    {
        if (evidenceIds.empty() || allowedSubjects.empty())
        {
            return false;
        }

        AZStd::vector<AZStd::string> sortedIds = evidenceIds;
        AZStd::sort(sortedIds.begin(), sortedIds.end());
        if (AZStd::adjacent_find(sortedIds.begin(), sortedIds.end())
            != sortedIds.end())
        {
            return false;
        }

        const FoundationService& foundation = FoundationService::Get();
        const GameProfile* profile =
            foundation.GetWorkspace().FindActiveGameProfile();
        const SourceEvidenceRegistry& registry = foundation.GetSourceRegistry();
        if (!profile || !profile->IsConfigured())
        {
            return false;
        }

        for (const AZStd::string& evidenceId : sortedIds)
        {
            const EvidenceRecord* evidence = registry.FindEvidence(evidenceId);
            const SourceRecord* source = evidence
                ? registry.FindSource(evidence->m_sourceId)
                : nullptr;
            if (!IsStableContractId(evidenceId)
                || !evidence
                || !source
                || !Contains(allowedSubjects, evidence->m_subjectRef)
                || evidence->m_claim.empty()
                || evidence->m_evidenceKind.empty()
                || evidence->m_confidence.empty()
                || evidence->m_locator.empty()
                || evidence->m_recordPath.empty()
                || !IsSha256Fingerprint(evidence->m_sourceFingerprint)
                || !IsStrictUtcTimestamp(evidence->m_extractedAt)
                || !IsSha256Fingerprint(source->m_fingerprint)
                || !IsStrictUtcTimestamp(source->m_capturedAt)
                || !IsStrictUtcTimestamp(source->m_importedAt)
                || !IsUsableImportStatus(source->m_importStatus)
                || evidence->m_sourceId != source->m_sourceId
                || evidence->m_sourceFingerprint != source->m_fingerprint
                || evidence->m_profileId != profile->m_profileId
                || evidence->m_gameVersion != profile->m_gameVersion
                || evidence->m_branch != profile->m_branch
                || source->m_profileId != profile->m_profileId
                || source->m_gameVersion != profile->m_gameVersion
                || source->m_branch != profile->m_branch
                || source->m_runtimeTarget != profile->m_runtimeTarget
                || source->m_capturedAt > evidence->m_extractedAt
                || evidence->m_extractedAt > source->m_importedAt)
            {
                return false;
            }
        }
        return true;
    }

    void ActorTroopEditorWidget::RefreshEvidenceList(
        QListWidget* list,
        QLineEdit* filter,
        const AZStd::vector<AZStd::string>& allowedSubjects,
        const AZStd::vector<AZStd::string>& selectedEvidenceIds)
    {
        const FoundationService& foundation = FoundationService::Get();
        const SourceEvidenceRegistry& registry = foundation.GetSourceRegistry();
        const GameProfile* profile =
            foundation.GetWorkspace().FindActiveGameProfile();
        const QString filterText = filter->text().trimmed();

        QSignalBlocker blocker(list);
        list->clear();
        AZStd::vector<AZStd::string> seenEvidenceIds;
        for (const EvidenceRecord& evidence : registry.GetEvidence())
        {
            const SourceRecord* source = registry.FindSource(evidence.m_sourceId);
            const bool profileMatches = profile
                && source
                && evidence.m_profileId == profile->m_profileId
                && evidence.m_gameVersion == profile->m_gameVersion
                && evidence.m_branch == profile->m_branch
                && source->m_profileId == profile->m_profileId
                && source->m_gameVersion == profile->m_gameVersion
                && source->m_branch == profile->m_branch
                && source->m_runtimeTarget == profile->m_runtimeTarget;
            const bool subjectMatches =
                Contains(allowedSubjects, evidence.m_subjectRef);
            const bool wasSelected =
                Contains(selectedEvidenceIds, evidence.m_evidenceId);
            if (!profileMatches || (!subjectMatches && !wasSelected))
            {
                continue;
            }

            const QString searchable = QStringLiteral("%1 %2 %3 %4 %5")
                .arg(ToQString(evidence.m_evidenceId))
                .arg(ToQString(evidence.m_subjectRef))
                .arg(ToQString(evidence.m_claim))
                .arg(ToQString(evidence.m_sourceId))
                .arg(ToQString(evidence.m_locator));
            if (!filterText.isEmpty()
                && !searchable.contains(filterText, Qt::CaseInsensitive)
                && !wasSelected)
            {
                continue;
            }

            QString label = QStringLiteral("%1 — %2")
                .arg(ToQString(evidence.m_evidenceId))
                .arg(ToQString(evidence.m_subjectRef));
            if (!subjectMatches)
