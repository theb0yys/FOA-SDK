            m_actorModelRef->clear();
            m_actorTags->clear();
            m_actorRevert->setEnabled(false);
            RefreshActorEvidenceChoices({});
            RefreshReviewTables(
                {},
                false,
                false,
                false,
                m_actorValidation,
                m_actorGovernance,
                m_actorBlockers,
                m_actorRelationships,
                m_actorLanes);
            return;
        }

        const QString owner = record->m_ownerPackId.empty()
            ? tr("native / no pack owner")
            : ToQString(record->m_ownerPackId);
        m_actorIdentity->setText(
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

        const PopulationActorProfile* profile =
            catalog.FindPopulationActorProfile(recordId);
        AZStd::vector<AZStd::string> evidenceIds;
        if (profile)
        {
            SetComboByText(m_actorKind, ToQString(profile->m_actorKind));
            m_actorArchetype->setText(ToQString(profile->m_archetype));
            SetComboByData(
                m_actorTemplateRecord,
                ToQString(profile->m_templateRecordId));
            m_actorTemplateSubject->setText(
                ToQString(profile->m_templateSubjectRef));
            m_actorMinimumLevel->setValue(
                static_cast<int>(profile->m_minimumLevel));
            m_actorMaximumLevel->setValue(
                static_cast<int>(profile->m_maximumLevel));
            m_actorUnique->setChecked(profile->m_uniqueActor);
            m_actorEssential->setChecked(profile->m_essentialActor);
            m_actorPersistent->setChecked(profile->m_persistentActor);
            m_actorNameRef->setText(
                ToQString(profile->m_localisationNameRef));
            m_actorDescriptionRef->setText(
                ToQString(profile->m_localisationDescriptionRef));
            m_actorPortraitRef->setText(
                ToQString(profile->m_portraitAssetRef));
            m_actorModelRef->setText(ToQString(profile->m_modelAssetRef));
            m_actorTags->setText(JoinValues(profile->m_tags));
            evidenceIds = profile->m_evidenceIds;
            m_actorState->setText(
                tr("Loaded persisted profile. Save publishes only after exact evidence, complete catalog integrity, "
                   "and schema-2 persistence succeed."));
        }
        else
        {
            m_actorKind->setCurrentIndex(0);
            m_actorArchetype->clear();
            m_actorTemplateRecord->setCurrentIndex(0);
            m_actorTemplateSubject->clear();
            m_actorMinimumLevel->setValue(0);
            m_actorMaximumLevel->setValue(0);
            m_actorUnique->setChecked(false);
            m_actorEssential->setChecked(false);
            m_actorPersistent->setChecked(false);
            m_actorNameRef->clear();
            m_actorDescriptionRef->clear();
            m_actorPortraitRef->clear();
            m_actorModelRef->clear();
            m_actorTags->clear();
            if (foundation.GetWorkspaceRootPath().empty())
            {
                m_actorState->setText(
                    tr("Blocked state — save or load the workspace before authoring this actor."));
            }
            else
            {
                m_actorState->setText(
                    foundation.GetActivePack()
                        ? tr("Empty typed-profile state — complete the form and select exact evidence before saving.")
                        : tr("Blocked state — select an active editor-owned pack before authoring this actor."));
            }
        }
        m_actorRevert->setEnabled(profile != nullptr);
        RefreshActorEvidenceChoices(evidenceIds);
        RefreshReviewTables(
            recordId,
            profile != nullptr,
            false,
            IsActorEvidenceReady(),
            m_actorValidation,
            m_actorGovernance,
            m_actorBlockers,
            m_actorRelationships,
            m_actorLanes);
    }

    void ActorTroopEditorWidget::SaveActorProfile()
    {
        PopulationActorProfile profile;
        profile.m_recordId =
            ToAzString(m_actorRecord->currentData().toString());
        profile.m_actorKind = ToAzString(m_actorKind->currentText());
        profile.m_archetype = ToAzString(m_actorArchetype->text());
        profile.m_templateRecordId =
            ToAzString(m_actorTemplateRecord->currentData().toString());
        profile.m_templateSubjectRef =
            ToAzString(m_actorTemplateSubject->text());
        profile.m_minimumLevel =
            static_cast<AZ::u32>(m_actorMinimumLevel->value());
        profile.m_maximumLevel =
            static_cast<AZ::u32>(m_actorMaximumLevel->value());
        profile.m_uniqueActor = m_actorUnique->isChecked();
        profile.m_essentialActor = m_actorEssential->isChecked();
        profile.m_persistentActor = m_actorPersistent->isChecked();
        profile.m_localisationNameRef = ToAzString(m_actorNameRef->text());
        profile.m_localisationDescriptionRef =
            ToAzString(m_actorDescriptionRef->text());
        profile.m_portraitAssetRef = ToAzString(m_actorPortraitRef->text());
        profile.m_modelAssetRef = ToAzString(m_actorModelRef->text());
        profile.m_tags = ParseCommaSeparated(m_actorTags->text());
        profile.m_evidenceIds = SelectedEvidenceIds(m_actorEvidence);

        m_actorDirty = false;
        AZStd::string error;
        if (!FoundationService::Get().UpsertPopulationActorProfile(
                profile,
                &error))
        {
            m_actorDirty = true;
            m_actorState->setText(
                tr("Save failed. The previously published catalog remains unchanged."));
            SetStatus(
                StatusKind::Error,
                tr("Actor profile save failed: %1").arg(ToQString(error)));
            return;
        }
