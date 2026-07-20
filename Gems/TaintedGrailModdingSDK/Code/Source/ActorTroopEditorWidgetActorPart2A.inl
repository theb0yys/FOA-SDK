            m_actorEvidence,
            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                if (!m_refreshing)
                {
                    RefreshEvidenceDetails(
                        m_actorEvidence,
                        nullptr,
                        m_actorEvidenceDetails);
                    MarkActorDirty();
                }
            });
        connect(m_actorSave, &QPushButton::clicked, this, [this]()
        {
            SaveActorProfile();
        });
        connect(m_actorRevert, &QPushButton::clicked, this, [this]()
        {
            RevertActorProfile();
        });

        connect(
            m_actorKind,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkActorDirty(); });
        for (QLineEdit* edit : {
                 m_actorArchetype,
                 m_actorNameRef,
                 m_actorDescriptionRef,
                 m_actorPortraitRef,
                 m_actorModelRef,
                 m_actorTags })
        {
            connect(edit, &QLineEdit::textEdited, this, [this]()
            {
                MarkActorDirty();
            });
        }
        connect(
            m_actorMinimumLevel,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkActorDirty(); });
        connect(
            m_actorMaximumLevel,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkActorDirty(); });
        for (QCheckBox* checkbox : {
                 m_actorUnique,
                 m_actorEssential,
                 m_actorPersistent })
        {
            connect(checkbox, &QCheckBox::toggled, this, [this](bool)
            {
                MarkActorDirty();
            });
        }

        return content;
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::ResolveActorEvidenceSubjects() const
    {
        AZStd::vector<AZStd::string> subjects;
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        auto addRecordSubject = [&subjects, &catalog](const QString& recordId)
        {
            const CatalogRecord* record = catalog.FindByRecordId(
                ActorTroopEditorWidget::ToAzString(recordId));
            if (record
                && AZStd::find(
                    subjects.begin(),
                    subjects.end(),
                    record->m_subjectRef) == subjects.end())
            {
                subjects.push_back(record->m_subjectRef);
            }
        };
        addRecordSubject(m_actorRecord->currentData().toString());
        addRecordSubject(m_actorTemplateRecord->currentData().toString());
        const AZStd::string unresolved =
            ToAzString(m_actorTemplateSubject->text());
        if (!unresolved.empty()
            && AZStd::find(subjects.begin(), subjects.end(), unresolved)
                == subjects.end())
        {
            subjects.push_back(unresolved);
        }
        return subjects;
    }

    bool ActorTroopEditorWidget::IsActorEvidenceReady() const
    {
        return EvidenceIdsReady(
            SelectedEvidenceIds(m_actorEvidence),
            ResolveActorEvidenceSubjects());
    }

    void ActorTroopEditorWidget::RefreshActorEvidenceChoices()
    {
        RefreshActorEvidenceChoices(SelectedEvidenceIds(m_actorEvidence));
    }

    void ActorTroopEditorWidget::RefreshActorEvidenceChoices(
        const AZStd::vector<AZStd::string>& selectedEvidenceIds)
    {
        RefreshEvidenceList(
            m_actorEvidence,
            m_actorEvidenceFilter,
            ResolveActorEvidenceSubjects(),
            selectedEvidenceIds);
        RefreshEvidenceDetails(
            m_actorEvidence,
            nullptr,
            m_actorEvidenceDetails);
    }

    void ActorTroopEditorWidget::LoadCurrentActor()
    {
        const AZStd::string recordId =
            ToAzString(m_actorRecord->currentData().toString());
        const FoundationService& foundation = FoundationService::Get();
        const CatalogDatabase& catalog = foundation.GetCatalog();
        const CatalogRecord* record = catalog.FindByRecordId(recordId);

        m_loadedActorRecordId = record ? recordId : AZStd::string{};
        m_actorDirty = false;
        m_actorSave->setEnabled(record != nullptr);
        if (!record)
        {
            m_actorIdentity->setText(tr("No canonical actor selected."));
            m_actorState->setText(
                tr("Empty state — use the actor filter and select an existing population/actor record."));
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
