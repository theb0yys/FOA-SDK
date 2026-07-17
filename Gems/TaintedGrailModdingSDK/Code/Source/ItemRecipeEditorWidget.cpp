/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ItemRecipeEditorWidget.h"

#include "FoundationService.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string ToAzString(const QString& value)
        {
            const QByteArray utf8 = value.trimmed().toUtf8();
            return AZStd::string(utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        QString ToQString(const AZStd::string& value)
        {
            return QString::fromUtf8(value.c_str());
        }

        AZStd::vector<AZStd::string> ParseCommaSeparated(const QString& value)
        {
            AZStd::vector<AZStd::string> values;
            for (const QString& part : value.split(',', Qt::SkipEmptyParts))
            {
                const AZStd::string converted = ToAzString(part);
                if (!converted.empty()
                    && AZStd::find(values.begin(), values.end(), converted) == values.end())
                {
                    values.push_back(converted);
                }
            }
            return values;
        }

        QString JoinValues(const AZStd::vector<AZStd::string>& values)
        {
            QStringList result;
            for (const AZStd::string& value : values)
            {
                result.push_back(ToQString(value));
            }
            return result.join(QStringLiteral(", "));
        }

        void ConfigureTable(QTableWidget* table)
        {
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
        }

        void SetCell(QTableWidget* table, int row, int column, const QString& value)
        {
            table->setItem(row, column, new QTableWidgetItem(value));
        }

        QWidget* WrapScrollable(QWidget* content, QWidget* parent)
        {
            auto* scroll = new QScrollArea(parent);
            scroll->setWidgetResizable(true);
            scroll->setWidget(content);
            return scroll;
        }

        bool IsAcquisitionKind(const AZStd::string& kind)
        {
            return kind == "sold_by" || kind == "dropped_by" || kind == "found_at"
                || kind == "rewarded_by" || kind == "learned_from" || kind == "granted_by"
                || kind == "crafted_at";
        }
    } // namespace

    ItemRecipeEditorWidget::ItemRecipeEditorWidget(QWidget* parent)
        : QWidget(parent)
    {
        auto* rootLayout = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Tainted Grail Item and Recipe Editor"), this);
        QFont headingFont = heading->font();
        headingFont.setPointSize(headingFont.pointSize() + 3);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        rootLayout->addWidget(heading);

        auto* description = new QLabel(
            tr("Author typed economy profiles on canonical records. This tool does not create identities, grant permissions, register runtime templates, mutate inventories, append recipes, or edit FoA."),
            this);
        description->setWordWrap(true);
        rootLayout->addWidget(description);

        m_tabs = new QTabWidget(this);
        rootLayout->addWidget(m_tabs, 1);

        auto* itemContent = new QWidget(m_tabs);
        auto* itemLayout = new QVBoxLayout(itemContent);

        auto* itemRecordGroup = new QGroupBox(tr("Canonical Item"), itemContent);
        auto* itemRecordLayout = new QFormLayout(itemRecordGroup);
        m_itemRecord = new QComboBox(itemRecordGroup);
        m_itemIdentity = new QLabel(itemRecordGroup);
        m_itemIdentity->setWordWrap(true);
        m_itemIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        itemRecordLayout->addRow(tr("Item record"), m_itemRecord);
        itemRecordLayout->addRow(tr("Identity and governance"), m_itemIdentity);
        itemLayout->addWidget(itemRecordGroup);

        auto* itemProfileGroup = new QGroupBox(tr("Typed Item Profile"), itemContent);
        auto* itemProfileLayout = new QFormLayout(itemProfileGroup);
        m_itemCategory = new QLineEdit(itemProfileGroup);
        m_itemSubtype = new QLineEdit(itemProfileGroup);
        m_itemStackLimit = new QSpinBox(itemProfileGroup);
        m_itemStackLimit->setRange(0, 1000000);
        m_itemWeight = new QDoubleSpinBox(itemProfileGroup);
        m_itemWeight->setRange(0.0, 1000000.0);
        m_itemWeight->setDecimals(4);
        m_itemBaseValue = new QDoubleSpinBox(itemProfileGroup);
        m_itemBaseValue->setRange(0.0, 1000000000.0);
        m_itemBaseValue->setDecimals(2);
        m_itemRarity = new QLineEdit(itemProfileGroup);
        m_itemQuality = new QLineEdit(itemProfileGroup);
        m_itemDurability = new QDoubleSpinBox(itemProfileGroup);
        m_itemDurability->setRange(0.0, 1000000.0);
        m_itemDurability->setDecimals(2);
        m_itemQuest = new QCheckBox(tr("Quest/story-sensitive"), itemProfileGroup);
        m_itemUnique = new QCheckBox(tr("Unique"), itemProfileGroup);
        m_itemHidden = new QCheckBox(tr("Hidden"), itemProfileGroup);
        m_itemNameRef = new QLineEdit(itemProfileGroup);
        m_itemDescriptionRef = new QLineEdit(itemProfileGroup);
        m_itemIconRef = new QLineEdit(itemProfileGroup);
        m_itemAssetRef = new QLineEdit(itemProfileGroup);
        m_itemTags = new QLineEdit(itemProfileGroup);
        m_itemEvidence = new QLineEdit(itemProfileGroup);
        m_itemTags->setPlaceholderText(tr("weapon, consumable, quest-risk"));
        m_itemEvidence->setPlaceholderText(tr("evidence.id.1, evidence.id.2"));
        itemProfileLayout->addRow(tr("Category"), m_itemCategory);
        itemProfileLayout->addRow(tr("Subtype"), m_itemSubtype);
        itemProfileLayout->addRow(tr("Stack limit (0 unknown)"), m_itemStackLimit);
        itemProfileLayout->addRow(tr("Weight"), m_itemWeight);
        itemProfileLayout->addRow(tr("Base value"), m_itemBaseValue);
        itemProfileLayout->addRow(tr("Rarity"), m_itemRarity);
        itemProfileLayout->addRow(tr("Quality"), m_itemQuality);
        itemProfileLayout->addRow(tr("Durability (0 unknown)"), m_itemDurability);
        itemProfileLayout->addRow(m_itemQuest);
        itemProfileLayout->addRow(m_itemUnique);
        itemProfileLayout->addRow(m_itemHidden);
        itemProfileLayout->addRow(tr("Name localisation ref"), m_itemNameRef);
        itemProfileLayout->addRow(tr("Description localisation ref"), m_itemDescriptionRef);
        itemProfileLayout->addRow(tr("Icon ref"), m_itemIconRef);
        itemProfileLayout->addRow(tr("Asset ref"), m_itemAssetRef);
        itemProfileLayout->addRow(tr("Tags"), m_itemTags);
        itemProfileLayout->addRow(tr("Evidence IDs"), m_itemEvidence);
        auto* saveItemButton = new QPushButton(tr("Save Item Profile"), itemProfileGroup);
        itemProfileLayout->addRow(saveItemButton);
        itemLayout->addWidget(itemProfileGroup);

        auto* itemLaneGroup = new QGroupBox(tr("Item Action Lanes — Governed Read-only Status"), itemContent);
        auto* itemLaneLayout = new QVBoxLayout(itemLaneGroup);
        m_itemLaneTable = new QTableWidget(0, 2, itemLaneGroup);
        m_itemLaneTable->setHorizontalHeaderLabels({ tr("Lane"), tr("Status") });
        ConfigureTable(m_itemLaneTable);
        itemLaneLayout->addWidget(m_itemLaneTable);
        itemLayout->addWidget(itemLaneGroup);
        itemLayout->addStretch(1);
        m_tabs->addTab(WrapScrollable(itemContent, m_tabs), tr("Items"));

        auto* recipeContent = new QWidget(m_tabs);
        auto* recipeLayout = new QVBoxLayout(recipeContent);

        auto* recipeRecordGroup = new QGroupBox(tr("Canonical Recipe"), recipeContent);
        auto* recipeRecordLayout = new QFormLayout(recipeRecordGroup);
        m_recipeRecord = new QComboBox(recipeRecordGroup);
        m_recipeIdentity = new QLabel(recipeRecordGroup);
        m_recipeIdentity->setWordWrap(true);
        m_recipeIdentity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        recipeRecordLayout->addRow(tr("Recipe record"), m_recipeRecord);
        recipeRecordLayout->addRow(tr("Identity and governance"), m_recipeIdentity);
        recipeLayout->addWidget(recipeRecordGroup);

        auto* recipeProfileGroup = new QGroupBox(tr("Typed Recipe Profile"), recipeContent);
        auto* recipeProfileLayout = new QFormLayout(recipeProfileGroup);
        m_recipeType = new QLineEdit(recipeProfileGroup);
        m_recipeTab = new QLineEdit(recipeProfileGroup);
        m_recipeStations = new QLineEdit(recipeProfileGroup);
        m_recipeUnlockMode = new QLineEdit(recipeProfileGroup);
        m_recipeUnlockRefs = new QLineEdit(recipeProfileGroup);
        m_recipeDuplicateKey = new QLineEdit(recipeProfileGroup);
        m_recipePersistence = new QComboBox(recipeProfileGroup);
        m_recipePersistence->addItems({ "unknown", "native_template", "runtime_append", "custom_template" });
        m_recipeHidden = new QCheckBox(tr("Hidden recipe"), recipeProfileGroup);
        m_recipeEvidence = new QLineEdit(recipeProfileGroup);
        m_recipeStations->setPlaceholderText(tr("economy.station.handcrafting, economy.station.forge"));
        m_recipeEvidence->setPlaceholderText(tr("evidence.id.1, evidence.id.2"));
        recipeProfileLayout->addRow(tr("Recipe type"), m_recipeType);
        recipeProfileLayout->addRow(tr("Recipe tab"), m_recipeTab);
        recipeProfileLayout->addRow(tr("Station record IDs"), m_recipeStations);
        recipeProfileLayout->addRow(tr("Unlock mode"), m_recipeUnlockMode);
        recipeProfileLayout->addRow(tr("Unlock subject refs"), m_recipeUnlockRefs);
        recipeProfileLayout->addRow(tr("Duplicate key"), m_recipeDuplicateKey);
        recipeProfileLayout->addRow(tr("Persistence mode"), m_recipePersistence);
        recipeProfileLayout->addRow(m_recipeHidden);
        recipeProfileLayout->addRow(tr("Evidence IDs"), m_recipeEvidence);
        auto* saveRecipeButton = new QPushButton(tr("Save Recipe Profile"), recipeProfileGroup);
        recipeProfileLayout->addRow(saveRecipeButton);
        recipeLayout->addWidget(recipeProfileGroup);

        auto* recipeLaneGroup = new QGroupBox(tr("Recipe Action Lanes — Governed Read-only Status"), recipeContent);
        auto* recipeLaneLayout = new QVBoxLayout(recipeLaneGroup);
        m_recipeLaneTable = new QTableWidget(0, 2, recipeLaneGroup);
        m_recipeLaneTable->setHorizontalHeaderLabels({ tr("Lane"), tr("Status") });
        ConfigureTable(m_recipeLaneTable);
        recipeLaneLayout->addWidget(m_recipeLaneTable);
        recipeLayout->addWidget(recipeLaneGroup);

        auto* recipeEvidenceGroup = new QGroupBox(
            tr("Station Visibility and Learnability Evidence — Read-only Research"),
            recipeContent);
        auto* recipeEvidenceLayout = new QVBoxLayout(recipeEvidenceGroup);
        auto* recipeEvidenceDescription = new QLabel(
            tr("This view combines exact station IDs, crafted_at and learned_from relationships, evidence health, governance, and blockers. It does not prove runtime visibility or learnability and cannot grant permission."),
            recipeEvidenceGroup);
        recipeEvidenceDescription->setWordWrap(true);
        recipeEvidenceLayout->addWidget(recipeEvidenceDescription);
        m_recipeEvidenceTable = new QTableWidget(0, 8, recipeEvidenceGroup);
        m_recipeEvidenceTable->setHorizontalHeaderLabels({
            tr("Station / unresolved subject"),
            tr("Exact identity"),
            tr("Association sources"),
            tr("Evidence"),
            tr("Research status"),
            tr("Governance"),
            tr("Learnability research"),
            tr("Blockers and reasons") });
        ConfigureTable(m_recipeEvidenceTable);
        recipeEvidenceLayout->addWidget(m_recipeEvidenceTable);
        recipeLayout->addWidget(recipeEvidenceGroup);

        auto* ingredientGroup = new QGroupBox(tr("Recipe Ingredients"), recipeContent);
        auto* ingredientLayout = new QVBoxLayout(ingredientGroup);
        m_ingredientTable = new QTableWidget(0, 6, ingredientGroup);
        m_ingredientTable->setHorizontalHeaderLabels({
            tr("Link ID"), tr("Item / subject"), tr("Quantity"), tr("Alternative"), tr("Consumed"), tr("Evidence") });
        ConfigureTable(m_ingredientTable);
        ingredientLayout->addWidget(m_ingredientTable);
        auto* ingredientForm = new QGridLayout();
        m_ingredientLinkId = new QLineEdit(ingredientGroup);
        m_ingredientItemRecord = new QComboBox(ingredientGroup);
        m_ingredientSubjectRef = new QLineEdit(ingredientGroup);
        m_ingredientQuantity = new QSpinBox(ingredientGroup);
        m_ingredientQuantity->setRange(1, 1000000);
        m_ingredientAlternativeGroup = new QLineEdit(ingredientGroup);
        m_ingredientConsumed = new QCheckBox(tr("Consumed"), ingredientGroup);
        m_ingredientConsumed->setChecked(true);
        m_ingredientConditions = new QLineEdit(ingredientGroup);
        m_ingredientEvidence = new QLineEdit(ingredientGroup);
        ingredientForm->addWidget(new QLabel(tr("Link ID"), ingredientGroup), 0, 0);
        ingredientForm->addWidget(m_ingredientLinkId, 0, 1);
        ingredientForm->addWidget(new QLabel(tr("Item record"), ingredientGroup), 0, 2);
        ingredientForm->addWidget(m_ingredientItemRecord, 0, 3);
        ingredientForm->addWidget(new QLabel(tr("Unresolved item subject"), ingredientGroup), 1, 0);
        ingredientForm->addWidget(m_ingredientSubjectRef, 1, 1);
        ingredientForm->addWidget(new QLabel(tr("Quantity"), ingredientGroup), 1, 2);
        ingredientForm->addWidget(m_ingredientQuantity, 1, 3);
        ingredientForm->addWidget(new QLabel(tr("Alternative group"), ingredientGroup), 2, 0);
        ingredientForm->addWidget(m_ingredientAlternativeGroup, 2, 1);
        ingredientForm->addWidget(m_ingredientConsumed, 2, 2);
        ingredientForm->addWidget(new QLabel(tr("Conditions"), ingredientGroup), 3, 0);
        ingredientForm->addWidget(m_ingredientConditions, 3, 1);
        ingredientForm->addWidget(new QLabel(tr("Evidence IDs"), ingredientGroup), 3, 2);
        ingredientForm->addWidget(m_ingredientEvidence, 3, 3);
        auto* saveIngredientButton = new QPushButton(tr("Add / Update Ingredient"), ingredientGroup);
        ingredientForm->addWidget(saveIngredientButton, 4, 3);
        ingredientLayout->addLayout(ingredientForm);
        recipeLayout->addWidget(ingredientGroup);

        auto* outputGroup = new QGroupBox(tr("Recipe Outputs and By-products"), recipeContent);
        auto* outputLayout = new QVBoxLayout(outputGroup);
        m_outputTable = new QTableWidget(0, 6, outputGroup);
        m_outputTable->setHorizontalHeaderLabels({
            tr("Link ID"), tr("Item / subject"), tr("Quantity"), tr("Chance"), tr("By-product"), tr("Evidence") });
        ConfigureTable(m_outputTable);
        outputLayout->addWidget(m_outputTable);
        auto* outputForm = new QGridLayout();
        m_outputLinkId = new QLineEdit(outputGroup);
        m_outputItemRecord = new QComboBox(outputGroup);
        m_outputSubjectRef = new QLineEdit(outputGroup);
        m_outputQuantity = new QSpinBox(outputGroup);
        m_outputQuantity->setRange(1, 1000000);
        m_outputChance = new QDoubleSpinBox(outputGroup);
        m_outputChance->setRange(0.000001, 1.0);
        m_outputChance->setDecimals(6);
        m_outputChance->setValue(1.0);
        m_outputByProduct = new QCheckBox(tr("By-product"), outputGroup);
        m_outputConditions = new QLineEdit(outputGroup);
        m_outputEvidence = new QLineEdit(outputGroup);
        outputForm->addWidget(new QLabel(tr("Link ID"), outputGroup), 0, 0);
        outputForm->addWidget(m_outputLinkId, 0, 1);
        outputForm->addWidget(new QLabel(tr("Item record"), outputGroup), 0, 2);
        outputForm->addWidget(m_outputItemRecord, 0, 3);
        outputForm->addWidget(new QLabel(tr("Unresolved item subject"), outputGroup), 1, 0);
        outputForm->addWidget(m_outputSubjectRef, 1, 1);
        outputForm->addWidget(new QLabel(tr("Quantity"), outputGroup), 1, 2);
        outputForm->addWidget(m_outputQuantity, 1, 3);
        outputForm->addWidget(new QLabel(tr("Chance (0–1)"), outputGroup), 2, 0);
        outputForm->addWidget(m_outputChance, 2, 1);
        outputForm->addWidget(m_outputByProduct, 2, 2);
        outputForm->addWidget(new QLabel(tr("Conditions"), outputGroup), 3, 0);
        outputForm->addWidget(m_outputConditions, 3, 1);
        outputForm->addWidget(new QLabel(tr("Evidence IDs"), outputGroup), 3, 2);
        outputForm->addWidget(m_outputEvidence, 3, 3);
        auto* saveOutputButton = new QPushButton(tr("Add / Update Output"), outputGroup);
        outputForm->addWidget(saveOutputButton, 4, 3);
        outputLayout->addLayout(outputForm);
        recipeLayout->addWidget(outputGroup);
        recipeLayout->addStretch(1);
        m_tabs->addTab(WrapScrollable(recipeContent, m_tabs), tr("Recipes"));

        auto* relationshipGroup = new QGroupBox(tr("Economy Acquisition Relationship"), this);
        auto* relationshipLayout = new QGridLayout(relationshipGroup);
        m_relationshipSource = new QComboBox(relationshipGroup);
        m_relationshipId = new QLineEdit(relationshipGroup);
        m_relationshipKind = new QComboBox(relationshipGroup);
        m_relationshipKind->addItems({
            "sold_by", "dropped_by", "found_at", "rewarded_by", "learned_from", "granted_by", "crafted_at" });
        m_relationshipTargetRecord = new QComboBox(relationshipGroup);
        m_relationshipTargetSubject = new QLineEdit(relationshipGroup);
        m_relationshipEvidence = new QLineEdit(relationshipGroup);
        m_relationshipAttributes = new QLineEdit(relationshipGroup);
        auto* saveRelationshipButton = new QPushButton(tr("Add / Update Relationship"), relationshipGroup);
        m_relationshipTable = new QTableWidget(0, 4, relationshipGroup);
        m_relationshipTable->setHorizontalHeaderLabels({ tr("ID"), tr("Kind"), tr("Target"), tr("Validation") });
        ConfigureTable(m_relationshipTable);
        relationshipLayout->addWidget(new QLabel(tr("Source item / recipe"), relationshipGroup), 0, 0);
        relationshipLayout->addWidget(m_relationshipSource, 0, 1);
        relationshipLayout->addWidget(new QLabel(tr("Relationship ID"), relationshipGroup), 0, 2);
        relationshipLayout->addWidget(m_relationshipId, 0, 3);
        relationshipLayout->addWidget(new QLabel(tr("Kind"), relationshipGroup), 1, 0);
        relationshipLayout->addWidget(m_relationshipKind, 1, 1);
        relationshipLayout->addWidget(new QLabel(tr("Target record"), relationshipGroup), 1, 2);
        relationshipLayout->addWidget(m_relationshipTargetRecord, 1, 3);
        relationshipLayout->addWidget(new QLabel(tr("Unresolved target subject"), relationshipGroup), 2, 0);
        relationshipLayout->addWidget(m_relationshipTargetSubject, 2, 1);
        relationshipLayout->addWidget(new QLabel(tr("Evidence IDs"), relationshipGroup), 2, 2);
        relationshipLayout->addWidget(m_relationshipEvidence, 2, 3);
        relationshipLayout->addWidget(new QLabel(tr("Attributes"), relationshipGroup), 3, 0);
        relationshipLayout->addWidget(m_relationshipAttributes, 3, 1, 1, 2);
        relationshipLayout->addWidget(saveRelationshipButton, 3, 3);
        relationshipLayout->addWidget(m_relationshipTable, 4, 0, 1, 4);
        rootLayout->addWidget(relationshipGroup);

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        rootLayout->addWidget(m_status);

        connect(m_itemRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { LoadCurrentItem(); });
        connect(m_recipeRecord, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { LoadCurrentRecipe(); });
        connect(m_relationshipSource, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { RefreshAcquisitionRelationships(); });
        connect(saveItemButton, &QPushButton::clicked, this, [this]() { SaveItemProfile(); });
        connect(saveRecipeButton, &QPushButton::clicked, this, [this]() { SaveRecipeProfile(); });
        connect(saveIngredientButton, &QPushButton::clicked, this, [this]() { SaveIngredient(); });
        connect(saveOutputButton, &QPushButton::clicked, this, [this]() { SaveOutput(); });
        connect(saveRelationshipButton, &QPushButton::clicked, this, [this]() { SaveAcquisitionRelationship(); });

        FoundationNotificationBus::Handler::BusConnect();
        RefreshAll();
    }

    ItemRecipeEditorWidget::~ItemRecipeEditorWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
    }

    void ItemRecipeEditorWidget::OnFoundationChanged()
    {
        RefreshAll();
    }

    void ItemRecipeEditorWidget::RefreshAll()
    {
        if (m_refreshing)
        {
            return;
        }
        m_refreshing = true;
        RefreshRecordChoices();
        LoadCurrentItem();
        LoadCurrentRecipe();
        RefreshAcquisitionRelationships();
        m_refreshing = false;
    }

    void ItemRecipeEditorWidget::RefreshRecordChoices()
    {
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const QString previousItem = m_itemRecord->currentData().toString();
        const QString previousRecipe = m_recipeRecord->currentData().toString();
        const QString previousSource = m_relationshipSource->currentData().toString();
        const QString previousIngredient = m_ingredientItemRecord->currentData().toString();
        const QString previousOutput = m_outputItemRecord->currentData().toString();
        const QString previousTarget = m_relationshipTargetRecord->currentData().toString();

        QSignalBlocker itemBlocker(m_itemRecord);
        QSignalBlocker recipeBlocker(m_recipeRecord);
        QSignalBlocker sourceBlocker(m_relationshipSource);
        QSignalBlocker ingredientBlocker(m_ingredientItemRecord);
        QSignalBlocker outputBlocker(m_outputItemRecord);
        QSignalBlocker targetBlocker(m_relationshipTargetRecord);
        m_itemRecord->clear();
        m_recipeRecord->clear();
        m_relationshipSource->clear();
        m_ingredientItemRecord->clear();
        m_outputItemRecord->clear();
        m_relationshipTargetRecord->clear();
        m_itemRecord->addItem(tr("Select canonical item…"), QString());
        m_recipeRecord->addItem(tr("Select canonical recipe…"), QString());
        m_relationshipSource->addItem(tr("Select item or recipe…"), QString());
        m_ingredientItemRecord->addItem(tr("Use unresolved subject ref"), QString());
        m_outputItemRecord->addItem(tr("Use unresolved subject ref"), QString());
        m_relationshipTargetRecord->addItem(tr("Use unresolved target subject"), QString());

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            const QString id = ToQString(record.m_recordId);
            QString label = id;
            if (!record.m_displayName.empty())
            {
                label += QStringLiteral(" — ") + ToQString(record.m_displayName);
            }
            m_relationshipTargetRecord->addItem(label, id);
            if (record.m_domain == "economy" && record.m_recordKind == "item")
            {
                m_itemRecord->addItem(label, id);
                m_relationshipSource->addItem(label, id);
                m_ingredientItemRecord->addItem(label, id);
                m_outputItemRecord->addItem(label, id);
            }
            else if (record.m_domain == "economy" && record.m_recordKind == "recipe")
            {
                m_recipeRecord->addItem(label, id);
                m_relationshipSource->addItem(label, id);
            }
        }

        auto restore = [](QComboBox* combo, const QString& value)
        {
            const int index = combo->findData(value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
        };
        restore(m_itemRecord, previousItem);
        restore(m_recipeRecord, previousRecipe);
        restore(m_relationshipSource, previousSource);
        restore(m_ingredientItemRecord, previousIngredient);
        restore(m_outputItemRecord, previousOutput);
        restore(m_relationshipTargetRecord, previousTarget);
    }

    void ItemRecipeEditorWidget::LoadCurrentItem()
    {
        const AZStd::string recordId = ToAzString(m_itemRecord->currentData().toString());
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const CatalogRecord* record = catalog.FindByRecordId(recordId);
        if (!record)
        {
            m_itemIdentity->setText(tr("No canonical item selected."));
            m_itemCategory->clear();
            m_itemSubtype->clear();
            m_itemStackLimit->setValue(0);
            m_itemWeight->setValue(0.0);
            m_itemBaseValue->setValue(0.0);
            m_itemRarity->clear();
            m_itemQuality->clear();
            m_itemDurability->setValue(0.0);
            m_itemQuest->setChecked(false);
            m_itemUnique->setChecked(false);
            m_itemHidden->setChecked(false);
            m_itemNameRef->clear();
            m_itemDescriptionRef->clear();
            m_itemIconRef->clear();
            m_itemAssetRef->clear();
            m_itemTags->clear();
            m_itemEvidence->clear();
            RefreshItemLaneTable();
            return;
        }
        m_itemIdentity->setText(
            tr("%1 | %2 | exact ref: %3 | validation: %4 | staleness: %5")
                .arg(ToQString(record->m_identityKind))
                .arg(record->m_ownerPackId.empty() ? tr("native/no pack owner") : ToQString(record->m_ownerPackId))
                .arg(record->m_nativeRefExact.empty() ? tr("not applicable") : ToQString(record->m_nativeRefExact))
                .arg(ToQString(record->m_validationState))
                .arg(ToQString(record->m_stalenessState)));
        const EconomyItemProfile* profile = catalog.FindEconomyItem(recordId);
        if (profile)
        {
            m_itemCategory->setText(ToQString(profile->m_category));
            m_itemSubtype->setText(ToQString(profile->m_subtype));
            m_itemStackLimit->setValue(static_cast<int>(profile->m_stackLimit));
            m_itemWeight->setValue(profile->m_weight);
            m_itemBaseValue->setValue(profile->m_baseValue);
            m_itemRarity->setText(ToQString(profile->m_rarity));
            m_itemQuality->setText(ToQString(profile->m_quality));
            m_itemDurability->setValue(profile->m_durability);
            m_itemQuest->setChecked(profile->m_questItem);
            m_itemUnique->setChecked(profile->m_uniqueItem);
            m_itemHidden->setChecked(profile->m_hiddenItem);
            m_itemNameRef->setText(ToQString(profile->m_localisationNameRef));
            m_itemDescriptionRef->setText(ToQString(profile->m_localisationDescriptionRef));
            m_itemIconRef->setText(ToQString(profile->m_iconRef));
            m_itemAssetRef->setText(ToQString(profile->m_assetRef));
            m_itemTags->setText(JoinValues(profile->m_tags));
            m_itemEvidence->setText(JoinValues(profile->m_evidenceIds));
        }
        else
        {
            m_itemCategory->clear();
            m_itemSubtype->clear();
            m_itemStackLimit->setValue(0);
            m_itemWeight->setValue(0.0);
            m_itemBaseValue->setValue(0.0);
            m_itemRarity->clear();
            m_itemQuality->clear();
            m_itemDurability->setValue(0.0);
            m_itemQuest->setChecked(false);
            m_itemUnique->setChecked(false);
            m_itemHidden->setChecked(false);
            m_itemNameRef->clear();
            m_itemDescriptionRef->clear();
            m_itemIconRef->clear();
            m_itemAssetRef->clear();
            m_itemTags->clear();
            m_itemEvidence->setText(JoinValues(record->m_evidenceIds));
        }
        RefreshItemLaneTable();
    }

    void ItemRecipeEditorWidget::LoadCurrentRecipe()
    {
        const AZStd::string recordId = ToAzString(m_recipeRecord->currentData().toString());
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const CatalogRecord* record = catalog.FindByRecordId(recordId);
        if (!record)
        {
            m_recipeIdentity->setText(tr("No canonical recipe selected."));
            m_recipeType->clear();
            m_recipeTab->clear();
            m_recipeStations->clear();
            m_recipeUnlockMode->clear();
            m_recipeUnlockRefs->clear();
            m_recipeDuplicateKey->clear();
            m_recipePersistence->setCurrentText(QStringLiteral("unknown"));
            m_recipeHidden->setChecked(false);
            m_recipeEvidence->clear();
            RefreshRecipeLaneTable();
            RefreshRecipeEvidence();
            RefreshRecipeJoins();
            return;
        }
        m_recipeIdentity->setText(
            tr("%1 | %2 | exact ref: %3 | validation: %4 | staleness: %5")
                .arg(ToQString(record->m_identityKind))
                .arg(record->m_ownerPackId.empty() ? tr("native/no pack owner") : ToQString(record->m_ownerPackId))
                .arg(record->m_nativeRefExact.empty() ? tr("not applicable") : ToQString(record->m_nativeRefExact))
                .arg(ToQString(record->m_validationState))
                .arg(ToQString(record->m_stalenessState)));
        const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(recordId);
        if (profile)
        {
            m_recipeType->setText(ToQString(profile->m_recipeType));
            m_recipeTab->setText(ToQString(profile->m_recipeTab));
            m_recipeStations->setText(JoinValues(profile->m_stationRecordIds));
            m_recipeUnlockMode->setText(ToQString(profile->m_unlockMode));
            m_recipeUnlockRefs->setText(JoinValues(profile->m_unlockSubjectRefs));
            m_recipeDuplicateKey->setText(ToQString(profile->m_duplicateKey));
            m_recipePersistence->setCurrentText(ToQString(profile->m_persistenceMode));
            m_recipeHidden->setChecked(profile->m_hiddenRecipe);
            m_recipeEvidence->setText(JoinValues(profile->m_evidenceIds));
        }
        else
        {
            m_recipeType->clear();
            m_recipeTab->clear();
            m_recipeStations->clear();
            m_recipeUnlockMode->clear();
            m_recipeUnlockRefs->clear();
            m_recipeDuplicateKey->setText(ToQString(record->m_nativeRefExact.empty() ? record->m_recordId : record->m_nativeRefExact));
            m_recipePersistence->setCurrentText(record->m_identityKind == "native" ? QStringLiteral("native_template") : QStringLiteral("unknown"));
            m_recipeHidden->setChecked(false);
            m_recipeEvidence->setText(JoinValues(record->m_evidenceIds));
        }
        RefreshRecipeLaneTable();
        RefreshRecipeEvidence();
        RefreshRecipeJoins();
    }

    void ItemRecipeEditorWidget::SaveItemProfile()
    {
        EconomyItemProfile profile;
        profile.m_recordId = ToAzString(m_itemRecord->currentData().toString());
        profile.m_category = ToAzString(m_itemCategory->text());
        profile.m_subtype = ToAzString(m_itemSubtype->text());
        profile.m_stackLimit = static_cast<AZ::u32>(m_itemStackLimit->value());
        profile.m_weight = m_itemWeight->value();
        profile.m_baseValue = m_itemBaseValue->value();
        profile.m_rarity = ToAzString(m_itemRarity->text());
        profile.m_quality = ToAzString(m_itemQuality->text());
        profile.m_durability = m_itemDurability->value();
        profile.m_questItem = m_itemQuest->isChecked();
        profile.m_uniqueItem = m_itemUnique->isChecked();
        profile.m_hiddenItem = m_itemHidden->isChecked();
        profile.m_localisationNameRef = ToAzString(m_itemNameRef->text());
        profile.m_localisationDescriptionRef = ToAzString(m_itemDescriptionRef->text());
        profile.m_iconRef = ToAzString(m_itemIconRef->text());
        profile.m_assetRef = ToAzString(m_itemAssetRef->text());
        profile.m_tags = ParseCommaSeparated(m_itemTags->text());
        profile.m_evidenceIds = ParseCommaSeparated(m_itemEvidence->text());
        AZStd::string error;
        if (!FoundationService::Get().UpsertEconomyItemProfile(profile, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Typed item profile saved to the canonical catalog."));
    }

    void ItemRecipeEditorWidget::SaveRecipeProfile()
    {
        EconomyRecipeProfile profile;
        profile.m_recordId = ToAzString(m_recipeRecord->currentData().toString());
        profile.m_recipeType = ToAzString(m_recipeType->text());
        profile.m_recipeTab = ToAzString(m_recipeTab->text());
        profile.m_stationRecordIds = ParseCommaSeparated(m_recipeStations->text());
        profile.m_unlockMode = ToAzString(m_recipeUnlockMode->text());
        profile.m_unlockSubjectRefs = ParseCommaSeparated(m_recipeUnlockRefs->text());
        profile.m_duplicateKey = ToAzString(m_recipeDuplicateKey->text());
        profile.m_persistenceMode = ToAzString(m_recipePersistence->currentText());
        profile.m_hiddenRecipe = m_recipeHidden->isChecked();
        profile.m_evidenceIds = ParseCommaSeparated(m_recipeEvidence->text());
        AZStd::string error;
        if (!FoundationService::Get().UpsertEconomyRecipeProfile(profile, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Typed recipe profile saved to the canonical catalog."));
    }

    void ItemRecipeEditorWidget::SaveIngredient()
    {
        EconomyRecipeIngredient ingredient;
        ingredient.m_linkId = ToAzString(m_ingredientLinkId->text());
        ingredient.m_recipeRecordId = ToAzString(m_recipeRecord->currentData().toString());
        ingredient.m_itemRecordId = ToAzString(m_ingredientItemRecord->currentData().toString());
        ingredient.m_itemSubjectRef = ToAzString(m_ingredientSubjectRef->text());
        ingredient.m_quantity = static_cast<AZ::u32>(m_ingredientQuantity->value());
        ingredient.m_alternativeGroup = ToAzString(m_ingredientAlternativeGroup->text());
        ingredient.m_consumed = m_ingredientConsumed->isChecked();
        ingredient.m_conditions = ParseCommaSeparated(m_ingredientConditions->text());
        ingredient.m_evidenceIds = ParseCommaSeparated(m_ingredientEvidence->text());
        AZStd::string error;
        if (!FoundationService::Get().UpsertEconomyRecipeIngredient(ingredient, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Recipe ingredient saved."));
    }

    void ItemRecipeEditorWidget::SaveOutput()
    {
        EconomyRecipeOutput output;
        output.m_linkId = ToAzString(m_outputLinkId->text());
        output.m_recipeRecordId = ToAzString(m_recipeRecord->currentData().toString());
        output.m_itemRecordId = ToAzString(m_outputItemRecord->currentData().toString());
        output.m_itemSubjectRef = ToAzString(m_outputSubjectRef->text());
        output.m_quantity = static_cast<AZ::u32>(m_outputQuantity->value());
        output.m_chance = m_outputChance->value();
        output.m_byProduct = m_outputByProduct->isChecked();
        output.m_conditions = ParseCommaSeparated(m_outputConditions->text());
        output.m_evidenceIds = ParseCommaSeparated(m_outputEvidence->text());
        AZStd::string error;
        if (!FoundationService::Get().UpsertEconomyRecipeOutput(output, &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Recipe output saved."));
    }

    void ItemRecipeEditorWidget::SaveAcquisitionRelationship()
    {
        EconomyAcquisitionRequest request;
        request.m_relationshipId = ToAzString(m_relationshipId->text());
        request.m_sourceRecordId = ToAzString(m_relationshipSource->currentData().toString());
        request.m_targetRecordId = ToAzString(m_relationshipTargetRecord->currentData().toString());
        request.m_targetSubjectRef = ToAzString(m_relationshipTargetSubject->text());
        request.m_relationshipKind = ToAzString(m_relationshipKind->currentText());
        request.m_evidenceIds = ParseCommaSeparated(m_relationshipEvidence->text());
        request.m_attributes = ParseCommaSeparated(m_relationshipAttributes->text());
        AZ::Outcome<CatalogRelationship, AZStd::string> result = m_economyAuthoring.BuildAcquisitionRelationship(
            request,
            FoundationService::Get().GetCatalog());
        if (!result.IsSuccess())
        {
            SetStatus(ToQString(result.GetError()), true);
            return;
        }
        AZStd::string error;
        if (!FoundationService::Get().UpsertCatalogRelationship(result.GetValue(), &error))
        {
            SetStatus(ToQString(error), true);
            return;
        }
        SetStatus(tr("Economy acquisition relationship saved as reviewed-but-unvalidated."));
    }

    void ItemRecipeEditorWidget::RefreshItemLaneTable()
    {
        const CatalogRecord* record = FoundationService::Get().GetCatalog().FindByRecordId(
            ToAzString(m_itemRecord->currentData().toString()));
        const AZStd::vector<EconomyActionLaneStatus> lanes = record
            ? m_economyAuthoring.BuildActionLaneMatrix(*record)
            : AZStd::vector<EconomyActionLaneStatus>{};
        m_itemLaneTable->setRowCount(static_cast<int>(lanes.size()));
        for (int row = 0; row < static_cast<int>(lanes.size()); ++row)
        {
            SetCell(m_itemLaneTable, row, 0, ToQString(lanes[static_cast<size_t>(row)].m_lane));
            SetCell(m_itemLaneTable, row, 1, ToQString(lanes[static_cast<size_t>(row)].m_status));
        }
    }

    void ItemRecipeEditorWidget::RefreshRecipeLaneTable()
    {
        const CatalogRecord* record = FoundationService::Get().GetCatalog().FindByRecordId(
            ToAzString(m_recipeRecord->currentData().toString()));
        const AZStd::vector<EconomyActionLaneStatus> lanes = record
            ? m_economyAuthoring.BuildActionLaneMatrix(*record)
            : AZStd::vector<EconomyActionLaneStatus>{};
        m_recipeLaneTable->setRowCount(static_cast<int>(lanes.size()));
        for (int row = 0; row < static_cast<int>(lanes.size()); ++row)
        {
            SetCell(m_recipeLaneTable, row, 0, ToQString(lanes[static_cast<size_t>(row)].m_lane));
            SetCell(m_recipeLaneTable, row, 1, ToQString(lanes[static_cast<size_t>(row)].m_status));
        }
    }

    void ItemRecipeEditorWidget::RefreshRecipeEvidence()
    {
        const FoundationService& foundation = FoundationService::Get();
        const AZStd::vector<EconomyRecipeStationEvidenceRow> rows = m_economyAuthoring.BuildRecipeStationEvidence(
            ToAzString(m_recipeRecord->currentData().toString()),
            foundation.GetSourceRegistry(),
            foundation.GetCatalog(),
            foundation.GetSnapshot().m_blockers);

        m_recipeEvidenceTable->setRowCount(static_cast<int>(rows.size()));
        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex)
        {
            const EconomyRecipeStationEvidenceRow& row = rows[static_cast<size_t>(rowIndex)];
            const QString station = row.m_stationRecordId.empty()
                ? ToQString(row.m_stationSubjectRef)
                : ToQString(row.m_stationRecordId);
            const QString governance = tr("validation: %1 | staleness: %2")
                .arg(ToQString(row.m_validationState))
                .arg(ToQString(row.m_stalenessState));
            QString learnability = ToQString(row.m_unlockMode);
            if (!row.m_unlockSubjectRefs.empty())
            {
                learnability += tr(" | unlock refs: %1").arg(JoinValues(row.m_unlockSubjectRefs));
            }
            if (!row.m_learnedFromRelationshipIds.empty())
            {
                learnability += tr(" | learned_from: %1").arg(JoinValues(row.m_learnedFromRelationshipIds));
            }
            QString blockersAndReasons = JoinValues(row.m_blockerIds);
            if (!row.m_reasons.empty())
            {
                if (!blockersAndReasons.isEmpty())
                {
                    blockersAndReasons += QStringLiteral(" | ");
                }
                blockersAndReasons += JoinValues(row.m_reasons);
            }

            SetCell(m_recipeEvidenceTable, rowIndex, 0, station);
            SetCell(m_recipeEvidenceTable, rowIndex, 1, ToQString(row.m_stationIdentity));
            SetCell(m_recipeEvidenceTable, rowIndex, 2, JoinValues(row.m_associationSources));
            SetCell(m_recipeEvidenceTable, rowIndex, 3, JoinValues(row.m_evidenceIds));
            SetCell(m_recipeEvidenceTable, rowIndex, 4, ToQString(row.m_status));
            SetCell(m_recipeEvidenceTable, rowIndex, 5, governance);
            SetCell(m_recipeEvidenceTable, rowIndex, 6, learnability);
            SetCell(m_recipeEvidenceTable, rowIndex, 7, blockersAndReasons);
        }
    }

    void ItemRecipeEditorWidget::RefreshRecipeJoins()
    {
        const AZStd::string recipeRecordId = ToAzString(m_recipeRecord->currentData().toString());
        const CatalogDatabase& catalog = FoundationService::Get().GetCatalog();
        const AZStd::vector<EconomyRecipeIngredient> ingredients = catalog.FindIngredientsForRecipe(recipeRecordId);
        m_ingredientTable->setRowCount(static_cast<int>(ingredients.size()));
        for (int row = 0; row < static_cast<int>(ingredients.size()); ++row)
        {
            const EconomyRecipeIngredient& ingredient = ingredients[static_cast<size_t>(row)];
            const QString item = ingredient.m_itemRecordId.empty()
                ? ToQString(ingredient.m_itemSubjectRef)
                : ToQString(ingredient.m_itemRecordId);
            SetCell(m_ingredientTable, row, 0, ToQString(ingredient.m_linkId));
            SetCell(m_ingredientTable, row, 1, item);
            SetCell(m_ingredientTable, row, 2, QString::number(ingredient.m_quantity));
            SetCell(m_ingredientTable, row, 3, ToQString(ingredient.m_alternativeGroup));
            SetCell(m_ingredientTable, row, 4, ingredient.m_consumed ? tr("Yes") : tr("No"));
            SetCell(m_ingredientTable, row, 5, JoinValues(ingredient.m_evidenceIds));
        }

        const AZStd::vector<EconomyRecipeOutput> outputs = catalog.FindOutputsForRecipe(recipeRecordId);
        m_outputTable->setRowCount(static_cast<int>(outputs.size()));
        for (int row = 0; row < static_cast<int>(outputs.size()); ++row)
        {
            const EconomyRecipeOutput& output = outputs[static_cast<size_t>(row)];
            const QString item = output.m_itemRecordId.empty()
                ? ToQString(output.m_itemSubjectRef)
                : ToQString(output.m_itemRecordId);
            SetCell(m_outputTable, row, 0, ToQString(output.m_linkId));
            SetCell(m_outputTable, row, 1, item);
            SetCell(m_outputTable, row, 2, QString::number(output.m_quantity));
            SetCell(m_outputTable, row, 3, QString::number(output.m_chance, 'f', 6));
            SetCell(m_outputTable, row, 4, output.m_byProduct ? tr("Yes") : tr("No"));
            SetCell(m_outputTable, row, 5, JoinValues(output.m_evidenceIds));
        }
    }

    void ItemRecipeEditorWidget::RefreshAcquisitionRelationships()
    {
        const AZStd::string sourceRecordId = ToAzString(m_relationshipSource->currentData().toString());
        AZStd::vector<CatalogRelationship> relationships;
        for (const CatalogRelationship& relationship : FoundationService::Get().GetCatalog().FindRelationshipsForRecord(sourceRecordId))
        {
            if (relationship.m_fromRecordId == sourceRecordId && IsAcquisitionKind(relationship.m_relationshipKind))
            {
                relationships.push_back(relationship);
            }
        }
        m_relationshipTable->setRowCount(static_cast<int>(relationships.size()));
        for (int row = 0; row < static_cast<int>(relationships.size()); ++row)
        {
            const CatalogRelationship& relationship = relationships[static_cast<size_t>(row)];
            const QString target = relationship.m_toRecordId.empty()
                ? ToQString(relationship.m_targetSubjectRef)
                : ToQString(relationship.m_toRecordId);
            SetCell(m_relationshipTable, row, 0, ToQString(relationship.m_relationshipId));
            SetCell(m_relationshipTable, row, 1, ToQString(relationship.m_relationshipKind));
            SetCell(m_relationshipTable, row, 2, target);
            SetCell(m_relationshipTable, row, 3, ToQString(relationship.m_validationState));
        }
    }

    void ItemRecipeEditorWidget::SetStatus(const QString& message, bool error)
    {
        m_status->setText(message);
        m_status->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
    }
} // namespace TaintedGrailModdingSDK
