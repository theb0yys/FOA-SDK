/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "EconomyAuthoringService.h"
#include "FoundationNotificationBus.h"

#include <AzCore/std/algorithm.h>

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QString;
class QTableWidget;
class QTabWidget;

namespace TaintedGrailModdingSDK
{
    class ItemRecipeEditorWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ItemRecipeEditorWidget(QWidget* parent = nullptr);
        ~ItemRecipeEditorWidget() override;

    private:
        void OnFoundationChanged() override;
        void RefreshAll();
        void RefreshRecordChoices();
        void LoadCurrentItem();
        void LoadCurrentRecipe();
        void SaveItemProfile();
        void SaveRecipeProfile();
        void SaveIngredient();
        void SaveOutput();
        void SaveAcquisitionRelationship();
        void RefreshItemLaneTable();
        void RefreshRecipeLaneTable();
        void RefreshRecipeEvidence();
        void RefreshRecipeJoins();
        void RefreshAcquisitionRelationships();
        void SetStatus(const QString& message, bool error = false);

        QTabWidget* m_tabs = nullptr;

        QComboBox* m_itemRecord = nullptr;
        QLabel* m_itemIdentity = nullptr;
        QLineEdit* m_itemCategory = nullptr;
        QLineEdit* m_itemSubtype = nullptr;
        QSpinBox* m_itemStackLimit = nullptr;
        QDoubleSpinBox* m_itemWeight = nullptr;
        QDoubleSpinBox* m_itemBaseValue = nullptr;
        QLineEdit* m_itemRarity = nullptr;
        QLineEdit* m_itemQuality = nullptr;
        QDoubleSpinBox* m_itemDurability = nullptr;
        QCheckBox* m_itemQuest = nullptr;
        QCheckBox* m_itemUnique = nullptr;
        QCheckBox* m_itemHidden = nullptr;
        QLineEdit* m_itemNameRef = nullptr;
        QLineEdit* m_itemDescriptionRef = nullptr;
        QLineEdit* m_itemIconRef = nullptr;
        QLineEdit* m_itemAssetRef = nullptr;
        QLineEdit* m_itemTags = nullptr;
        QLineEdit* m_itemEvidence = nullptr;
        QTableWidget* m_itemLaneTable = nullptr;

        QComboBox* m_recipeRecord = nullptr;
        QLabel* m_recipeIdentity = nullptr;
        QLineEdit* m_recipeType = nullptr;
        QLineEdit* m_recipeTab = nullptr;
        QLineEdit* m_recipeStations = nullptr;
        QLineEdit* m_recipeUnlockMode = nullptr;
        QLineEdit* m_recipeUnlockRefs = nullptr;
        QLineEdit* m_recipeDuplicateKey = nullptr;
        QComboBox* m_recipePersistence = nullptr;
        QCheckBox* m_recipeHidden = nullptr;
        QLineEdit* m_recipeEvidence = nullptr;
        QTableWidget* m_recipeLaneTable = nullptr;
        QTableWidget* m_recipeEvidenceTable = nullptr;

        QTableWidget* m_ingredientTable = nullptr;
        QLineEdit* m_ingredientLinkId = nullptr;
        QComboBox* m_ingredientItemRecord = nullptr;
        QLineEdit* m_ingredientSubjectRef = nullptr;
        QSpinBox* m_ingredientQuantity = nullptr;
        QLineEdit* m_ingredientAlternativeGroup = nullptr;
        QCheckBox* m_ingredientConsumed = nullptr;
        QLineEdit* m_ingredientConditions = nullptr;
        QLineEdit* m_ingredientEvidence = nullptr;

        QTableWidget* m_outputTable = nullptr;
        QLineEdit* m_outputLinkId = nullptr;
        QComboBox* m_outputItemRecord = nullptr;
        QLineEdit* m_outputSubjectRef = nullptr;
        QSpinBox* m_outputQuantity = nullptr;
        QDoubleSpinBox* m_outputChance = nullptr;
        QCheckBox* m_outputByProduct = nullptr;
        QLineEdit* m_outputConditions = nullptr;
        QLineEdit* m_outputEvidence = nullptr;

        QComboBox* m_relationshipSource = nullptr;
        QLineEdit* m_relationshipId = nullptr;
        QComboBox* m_relationshipKind = nullptr;
        QComboBox* m_relationshipTargetRecord = nullptr;
        QLineEdit* m_relationshipTargetSubject = nullptr;
        QLineEdit* m_relationshipEvidence = nullptr;
        QLineEdit* m_relationshipAttributes = nullptr;
        QTableWidget* m_relationshipTable = nullptr;

        QLabel* m_status = nullptr;
        EconomyAuthoringService m_economyAuthoring;
        bool m_refreshing = false;
    };
} // namespace TaintedGrailModdingSDK
