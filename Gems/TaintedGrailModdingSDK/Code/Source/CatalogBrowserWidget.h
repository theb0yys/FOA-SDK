/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "FoundationNotificationBus.h"

#include <QFont>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace TaintedGrailModdingSDK
{
    class CatalogBrowserWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit CatalogBrowserWidget(QWidget* parent = nullptr);
        ~CatalogBrowserWidget() override;

    private:
        void OnFoundationChanged() override;
        void ReloadForWorkspaceIfNeeded();
        void RefreshChoices();
        void RunSearch();
        void InspectCurrentRow();
        void InspectRecord(const CatalogRecord* record);
        void PromoteEvidence();
        CatalogQuery BuildQuery() const;
        CatalogPromotionRequest BuildPromotionRequest() const;
        void SetStatus(const QString& message, bool error = false);

        QLineEdit* m_searchEdit = nullptr;
        QLineEdit* m_recordIdFilter = nullptr;
        QLineEdit* m_exactRefFilter = nullptr;
        QLineEdit* m_subjectFilter = nullptr;
        QLineEdit* m_evidenceFilter = nullptr;
        QComboBox* m_packFilter = nullptr;
        QComboBox* m_domainFilter = nullptr;
        QComboBox* m_kindFilter = nullptr;
        QComboBox* m_identityFilter = nullptr;
        QComboBox* m_stageFilter = nullptr;
        QComboBox* m_confidenceFilter = nullptr;
        QComboBox* m_riskFilter = nullptr;
        QComboBox* m_validationFilter = nullptr;
        QComboBox* m_stalenessFilter = nullptr;
        QLineEdit* m_permissionFilter = nullptr;
        QCheckBox* m_blockedOnly = nullptr;
        QCheckBox* m_includeSuperseded = nullptr;
        QTableWidget* m_resultsTable = nullptr;

        QLabel* m_catalogPathValue = nullptr;
        QLabel* m_recordIdentityValue = nullptr;
        QLabel* m_recordOwnerValue = nullptr;
        QLabel* m_recordSubjectValue = nullptr;
        QLabel* m_recordNativeRefValue = nullptr;
        QLabel* m_recordStateValue = nullptr;
        QPlainTextEdit* m_recordAliasesView = nullptr;
        QPlainTextEdit* m_recordEvidenceView = nullptr;
        QPlainTextEdit* m_recordRelationshipsView = nullptr;
        QPlainTextEdit* m_recordValidationView = nullptr;
        QPlainTextEdit* m_recordGovernanceView = nullptr;
        QPlainTextEdit* m_recordBlockersView = nullptr;

        QComboBox* m_promotionEvidence = nullptr;
        QLineEdit* m_promotionRecordId = nullptr;
        QComboBox* m_promotionOwnerPack = nullptr;
        QLineEdit* m_promotionDomain = nullptr;
        QLineEdit* m_promotionKind = nullptr;
        QLineEdit* m_promotionSubject = nullptr;
        QLineEdit* m_promotionNativeRef = nullptr;
        QComboBox* m_promotionIdentityKind = nullptr;
        QLineEdit* m_promotionDisplayName = nullptr;
        QLineEdit* m_promotionAliases = nullptr;
        QComboBox* m_promotionStage = nullptr;
        QComboBox* m_promotionConfidence = nullptr;
        QComboBox* m_promotionRisk = nullptr;
        QLineEdit* m_promotionForbidden = nullptr;
        QLineEdit* m_promotionTags = nullptr;

        QLabel* m_statusLabel = nullptr;
        AZStd::vector<CatalogRecord> m_results;
        AZStd::string m_lastWorkspaceRoot;
        AZStd::string m_lastProfileId;
        bool m_reloading = false;
    };
} // namespace TaintedGrailModdingSDK
