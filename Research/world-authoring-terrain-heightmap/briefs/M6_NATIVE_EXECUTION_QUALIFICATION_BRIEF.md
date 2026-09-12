# M6 native execution qualification brief

Date: 13 September 2026. State: BLOCKED for native implementation assumptions;
PARTIAL for the requested M6 milestone. This is a research/qualification brief,
not an implementation or runtime acceptance result.

## Request and blocked decision

The owner requested the Heightmap Vertical Slice through build, package, deploy,
verify and rollback. The initial working target is one SDK-owned test terrain.
Determine the exact native build and game-launch execution profiles, their
isolation/resource/dependency requirements, and a verified terrain load/observe/
unload route. None is established by the current synthetic process profile.

Implementation must not invent those security or runtime properties, copy M5's
synthetic qualifications, weaken sandbox limits, or claim a Unity-native/game
artifact from a neutral text fixture. Independent M5 acceptance and M6 scope
work can proceed while these native boundaries remain unresolved.

## Known from the repository

Controlling authority: the owner's current M6 request, AGENTS.md, Engineering
Process, Capability Execution Contract (M6 and CEC-G11 through CEC-G13), existing
terrain V1 contracts, Tool Execution M2 design and protected-files policy.

At M5 commit 523cfbc4a862c98f56fffd2769d11eff43b5bb17:

- The Framework and M2 execute the complete isolated text fixture; the actual
  Editor passed success, cancel, deliberate hard crash and fresh recovery.
- Terrain V1 is neutral local authoring data. Its six false authority fields are
  retained permanently; executable permission belongs to the newer shared spine.
- The only M2 profile is bounded LPAC batch execution, with 64 companion entries
  and a one-GiB aggregate artifact/memory ceiling. Unity and game launch are not
  qualified. A bound is not a measured workload requirement.
- Framework later phases are explicitly restricted to the synthetic target.
- The Mono runtime adapter logs a startup marker; no terrain-loading command is
  implemented. Mono and IL2CPP remain separate.
- Campaign source binding is a distinct unresolved research lane. PR #266's
  source-preservation work does not provide this M6 execution binding or game
  return proof. Its uncommitted work must not be adopted implicitly.

Read-only local version-resource inspection identified candidate Unity tooling.
Private paths and machine observations remain outside the repository. File
presence/version metadata is not a qualified profile or a live runtime result.
No Unity process, game launch, installation mutation or save access was performed
for this intake.

## Questions and missing proof

| Area | Why it matters | Required answer and evidence |
| --- | --- | --- |
| Native build process | Current M2 profile is qualified for bounded native batch fixtures | Exact Unity executable/module hashes, required companion set, project/cache writes, descendants, package resolution, licensing/IPC and limits; independently observed access-denial, cancel, timeout and cleanup results |
| Versioned profile contract | Existing V1/M2 semantics cannot silently change | Show whether the qualified workload fits current contracts; if it does not, propose the smallest additive profile/API change, compatibility fixtures and threat analysis |
| Native terrain output | Canonical heightfields do not prove a Unity/FoA loadable artifact | Exact Unity conversion recipe and output kind, native metadata ownership, asymmetric sample/transform readback, numeric tolerances, complete inventory and repeatability/loss evidence |
| Exact runtime route | Launcher metadata and old adapter declarations do not establish the loaded game | Bounded read-only installation identity plan, loaded player/game/loader/adapter identity, chosen Mono or IL2CPP route and required dependency/license review |
| Terrain load/observe/unload | Startup marker is not a working terrain feature | Exact SDK-owned instance lifecycle using reviewed APIs, input validation, actual height/collision observations, observation transport bounds, stale/replayed receipt rejection and unload proof |
| Game-launch isolation | Interactive game behavior differs from batch compilation | Qualified executable/session/graphics/process-tree profile, normal write inventory, save boundary, cancellation/shutdown behavior and unrelated-access refusal |
| Deployment and rollback | M5's fixed text target cannot be redirected to a game install | Exact destination/inventory, ownership/preimages, durable intent/backup/inverse, corruption/drift/concurrency tests and fresh-confirmation interrupted recovery |

Do not retry broad campaign web research for this new-terrain slice. Use public
Unity API documentation as supporting context, qualified synthetic Unity tests for
native authoring claims, and exact-install evidence only for runtime claims.
Unity documentation does not establish LPAC compatibility or FoA terrain loading.

## Path map

Paths below are repository-relative unless a public link is supplied.

| Path | Relevant section or symbol | Role |
| --- | --- | --- |
| `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md` | M6; CEC-G11, G12 and G13 | Controlling milestone and proof |
| `docs/tainted-grail-sdk/HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md` | First fixture, owners and threat boundary | Current bounded intake and acceptance |
| `docs/tainted-grail-sdk/TOOL_EXECUTION_M2_DESIGN.md` | Scope; accepted LPAC profile | Existing process boundary |
| `Gems/ExternalToolchain/Code/Include/ExternalToolchain/ToolExecutionTypes.h` | ToolExecutionProfile; ToolMaxFiles; ToolMaxArtifactBytes | Exact implemented limits |
| `Gems/ExternalToolchain/Code/Source/Execution/Platform/Windows/ToolProcessBackend_Windows.cpp` | WindowsToolProcess::Run; companion staging; sandbox; process cleanup | Actual operating-system boundary |
| `Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkProviderService.cpp` | Supported; Supports; Resolve; Recheck | Exact phase/profile admission |
| `Gems/TaintedGrailModdingSDK/Code/Source/TerrainHeightmapDocument.h` | TerrainHeightmapDocumentV1; Authority | Existing canonical input |
| `Plugins/RuntimeAdapters/Mono/Source/MonoRuntimeAdapterPlugin.cs` | Awake | Current startup-only adapter |
| `Research/o3de-to-unity-conversion-and-runtime-bridge/areas/09-qualification-and-experiments/README.md` | Determinism classes; negative coverage | Supporting qualification requirements |
| `Research/world-authoring-terrain-heightmap/intakes/DR_TH_004_CLEAN_INTAKE.md` | Per-map disposition | Historical campaign-source uncertainty, outside this fixture |
| [PR #266](https://github.com/theb0yys/FOA-SDK/pull/266) | Native campaign source preservation | Separate in-progress source/authoring context |

The old terrain and Unity research documents predate M2-M6. Their blanket
future-process statements do not cancel the current owner's M6 request. Their
missing compatibility, source and runtime evidence is still missing evidence;
the current request does not turn it into a pass.

## Protected data and required permission

Keep the game installation, saves, credentials/license material, proprietary
assets and pinned external engine read-only. Do not copy them into source or use
them as committed fixtures. Keep generated Unity/native artifacts and private
observations outside source. Do not activate, return, replace or expose licenses.

Before any game mutation or launch experiment, prepare the exact artifact,
destination, inventory, observed side effects and inverse operation for the
owner's review. An abstract milestone, a discovery result or a hash does not
supply an exact external-write review. No such game write was attempted here.

## Research output and handoff

Return source URLs and exact source revisions, direct answers per row, uncertain
or contradictory facts, reproducible qualification steps and expected failure
results. Distinguish facts observed from inferences and proposed designs. State
explicitly when no qualified route exists. No general research report can stand
in for execution or game observations.

Consuming gates: CEC-G0 native threat boundary, CEC-G3 provider resolution, CEC-G4 policy separation, CEC-G5 supervisor security,
CEC-G12 Editor reachability and CEC-G13 exact-profile heightmap runtime.
Evidence fields: research_authority.missing_authority, tests.missing_lanes,
build_deploy.status and runtime_proof.runtime_signoff_status.

Next action when the missing profile facts are established: implement the
smallest reviewed native process profiles and terrain provider bindings, run the
synthetic qualification fixture, then prepare the concrete game-test inventory
and rollback review. M6 remains incomplete until actual load, verification and
rollback are proven. Runtime sign-off not performed.
