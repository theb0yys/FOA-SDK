# M6: heightmap through the shared execution workflow

Status: PARTIAL design and intake. The owner requested M6 on 13 September 2026.
This records the requested outcome and the concrete blockers; it is not a claim
that a terrain provider, Unity execution profile or runtime adapter is qualified.

## Outcome and first fixture

A local `foa.terrain-heightmap` document must produce native terrain, build and
package it, deploy the reviewed package, load it in the exact supported Fall of
Avalon profile, verify the terrain from runtime observations, then restore the
owned deployment target. All phases run through the existing Framework and its
provider bindings. Success includes verified rollback, not just a package file.

Use one SDK-owned 33 by 33 U16 little-endian heightfield with one-metre sample
spacing and an asymmetric ramp/corner pattern. This is a proposed bounded fixture,
not a source identity for a campaign map. Use an explicit eight-metre vertical
range and explicit coordinate/row orientation. No campaign asset, save, extracted
commercial content or proprietary Unity project belongs in this fixture.

The first target is a new disposable terrain scene. Existing campaign replacement
needs a separate proven source-object binding and is excluded. The active campaign
import work is separate; its source preservation does not prove game return.

## Owners and boundaries

| Surface | Owner | Required behavior |
| --- | --- | --- |
| Canonical document, tile hashes, dimensions and transforms | Existing Core terrain contracts | Validate V1; retain its canonical bytes and all six false authority fields |
| Native terrain creation and domain comparisons | World Authoring with the external Unity provider | Unity authors TerrainData, native metadata and outputs; explicitly report transforms and losses |
| Execution plan, qualification, policy and confirmation | Framework | Resolve every phase to an exact binding; recheck drift and admission before effects |
| Processes and process-tree cleanup | ExternalToolchain | Use qualified execution profiles with bounded IO, lifetime and observations |
| Package and installation inventory | Existing packaging/deployment owners | Derive a reviewed exact inventory from verified native artifacts |
| Owned target mutations and recovery | Deployment provider | Pin target identities and preimages; persist intent; restore only owned expected bytes |
| Terrain load and observation | Exact runtime adapter | Instantiate only the SDK fixture, observe actual terrain, unload it and report failure accurately |
| Evidence assessment | Existing assessment/evidence owners | Consume observations as candidate evidence; never turn a receipt into permission |

M5 remains an isolated fixed text fixture. Do not rename its profile, relax its
fixed target, or treat its value interpreter as a terrain runtime provider.
Default M3 staging and existing V1 validators remain unchanged. If the new native
execution profile cannot be expressed without changing a frozen contract, use an
explicitly versioned additive contract with old-input rejection/compatibility proof.
Do not extend the old adapter capability enum merely because it contains fewer
capabilities than the generic M1 registry.

## Inspected gaps

The M5 baseline contains the terrain document/importer, the Framework, and M2.
It does not contain an executable terrain binding. `FrameworkProviderService`
permits the original staging phases or the explicit synthetic profile. The
current Mono adapter only emits its startup marker; it has no terrain command.
The IL2CPP adapter is a separate route and cannot inherit Mono qualification.

M2 currently has one profile, `windows-lpac-registry-read-batch-v1`. It copies a
bounded executable and companion set into private staging, accepts at most 64
companions, and caps aggregate artifact bytes and process memory at one GiB.
Unity build and interactive game launch have not been qualified under that
profile. These bounds are implementation facts, not a measured minimum Unity
requirement. Raising them or introducing an unsandboxed fallback is not a fix.

The installed Unity Editor's version resource can establish a discovery candidate,
not process, licensing, content, game-profile or runtime qualification. Likewise,
a game's launcher version resource is not sufficient to identify the loaded
Unity player. Bind actual exact-profile observations and do not select a route
from a filename or an old startup marker.

The [focused qualification brief](../../Research/world-authoring-terrain-heightmap/briefs/M6_NATIVE_EXECUTION_QUALIFICATION_BRIEF.md)
names the missing facts, evidence and next implementation boundary.

## Native conversion requirements

Validate the source through the existing terrain contract before native work.
Reject unsupported dimensions, gaps/overlaps, corrupt tile lengths/hashes,
unknown versions, unsafe paths, and unreviewed transforms. No silent resampling.

Unity's documented `SetHeights` takes normalized values and indexes the array as
row then column. Its heightmap resolution is constrained; verify the actual
resolution after assignment instead of assuming an arbitrary canonical grid was
accepted. See [SetHeights](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainData.SetHeights.html)
and [heightmapResolution](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainData-heightmapResolution.html).

Qualification must compare the complete asymmetric input against native readback,
including corner orientation, extents and elevation. Declare a numeric tolerance
before running the test and retain measured error; do not claim byte determinism
for Unity-generated bundles from a successful process exit. Native artifacts,
Unity-generated metadata, package locks and logs stay outside the source checkout.
The Unity build API is supporting context, not FoA compatibility proof:
[BuildAssetBundles](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/BuildPipeline.BuildAssetBundles.html).

## Process, deployment and runtime threat boundary

The native build profile must identify its entire executable/dependency set,
immutable source inputs, writable project/cache/output roots, package resolution,
license/IPC behavior, descendants, memory/disk/log/time limits and cleanup.
Qualification must exercise denial of unrelated filesystem/network/IPC access.
A new profile requires its own negative operational proof; it must not silently
change what the existing LPAC profile means.

Game launch is a distinct boundary: confirm graphics/session requirements,
loader behavior, normal game writes, bounded observation transport and shutdown.
No production shell command, arbitrary script route, unrestricted process launch
or blanket approval may substitute for that qualification.

Only a new SDK-owned deployment directory is proposed for the first slice.
Finalize the actual package inventory and destination after native artifacts
exist. Refuse an existing foreign directory, reparse/hard links, concurrent
writers, stale target identities and any drift between preview and mutation.
Intent and rollback records must survive interruption. Rollback removes only
unchanged files created by this execution, or restores preimages explicitly
bound by the plan; it must not overwrite a later user or third-party change.

The adapter must never access saves or replace campaign terrain in this slice.
This is an adapter restriction, not an assertion that the game's own ordinary
startup/shutdown performs no writes. Those observed side effects and the exact
installation must be included in the final game-test review.

Runtime success requires the exact fixture instance, artifact/profile/plan
bindings, native grid/size, and observed height samples. Include independent
collision/height probes where supported and evidence that unloading removes the
owned instance. A startup log, an AssetBundle load result or an Editor screenshot
alone cannot satisfy runtime verification.

## Acceptance and status

| Lane | Required proof | Current state |
| --- | --- | --- |
| M5 prerequisite | Six actual processes, failures, shutdown and fresh crash recovery | PASSED locally on M5; separate exact-commit handoff |
| M6 canonical input | Existing V1 fixtures unchanged; malformed inputs and transforms rejected | NOT_RUN for M6 |
| Native build profile | Exact executable/dependency/IO profile; success, cancellation, timeout and isolation denial | BLOCKED: no qualified profile |
| Native materialisation | Real Unity creation, readback, inventory and repeatability/loss measurement | NOT_RUN |
| Framework reachability | Real bindings for build/package/deploy/launch/verify/rollback and a reachable Editor command | BLOCKED: native bindings absent |
| Deployment/recovery | Exact owned-target mutation, failure injection, drift refusal and fresh recovery | NOT_RUN for terrain |
| Runtime | Exact installation and adapter; actual fixture load, observations and unload | BLOCKED: adapter and launch profile absent |
| End-to-end rollback | Original deployment inventory restored and runtime instance removed | NOT_RUN for terrain |

L0 document/link review applies to this intake. Subsequent implementation requires
L0-L2, actual Editor integration, and the exact L4 provider/deployment/runtime
lanes. Preserve all failing attempts and explicit skips. Measure conversion cost
at the stated 1089-sample fixture and resource ceilings before broadening terrain
size. M6 is complete only when CEC-G13 and applicable CEC-G12/G15 proof passes.
Runtime sign-off not performed. No M7 work is included.
