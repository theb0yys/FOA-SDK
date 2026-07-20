            &QListWidget::itemSelectionChanged,
            this,
            [this]()
            {
                if (!m_refreshing)
                {
                    RefreshEvidenceDetails(
                        m_troopEvidence,
                        m_memberEvidence,
                        m_troopEvidenceDetails);
                    MarkMemberFormDirty();
                }
            });
        connect(m_memberApply, &QPushButton::clicked, this, [this]()
        {
            AddOrUpdateMember();
        });
        connect(m_memberRemove, &QPushButton::clicked, this, [this]()
        {
            RemoveSelectedMember();
        });
        connect(m_memberClear, &QPushButton::clicked, this, [this]()
        {
            ClearMemberEditor();
            SetStatus(
                StatusKind::Neutral,
                tr("The member form was cleared; the staged member set was not changed."));
        });
        connect(m_troopSave, &QPushButton::clicked, this, [this]()
        {
            SaveTroopDefinition();
        });
        connect(m_troopRevert, &QPushButton::clicked, this, [this]()
        {
            RevertTroopDefinition();
        });

        connect(
            m_troopKind,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(
            m_troopMinimumSize,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(
            m_troopMaximumSize,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkTroopDirty(); });
        connect(m_troopFormation, &QLineEdit::textEdited, this, [this]()
        {
            MarkTroopDirty();
        });
        connect(m_troopTags, &QLineEdit::textEdited, this, [this]()
        {
            MarkTroopDirty();
        });
        connect(
            m_memberRole,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { MarkMemberFormDirty(); });
        connect(
            m_memberMinimumCount,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkMemberFormDirty(); });
        connect(
            m_memberMaximumCount,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) { MarkMemberFormDirty(); });
        connect(
            m_memberWeight,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) { MarkMemberFormDirty(); });
        connect(m_memberRequired, &QCheckBox::toggled, this, [this](bool)
        {
            MarkMemberFormDirty();
        });
        connect(m_memberConditions, &QLineEdit::textEdited, this, [this]()
        {
            MarkMemberFormDirty();
        });

        return content;
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::ResolveTroopEvidenceSubjects() const
    {
        AZStd::vector<AZStd::string> subjects;
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const CatalogRecord* troop = catalog.FindByRecordId(
            ToAzString(m_troopRecord->currentData().toString()));
        if (troop)
        {
            subjects.push_back(troop->m_subjectRef);
        }
        const AZStd::string leaderSubject = ResolveActorSubject(
            ToAzString(m_troopLeaderRecord->currentData().toString()),
            ToAzString(m_troopLeaderSubject->text()));
        if (!leaderSubject.empty() && !Contains(subjects, leaderSubject))
        {
            subjects.push_back(leaderSubject);
        }
        return subjects;
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::ResolveMemberEvidenceSubjects() const
    {
        PopulationTroopMember member;
        member.m_linkId = ToAzString(m_memberLinkId->text());
        member.m_troopRecordId =
            ToAzString(m_troopRecord->currentData().toString());
        member.m_actorRecordId =
            ToAzString(m_memberActorRecord->currentData().toString());
        member.m_actorSubjectRef = ToAzString(m_memberActorSubject->text());
        return ResolveMemberEvidenceSubjects(member);
    }

    AZStd::vector<AZStd::string>
    ActorTroopEditorWidget::ResolveMemberEvidenceSubjects(
        const PopulationTroopMember& member) const
    {
        AZStd::vector<AZStd::string> subjects;
        if (!member.m_linkId.empty())
        {
            subjects.push_back(
                "population-troop-member:" + member.m_linkId);
        }
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const CatalogRecord* troop =
            catalog.FindByRecordId(member.m_troopRecordId);
        if (troop && !Contains(subjects, troop->m_subjectRef))
        {
            subjects.push_back(troop->m_subjectRef);
        }
        const AZStd::string actorSubject = ResolveActorSubject(
            member.m_actorRecordId,
            member.m_actorSubjectRef);
        if (!actorSubject.empty() && !Contains(subjects, actorSubject))
        {
            subjects.push_back(actorSubject);
        }
