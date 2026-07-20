
        m_actorDirty = false;
        SetStatus(
            StatusKind::Success,
            tr("Actor profile persisted and published through the Foundation transaction boundary."));
    }

    void ActorTroopEditorWidget::RevertActorProfile()
    {
        const bool previousRefreshing = m_refreshing;
        m_refreshing = true;
        LoadCurrentActor();
        m_refreshing = previousRefreshing;
        m_actorDirty = false;
        SetStatus(
            StatusKind::Neutral,
            tr("Unsaved actor changes were discarded; persisted state was reloaded."));
    }
} // namespace TaintedGrailModdingSDK
