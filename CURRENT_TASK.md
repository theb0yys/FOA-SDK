# Open PR integration

Status: integrating the owner's requested open PRs and resolving conflicts.

Scope: PRs #252–#258, retaining Pack Manager draft/exit protection and the encounter, faction, world and quest authoring stack. The owner explicitly authorized merging all open PRs and fixing conflicts. No release, deployment, game/save changes, branch deletion or history rewrite is included.

Classification: Significant integration of existing authoring/persistence surfaces. The focused CI checkout correction is Routine; primary owner validation-and-evidence. Supporting owners: workspace-and-packs, content-pack-authoring, schemas-and-persistence and ui-framework.

The shared CURRENT_TASK conflict is resolved with this integration record. Foundation's pack-save and authoring declarations are combined without changing either command contract. Item Viewer CI now hydrates the pinned O3DE Git LFS assets so the Windows resource compiler receives real icon data; negative coverage prevents disabling or omitting engine LFS checkout.

Validation: original PR evidence remains attached to each PR. Required integration checks include focused CI regression tests, repository/static/source-policy checks, and combined SDK build/compiled checks where affected. Hosted pending, skipped and failed runs retain their actual state. Runtime sign-off not performed.

Branch: codex/pr-conflicts-252-20260910 in a dedicated integration worktree. Other active worktrees and their drafts remain untouched.
