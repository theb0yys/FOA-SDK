/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "FoundationNotificationBus.h"

#include <QObject>
#include <QPointer>
#include <QSignalBlocker>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTableWidget;
class QWidget;

namespace TaintedGrailModdingSDK
{
    //! Adds the researched visual browsing lifecycle to the existing validated selector.
    //! The original selector remains responsible for validation, preview and assignment.
    class ItemVisualLifecycleEnhancer final
        : public QObject
        , private FoundationNotificationBus::Handler
    {
    public:
        explicit ItemVisualLifecycleEnhancer(QWidget* selector);
        ~ItemVisualLifecycleEnhancer() override;

    private:
        void OnFoundationChanged() override;
        void InstallGrid();
        void SimplifyModelControls();
        void ScheduleAutomaticModelLoad();
        void RefreshAssets();
        void LoadLatestAvailableModel();
        QString FindLatestModelPath() const;
        QString ResolveExtractedDataRoot() const;
        QString CurrentProfileBindingKey() const;
        bool CandidateMatchesActiveProfile(const QString& path, QString* capturedAt = nullptr) const;
        void ScheduleRebuild();
        void RebuildGrid();
        void ApplyFilter();
        void SelectGridItem();
        void SelectTableRow();
        void SaveState();
        void RestoreState();
        QString SettingsPrefix() const;

        QPointer<QWidget> m_selector;
        QPointer<QTableWidget> m_table;
        QPointer<QListWidget> m_grid;
        QPointer<QLineEdit> m_search;
        QPointer<QLineEdit> m_modelPath;
        QPointer<QComboBox> m_target;
        QPointer<QPushButton> m_chooseModel;
        QPointer<QPushButton> m_reload;
        QPointer<QPushButton> m_refreshAssets;
        QPointer<QSlider> m_sizeSlider;
        QPointer<QLabel> m_status;
        QString m_pendingAssetId;
        QString m_profileBindingKey;
        bool m_syncing = false;
        bool m_rebuildPending = false;
        bool m_restoring = false;
        bool m_autoLoadPending = false;
    };
}