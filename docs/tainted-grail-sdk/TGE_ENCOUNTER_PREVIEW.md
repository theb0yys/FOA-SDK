# Preview Encounter in the SDK Editor

Open **FOA-SDK Home → Connect to Game**, connect to TGE, choose an encounter
composition with **Browse...**, then choose **Preview Encounter**. The connected
host must advertise `tge.foa.encounters@0.1` owned by `tge.foa.encounters`.

Use an exported `foa-tge-encounter-composition/1` file, not the authoring catalog
itself. [Encounter commands](TGE_ENCOUNTERS.md#saved-editor-compositions) describe
exporting fixed actor counts with explicit native-template bindings. Files are
limited to 64 KiB and eight actor slots. Duplicate JSON fields, unsupported
templates, invalid counts and unsupported source metadata are rejected before
connecting to the host.

The pane shows the composition name, actor slots, host placement description,
SHA-256 of the exact file bytes and local preview time. File contents are read
again on each request. Editing the selected path clears the old preview; changing
the file on disk requires another preview. The digest identifies the observed
bytes, not their authenticity or authorization. Names and placement are plain text.

Preview creates or replaces TGE's temporary pending plan. It does not spawn or
remove actors. TGE may reject preview if encounters are disabled, the player or
placement is unavailable, or multiple actors have not yet passed the host's
single-actor check. The native placement checks and plan ownership remain in TGE.
This pane is an observation of the response, not authority to execute the plan.

Plans last at most thirty seconds and may become invalid earlier after movement,
world changes or another client's preview. The display expires conservatively
from before the worker starts, so transport delay cannot extend its lifetime.
The pane does not poll, automatically retry or preserve plans across reconnects.
Position and vitals keep their own observation times.

Local file errors preserve the connection and existing player observations.
Protocol errors, rejected plans and session changes clear the connection and
require reconnecting. Cancel, disconnect, connection refresh and pane close clear
the preview. One asynchronous worker runs at a time with the existing five-second
network, twenty-second operation, twenty-one-second supervisor and 2 MiB output
limits. The worker is the only file reader; the UI thread does no file parsing.

Packaging adds the existing `tge_encounters.py` beside the connection worker in
`scripts/foa-sdk`. No composition, wire, workspace or save schema changes.
The runtime service and command-line spawn/remove workflow are unchanged.

Validation on 2026-09-12: PASSED for 90 Python tests with no skips, 22 compiled
connection-service tests, exact-pin Windows Profile configure/build, and 28 actual
Editor UI checks using a synthetic authenticated listener. The picker opens
asynchronously; cancelling it preserves the selected file. UI checks cover
composition display, new file contents, expiry, invalid input, absent service,
rejected plans, cancellation and pane lifecycle. The preview text was visually
inspected for clipping. Maximum event gaps were 12 ms in compiled tests and
110 ms in the Editor, below the 500 ms test budget. Scoped static/source-policy
checks passed. Evidence and preserved failed test attempts remain outside the
source checkout.

Actual installed-game preview acceptance is NOT_RUN; overall runtime acceptance
is PARTIAL. Synthetic tests do not establish native placement, spawn or save
behavior.

A subsequent owner-approved live attempt passed authenticated game identity and
encounter/player discovery. Three typed player reads returned player_unavailable;
the connection failed and the game exited before any preview request. Native
shutdown markers were observed, but the process exit code was not retained.
The temporary app-ID was removed, original config bytes restored, and installed
profile/DLL integrity verified. No preview or spawn/remove request was sent.

That attempt's Editor launcher omitted --rhi=null, the renderer mode used by the
passed pane checks, and failed while loading a viewport shader. A disconnected
recheck with the original mode opened the Preview Encounter pane successfully.
Actual loaded-game preview remains a separate pending check; failed-attempt and
cleanup evidence is retained outside the source checkout.

A later attempt connected the actual Editor to the installed game successfully.
All twenty typed position reads reported player_unavailable, so no preview was
released. Computer Use was stopped by the owner; after game exit, original
configuration restoration and temporary app-ID removal PASSED. A title-screen
language diagnostic was observed; its cause and relation to exit are unproven.
The subsequent retry used a fresh native-loading completion check plus a
same-session player read before releasing the one Editor preview. The launcher
authenticated, but transport failed while the game remained active and the
Editor observer never accepted a preview. The owner again stopped Computer Use
with Escape; the queued preview was revoked. After game exit, exact config and
app-ID cleanup and installed profile/DLL checks PASSED. Exit cause and code were
not established. No preview or spawn/remove request was sent. Live preview,
expiry and cancellation remain NOT_RUN; UI control is paused. NPC appearance/
removal evidence from another task does not prove this UI route.

The latest attempt passed native loading completion and loaded-player readiness,
then connected the actual Editor to that same game session. Its loaded SDK module
matched the validated binary. The file-selection step remained pending: Browse
was invoked but no visible picker or selected file was observed. The cause remains
unproven. The owner stopped Computer Use with Escape before further diagnosis;
no preview request was sent. The game exited with code 0, original config/app-ID
cleanup and installed profile/DLL integrity PASSED, and the disposable Editor was
closed. Actual loaded-game preview, expiry and cancellation remain NOT_RUN.

A private observer mode is prepared for the next owner-resumed check. It enters
the composition path directly in the editable field and refreshes the bound
session, skipping both automatic Browse steps. Picker selection/cancellation are
NOT_RUN in this mode; the live picker issue remains unresolved. Fifty-three
synthetic observer/controller tests PASSED with no skips, including retaining an
accepted preview when a later Cancel check is PARTIAL. No SDK binary changed and
no game or Editor was launched for this preparation. Live acceptance remains
NOT_RUN while UI control is paused.
