# Pack Manager draft recovery

## Ownership and boundary

Foundation's workspace-and-packs system owns `PackDraftRecoveryService`; Pack Manager
captures and presents form state. Recovery is separate from validated pack manifests,
workspace documents, active-pack selection, packaging and game runtime. Restore changes
only the form. The ordinary Save mod command remains the only route to a saved mod.

This is a Significant addition of a separate persistence format. Existing pack and
workspace schemas, ExtensionAPI, dependencies and the O3DE pin are unchanged. Earlier
Editors ignore recovery copies. No migration or downgrade of saved mods is required.

## Store and format

On Windows, the store is under `%LOCALAPPDATA%/FOA-SDK/Recovery/PackDrafts`; other hosts
use Qt's generic application-data location with the same suffix. Operational acceptance
for this feature is Windows-specific. Synthetic tests inject an external temporary root.
Canonical authoring documents continue to use the normal workspace persistence boundary.

One `.packdraft.json` file is selected by SHA-256 of the workspace ID, canonical root,
and canonical workspace-document path (empty for an unsaved workspace). Windows paths
are compared without case. The full binding is also validated inside the document.
Copying a record to another workspace's filename cannot make it eligible for Restore.
Relocating a workspace does not silently rebind an old recovery copy.

The schema-1 JSON object has `Format: FOA-SDK.PackDraftRecovery`, `SchemaVersion`,
`WorkspaceId`, `WorkspaceRoot`, `WorkspaceDocument`, `PackId`, `IsNew`,
`AdvancedExpanded`, `Fields` and `Baseline`. Both maps require exactly the 18 named
editable fields. Strings preserve whitespace, line breaks, Unicode and incomplete
values. The two combo fields require existing choices. Pack ID/new-state and advanced
visibility are separate presentation metadata. This document is never a pack manifest.

Each field is limited to 16,384 UTF-16 characters; the complete document is limited to
256 KiB. Unknown versions/formats, foreign bindings, missing/extra fields, wrong types,
unsupported combo choices, malformed JSON and oversized records are rejected and kept.
Explicit Discard recovery copy may remove an unreadable record after a successful bind.
Linked recovery paths are rejected. There is no broad recovery-directory scan or cleanup.

## Checkpoint and lifetime

Field signals only update the in-memory form and arm one 750 ms timer. The timer captures
one immutable snapshot. A serial background worker owns the service and all recovery IO;
only one checkpoint is outstanding, so continued typing cannot build an IO queue. Each
write uses QSaveFile atomic replacement with direct-write fallback disabled. Failed writes
retain the previous good bytes and report an inline message. A further edit retries.

A QLockFile is held for the bound workspace. A living second Editor cannot read, replace
or discard the first Editor's copy. After a crash, Qt's stale-process lock handling permits
the next Editor to bind. The lock does not expire merely because a live session is long.

After a successful bind/read, an unresolved recovery copy disables form replacement and
Save until Restore or Discard is chosen. Restore retains the copy, restores the dirty
baseline, and does not activate or save a mod. Closing the pane or switching workspaces
without resolving the offer preserves that copy for the next visit. Retry recovery handles
unavailable storage/locks; saved files are never modified by a recovery failure.

Successful Save, New, Open, explicit Discard and reversion to the clean baseline retire
recovery. Explicit Save/Discard boundaries drain earlier checkpoints before removing the
copy, preventing a late worker write from resurrecting it. Cancel and failed Save keep the
form and recovery. A failed retirement keeps the copy and reports the problem. A successful
Save whose retirement fails remains saved, but the pane stays open for retry.

Workspace-switch Discard retires the old copy only after publication. A later admission
veto therefore leaves the old draft recoverable. A failure to retire after publication is
reported explicitly; the old copy remains associated with the old workspace.

Recovery is best effort between checkpoints: a forced shutdown can lose edits made since
the latest completed checkpoint. It is not a saved-mod backup or power-loss durability
claim. A recovered form remains unsaved until Save mod succeeds.

## Validation

Compiled `PackDraftRecoveryTests` cover raw round trips, deterministic bytes, fresh service
reads, workspace/document isolation, malformed/future/foreign/oversized rejection, invalid
manifest values, process-lock exclusion, explicit cleanup and actual Windows locked-file
write/delete failures preserving the previous bytes.

With a built SDK Editor and prepared engine-asset cache:

```powershell
./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_pack_draft_recovery_smoke.ps1 -EditorExecutable <build>/bin/profile/Editor.exe -EngineRoot <pinned-engine> -CacheRoot <prepared-cache> -OutputRoot <fresh-external-output>
```

The default suite runs 11 fresh Editor phases on a private Windows desktop that is never
activated. `-Suite new`, `saved` or `failures` selects a bounded diagnostic group. Every
phase waits for NotifyEditorInitialized and records the actual loaded SDK module hash.
The parent force-stops only its owned seed/recovered process after verifying the complete
checkpoint's hash on disk; those phases require nonzero exit and no about-to-quit signal.
All other phases require exit zero, about-to-quit and no forced cleanup. Missing phases,
timeouts, mismatched modules and failed assertions fail the suite. UI waits have a
five-second hang guard; these small synthetic fixtures are not storage throughput evidence.

The suite covers new/saved raw drafts, repeated forced termination, Restore/Discard,
workspace switch/return, failed Save and Cancel, failed recovery writes and retry, corrupted
copy rejection and explicit removal, and fresh-process checks that retired copies do not
reappear. Relevant normal shutdown and workspace-switch suites remain separate regressions.
Logs, fixtures, images and receipts stay outside source. No game/runtime sign-off is claimed.
