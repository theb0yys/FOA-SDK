        connect(m_troopFilter, &QLineEdit::textChanged, this, [this]()
        {
            if (m_refreshing)
            {
                return;
            }
            m_refreshing = true;
            RefreshRecordChoices();
            if (m_troopDirty || m_memberFormDirty)
            {
                RefreshTroopEvidenceChoices();
                RefreshMemberEvidenceChoices();
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
            else
            {
                LoadCurrentTroop();
            }
            m_refreshing = false;
        });
        connect(
            m_troopRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                if (m_refreshing)
                {
                    return;
                }
                const AZStd::string requestedRecordId =
                    ToAzString(m_troopRecord->currentData().toString());
                if ((m_troopDirty || m_memberFormDirty)
                    && requestedRecordId != m_loadedTroopRecordId)
                {
                    QSignalBlocker blocker(m_troopRecord);
                    SetComboByData(
                        m_troopRecord,
                        ToQString(m_loadedTroopRecordId));
                    SetStatus(
                        StatusKind::Warning,
                        tr("Save or revert the unsaved troop changes before selecting another record."));
                    return;
                }
                m_refreshing = true;
                LoadCurrentTroop();
                m_refreshing = false;
            });
        connect(
            m_troopLeaderRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                if (m_refreshing)
                {
                    return;
                }
                SynchronizeResolvedSubject(
                    m_troopLeaderRecord,
                    m_troopLeaderSubject);
                RefreshTroopEvidenceChoices();
                MarkTroopDirty();
            });
        connect(m_troopLeaderSubject, &QLineEdit::textEdited, this, [this]()
        {
            RefreshTroopEvidenceChoices();
            MarkTroopDirty();
        });
        connect(m_troopEvidenceFilter, &QLineEdit::textChanged, this, [this]()
        {
            if (!m_refreshing)
            {
                RefreshTroopEvidenceChoices();
            }
        });
        connect(
            m_troopEvidence,
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
                    MarkTroopDirty();
                }
            });

        connect(
            m_memberTable,
            &QTableWidget::itemSelectionChanged,
            this,
            [this]()
            {
                if (!m_refreshing)
                {
                    LoadSelectedMember();
                }
            });
        connect(
            m_memberActorRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                if (m_refreshing)
                {
                    return;
                }
                SynchronizeResolvedSubject(
                    m_memberActorRecord,
                    m_memberActorSubject);
                RefreshMemberEvidenceChoices();
                MarkMemberFormDirty();
            });
        connect(m_memberActorSubject, &QLineEdit::textEdited, this, [this]()
        {
            RefreshMemberEvidenceChoices();
            MarkMemberFormDirty();
        });
        connect(m_memberLinkId, &QLineEdit::textEdited, this, [this]()
        {
            RefreshMemberEvidenceChoices();
            MarkMemberFormDirty();
        });
        connect(m_memberEvidenceFilter, &QLineEdit::textChanged, this, [this]()
        {
            if (!m_refreshing)
            {
                RefreshMemberEvidenceChoices();
            }
        });
        connect(
            m_memberEvidence,
