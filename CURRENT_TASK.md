# Current Task

Status: implemented; required local validation PASSED. PR handoff follows.
Goal: protect unsaved Spawn & Encounter Editor drafts when switching workspaces.
Classification: Routine. Primary owner: ui-framework; existing foundation-services,
workspace-and-packs and content-pack-authoring retain admission, publication and
complete encounter-save ownership.

Join existing CanChangeWorkspace / OnWorkspaceChanged notifications. Save writes
only to the original workspace before replacement. Cancel, Escape, dismissal and
failed Save preserve the workspace and raw fields/composition staging. Discard
retires the draft only after commit; later veto or candidate failure retains it.
Bind saving to the canonical workspace document and root as well as IDs. Successful
switches and same-workspace reloads reset old forms without leaking matching IDs.

Branch: codex/spawn-encounter-workspace-protection, based on PR #280 at
3ce5f645f3757e838247f45795483aa537e7e7e0. Verified main remains
8fa4601abc3326e7166c59a47948003ec1462acb and is already integrated.
The previous unmerged feature stack remains inherited. No writes use the original
dirty SDK-client checkout.

Smallest scope: SpawnEncounterEditorWidget header/implementation; dedicated native
workspace fixture and existing encounter runner; encounter guide/design; workspace
atomicity documentation and this record. No public contract/schema, build graph,
engine, whole-Editor exit coordination, crash recovery, deployment or runtime change.

Executed proof: static/source policy; existing exact-pin Windows Profile build and
compiled workspace/encounter regressions; both Status and Catalog Browser picker
routes with docked/floating panes, real write-lock failure and retry, same-root
reload, alias/ID reuse, later handler veto and post-admission failure; close regression.
Synthetic fixtures and proof stay outside both source checkouts. Measure bounded
native transitions and require normal Editor exit plus the built SDK module hash.

Validation, September 13, 2026:
- PASSED: pinned Windows Profile Editor/Catalog build; unchanged graph, inspected
  exact-pin cache reused. SDK DLL SHA-256:
  af80c45f05e7167f15d2119e387a6e3853deb94e53dcb6096f576af9c7d76916.
- PASSED: Catalog CTest, 541 of 543 discovered; all 23 workspace-load tests and six
  encounter regressions passed. Two existing Windows symlink-privilege tests skipped.
- PASSED: native workspace-status 47 checks and workspace-catalog 47 checks, each
  covering docked/floating panes. Normal exit 0, aboutToQuit and matching module hash
  observed, no forced stop. Maximum transitions 0.984 s and 0.985 s; total fixture
  durations 82.269 s and 76.602 s. Workspace prompt image visually checked.
- PASSED: independent native close regression, 28 checks, normal exit 0 and no
  forced stop; maximum close 0.406 s and total fixture duration 37.590 s.
- PASSED: full static lane (918 Python plus ten ExternalToolchain tests, 33
  conditional/platform skips), ten SDK source-policy validators, Python/PowerShell
  fixture syntax, focused scope and reviewed-range whitespace.
- FAILED initial fixture runs: snapshots included retired Qt table editors awaiting
  deferred deletion. Corrected the harness to capture current cells and their raw
  quantity text; both final workspace routes passed. No product fix was needed.
- NOT_APPLICABLE: schema migration, new configure, runtime/deployment/installer or
  release proof for this increment. No protected game data was read or written.

Evidence logs, JSON, screenshots and synthetic files remain outside both source
checkouts. The normal handoff is a signed commit and new PR to main. Hosted CI is
reported separately; maintainer approval and merge remain separate. No further
feature is started.
