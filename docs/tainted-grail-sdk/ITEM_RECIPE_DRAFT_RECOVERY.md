# Item and Recipe draft recovery

## Ownership and compatibility

The schemas-and-persistence owner provides a private ItemRecipeDraftRecoveryService;
ui-framework captures and restores raw form state. Restore changes forms only.
The catalog, workspace, pack, evidence and public API contracts are unchanged.
Explicit existing Save commands remain the only path into canonical authoring data.

This Significant addition follows Pack Manager's atomic, workspace-bound recovery
pattern. Earlier Editors ignore the new files. Unknown formats/versions, malformed
or incompatible fields, missing definitions/choices, and foreign workspace bindings
are rejected with an explanation and kept until explicit Discard. No automatic
migration or relocation of recovery copies is supported.

## Format and boundary

Storage is under the platform's local application-data FOA-SDK/Recovery/ItemRecipeDrafts
folder. A SHA-256 key of workspace ID, canonical root and canonical document path
selects one .itemrecipedrafts.json file. Windows identity is case-folded. The same
binding is checked inside the document. Saved catalog and game files are not touched.
Linked recovery paths are rejected, and there is no directory-wide scan or cleanup.

Schema 1 uses Format FOA-SDK.ItemRecipeDraftRecovery, SchemaVersion, WorkspaceId,
WorkspaceRoot, WorkspaceDocument, Drafts, Item, Recipe, Tab and RecipeExpanded.
Drafts maps kind:record-id keys (item, recipe, ingredient, output, or acquisition:)
to Values and Baseline maps. Stable named form fields use explicit string, integer,
number, boolean and selection tags; selections preserve exact item data separately
from display text. Unknown fields, wrong types, out-of-range values and unavailable
choices cannot be silently coerced. A newer UI must bump the format or supply an
explicit tested migration if these meanings change.

Limits: 256 dirty forms, 64 fields per form, 16,384 UTF-16 characters per string,
512 characters per record ID, and 2 MiB for the complete document. Invalid and
oversized writes keep the previous good copy. Invalid reads block replacement
until Restore (if valid), Discard or Retry resolves the offer.

## Checkpoints and lifecycle

Field edits arm one 750 ms timer. A serial worker owns storage and locks; at most
one background write is outstanding. Continued typing does not postpone checkpoints
indefinitely. QSaveFile performs atomic replacement without direct-write fallback.
A per-workspace QLockFile excludes a living second Editor and permits recovery once
the prior process is dead. Recovery failures appear inline and can be retried.

Restore preserves the captured dirty baselines and selected forms; review restored
values before saving, particularly if catalog data changed after the checkpoint.
Unresolved offers survive closing or switching away. Save checkpoints the remaining
unsaved forms; successful Discard/close retires recovery. Cancel and failed Save keep
it. Workspace Discard retires the old copy only after replacement commits. Shutdown
rollback restores forms and resumes checkpointing if a later pane vetoes exit.
During whole-Editor exit, the last checkpoint and workspace lock remain held across
all later pane/file prompts. A drained worker transfers the store to the close
dispatch; only a fully accepted exit retires it. If cleanup fails after the host
has already accepted exit, the copy is kept for the next visit and the Editor log
records the failure. A forced shutdown at a later pane's prompt remains recoverable.

Only completed checkpoints survive forced termination. Edits since the most recent
checkpoint can be lost; this is neither a catalog backup nor a power-loss guarantee.

## Validation

Required proof: typed round trips, fresh-process recovery, malformed/future/foreign
rejection, limits, locked-file preservation, concurrent-owner exclusion and retry;
actual Editor Restore/Discard with all retained form kinds, repeat termination,
saved-byte checks, workspace isolation and close/exit/workspace regressions.
With the pinned built Editor and prepared asset cache, run:

```powershell
./Gems/TaintedGrailModdingSDK/Tools/editor_tests/run_item_recipe_draft_recovery_smoke.ps1 -EditorExecutable <build>/bin/profile/Editor.exe -EngineRoot <pinned-engine> -CacheRoot <prepared-cache> -OutputRoot <fresh-external-output>
```

The runner uses a private desktop and 13 process phases. Forced termination is
allowed only after all nine form snapshots match durable bytes; the next Editor
must restore those fields without catalog writes. Other phases require native
exit zero and about-to-quit. The loaded SDK module hash is checked for every phase.
The fixture includes same-ID workspace isolation, repeat termination, saved and
discarded copy retirement, actual file locks, corrupted/incompatible copies,
missing definitions and retry. Background checkpoint event gaps must stay below
500 ms; each fixture interaction must complete within five seconds.

Run the separate pane-close, whole-Editor exit and both workspace-switch suites
as regressions. These checks prove Editor authoring behavior only.
