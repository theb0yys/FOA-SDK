# Current Task

Status: implementation in progress; current-task validation NOT_RUN.
Goal: protect unsaved Actor/Troop Editor drafts when switching workspaces.
Classification: Routine UI integration within the accepted workspace admission
and commit contract. Primary owner: ui-framework; Foundation retains candidate
validation, workspace replacement and population persistence ownership.

Scope: Save / Discard / Cancel for actor, troop and member drafts through both
workspace pickers; save to the current workspace; reset drafts only after a
successful replacement, including same-root reload; preserve drafts on Cancel,
failed Save, another handler veto or failed post-admission load. Reuse pane-close
save logic and test both docked and floating panes with synthetic workspaces.
No public API, schema, runtime, deployment, game or installation changes.
Whole-Editor exit and Actor/Troop crash recovery are separate tasks.

Branch: codex/actor-troop-workspace-protection, based on PR #273 head
3f2fda6b4f0da19125f4736d22c00243a0461e9b, integrating main at
8fa4601abc3326e7166c59a47948003ec1462acb. Only CURRENT_TASK.md conflicted;
this owner request replaces the completed task record. Original dirty SDK-client
checkout is preserved. Existing prerequisite PRs remain maintainer decisions.

Acceptance: focused static and validator tests, exact-pin Windows Profile build,
compiled catalog/workspace tests, real Editor workspace admission and existing
pane-close regression. Verify bytes, retained fields, reset state, loaded DLL
hash, native process exit and bounded synthetic interaction time. Generated data
and evidence remain outside source. Next action: implement the thin widget
handler and native workspace regression, then run the applicable validation.
