# M6 native heightmap authoring qualification

Status: PASSED for the fixed synthetic Unity authoring fixture on 13 September
2026. Overall M6 remains PARTIAL. This is the native behavior prerequisite from
[the M6 design](HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md), not a production provider
or an assertion that the game can load the resulting terrain.

## Fixture and ownership

World Authoring's test source is
[`FoaHeightmapNativeProbe.cs`](../../Gems/TaintedGrailModdingSDK/Tools/editor_tests/FoaHeightmapNativeProbe.cs).
It generates only the SDK-owned 33 by 33 U16LE pattern defined in that source.
It accepts no game input, arbitrary script, canonical document or campaign map.
Its raw fixture SHA-256 is
`65c80672d5236056df92ae134fe3374e25f5e491d27dd81258ab32dcf1946a18`.

Source rows start at the north edge; samples are grid vertices one metre apart.
The source uses right-handed +X east, +Y north, +Z up coordinates. Mapping into
Unity swaps canonical Y/Z and reverses source rows. The 33 vertices span 32
metres, not 33 metres. Elevation spans -2 to +6 metres: native size is (32,8,32)
and the instance origin is (0,-2,0). The asymmetric corners and full interior
pattern detect row flips and transposition. No resampling occurs.

Unity owns and writes TerrainData, prefab, scene, GUIDs, `.meta` files and
AssetBundles in the private project/output roots. No native artifacts or
installed Unity binaries are redistributed by this source-only fixture.

## Exact development execution boundary

The measured Editor was `6000.0.64f1` (`5360b7cd7953`), Windows standalone target.
The script rejects a different Editor and non-batch invocation. The inspected
executable SHA-256 was
`3b1d93eefa2bbca789d99c83886266f13a58d5cbe5cfe8f2392a41cb45e11b05`;
its adjacent Unity DLL was
`0a43985e3f46a33cd6c3705d21ef8b76f6eedbf88fa695e0bf9376eff33a8d8a`.
These identify observations; they are not an authenticity or licence grant.

Use a fresh private project containing only this Editor script, and an existing
empty output directory outside that project and all source checkouts. Let Unity
create its own project and metadata. Set `FOA_HEIGHTMAP_OUTPUT` to that output
root. Disable Package Manager with `-noUpm`; the measured projects contained no
UPM manifest or lock. Retain the installed compiler references separately.

The bounded local qualification harness created Unity suspended, assigned it to
a Windows job, then resumed it. The job had a four-GiB aggregate memory ceiling,
a 32-process ceiling and kill-on-close cleanup. Each invocation had a 300-second
deadline, a 16-MiB log observation limit and a four-GiB project disk observation
limit sampled every ten seconds. Disk/log observations are sampled checks, not
filesystem quotas. Unity and compiler worker counts were both four. Only this
job's descendants were terminated during cleanup. No existing Unity process,
installation, game directory, save or licence file was changed by the harness.
Unity's own existing licensing IPC was used; this run did not qualify or isolate
that channel, unrelated filesystem access, network access or registry access.

The actual development invocations were equivalent to:

```text
Unity -batchmode -nographics -noUpm -job-worker-count 4 -createProject <private-project> -quit -logFile <private-create-log>
Unity -batchmode -nographics -noUpm -job-worker-count 4 -projectPath <private-project> -executeMethod FoaHeightmapNativeProbe.Run -logFile <private-run-log>
Unity -batchmode -nographics -noUpm -job-worker-count 4 -projectPath <private-project> -executeMethod FoaHeightmapNativeProbe.Reopen -logFile <private-reopen-log>
```

Set the process-local `BEE_BUILD_THREADS=4` as well. Unity documents that setting
in its [6000.0 release notes](https://release-notes.ds.unity3d.com/search?from_release=6000.0.32f1&to_release=6000.0.71f1)
and describes `-noUpm` and batch execution in the
[Editor command line reference](https://docs.unity3d.com/6000.0/Documentation/Manual/EditorCommandLineArguments.html).
The first attempt omitted the Bee limit and failed with process quota exhaustion;
that failure was retained. Another repetition was interrupted by a harness
observation race when Unity deleted a temporary shader-cache file. The harness
was corrected to tolerate that disappearance and the whole run was repeated in
a new project. Neither failed attempt is reported as a native pass.

This development fixture does not register a Framework command or ship a process
launcher. A bounded Windows job is not M2's LPAC isolation profile. Ordinary
compilation in the successful project referenced 247 installed managed assemblies
(27,453,864 bytes); that observed reference set alone exceeds M2's current 64-file
staging limit. No existing M2 limit, qualification, permission or API was relaxed.
The complete dependency closure and its isolation/IPC behavior remain to be
qualified before production binding.

## Checks and measured results

The tolerance was declared before the run as `2/65535 + 1e-7` normalized units
(less than 0.245 millimetres across the eight-metre height range).

Two independent fresh projects completed native creation, saved-asset reopening,
two bundle builds and a fresh-process bundle reopening. Per project there were
five full readbacks of 1,089 samples and twenty independent collider raycasts.
The largest normalized error was `0.000015247070044466238`, or
`0.0001219765603557299` metres. The largest collision error was approximately
`0.00011896405693345713` metres. All five readbacks had identical decoded sample
bytes. Both rebuilds within each project produced equal bundle bytes; that is
an observation for these inputs, not a cross-project or general determinism claim.

The complete creation/build invocation took 23.563 seconds and 20.719 seconds
in the two successful projects. Their peak aggregate job memory was 812,912,640
and 953,122,816 bytes respectively. Fresh-process reopen took 6.094 and 4.454
seconds. The final LF-normalized source was compiled and run again: creation/
build took 17.953 seconds and fresh-process reopen took 3.719 seconds, with the
same height measurements. These are measurements of the small fixture, not
larger terrain budgets.

The independent
[`verify_foa_heightmap_native_probe.py`](../../Gems/TaintedGrailModdingSDK/Tools/verify_foa_heightmap_native_probe.py)
checks exact source bytes, every float readback against source coordinates,
reported maxima, corners, extents, stage coverage, collision observations,
native inventories/hashes and all six false authority fields. Run:

```text
python Gems/TaintedGrailModdingSDK/Tools/verify_foa_heightmap_native_probe.py <private-output>
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p test_verify_foa_heightmap_native_probe.py -v
```

A fresh Unity process also rejected a deliberately malformed SDK-owned bundle
with exit code 1, a failed report and zero successful terrain observations.

The 19 auditor tests passed. They use invented report bytes to exercise rejection
of changed/truncated/nonfinite or flipped samples, source/bundle drift, missing
stages and collisions, duplicate JSON/inventories, future versions, excessive
reports, unsafe paths/hard links, altered tolerances, and false permission or
repeatability claims. Those unit fixtures are not native execution evidence.

The complete repository static lane passed: 1438 unit tests with 34 explicit
skips, ten tooling tests, and four sets of ten source-policy checks. Its first
attempt failed because the test environment lacked the already-pinned UnityPy
and lz4 dependencies. They were installed into a private validation directory,
and the complete lane was rerun successfully. No dependency pin changed.
O3DE configure/build and product UI interaction are NOT_APPLICABLE to this
source-only Unity test fixture and Python auditor; no C++, CMake or pane changed.

The versioned qualification report is test-owned candidate evidence; no existing
public contract, canonical V1 schema, persistent workspace format or M1/M2/M3
execution API changed. Private evidence retains command lines, process results,
compiler references, source hashes, Unity-generated metadata, artifact inventories,
logs, failed attempts and independent auditor results. Hashes do not prove that
an untrusted receipt is authentic or authorize a subsequent operation.

## Remaining M6 acceptance

| Lane | State |
| --- | --- |
| Fixed synthetic native terrain creation/readback | PASSED |
| Fresh-process bundle reopen and collider observations | PASSED in Unity Editor |
| Evidence auditor negative tests | PASSED |
| Canonical/Core import and handoff to native provider | NOT_RUN; fixture currently generates its own fixed source |
| Production M2 native process isolation and dependency closure | BLOCKED; current profile does not cover this invocation |
| Framework six-phase native terrain binding | NOT_RUN |
| Owned-target deployment and fresh-process recovery | NOT_RUN for terrain |
| Exact-install game load, observations and unload | NOT_RUN |
| End-to-end game rollback | NOT_RUN |

Next implementation must bind Core-validated terrain data to the native provider
and qualify the exact process/dependency boundary, including negative isolation,
cancellation and timeout proof. Do not replace that work with a direct Unity or
game subprocess fallback. Final game execution requires the reviewed package,
exact destination/side effects and rollback plan. Runtime sign-off not performed.
