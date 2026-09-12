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

Validation status: focused input/plan/session tests PASSED; build and Editor
checks are in progress. Actual installed-game preview acceptance is NOT_RUN.
Synthetic tests do not establish native placement, spawn or save behavior.
