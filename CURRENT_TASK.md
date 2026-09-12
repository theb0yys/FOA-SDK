# Current Task

Status: PAUSED by owner; loaded-game Editor connection PASSED; preview NOT_RUN.

Goal: load an existing encounter composition and display the connected TGE host's
temporary placement plan. The owner requested the next function after Player Vitals.

Classification: Significant Editor integration / Critical runtime request boundary.
Primary SDK owner: runtime-adapter-contracts. TGE owns native placement and pending
plans; Framework owns transient observations; the widget presents them.

In scope: composition picker, bounded file validation, same-session preview,
template/count/name/placement display, file digest, conservative expiry,
cancellation and existing worker packaging. Existing encounter composition,
transport and host contracts remain unchanged. Current branch: `codex/tge-sdk-client`.

Out of scope: spawn/remove execution, encounter runtime edits, saves, game launch
or deployment, previous live-player checks, unrelated Editor work and publication.
Preview changes the host's temporary pending plan; it is not a zero-side-effect
claim. It replaces earlier pending previews and may expire early on world changes.

Acceptance: focused Python input/plan/session/deadline tests, compiled service
validation, exact-pin build, and actual Editor UI checks with a synthetic listener.
Confirm that no spawn/remove request is sent, no composition is written, invalid
inputs preserve the connection, and expired plans cannot appear current.
Actual installed-game preview acceptance remains a separate NOT_RUN lane.

Files: existing connection worker/service/widget/tests and CMake package list;
new preview worker tests and usage guide; focused smoke and adapter/README links.

Validation: PASSED for 90 Python tests (no skips), 22 compiled connection-service
tests, exact-pin Windows Profile configure/build and 28 Editor UI checks with a
synthetic authenticated listener. Preview rendering was visually inspected.
Measured maximum event gaps: 12 ms compiled, 110 ms Editor; budget 500 ms.
Scoped static/source-policy checks PASSED. Actual installed-game preview remains
NOT_RUN and runtime acceptance PARTIAL. See TGE_ENCOUNTER_PREVIEW.md for usage.

The initial configure failed on build-directory permissions and CMake PATH;
the corrected invocation passed. Early picker checks failed; the final picker
opens asynchronously and the final UI run passed. Failed attempts, hashes and
results are retained in the external evidence pack. Concurrent task commits in
the shared checkout were preserved; this task created no commit or publication.

The owner subsequently authorized installed-game preview validation, including
temporary encounter configuration and app-ID creation with exact restoration.
Authenticated game identity and encounter/player service discovery PASSED.
Three typed position reads returned player_unavailable; the connection then
failed and the game exited before preview. Native quit/core-stopped/host-stopped
markers were observed; the game exit code was not retained. Zero preview or
spawn/remove requests were sent. App-ID cleanup, original config restoration
and installed profile/DLL integrity checks PASSED after the game exited.

The test Editor initially failed on a missing viewport shader. Its launcher
omitted the null-renderer mode used by the passed UI checks. Restoring --rhi=null
allowed the disconnected Preview Encounter pane to open successfully. This
launcher correction remains private test setup, with no SDK source change.

A later attempt connected the actual Editor to the authenticated game, but all
twenty typed position reads returned player_unavailable. Zero preview or
spawn/remove requests were sent. The owner stopped Computer Use with Escape;
the game subsequently exited and configuration/app-ID cleanup PASSED.

The owner resumed the check after the NPC and inventory sessions completed.
The current installed inventory host was separately bound to its reviewed
build/deployment evidence and preserved. The copied native-loading gate and
runner guards passed 35 focused tests with no skips. The new launcher
authenticated successfully, but transport failed while the game remained active;
the Editor observer never accepted a preview. The owner again stopped Computer
Use with physical Escape. The queued preview was revoked; zero preview or
spawn/remove requests were sent. The cause of transport failure and later game
exit is unknown; the exit code was not observed.

After game exit, original config restoration, exact temporary app-ID removal,
and installed profile/DLL integrity checks PASSED. The disposable test Editor
was closed and game/config ownership released. No passive cleanup helper remains.
Live preview, expiry and cancellation remain NOT_RUN. Both exact-path approvals
persist. The owner has resumed UI control again. The private controller now waits
for native loading before player reads and Editor launch, tolerates bounded
read-only transport failures while keeping protocol/session errors fatal, and
retains passive cleanup after failure. Eleven focused tests and independent
review PASSED. A concurrent inventory session prevented the next launch; no
protected writes or lease were acquired. The reviewed retry is prepared and
awaited that session's cleanup. No next SDK function is started here.

The subsequent attempt loaded the save, passed native loading/player readiness,
and connected the actual Editor to the same game session. The exact loaded SDK
module matched the validated artifact. The observer then remained at the picker:
Browse was invoked, but no visible dialog or selected file was observed. Its cause
is unproven; the proposed in-Editor diagnostic was not executed. The owner again
stopped Computer Use with physical Escape. Zero preview or spawn/remove requests
were sent. The game exited with code 0; original config/app-ID cleanup and installed
profile/DLL integrity PASSED. The disposable Editor closed and ownership released.
Preview, expiry and cancellation remain NOT_RUN; no live UI work is active.

Preparation after that stop changes only the private observer/controller and these
status documents. The new observer enters the exact composition path in the actual
editable field and explicitly refreshes the same session. It skips both automatic
Browse steps; picker selection/cancellation remain NOT_RUN in this mode. Accepted
preview, expiry, a deliberate second preview for Cancel, and disconnect clearing
retain bounded checks. A too-fast cancellation check produces PARTIAL, and the
controller preserves any earlier accepted preview count on partial/failing exits.
Fifty-three focused synthetic state/controller tests PASSED with no skips. This
preparation launched no game or Editor and acquired no installation lease. The
previous release receipt is preserved; live preview acceptance remains NOT_RUN.
