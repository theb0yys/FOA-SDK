# Actor and Troop draft recovery

## Owner and compatibility

This Significant addition follows the existing Pack and Item/Recipe private
recovery pattern. schemas-and-persistence owns ActorTroopDraftRecoveryService;
ui-framework captures/restores raw authoring state. Foundation remains the only
publisher of canonical population data through existing Save commands. Restore
changes forms only. No public API, catalog, pack, workspace or game-save schema changes.

Schema 1 is new. Older Editors ignore its separate folder/extension. Unknown
formats/versions, malformed/incomplete values, missing definitions/choices and
foreign workspace bindings are rejected with diagnostics and kept until explicit
Discard. There is no automatic migration or relocation. Future shape changes
require a format bump or an explicit tested compatibility reader.

## Private format and bounds

One .actortroopdrafts.json copy lives under local application-data
FOA-SDK/Recovery/ActorTroopDrafts. Its SHA-256 filename binds workspace ID,
canonical root and canonical document path; Windows paths are case-folded. The
same identity is checked inside the file. Linked recovery paths are rejected.
The service reads/writes only this file and its ownership lock, without scanning
or cleaning unrelated directories.

Format FOA-SDK.ActorTroopDraftRecovery has SchemaVersion 1, WorkspaceId,
WorkspaceRoot, WorkspaceDocument, Values, Members, RemovedMembers, Actor, Troop,
Member, ActorDirty, TroopDirty, MemberDirty, RefreshPending and Tab. Values uses
stable named fields with explicit string/integer/boolean/selection/string-list
tags. Selection data remains separate from labels and retained record choices.
Members contains staged population member values, including immutable LinkId and
TroopRecordId; RemovedMembers retains staged deletions. Raw invalid member text
is stored in Values and never normalized through a population model.

Limits: 64 fields, 16,384 UTF-16 characters per string, 512 per stable ID, 4,096
member/selection/list entries and 2 MiB per complete document. Invalid/oversized
reads or writes keep the previous copy. UI compatibility also checks exact fields,
types, bounds and referenced definitions before allowing Restore.

## Checkpoints and lifecycle

Edits arm one 750 ms timer without delaying indefinitely during continuous
typing. A serial worker owns storage; at most one background write is outstanding.
QSaveFile replaces atomically with direct-write fallback disabled. QLockFile
excludes a live second Editor and permits a fresh process after the owner dies.

Recovery offers are inline Restore drafts / Discard recovery copy controls.
Unresolved offers block editing and survive closing or switching away. Errors
explain why the copy was kept and offer Retry. Successful Save checkpoints only
remaining dirty state; clean state retires the copy. Pane Discard retires it only
on accepted close, and workspace Discard only after the replacement commits.
Whole-Editor exit retains the last checkpoint and lock throughout later pane/file
prompts; the synchronous close guard owns the drained store until completion.
Cancellation restores the forms and resumes checkpointing after comparing the
reloaded copy with the checkpoint retained by that close attempt. Layout or focus
events during the asynchronous read do not turn the retained copy into a foreign
recovery offer. A different copy still blocks overwriting. Failed final cleanup
keeps the copy and records the failure rather than claiming it was retired.

Only completed checkpoints survive forced termination. Edits since the last
checkpoint can be lost. This is not a catalog backup or a power-loss guarantee.
Restored drafts require review before Save if the saved catalog changed.

## Required validation

Compiled storage tests cover typed round trips, deterministic bytes, workspace
binding, unknown/future/corrupt data, limits, read-before-write admission,
concurrent-owner exclusion, file-lock failure and retry. Native Editor phases
must confirm a matching durable checkpoint before terminating only the owned
fixture process, then reopen and restore without catalog writes. Cover repeated
termination, member staging/removal, partial saves, missing definitions, damaged
copies, workspace isolation, later-pane exit veto and recovery retirement.
Every normal phase requires Editor initialization, expected module hash,
about-to-quit and normal exit zero. Checkpoint UI event gaps have a 500 ms budget;
synthetic interactions have a five-second budget. Run existing pane-close,
whole-Editor exit and both workspace-picker suites as regressions.

Run the recovery suites against the pinned built Editor and a prepared asset cache:

```powershell
./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_actor_troop_draft_recovery_smoke.ps1 `
  -EditorExecutable "$BuildRoot/bin/profile/Editor.exe" `
  -EngineRoot $EngineRoot -CacheRoot $CacheRoot -OutputRoot $FreshExternalOutput
```

The runner uses an inactive private desktop and 13 process phases. Its forced
termination paths require the matching durable checkpoint and the exact owned
Editor executable. Save, Discard, failure/retry and pending-exit suites can also
be selected separately with `-Suite`. Generated fixtures and evidence remain
outside both source trees.
