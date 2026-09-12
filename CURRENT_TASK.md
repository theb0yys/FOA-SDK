# M6 - Heightmap Vertical Slice

Status: PARTIAL. Milestone requested by the owner; native execution is BLOCKED
pending the concrete provider/profile qualification described below. M6 has not
built or deployed terrain, launched Fall of Avalon, verified terrain in the game,
or performed game rollback.

Primary owner: world-authoring. The Framework owns the shared execution workflow;
ExternalToolchain owns process isolation, and the deployment/runtime providers own
their effects. Classification: Significant for this design; Critical/Runtime for
its subsequent executable implementation.

Branch: codex/heightmap-vertical-slice-m6.
Prerequisite: M5 at 523cfbc4a862c98f56fffd2769d11eff43b5bb17. M5's six-process native
and actual Editor success/cancel/crash/recovery checks passed locally. Its
exact-commit receipt and unmerged PR handoff are separate from M6 acceptance.

Scope and acceptance: [Heightmap M6 design](docs/tainted-grail-sdk/HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md).
Blocking questions and required evidence:
[M6 native execution brief](Research/world-authoring-terrain-heightmap/briefs/M6_NATIVE_EXECUTION_QUALIFICATION_BRIEF.md).

Initial target assumption: one small SDK-owned terrain in a disposable test scene.
Existing campaign replacement and the separate campaign importer PR are excluded
from this first slice. No uncommitted work from other terrain tasks is included.

Next concrete work: qualify the native build and game-launch process profiles,
then implement terrain materialisation and the exact target-owned deployment and
runtime observation bindings through the existing Framework. Do not enable a
terrain command, weaken the existing sandbox, or flip V1 authority flags to bypass
missing qualification. Any actual game operation needs its final artifact
inventory, exact destination and rollback review first.
