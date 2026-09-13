# Current Task

Status: IN_PROGRESS.
Goal: integrate the owner's pending FOA-SDK commits, resolve merge conflicts and
validate the combined Editor and Framework result.
Classification: Critical/Runtime, because the requested integration includes
previously implemented external process execution and synthetic rollback.
Primary coordination owner: integration; existing subsystem ownership is retained.

In scope: pending PR histories #266, #269, #270, #271, #273, #274, #275, #276,
#277, #278, #279, #280 and #281; necessary conflict resolution and validation.
The owner explicitly requested merging the commits. Use normal merge commits and
a reviewable integration PR; preserve existing feature commits and evidence.

Out of scope: completing M6 native terrain qualification, unfinished campaign
rendering/export, paused SDK preview acceptance, game/save writes and releases.
M6 remains PARTIAL/BLOCKED as documented in its design and qualification brief.
Earlier evidence stays bound to its source/artifacts and is not promoted to new
runtime signoff. The original dirty SDK-client checkout is not modified.

Acceptance criteria: all requested PR heads are ancestors of the integrated
result; conflicts are reviewed; applicable static, compiled and Editor checks
are reported accurately; remote main and remaining PR states are verified.
Known hosted failures under investigation: M3 maximum-metadata reopen performance
and Item Viewer Editor startup. Pending or failed jobs are not reported as passes.

Current branch: codex/integrate-pending-sdk-work.
Next action: finish combining the independent branches and validate their result.
