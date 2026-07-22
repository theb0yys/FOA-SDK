/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;

namespace AvalonAIAuthoring
{
    class AvalonAIEditorWidget final
        : public QWidget
    {
    public:
        explicit AvalonAIEditorWidget(QWidget* parent = nullptr);

    private:
        void CreateDocument();
        void LoadDocument();
        void ValidateDocument();
        void SaveDocument();
        void RevertDocument();
        void SetStatus(const QString& text, bool error);

        QTextEdit* m_document = nullptr;
        QLabel* m_status = nullptr;
        QPushButton* m_save = nullptr;
        QByteArray m_lastLoaded;
        bool m_valid = false;
    };
} // namespace AvalonAIAuthoring
