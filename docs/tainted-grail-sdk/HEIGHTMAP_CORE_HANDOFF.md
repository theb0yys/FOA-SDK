# M6 terrain input and Framework BUILD preview

Status: PASSED for the bounded Core/Framework handoff and native authoring checks on 13 September 2026. Overall M6 remains PARTIAL.
World Authoring owns validation of the existing canonical terrain revision;
Framework owns the immutable request snapshot; Unity owns native materialisation.
This increment follows the [M6 design](HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md).

## Capturing a revision

`TerrainHeightmap::PrepareNativeTerrainBuildInput` reads a workspace-relative
manifest and requires the caller's exact expected document fingerprint and all
five profile binding fields. It uses the existing V1 parser and validation, then
requires the exact canonical JSON bytes. This stricter consumer rejects duplicate
fields and noncanonical whitespace; it does not change the general V1 parser.

The accepted conversion profile is `foa.heightmap.u16le-33.v1`: one 33 by 33
row-major U16LE tile, one-metre spacing, grid vertices, heights -2 to +6 metres,
right-handed +X east/+Y north/+Z up, north-first rows and identity transform.
Other geometry, transforms, stale revisions, changed/truncated tile bytes,
unsafe or linked paths and cancellation fail before producing an input. There
is no resampling. All canonical authority fields remain false.

Manifest reads are capped at 65,536 bytes and samples at 2,178 bytes. The function
captures validated metadata once, reads one bounded tile, verifies its digest,
and constructs the input in memory. It does not reread mutable metadata, allocate
the general terrain editing cache, write a file or start a process. The terrain
implementation and planner currently compile in `Framework.Static`; logical
terrain ownership is unchanged and no O3DE engine source is modified.

## Neutral packet V1

The additive packet contains no Unity-native objects and does not replace the
canonical V1 persistence format. Integers below are unsigned little-endian.

| Offset | Length | Value |
| --- | --- | --- |
| 0 | 8 | ASCII `FOAHM001` |
| 8 | 4 | Canonical UTF-8 document byte count, 1 through 65,536 |
| 12 | 4 | Sample byte count, exactly 2,178 |
| 16 | 64 | Lowercase ASCII SHA-256 hex of the document, without prefix |
| 80 | 64 | Lowercase ASCII SHA-256 hex of the samples, without prefix |
| 144 | Document count | Exact canonical document bytes |
| Following document | 2,178 | Original north-first U16LE samples |

Maximum total size is 67,858 bytes. Trailing bytes are forbidden. The complete
packet and document fingerprints use the existing `sha256:` notation. Digests
identify captured content; they do not prove authenticity, ownership or permission.
No package layout, dependency pin, canonical schema or persistent migration changes.

## Framework binding

`FrameworkPlannerService::BindTerrainBuild` accepts a valid, sealed request for
`capability.world.heightmap`, terminal phase BUILD, the exact profile fingerprint,
and no existing inputs, upstream preferences or `foa.planner.*` options. Workspace
and pack IDs remain caller-supplied request context; this method does not discover
or authorize a filesystem root from those IDs.

After terrain validation, it captures a short deterministic owner document under
`foa.planner.terrain-build.v1`. That document contains the conversion profile,
document/input fingerprints, byte count and `executionAllowed: false`. The existing
M1 phase-extension and option mechanism binds its digest into a resealed request;
M1 values contain no binary samples or private paths. The snapshot retains the
original neutral packet in memory and exposes it only through `ReadSource` for
the exact bound request, source kind and BUILD phase. Later workspace changes
cannot alter the captured bytes. A new preview rereads and revalidates the revision.

`TerrainBuild` is appended to the internal source-kind enum. Existing adapter build,
package and deployment planners keep their existing representations and behavior.
This is a BUILD preview integration, not a registered execution provider, six-phase
terrain workflow or reachable Editor command. It creates no execution permission.

## Native consumer and reproduction

The development qualification project must contain exactly these two SDK scripts
in `Assets/Editor`:

- `FoaHeightmapNativeProbe.cs`
- `FoaTerrainBuildInput.cs`

The C# reader verifies bounded framing, version, strict UTF-8, all digests and the
caller's independently supplied expected input and document fingerprints. The
canonical body remains opaque provenance: full semantic validation belongs to
the terrain service. It must receive the captured Framework binding, not hashes
inferred from an arbitrary untrusted packet. It rejects linked input paths and
bounds reads before allocating. No JSON package dependency is added.

Run the compiled `TerrainNativeInputTests.FrameworkBindsExactTerrainBytesWithoutMutatingTheRequest`
fixture with `FOA_M6_CORE_OUTPUT` set to an existing, empty, private directory
outside source checkouts. It imports only the SDK-owned asymmetric test pattern,
binds it through Framework, and exports `terrain-input.bin`, `binding.json`,
`framework-request.json` and `framework-source.json` using create-new writes.
Without this optional variable the tests leave no persistent export.

Use the bounded development launch procedure in the
[native qualification report](HEIGHTMAP_NATIVE_QUALIFICATION.md), replacing Run
and Reopen with `FoaHeightmapNativeProbe.RunCore` and
`FoaHeightmapNativeProbe.ReopenCore`. Set these process-local values from the
compiled export and its binding:

```text
FOA_HEIGHTMAP_INPUT=<private-export>/terrain-input.bin
FOA_HEIGHTMAP_INPUT_FINGERPRINT=<input_fingerprint from binding.json>
FOA_HEIGHTMAP_DOCUMENT_FINGERPRINT=<document_fingerprint from binding.json>
```

Audit using the independently retained binding:

```text
python Gems/TaintedGrailModdingSDK/Tools/verify_foa_heightmap_native_probe.py <private-native-output> --core-input <private-export>/terrain-input.bin --input-fingerprint <expected-input-fingerprint> --document-fingerprint <expected-document-fingerprint>
```

The auditor is intentionally specific to the SDK qualification pattern. Core-mode
reports require the independent packet and both expected fingerprints; the fixed
control cannot be reported as a Core run. Both native modes still compare all
1,089 heights, save and reopen native data, build twice and check collider results.
Core mode additionally executes twelve malformed/stale packet rejection cases.
The previous fixed-source mode remains available as a regression control.

## Measured local validation

The pinned O3DE host (68683f23fb747380d3efa2424bd5f30242e9c5a2) configured and
built the affected targets. The full compiled Catalog suite ran 592 tests:
590 passed and two existing Windows symlink tests skipped. All fifteen new terrain
and Framework cases passed; the controlled campaign-worker failure, timeout and
cancellation test also passed with the qualified Python runtime supplied.

The complete static lane ran 1446 unit tests with 34 explicit skips, ten tooling
tests, and four sets of ten source-policy checks. The 27 native auditor tests
passed. The first compiled attempt failed because the test fixture used an
unsupported `.u16le` import extension; changing the fixture to the existing `.u16`
route resolved it. A static invocation used the wrong runner path and did not run;
it was corrected. Both attempts remain in private evidence.

The final Framework export was consumed in a fresh Unity 6000.0.64f1 project.
Creation and two bundle builds took 21.953 seconds, with peak aggregate job memory
978,505,728 bytes. Fresh-process reopen took 3.938 seconds. Independent auditing
verified five complete 1089-sample readbacks and twenty collider observations;
maximum elevation error was 0.0001219765603557299 metres (about 0.122 mm), within
the predeclared tolerance. Both bundle builds had equal bytes within this project.
The fixed-source regression control also passed its complete workflow and audit.
No cross-project or general bundle determinism is claimed. Tested C# source hashes
match the submitted source bytes.

The complete native input fingerprint was
`sha256:ff74010e478698a4d859e56355dc723298213c11462635b376ce6b8c5549c8e0`;
its canonical source document fingerprint was
`sha256:b3820c7139012914fc327e08a087fa271256903a9bf26d7f44a8c89bf2f475a9`.
These identify the SDK-owned synthetic fixture, not a game-derived asset or runtime
permission. Generated artifacts, exact command lines and failed attempts remain
outside the source repository. Product Editor/UI interaction is NOT_APPLICABLE
to these backend and qualification changes; no pane or user command was added.

## Acceptance boundary

Compiled tests cover import, bounded preparation, stale/invalid input, cancellation,
Framework request drift and immutable captured bytes. The native run and fresh
process reopening establish Unity authoring behavior only. The independent audit
checks the exact input binding, all samples and native output inventories.

Production M2 qualification remains BLOCKED: the only existing LPAC profile cannot
represent this dependency/IO invocation. Its 64-file limit is smaller than the
247 installed managed compiler references observed in the native run. Full native
dependency closure, licensing IPC, unrelated filesystem/network denial, cancellation,
timeout and cleanup require their own operational qualification. Existing M2 limits
and isolation claims are unchanged; no unrestricted production fallback is added.

Terrain packaging, deployment, game loading, runtime observations, unload and
rollback are NOT_RUN. The game adapter and qualified launch path are still absent.
No game installation, save, campaign or licensing file was modified by this work.
Runtime sign-off not performed. See CURRENT_TASK for the current validated state.


The subsequent [native process isolation experiment](HEIGHTMAP_NATIVE_PROCESS_QUALIFICATION.md)
found a concrete licensing IPC failure in both tested container routes. An
ordinary-token control passed; production isolation remains BLOCKED. The report
records the exact negative observations and remaining environment decision.
