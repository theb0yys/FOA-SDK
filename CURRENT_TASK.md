# Current Task

Status: Preview Encounter inside Connect to Game is implemented; validation in progress.

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

Next action: finish compiled and Editor checks and record exact results in
the preview guide and an external evidence pack.
