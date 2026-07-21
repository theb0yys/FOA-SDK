/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"
#include "PopulationModels.h"

#include <QWidget>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QString;
class QTableWidget;
class QTabWidget;

namespace TaintedGrailModdingSDK
{
    struct CatalogRecord;

    //! Thin Editor client for evidence-bound actor and atomic troop authoring.
    //! The widget never writes catalog files or invokes runtime/deployment APIs.
    class ActorTroopEditorWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ActorTroopEditorWidget(QWidget* parent = nullptr);
        ~ActorTroopEditorWidget() override;

    private:
        void closeEvent(QCloseEvent* event) override;
        void OnFoundationChanged() override;

        void RefreshAll();
        void ApplyPendingFoundationRefresh();
        void RefreshRecordChoices();
        void RefreshReferenceChoices();
        void RefreshActorEvidenceChoices();
        void RefreshTroopEvidenceChoices();
        void RefreshMemberEvidenceChoices();
        void RefreshEvidenceDetails(QListWidget* selector, QTableWidget* details);
        void RefreshActorReviewContext(const CatalogRecord* record);
        void RefreshTroopReviewContext(const CatalogRecord* record);
        void RefreshActorActionLanes(const CatalogRecord* record);
        void RefreshTroopActionLanes(const CatalogRecord* record);
        void RefreshMemberTable();

        void LoadCurrentActor();
        void LoadCurrentTroop();
        void LoadSelectedMember();
        void ClearMemberEditor();
        bool StageMember();
        void SaveActorProfile();
        void SaveTroopDefinition();
        void RevertActorProfile();
        void RevertTroopDefinition();
        void MarkActorDirty();
        void MarkTroopDirty();
        void MarkMemberEditorDirty();
        void HandleActorRecordChange();
        void HandleTroopRecordChange();
        void HandleMemberSelectionChange();
        bool HasDirtyDrafts() const;

        PopulationActorProfile BuildActorProfile() const;
        PopulationTroopProfile BuildTroopProfile() const;
        PopulationTroopMember BuildMember() const;

        void SetActorState(const QString& message, bool error = false);
        void SetTroopState(const QString& message, bool error = false);
        void SetStatus(const QString& message, bool error = false);
        void UpdateEnabledStates();

        QTabWidget* m_tabs = nullptr;

        QLineEdit* m_actorFilter = nullptr;
        QComboBox* m_actorRecord = nullptr;
        QLabel* m_actorIdentity = nullptr;
        QLabel* m_actorState = nullptr;
        QComboBox* m_actorKind = nullptr;
        QLineEdit* m_actorArchetype = nullptr;
        QComboBox* m_actorTemplateRecord = nullptr;
        QLineEdit* m_actorTemplateSubject = nullptr;
        QSpinBox* m_actorMinimumLevel = nullptr;
        QSpinBox* m_actorMaximumLevel = nullptr;
        QCheckBox* m_actorUnique = nullptr;
        QCheckBox* m_actorEssential = nullptr;
        QCheckBox* m_actorPersistent = nullptr;
        QLineEdit* m_actorNameRef = nullptr;
        QLineEdit* m_actorDescriptionRef = nullptr;
        QLineEdit* m_actorPortraitRef = nullptr;
        QLineEdit* m_actorModelRef = nullptr;
        QLineEdit* m_actorTags = nullptr;
        QListWidget* m_actorEvidence = nullptr;
        QTableWidget* m_actorEvidenceDetails = nullptr;
        QPlainTextEdit* m_actorValidationSummary = nullptr;
        QPlainTextEdit* m_actorGovernanceSummary = nullptr;
        QPlainTextEdit* m_actorBlockerSummary = nullptr;
        QPlainTextEdit* m_actorRelationshipSummary = nullptr;
        QTableWidget* m_actorActionLanes = nullptr;
        QPushButton* m_saveActor = nullptr;
        QPushButton* m_revertActor = nullptr;

        QLineEdit* m_troopFilter = nullptr;
        QComboBox* m_troopRecord = nullptr;
        QLabel* m_troopIdentity = nullptr;
        QLabel* m_troopState = nullptr;
        QComboBox* m_troopKind = nullptr;
        QComboBox* m_troopLeaderRecord = nullptr;
        QLineEdit* m_troopLeaderSubject = nullptr;
        QSpinBox* m_troopMinimumSize = nullptr;
        QSpinBox* m_troopMaximumSize = nullptr;
        QLineEdit* m_troopFormation = nullptr;
        QLineEdit* m_troopTags = nullptr;
        QListWidget* m_troopEvidence = nullptr;
        QTableWidget* m_troopEvidenceDetails = nullptr;

        QTableWidget* m_memberTable = nullptr;
        QLineEdit* m_memberLinkId = nullptr;
        QComboBox* m_memberActorRecord = nullptr;
        QLineEdit* m_memberActorSubject = nullptr;
        QComboBox* m_memberRole = nullptr;
        QSpinBox* m_memberMinimumCount = nullptr;
        QSpinBox* m_memberMaximumCount = nullptr;
        QLineEdit* m_memberWeight = nullptr;
        QCheckBox* m_memberRequired = nullptr;
        QLineEdit* m_memberConditions = nullptr;
        QListWidget* m_memberEvidence = nullptr;
        QTableWidget* m_memberEvidenceDetails = nullptr;
        QPushButton* m_stageMember = nullptr;
        QPushButton* m_clearMember = nullptr;

        QPlainTextEdit* m_troopValidationSummary = nullptr;
        QPlainTextEdit* m_troopGovernanceSummary = nullptr;
        QPlainTextEdit* m_troopBlockerSummary = nullptr;
        QPlainTextEdit* m_troopRelationshipSummary = nullptr;
        QTableWidget* m_troopActionLanes = nullptr;
        QPushButton* m_saveTroop = nullptr;
        QPushButton* m_revertTroop = nullptr;

        QLabel* m_status = nullptr;
        AZStd::vector<PopulationTroopMember> m_draftMembers;
        AZStd::string m_loadedActorRecordId;
        AZStd::string m_loadedTroopRecordId;
        AZStd::string m_selectedMemberLinkId;
        bool m_actorDirty = false;
        bool m_troopDirty = false;
        bool m_memberEditorDirty = false;
        bool m_loadingActor = false;
        bool m_loadingTroop = false;
        bool m_loadingMember = false;
        bool m_foundationRefreshPending = false;
        bool m_refreshing = false;
    };
} // namespace TaintedGrailModdingSDK
