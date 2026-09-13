# Current Task

Status: native fixture implemented and verified; M6 remains PARTIAL. The owner
merged PRs #274, #278, #279 and integration PR #282, then requested continuation.
Branch: codex/heightmap-native-m6, based on integrated main efa76f5f8523b8a30ec731a03fe2cda2e50bca2a.

Current boundary: establish the native materialisation behavior of the accepted
SDK-owned 33 by 33 fixture in a disposable Unity authoring project. World Authoring
owns the comparison; Unity owns TerrainData, prefab, scene, metadata and bundle
output. This is Critical/Runtime qualification work under the accepted
[design](docs/tainted-grail-sdk/HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md).

Acceptance for this increment: native creation and full 1089-sample readback,
explicit coordinate/elevation tolerances, saved asset and bundle reopening,
independent collision observations, repeated builds, a fresh Unity process
reopen, and retained exact-tool/input/output evidence outside the source tree.
A bounded development qualification launch is separate from M2 isolation proof.
It must not register or ship an unrestricted production execution fallback.

Remaining M6 work includes canonical/Core handoff, a qualified native execution
profile, Framework bindings, exact owned-target deployment/recovery, and an
approved exact-install runtime test. No game/save writes, campaign replacement,
M7 work, releases, or inferred runtime authority are part of this increment.
All six canonical authority fields remain false. The dirty SDK-client checkout
and other worktrees are not modified. Runtime sign-off not performed.

Native qualification: two fresh Unity projects passed five complete readbacks
and twenty collider probes each, with a maximum height error of 0.122 mm.
The 19 independent auditor tests passed. Details and the remaining acceptance
lanes are in [the qualification report](docs/tainted-grail-sdk/HEIGHTMAP_NATIVE_QUALIFICATION.md).

Final validation: the complete static lane passed after installing the existing
pinned test dependencies in a private directory: 1438 tests with 34 explicit
skips, ten tooling tests, and four sets of ten source-policy checks. The final
LF-normalized Unity source was compiled and executed successfully again.
