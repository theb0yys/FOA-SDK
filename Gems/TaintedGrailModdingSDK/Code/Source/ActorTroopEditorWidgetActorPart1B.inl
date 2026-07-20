        ConfigureTable(
            m_actorEvidenceDetails,
            tr("Selected actor evidence details"));
        evidenceLayout->addWidget(m_actorEvidenceDetails);
        layout->addWidget(evidenceGroup);

        auto* buttonRow = new QHBoxLayout();
        m_actorSave = new QPushButton(tr("&Save Actor Profile"), content);
        m_actorRevert = new QPushButton(tr("&Revert Actor Changes"), content);
        buttonRow->addWidget(m_actorSave);
        buttonRow->addWidget(m_actorRevert);
        buttonRow->addStretch(1);
        layout->addLayout(buttonRow);

        auto* reviewGroup = new QGroupBox(
            tr("Read-only Validation, Governance, Blockers, Relationships, and Action Lanes"),
            content);
        auto* reviewLayout = new QVBoxLayout(reviewGroup);
        auto* reviewDescription = new QLabel(
            tr("These review surfaces are informational. The pane cannot validate a claim, grant permission, "
               "clear a blocker, or invoke a runtime lane."),
            reviewGroup);
        reviewDescription->setWordWrap(true);
        reviewLayout->addWidget(reviewDescription);
        auto* reviewTabs = new QTabWidget(reviewGroup);

        m_actorValidation = new QTableWidget(0, 6, reviewTabs);
        m_actorValidation->setHorizontalHeaderLabels({
            tr("Checked at"), tr("State"), tr("Method"), tr("Validator"), tr("Evidence"), tr("Notes") });
        ConfigureTable(m_actorValidation, tr("Actor validation history"));
        reviewTabs->addTab(m_actorValidation, tr("Validation"));

        m_actorGovernance = new QTableWidget(0, 7, reviewTabs);
        m_actorGovernance->setHorizontalHeaderLabels({
            tr("Decided at"), tr("Axis"), tr("Decision / usage"), tr("Reviewer"),
            tr("Evidence"), tr("Validations"), tr("Notes") });
        ConfigureTable(m_actorGovernance, tr("Actor governance history"));
        reviewTabs->addTab(m_actorGovernance, tr("Governance"));

        m_actorBlockers = new QTableWidget(0, 6, reviewTabs);
        m_actorBlockers->setHorizontalHeaderLabels({
            tr("Blocker ID"), tr("Severity"), tr("Area"), tr("Status"), tr("Reason"), tr("Affected usages") });
        ConfigureTable(m_actorBlockers, tr("Actor blockers"));
        reviewTabs->addTab(m_actorBlockers, tr("Blockers"));

        m_actorRelationships = new QTableWidget(0, 7, reviewTabs);
        m_actorRelationships->setHorizontalHeaderLabels({
            tr("Relationship ID"), tr("Direction"), tr("Kind"), tr("Counterpart"),
            tr("Validation"), tr("Staleness"), tr("Evidence") });
        ConfigureTable(m_actorRelationships, tr("Actor relationships"));
        reviewTabs->addTab(m_actorRelationships, tr("Relationships"));

        m_actorLanes = new QTableWidget(0, 3, reviewTabs);
        m_actorLanes->setHorizontalHeaderLabels({
            tr("Lane"), tr("Status"), tr("Reason") });
        ConfigureTable(m_actorLanes, tr("Actor action lanes"));
        reviewTabs->addTab(m_actorLanes, tr("Action lanes"));

        reviewLayout->addWidget(reviewTabs);
        layout->addWidget(reviewGroup);
        layout->addStretch(1);

        connect(m_actorFilter, &QLineEdit::textChanged, this, [this]()
        {
            if (m_refreshing)
            {
                return;
            }
            m_refreshing = true;
            RefreshRecordChoices();
            if (m_actorDirty)
            {
                RefreshActorEvidenceChoices();
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
            else
            {
                LoadCurrentActor();
            }
            m_refreshing = false;
        });
        connect(
            m_actorRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                if (m_refreshing)
                {
                    return;
                }
                const AZStd::string requestedRecordId =
                    ToAzString(m_actorRecord->currentData().toString());
                if (m_actorDirty
                    && requestedRecordId != m_loadedActorRecordId)
                {
                    QSignalBlocker blocker(m_actorRecord);
                    SetComboByData(
                        m_actorRecord,
                        ToQString(m_loadedActorRecordId));
                    SetStatus(
                        StatusKind::Warning,
                        tr("Save or revert the unsaved actor changes before selecting another record."));
                    return;
                }
                m_refreshing = true;
                LoadCurrentActor();
                m_refreshing = false;
            });
        connect(
            m_actorTemplateRecord,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int)
            {
                if (m_refreshing)
                {
                    return;
                }
                SynchronizeResolvedSubject(
                    m_actorTemplateRecord,
                    m_actorTemplateSubject);
                RefreshActorEvidenceChoices();
                MarkActorDirty();
            });
        connect(m_actorTemplateSubject, &QLineEdit::textEdited, this, [this]()
        {
            RefreshActorEvidenceChoices();
            MarkActorDirty();
        });
        connect(m_actorEvidenceFilter, &QLineEdit::textChanged, this, [this]()
        {
            if (!m_refreshing)
            {
                RefreshActorEvidenceChoices();
            }
        });
        connect(
