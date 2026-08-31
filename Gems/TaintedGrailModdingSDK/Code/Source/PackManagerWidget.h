/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"
#include "FoundationNotificationBus.h"

#include <QWidget>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace TaintedGrailModdingSDK
{
    class PackManagerWidget final
        : public QWidget
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit PackManagerWidget(QWidget* parent = nullptr);
        ~PackManagerWidget() override;

    private:
        void OnFoundationChanged() override;

        PackManifest BuildPackFromForm() const;
        void PopulateFromPack(const PackManifest& pack);
        void ClearFormForNewPack();
        void UpdateGeneratedIdentity();
        void UpdateSummary();
        void SetStatus(const QString& message, bool error = false);
        bool ApplyPack();
        bool SavePack();
        QString CanonicalPackFilePath(const PackManifest& pack) const;
        bool IsInsideWorkspace(const QString& filePath) const;

        QLineEdit* m_packIdEdit = nullptr;
        QLineEdit* m_displayNameEdit = nullptr;
        QLineEdit* m_ownerIdEdit = nullptr;
        QLineEdit* m_versionEdit = nullptr;
        QLineEdit* m_targetGameVersionEdit = nullptr;
        QLineEdit* m_targetBranchEdit = nullptr;
        QPlainTextEdit* m_compatibleGameVersionsEdit = nullptr;
        QLineEdit* m_coreVersionEdit = nullptr;
        QLineEdit* m_adapterVersionEdit = nullptr;
        QPlainTextEdit* m_dlcScopesEdit = nullptr;
        QPlainTextEdit* m_dependenciesEdit = nullptr;
        QPlainTextEdit* m_requiredModsEdit = nullptr;
        QPlainTextEdit* m_incompatibilitiesEdit = nullptr;
        QComboBox* m_saveImpactCombo = nullptr;
        QPlainTextEdit* m_contentDefinitionsEdit = nullptr;
        QPlainTextEdit* m_assetPathsEdit = nullptr;
        QPlainTextEdit* m_localisationPathsEdit = nullptr;
        QLineEdit* m_buildConfigurationEdit = nullptr;
        QComboBox* m_releaseChannelCombo = nullptr;
        QLabel* m_activePackValue = nullptr;
        QLabel* m_manifestPathValue = nullptr;
        QLabel* m_statusLabel = nullptr;
        QGroupBox* m_advancedGroup = nullptr;
        QPushButton* m_advancedToggleButton = nullptr;
        bool m_isNewPack = true;
    };
} // namespace TaintedGrailModdingSDK
