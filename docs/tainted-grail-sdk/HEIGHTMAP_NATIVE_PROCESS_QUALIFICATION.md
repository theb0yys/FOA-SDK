# M6 native process isolation qualification

Status: BLOCKED for production Unity execution as observed on 13 September 2026.
The [Core/Framework handoff](HEIGHTMAP_CORE_HANDOFF.md) and Unity authoring checks
remain valid, but neither a successful ordinary process nor a source fingerprint
qualifies production isolation. Overall M6 remains PARTIAL.

Primary owner: `external-toolchain`; consumer: World Authoring's native terrain
provider. This Critical/Runtime qualification follows the
[M6 design](HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md). Existing M2 V2 contracts,
limits and the `windows-lpac-registry-read-batch-v1` profile are unchanged.
No production provider, unrestricted fallback or game execution was registered.

## Exact local experiment

The tool was the previously measured Unity Editor 6000.0.64f1:

- Unity executable SHA-256: `3b1d93eefa2bbca789d99c83886266f13a58d5cbe5cfe8f2392a41cb45e11b05`.
- Adjacent Unity DLL SHA-256: `0a43985e3f46a33cd6c3705d21ef8b76f6eedbf88fa695e0bf9376eff33a8d8a`.

A private local copy contained 46,239 installed tool files and 7,780,302,461 bytes.
Enumeration rejected links and was bounded to 60,000 files, ten GiB total, one GiB
per file and twenty path components. Each file was hashed while copying. A later
full digest comparison found zero changed files in the private tool copy. This
complete installation copy is a diagnostic control, not a minimal dependency
closure or redistributable product payload. Copying took 323.234 seconds; the
complete postflight digest check took 254.593 seconds on this machine.

The Windows experiment created a new AppContainer identity for each attempt,
granted read/execute access only to the private tool copy, and granted writes to
disposable scratch/output/profile directories. It did not change installed tool
ACLs. The first harness used an owner-rights ACE; it was corrected to the exact
user SID used by M2's private-directory policy, and the experiment was repeated.
The initial timeout remains a failed attempt, not a qualification pass.

Unity was created suspended and assigned to a kill-on-close job before resume.
The job allowed at most 32 processes and four GiB aggregate memory. Startup had
a 60-second deadline and a sixteen-MiB log limit. The exact LPAC token was checked
before resume: package identity, one registryRead capability, low integrity and
effective refusal of an ALL APPLICATION PACKAGES-only resource. Scratch and
licensing observations belonged to the disposable attempt. No network capability
was granted. These test-harness checks do not replace the existing M2 supervisor.

The fixed invocation used batch mode, no graphics, disabled Package Manager,
four Unity workers, `BEE_BUILD_THREADS=4`, an explicit disposable create-project
path, explicit log path and quit. The initial environment contained only the
Windows root, private temp/profile paths and Bee worker setting. An additional
comparison supplied the current username explicitly; it did not resolve startup.
Private paths, identities, tool inventories and raw logs remain outside source.

## Observed outcomes

| Attempt | Token and change | Outcome |
| --- | --- | --- |
| Corrected LPAC bootstrap | Exact package, registryRead, low integrity and ALL APPLICATION PACKAGES exclusion verified | FAILED: startup deadline; peak job memory 443,650,048 bytes |
| Matched ordinary-token control | Same tool copy, minimal environment, arguments and job bounds; no AppContainer | PASSED for startup only in 6.594 seconds; peak job memory 521,277,440 bytes |
| Standard AppContainer comparison | Package/capability/low-integrity checks; ALL APPLICATION PACKAGES access permitted | FAILED: startup deadline |
| Standard AppContainer with username | Added explicit username; observed owned children and final job state | FAILED: licensing did not connect; job empty after cleanup |
| Final LPAC with username | Exact LPAC checks; observed owned children and bounded redacted licensing errors | FAILED: startup deadline; peak job memory 443,482,112 bytes; job empty after cleanup |

The ordinary control connected to the licensing client, resolved entitlements and
finished project creation. The isolated attempts launched the licensing client
but did not connect successfully. The final licensing diagnostics reported:

- `System.UnauthorizedAccessException` while creating licensing/notification named-pipe servers;
- `System.Net.Sockets.SocketException` with error 10013 for attempted HTTPS requests;
- the Editor still waiting when the startup deadline expired.

The final LPAC attempt tracked the owned Editor, licensing client and console host.
All were terminated through their job; an independent job query confirmed zero
active processes before the temporary AppContainer profile was removed. The
standard AppContainer attempt also observed its crash handler and confirmed empty
job cleanup. Earlier attempts did not record this full cleanup observation and
are not substituted for these later checks.

These observations establish a failure of the tested local execution routes.
They do not establish that every possible Unity isolation environment is
unsupported. An AppContainer timeout also does not prove that increasing timeout
or granting an arbitrary capability would make conversion safe.

## IPC compatibility finding and permanent regression

Microsoft documents the additional capability restrictions on
[less-privileged AppContainers](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer)
and the `LOCAL` namespace requirement for
[named pipes within AppContainers](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe).
The observed licensing pipe failure is consistent with those restrictions; the
precise vendor-side repair is not established by that documentation. Unity's
[licensing troubleshooting guide](https://docs.unity.com/en-us/licensing-server/troubleshooting-client)
identifies the client log and pipe-server failure category. None of these sources
provides an approved transparent licensing broker for this SDK.

The SDK-owned M2 fixture now exercises a separate `pipe-namespace` mode. In the
actual M2 supervisor it requires global named-pipe creation to fail specifically
with access denied, then creates a unique `LOCAL` pipe and verifies a byte
round-trip. Its operational test also checks the existing profile fingerprint,
verified output and completed cleanup. It neither contacts Unity licensing nor
creates a foreign/global service endpoint. The outer supervisor bounds any failed
or stalled IPC operation.

## Validation of the permanent regression

The pinned O3DE build compiled `ExternalToolchain.Execution.Fixture` and
`ExternalToolchain.Execution.Operational.Tests`. The complete operational suite
passed all 22 tests with no skips in 12.953 seconds. This includes the new global
pipe denial / LOCAL round-trip and the existing network refusal, outside-write
refusal, child cleanup, deadlines, cancellation, memory ceiling, drift, journaling
and recovery checks. The fixture remains test-only and is not linked into the
Editor or runtime adapter. Production source, public APIs and V2 serialization
are unchanged; no migration or new dependency is required.

```text
cmake --build <pinned-build> --config profile --target ExternalToolchain.Execution.Fixture --parallel 2 -- /p:BuildProjectReferences=false
cmake --build <pinned-build> --config profile --target ExternalToolchain.Execution.Operational.Tests --parallel 2 -- /p:BuildProjectReferences=false
FOA_M2_FIXTURE=<pinned-bin>/ExternalToolchain.Execution.Fixture.exe
python Gems/TaintedGrailModdingSDK/Tools/run_aztest_without_gtest_xml.py <pinned-bin>/AzTestRunner.exe <pinned-bin>/ExternalToolchain.Execution.Operational.Tests.dll AzRunUnitTests
```

The complete static lane also passed: 1446 tests with 34 explicit skips, ten
tooling tests and four sets of ten pinned-engine source-policy checks.

Those M2 operational passes prove the existing profile's tested boundary. They
do not turn the failed native Unity attempts into a successful production
qualification. Product UI, game deployment, runtime and release lanes remain
NOT_RUN or NOT_APPLICABLE to this test/report increment.

## Remaining execution decision

A supported licensing boundary must be established before creating a production
Unity profile: either a separately licensed isolated worker/VM, or a documented
local route that passes equivalent filesystem/network/IPC denial, cancellation,
timeout, process-tree cleanup and artifact verification tests. Worker availability
and the requirement to keep conversion on this PC were requested from the owner.
No worker was selected or provisioned and no credentials were requested or copied.

Do not bypass AppContainer checks, patch the vendor licensing client, forward
unknown licensing RPCs through an unrestricted broker, or treat a successful
ordinary-token control as production qualification. A larger file budget alone
cannot repair the observed IPC/licensing incompatibility.

Terrain execution providers, package/deployment, game loading/observations/unload
and verified rollback remain NOT_RUN. Their preparation cannot be reported as a
completed six-phase runtime workflow. No game files, saves or campaign assets were
touched. Runtime sign-off not performed.
