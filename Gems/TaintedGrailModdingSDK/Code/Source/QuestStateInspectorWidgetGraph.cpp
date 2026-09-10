/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "QuestStateInspectorWidget.h"
#include "QuestAuthoringService.h"
#include <AzCore/std/sort.h>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QLabel>
#include <QMap>
#include <QPen>
#include <cmath>
namespace TaintedGrailModdingSDK
{
    void QuestStateInspectorWidget::DrawGraph()
    {
        m_scene->clear(); const auto& d = m_draft.m_definition;
        m_graphSummary->setText(tr("%1 phases | %2 transitions - authored progression; no quest execution")
            .arg(static_cast<qulonglong>(d.m_phases.size())).arg(static_cast<qulonglong>(d.m_transitions.size())));
        if (d.m_phases.empty()) { m_graphSummary->setText(tr("Create or load a quest to preview its phases.")); return; }
        if (d.m_phases.size() > 256 || d.m_transitions.size() > 256) { m_graphSummary->setText(tr("Graph exceeds the supported 256 phase/transition limit.")); return; }
        QMap<QString, QPointF> positions; QMap<QString, QString> names;
        const auto label = [this](const AZStd::string& id) { return QString::fromUtf8(QuestAuthoringService::Label(m_draft, id).c_str()); };
        // Entry phases lead the stable layout; ordering does not simulate progression.
        auto ordered = d.m_phases;
        AZStd::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b)
        {
            const int left = a.m_entryPhase ? 0 : a.m_terminalPhase ? 2 : 1;
            const int right = b.m_entryPhase ? 0 : b.m_terminalPhase ? 2 : 1;
            return left != right ? left < right : a.m_phaseId < b.m_phaseId;
        });
        const int columns = d.m_phases.size() <= 4 ? static_cast<int>(d.m_phases.size()) : 4;
        for (size_t i = 0; i < d.m_phases.size(); ++i)
        {
            const auto& p = ordered[i]; const QString id = QString::fromUtf8(p.m_phaseId.c_str());
            positions[id] = QPointF((static_cast<int>(i) % columns) * 380, (static_cast<int>(i) / columns) * 280);
            names[id] = label(p.m_phaseId);
        }
        int edgeIndex = 0;
        for (const auto& t : d.m_transitions)
        {
            const auto from = QString::fromUtf8(t.m_fromPhaseId.c_str()), to = QString::fromUtf8(t.m_toPhaseId.c_str());
            if (!positions.contains(from) || !positions.contains(to)) { continue; }
            const QPointF a = positions[from] + QPointF(130, 100), b = positions[to] + QPointF(130, 100);
            const bool loop = from == to; const QPointF mid = (a + b) / 2 + QPointF(0, 80 + (edgeIndex++ % 3) * 25);
            const QColor color = t.m_repeatAllowed ? QColor("#ffcc80") : QColor("#81d4fa");
            auto* first = m_scene->addLine(QLineF(a, mid), QPen(color, 2)); first->setData(0, "quest-transition"); first->setData(1, QString::fromUtf8(t.m_transitionId.c_str()));
            const QPointF end = loop ? b + QPointF(70, 0) : b;
            m_scene->addLine(QLineF(mid, end), QPen(color, 2));
            const auto direction = end - mid; const double angle = std::atan2(direction.y(), direction.x());
            const QPointF tip = end - QPointF(std::cos(angle) * 52, std::sin(angle) * 52);
            m_scene->addPolygon(QPolygonF{tip, tip - QPointF(std::cos(angle - .5) * 15, std::sin(angle - .5) * 15),
                tip - QPointF(std::cos(angle + .5) * 15, std::sin(angle + .5) * 15)}, QPen(color), QBrush(color));
            auto* text = m_scene->addText(label(t.m_transitionId) + (t.m_repeatAllowed ? tr(" (repeat)") : QString{}));
            text->setDefaultTextColor(color); text->setTextWidth(230); text->setPos(mid + QPointF(-100, 6)); text->setToolTip(QString::fromUtf8(t.m_triggerId.c_str()));
        }
        for (const auto& p : d.m_phases)
        {
            const auto id = QString::fromUtf8(p.m_phaseId.c_str()); const auto pos = positions[id];
            const QColor fill = p.m_entryPhase ? QColor("#235f55") : p.m_terminalPhase ? QColor("#5f4a78") : QColor("#30475e");
            auto* box = m_scene->addRect(QRectF(pos, QSizeF(260, 100)), QPen(QColor("#b0bec5"), 2), QBrush(fill));
            box->setData(0, "quest-phase"); box->setData(1, id); box->setToolTip(id);
            auto* text = m_scene->addText(names[id] + "\n" + (p.m_entryPhase ? tr("Entry") : p.m_terminalPhase ? tr("Terminal") : tr("Phase"))
                + tr(" | %1 objectives").arg(static_cast<qulonglong>(p.m_objectiveIds.size())));
            text->setDefaultTextColor(Qt::white); text->setTextWidth(235); text->setPos(pos + QPointF(10, 10)); text->setToolTip(id);
        }
        m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-30,-30,30,30));
        m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    }
}
