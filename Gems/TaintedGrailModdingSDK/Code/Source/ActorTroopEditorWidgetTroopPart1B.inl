            tr("Conditions"),
            tr("Evidence"),
        });
        ConfigureTable(m_memberTable, tr("Staged troop members"));
        memberLayout->addWidget(m_memberTable);

        auto* memberForm = new QGridLayout();
        m_memberLinkId = new QLineEdit(memberGroup);
        m_memberLinkId->setPlaceholderText(
            tr("population.member.unique-link"));
        m_memberActorRecord = new QComboBox(memberGroup);
        m_memberActorSubject = new QLineEdit(memberGroup);
        m_memberActorSubject->setPlaceholderText(
            tr("Exact unresolved actor subject, preserving case"));
        m_memberRole = new QComboBox(memberGroup);
        m_memberRole->addItems({
            QStringLiteral("leader"),
            QStringLiteral("melee"),
            QStringLiteral("ranged"),
            QStringLiteral("support"),
            QStringLiteral("specialist"),
            QStringLiteral("other"),
        });
        m_memberMinimumCount = new QSpinBox(memberGroup);
        m_memberMinimumCount->setRange(1, 1000);
        m_memberMaximumCount = new QSpinBox(memberGroup);
        m_memberMaximumCount->setRange(1, 1000);
        m_memberWeight = new QDoubleSpinBox(memberGroup);
        m_memberWeight->setRange(0.0, 1000000.0);
        m_memberWeight->setDecimals(4);
        m_memberWeight->setValue(1.0);
        m_memberRequired = new QCheckBox(tr("Required member"), memberGroup);
        m_memberConditions = new QLineEdit(memberGroup);
        m_memberConditions->setPlaceholderText(tr("morning, rain, state:key=value"));

        memberForm->addWidget(new QLabel(tr("Link ID"), memberGroup), 0, 0);
        memberForm->addWidget(m_memberLinkId, 0, 1);
        memberForm->addWidget(new QLabel(tr("Resolved actor"), memberGroup), 0, 2);
        memberForm->addWidget(m_memberActorRecord, 0, 3);
        memberForm->addWidget(new QLabel(tr("Exact actor subject"), memberGroup), 1, 0);
        memberForm->addWidget(m_memberActorSubject, 1, 1);
        memberForm->addWidget(new QLabel(tr("Role"), memberGroup), 1, 2);
        memberForm->addWidget(m_memberRole, 1, 3);
        memberForm->addWidget(new QLabel(tr("Minimum count"), memberGroup), 2, 0);
        memberForm->addWidget(m_memberMinimumCount, 2, 1);
        memberForm->addWidget(new QLabel(tr("Maximum count"), memberGroup), 2, 2);
        memberForm->addWidget(m_memberMaximumCount, 2, 3);
        memberForm->addWidget(new QLabel(tr("Planning weight"), memberGroup), 3, 0);
        memberForm->addWidget(m_memberWeight, 3, 1);
        memberForm->addWidget(m_memberRequired, 3, 2);
        memberForm->addWidget(new QLabel(tr("Conditions"), memberGroup), 4, 0);
        memberForm->addWidget(m_memberConditions, 4, 1, 1, 3);
        memberLayout->addLayout(memberForm);

        m_memberEvidenceFilter = new QLineEdit(memberGroup);
        m_memberEvidenceFilter->setPlaceholderText(
            tr("Filter evidence for the current actor, troop, or membership subject"));
        memberLayout->addWidget(m_memberEvidenceFilter);
        m_memberEvidence = new QListWidget(memberGroup);
        m_memberEvidence->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_memberEvidence->setMinimumHeight(120);
        memberLayout->addWidget(m_memberEvidence);

        auto* memberButtons = new QHBoxLayout();
        m_memberApply = new QPushButton(tr("Add / &Update Staged Member"), memberGroup);
        m_memberRemove = new QPushButton(tr("&Discard New Staged Member"), memberGroup);
        m_memberClear = new QPushButton(tr("&Clear Member Form"), memberGroup);
        memberButtons->addWidget(m_memberApply);
        memberButtons->addWidget(m_memberRemove);
        memberButtons->addWidget(m_memberClear);
        memberButtons->addStretch(1);
        memberLayout->addLayout(memberButtons);
        layout->addWidget(memberGroup);

        auto* evidenceDetailsGroup = new QGroupBox(
            tr("Selected Troop and Member Evidence Details"),
            content);
        auto* evidenceDetailsLayout = new QVBoxLayout(evidenceDetailsGroup);
        m_troopEvidenceDetails = new QTableWidget(0, 8, evidenceDetailsGroup);
        m_troopEvidenceDetails->setHorizontalHeaderLabels({
            tr("Evidence ID"),
            tr("Exact subject"),
            tr("Claim"),
            tr("Kind"),
            tr("Confidence"),
            tr("Source"),
            tr("Locator / record path"),
            tr("Provenance state"),
        });
        ConfigureTable(
            m_troopEvidenceDetails,
            tr("Selected troop and member evidence details"));
        evidenceDetailsLayout->addWidget(m_troopEvidenceDetails);
        layout->addWidget(evidenceDetailsGroup);

        auto* saveButtons = new QHBoxLayout();
        m_troopSave = new QPushButton(tr("Save &Troop Definition"), content);
        m_troopRevert = new QPushButton(tr("&Revert Troop Changes"), content);
        saveButtons->addWidget(m_troopSave);
        saveButtons->addWidget(m_troopRevert);
        saveButtons->addStretch(1);
        layout->addLayout(saveButtons);

        auto* reviewGroup = new QGroupBox(
            tr("Read-only Validation, Governance, Blockers, Relationships, and Action Lanes"),
            content);
        auto* reviewLayout = new QVBoxLayout(reviewGroup);
        auto* reviewDescription = new QLabel(
            tr("These review surfaces are informational. Runtime spawn and save mutation remain unavailable."),
            reviewGroup);
        reviewDescription->setWordWrap(true);
        reviewLayout->addWidget(reviewDescription);
        auto* reviewTabs = new QTabWidget(reviewGroup);

        m_troopValidation = new QTableWidget(0, 6, reviewTabs);
        m_troopValidation->setHorizontalHeaderLabels({
            tr("Checked at"), tr("State"), tr("Method"), tr("Validator"), tr("Evidence"), tr("Notes") });
        ConfigureTable(m_troopValidation, tr("Troop validation history"));
        reviewTabs->addTab(m_troopValidation, tr("Validation"));

        m_troopGovernance = new QTableWidget(0, 7, reviewTabs);
        m_troopGovernance->setHorizontalHeaderLabels({
            tr("Decided at"), tr("Axis"), tr("Decision / usage"), tr("Reviewer"),
            tr("Evidence"), tr("Validations"), tr("Notes") });
        ConfigureTable(m_troopGovernance, tr("Troop governance history"));
        reviewTabs->addTab(m_troopGovernance, tr("Governance"));

        m_troopBlockers = new QTableWidget(0, 6, reviewTabs);
        m_troopBlockers->setHorizontalHeaderLabels({
            tr("Blocker ID"), tr("Severity"), tr("Area"), tr("Status"), tr("Reason"), tr("Affected usages") });
        ConfigureTable(m_troopBlockers, tr("Troop blockers"));
        reviewTabs->addTab(m_troopBlockers, tr("Blockers"));

        m_troopRelationships = new QTableWidget(0, 7, reviewTabs);
        m_troopRelationships->setHorizontalHeaderLabels({
            tr("Relationship ID"), tr("Direction"), tr("Kind"), tr("Counterpart"),
            tr("Validation"), tr("Staleness"), tr("Evidence") });
        ConfigureTable(m_troopRelationships, tr("Troop relationships"));
        reviewTabs->addTab(m_troopRelationships, tr("Relationships"));

        m_troopLanes = new QTableWidget(0, 3, reviewTabs);
        m_troopLanes->setHorizontalHeaderLabels({
            tr("Lane"), tr("Status"), tr("Reason") });
        ConfigureTable(m_troopLanes, tr("Troop action lanes"));
        reviewTabs->addTab(m_troopLanes, tr("Action lanes"));

        reviewLayout->addWidget(reviewTabs);
        layout->addWidget(reviewGroup);
        layout->addStretch(1);

