/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorldRouteEditorWidget.h"
#include "FoundationService.h"
#include "WorldPlanningService.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include <QPolygonF>
#include <cmath>
namespace TaintedGrailModdingSDK
{
    void WorldRouteEditorWidget::Preview()
    {
        const auto q = [](const AZStd::string& value) { return QString::fromUtf8(value.c_str()); };
        m_scene->clear(); const auto& catalog = FoundationService::Get().GetCatalog();
        QStringList notes; const auto text = [this](const QString& value, const QPointF& point, const QColor& color)
        {
            auto* item = m_scene->addText(value); item->setDefaultTextColor(color); item->setPos(point); return item;
        };
        if (m_id.empty())
        { notes << tr("Create a region, then a scene and locations to start a world plan."); }
        else if (m_isPlace)
        {
            const auto valid = WorldPlanningService::ValidatePlace(m_place, m_kind, catalog);
            if (!valid.IsSuccess()) { notes << q(valid.GetError()); }
            const auto* record = catalog.FindByRecordId(m_id); const auto* parent = catalog.FindByRecordId(m_place.m_parentRecordId);
            notes << tr("%1 | %2").arg(q(m_kind), m_place.m_hasPosition
                ? tr("Plan position X %1, Z %2").arg(m_place.m_x).arg(m_place.m_z) : tr("No plan position"));
            if (parent)
            {
                text(q(parent->m_displayName).left(80), QPointF(0, -100), QColor("#d0dfeb"));
                m_scene->addLine(70, -60, 70, -15, QPen(QColor("#64c5b5"), 3));
            }
            m_scene->addEllipse(58, -12, 24, 24, QPen(QColor("#64c5b5"), 2), QBrush(QColor("#284e54")));
            text(record ? q(record->m_displayName).left(80) : q(m_id), QPointF(0, 18), QColor("#ffffff"));
            notes << tr("Authored place hierarchy");
        }
        else
        {
            const auto analysis = WorldPlanningService::Analyze(m_path, m_kind, catalog);
            notes << tr("%1 nodes | %2 connections | %3").arg(m_path.m_nodes.size()).arg(m_path.m_edges.size())
                .arg(analysis.m_usesPlanPositions ? tr("Scene-local plan positions") : tr("Schematic topology"));
            int shown = 0;
            for (const auto& error : analysis.m_errors) { if (++shown <= 5) { notes << q(error); } }
            for (const auto& warning : analysis.m_warnings) { if (++shown <= 5) { notes << q(warning); } }
            if (shown > 5) { notes << tr("%1 more review notes").arg(shown - 5); }
            QMap<QString, QPointF> points;
            double minimumX = 1000000, maximumX = -1000000, minimumZ = 1000000, maximumZ = -1000000;
            if (analysis.m_usesPlanPositions)
            {
                for (const auto& node : m_path.m_nodes)
                {
                    const auto* place = catalog.FindWorldPlace(node.m_locationRecordId);
                    if (!place) { continue; }
                    minimumX = qMin(minimumX, place->m_x); maximumX = qMax(maximumX, place->m_x);
                    minimumZ = qMin(minimumZ, place->m_z); maximumZ = qMax(maximumZ, place->m_z);
                }
            }
            const double scale = 600.0 / qMax(1.0, qMax(maximumX - minimumX, maximumZ - minimumZ));
            for (size_t index = 0; index < m_path.m_nodes.size() && index < 128; ++index)
            {
                const auto& node = m_path.m_nodes[index];
                const auto* place = catalog.FindWorldPlace(node.m_locationRecordId);
                if (analysis.m_usesPlanPositions && place)
                { points[q(node.m_nodeId)] = QPointF((place->m_x - minimumX) * scale, -(place->m_z - minimumZ) * scale); }
                else
                {
                    const double angle = 6.283185307179586 * static_cast<double>(index) / qMax<size_t>(1, m_path.m_nodes.size());
                    points[q(node.m_nodeId)] = QPointF(std::cos(angle) * 280, std::sin(angle) * 220);
                }
            }
            const QColor edgeColor("#7cc8ec");
            const auto arrow = [this, &edgeColor](const QPointF& from, const QPointF& to)
            {
                QLineF line(from, to); const double length = line.length(); if (length < 30) { return; }
                const QPointF direction = (to - from) / length, tip = to - direction * 15;
                const QPointF normal(-direction.y(), direction.x());
                QPolygonF triangle; triangle << tip << tip - direction * 12 + normal * 5 << tip - direction * 12 - normal * 5;
                m_scene->addPolygon(triangle, QPen(edgeColor), QBrush(edgeColor));
            };
            size_t rendered = 0;
            for (const auto& edge : m_path.m_edges)
            {
                if (++rendered > 256) { break; }
                if (!points.contains(q(edge.m_fromNodeId)) || !points.contains(q(edge.m_toNodeId))) { continue; }
                const auto from = points.value(q(edge.m_fromNodeId)), to = points.value(q(edge.m_toNodeId));
                auto* line = m_scene->addLine(QLineF(from, to), QPen(edgeColor, 3));
                line->setData(0, "world-edge"); line->setData(1, q(edge.m_edgeId));
                line->setToolTip(tr("%1 | cost %2\n%3").arg(q(edge.m_travelMode)).arg(edge.m_travelCost).arg(q(edge.m_notes)));
                arrow(from, to); if (edge.m_bidirectional) { arrow(to, from); }
                text(tr("%1 / %2").arg(q(edge.m_travelMode)).arg(edge.m_travelCost), (from + to) / 2.0 + QPointF(4, 4), edgeColor);
            }
            for (const auto& node : m_path.m_nodes)
            {
                if (!points.contains(q(node.m_nodeId))) { continue; }
                const auto point = points.value(q(node.m_nodeId)); const auto* record = catalog.FindByRecordId(node.m_locationRecordId);
                auto* item = m_scene->addEllipse(point.x() - 12, point.y() - 12, 24, 24, QPen(QColor("#b3f0d9"), 2), QBrush(QColor("#316657")));
                item->setData(0, "world-node"); item->setData(1, q(node.m_nodeId));
                item->setToolTip((record ? q(record->m_displayName) : q(node.m_locationRecordId)) + "\n" + q(node.m_notes));
                text(record ? q(record->m_displayName).left(60) : tr("Missing location"), point + QPointF(16, -12), QColor("#ffffff"));
            }
        }
        if (m_scene->items().isEmpty()) { text(tr("Add saved locations as nodes to preview a road or route."), QPointF(0, 0), QColor("#d0dfeb")); }
        m_preview->setText(notes.join('\n'));
        m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-35, -35, 35, 35));
        m_graph->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        m_save->setEnabled(m_dirty && SameWorkspace());
    }
}
