/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "ActorTroopEditorWidget.h"
#include "FoundationService.h"
#include "NativeItemPreviewService.h"
#include "PopulationPortraitService.h"
#include <AzCore/std/algorithm.h>
#include <QComboBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString Q(const AZStd::string& value) { return QString::fromUtf8(value.c_str()); }
        AZStd::string A(const QString& value)
        {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }
    }

    void ActorTroopEditorWidget::ReadGameDefinitions()
    {
        if (m_nativeReader->IsRunning()) { m_nativeReader->Cancel(); return; }
        if (HasDirtyDrafts()) { SetStatus(tr("Save or revert your drafts before loading game actors."), true); return; }
        const QString workspace = Q(FoundationService::Get().GetWorkspaceFilePath());
        m_readGame->setText(tr("Cancel loading"));
        SetStatus(tr("Reading game actor templates..."));
        m_nativeReader->StartPopulation(workspace, [this](const auto& progress) { SetStatus(progress); },
            [this, workspace](const QString& path, const QString& failure)
            {
                m_readGame->setText(tr("Load game actors"));
                if (!failure.isEmpty())
                {
                    const bool cancelled = failure.startsWith("Item preview refresh cancelled.");
                    SetStatus(cancelled ? tr("Loading cancelled. Saved actors and troops are unchanged.") : failure, !cancelled);
                    return;
                }
                auto& foundation = FoundationService::Get();
                if (workspace != Q(foundation.GetWorkspaceFilePath()) || HasDirtyDrafts())
                {
                    SetStatus(tr("The workspace or a draft changed while loading. Save or revert and load actors again."), true);
                    return;
                }
                AZStd::string error;
                if (!foundation.ImportNativePopulation(A(path), &error)) { SetStatus(Q(error), true); return; }
                if (m_actorRecord->currentIndex() == 0 && m_actorRecord->count() > 1) { m_actorRecord->setCurrentIndex(1); }
                SetStatus(tr("Supported game actors loaded. Existing local changes were preserved. Unsupported entries are recorded in the reader report."));
            });
    }

    void ActorTroopEditorWidget::CreateRecord(bool troop)
    {
        if (HasDirtyDrafts() || m_nativeReader->IsRunning())
        {
            SetStatus(tr("Save or revert drafts and finish loading before creating a definition."), true); return;
        }
        auto& foundation = FoundationService::Get();
        if (!foundation.GetActivePack() || foundation.GetActivePackFilePath().empty())
        {
            SetStatus(tr("Choose or create a mod, then save it before creating actors or troops."), true); return;
        }
        AZStd::string leader;
        bool accepted = false;
        if (troop)
        {
            QStringList labels;
            AZStd::vector<AZStd::string> ids;
            int selected = 0;
            for (const auto& actor : foundation.GetCatalog().GetPopulationActorProfiles())
            {
                const auto* record = foundation.GetCatalog().FindByRecordId(actor.m_recordId);
                if (!record) { continue; }
                if (actor.m_recordId == m_loadedActorRecordId) { selected = labels.size(); }
                labels.append(Q(record->m_displayName) + " [" + Q(record->m_recordId) + "]");
                ids.push_back(actor.m_recordId);
            }
            if (labels.isEmpty()) { SetStatus(tr("Load or create an actor before creating a troop."), true); return; }
            const auto label = QInputDialog::getItem(this, tr("New troop"), tr("Choose the first leader"), labels, selected, false, &accepted);
            if (!accepted) { return; }
            const auto index = labels.indexOf(label);
            if (index < 0) { return; }
            leader = ids[static_cast<size_t>(index)];
        }
        const auto name = QInputDialog::getText(this, troop ? tr("New troop") : tr("New actor"),
            tr("Name"), QLineEdit::Normal, {}, &accepted).trimmed();
        if (!accepted || name.isEmpty()) { return; }
        AZStd::string id, error;
        if (!foundation.CreatePopulationRecord(troop ? "troop" : "actor", A(name), leader, id, &error))
        {
            SetStatus(Q(error), true); return;
        }
        auto* filter = troop ? m_troopFilter : m_actorFilter;
        { const QSignalBlocker blocker(filter); filter->clear(); }
        RefreshAll();
        auto* combo = troop ? m_troopRecord : m_actorRecord;
        combo->setCurrentIndex(combo->findData(Q(id)));
        m_tabs->setCurrentIndex(troop ? 1 : 0);
        SetStatus(tr("Created %1 in the active mod. Edit its fields and save your changes.").arg(name));
    }

    void ActorTroopEditorWidget::RemoveSelectedMember()
    {
        if (m_memberEditorDirty) { SetStatus(tr("Stage or clear the member form before removing a member."), true); return; }
        const auto found = AZStd::find_if(m_draftMembers.begin(), m_draftMembers.end(),
            [this](const auto& member) { return member.m_linkId == m_selectedMemberLinkId; });
        if (found == m_draftMembers.end()) { SetStatus(tr("Select a troop member to remove."), true); return; }
        for (const auto& saved : FoundationService::Get().GetCatalog().GetPopulationTroopMembers())
        {
            if (saved.m_linkId == found->m_linkId && saved.m_troopRecordId == m_loadedTroopRecordId)
            {
                m_removedMemberIds.push_back(saved.m_linkId); break;
            }
        }
        m_draftMembers.erase(found);
        ClearMemberEditor();
        RefreshMemberTable();
        m_troopDirty = true;
        SetTroopState(tr("Member removal staged. Save the troop to apply it, or revert to restore it."));
        SetStatus(tr("Before saving, the remaining members must include the selected leader and satisfy the troop size range."));
    }

    void ActorTroopEditorWidget::ChoosePortrait()
    {
        const auto root = Q(FoundationService::Get().GetWorkspaceRootPath());
        const auto file = QFileDialog::getOpenFileName(this, tr("Choose a portrait inside the authoring workspace"),
            root, tr("Portrait images (*.png *.jpg *.jpeg)"));
        if (file.isEmpty()) { return; }
        auto reference = PopulationPortraitService::ReferenceForFile(root, file);
        if (!reference.IsSuccess()) { SetStatus(reference.GetError(), true); return; }
        auto image = PopulationPortraitService::Read(root, reference.GetValue());
        if (!image.IsSuccess()) { SetStatus(image.GetError(), true); return; }
        m_actorPortraitRef->setText(reference.GetValue());
        MarkActorDirty();
        RefreshPortrait();
        SetStatus(tr("Portrait selected. Save the actor to keep this reference."));
    }

    void ActorTroopEditorWidget::RefreshPortrait()
    {
        m_portrait->clear();
        if (m_actorRecord->currentData().toString().isEmpty())
        {
            m_portraitState->setText(tr("Select an actor to preview its assigned portrait.")); return;
        }
        auto& foundation = FoundationService::Get();
        const auto* pack = foundation.GetActivePack();
        const auto* binding = pack ? foundation.GetCatalog().FindPresentationBinding(
            pack->m_packId, m_loadedActorRecordId, "portrait") : nullptr;
        if (binding && !binding->m_valueRecordId.empty())
        {
            QImage managed; AZStd::string error;
            if (!foundation.ReadProjectAssetImage(binding->m_valueRecordId, managed, &error)) { m_portraitState->setText(Q(error)); return; }
            m_portrait->setPixmap(QPixmap::fromImage(managed).scaled(m_portrait->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_portraitState->setText(tr("Active mod portrait from Assets and text. Change this assignment in the Manager.")); return;
        }
        auto image = PopulationPortraitService::Read(Q(foundation.GetWorkspaceRootPath()), m_actorPortraitRef->text());
        if (!image.IsSuccess()) { m_portraitState->setText(image.GetError()); return; }
        m_portrait->setPixmap(QPixmap::fromImage(image.GetValue()).scaled(m_portrait->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_portraitState->setText(tr("Local authoring portrait: %1").arg(m_actorPortraitRef->text()));
    }
}
