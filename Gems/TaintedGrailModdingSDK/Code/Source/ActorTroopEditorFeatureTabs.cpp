/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorTroopEditorWidget.h"

#include <QTabWidget>

namespace TaintedGrailModdingSDK
{
    void ActorTroopEditorWidget::AddFeatureTab(QWidget* widget, const QString& title)
    {
        if (!widget || !m_tabs)
        {
            return;
        }
        widget->setParent(m_tabs);
        m_tabs->addTab(widget, title);
    }
} // namespace TaintedGrailModdingSDK
