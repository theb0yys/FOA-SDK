/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationNotificationBus.h"
#include "PopulationAuthoringService.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QString;
class QTableWidget;
class QTabWidget;

namespace TaintedGrailModdingSDK
{
    class ActorTroopEditorWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ActorTroopEditorWidget(QWidget* parent = nullptr);
        ~ActorTroopEditorWidget() override;

    private:
        enum class StatusKind
        {
            Neutral,
            Success,
            Warning,
            Error,
        };

        void OnFoundationChanged() override;

        QWidget* BuildActorTab(QWidget* parent);
        QWidget* BuildTroopTab(QWidget* parent);
        void ConfigureAccessibility();

        void RefreshAll();
        void RefreshRecordChoices();
        void RefreshActorEvidenceChoices();
        void RefreshActorEvidenceChoices(
            const AZStd::vector<AZStd::string>& selectedEvidenceIds);
        void RefreshTroopEvidenceChoices();
        void RefreshTroopEvidenceChoices(
            const AZStd::vector<AZStd::string>& selectedEvidenceIds);
        void RefreshMemberEvidenceChoices();
        void RefreshMemberEvidenceChoices(
            const AZStd::vector<AZStd::string>& selectedEvidenceIds);
        void RefreshEvidenceList(
            QListWidget* list,
            QLineEdit* filter,
            const AZStd::vector<AZStd::string>& allowedSubjects,
            const AZStd::vector<AZStd::string>& selectedEvidenceIds);
        void RefreshEvidenceDetails(
            const QListWidget* primary,
            const QListWidget* secondary,
            QTableWidget* table) const;
        void RefreshReviewTables(
            const AZStd::string& recordId,
            bool hasTypedProfile,
            bool isTroop,
            bool authoringEvidenceReady,
            QTableWidget* validationTable,
            QTableWidget* governanceTable,
            QTableWidget* blockerTable,
            QTableWidget* relationshipTable,
            QTableWidget* laneTable) const;

        void LoadCurrentActor();
        void SaveActorProfile();
        void RevertActorProfile();
        void MarkActorDirty();

        void LoadCurrentTroop();
        void SaveTroopDefinition();
        void RevertTroopDefinition();
        void MarkTroopDirty();
        void MarkMemberFormDirty();
        void RefreshMemberTable();
        void LoadSelectedMember();
        void AddOrUpdateMember();
        void RemoveSelectedMember();
        void ClearMemberEditor();

        AZStd::vector<AZStd::string> SelectedEvidenceIds(
            const QListWidget* list) const;
        bool EvidenceIdsReady(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::vector<AZStd::string>& allowedSubjects) const;
        AZStd::vector<AZStd::string> ResolveActorEvidenceSubjects() const;
        bool IsActorEvidenceReady() const;
        AZStd::vector<AZStd::string> ResolveTroopEvidenceSubjects() const;
        bool IsTroopEvidenceReady() const;
        AZStd::vector<AZStd::string> ResolveMemberEvidenceSubjects() const;
        AZStd::vector<AZStd::string> ResolveMemberEvidenceSubjects(
            const PopulationTroopMember& member) const;
        AZStd::string ResolveActorSubject(
            const AZStd::string& recordId,
            const AZStd::string& unresolvedSubject) const;
        void SynchronizeResolvedSubject(
            QComboBox* recordCombo,
            QLineEdit* subjectEdit);
        void SetStatus(StatusKind kind, const QString& message);

        static AZStd::string ToAzString(const QString& value);
        static QString ToQString(const AZStd::string& value);
        static AZStd::vector<AZStd::string> ParseCommaSeparated(
            const QString& value);
        static QString JoinValues(
            const AZStd::vector<AZStd::string>& values);
        static void ConfigureTable(
            QTableWidget* table,
            const QString& accessibleName);
        static void SetCell(
            QTableWidget* table,
            int row,
            int column,
            const QString& value);

        QTabWidget* m_tabs = nullptr;
        QLabel* m_status = nullptr;

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
        QLineEdit* m_actorEvidenceFilter = nullptr;
        QListWidget* m_actorEvidence = nullptr;
        QTableWidget* m_actorEvidenceDetails = nullptr;
        QPushButton* m_actorSave = nullptr;
        QPushButton* m_actorRevert = nullptr;
        QTableWidget* m_actorValidation = nullptr;
        QTableWidget* m_actorGovernance = nullptr;
        QTableWidget* m_actorBlockers = nullptr;
        QTableWidget* m_actorRelationships = nullptr;
        QTableWidget* m_actorLanes = nullptr;

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
        QLineEdit* m_troopEvidenceFilter = nullptr;
        QListWidget* m_troopEvidence = nullptr;
        QTableWidget* m_troopEvidenceDetails = nullptr;
        QTableWidget* m_memberTable = nullptr;
        QLineEdit* m_memberLinkId = nullptr;
        QComboBox* m_memberActorRecord = nullptr;
        QLineEdit* m_memberActorSubject = nullptr;
        QComboBox* m_memberRole = nullptr;
        QSpinBox* m_memberMinimumCount = nullptr;
        QSpinBox* m_memberMaximumCount = nullptr;
        QDoubleSpinBox* m_memberWeight = nullptr;
        QCheckBox* m_memberRequired = nullptr;
        QLineEdit* m_memberConditions = nullptr;
        QLineEdit* m_memberEvidenceFilter = nullptr;
        QListWidget* m_memberEvidence = nullptr;
        QPushButton* m_memberApply = nullptr;
        QPushButton* m_memberRemove = nullptr;
        QPushButton* m_memberClear = nullptr;
        QPushButton* m_troopSave = nullptr;
        QPushButton* m_troopRevert = nullptr;
        QTableWidget* m_troopValidation = nullptr;
        QTableWidget* m_troopGovernance = nullptr;
        QTableWidget* m_troopBlockers = nullptr;
        QTableWidget* m_troopRelationships = nullptr;
        QTableWidget* m_troopLanes = nullptr;

        AZStd::vector<PopulationTroopMember> m_stagedMembers;
        AZStd::string m_loadedActorRecordId;
        AZStd::string m_loadedTroopRecordId;
        AZStd::string m_loadedMemberLinkId;
        bool m_refreshing = false;
        bool m_actorDirty = false;
        bool m_troopDirty = false;
        bool m_memberFormDirty = false;
    };
} // namespace TaintedGrailModdingSDK
