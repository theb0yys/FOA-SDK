# Tool Execution Service M2: accepted implementation decision

Status: ACCEPTED by the repository owner on 12 September 2026: "Approve this M2 scope".
Decision owner: @theb0yys.
Prepared: 12 September 2026.
Primary owner: `external-toolchain`.
Design change classification: Significant. Subsequent process implementation: Critical/Runtime.
Base: `66549462b3`, containing the merged M1 Core contracts from PR #263.

## Accepted decision

Implement M2 as a host-owned, asynchronous Windows batch-process service with a
separate V2 invocation API, explicit admission, process-tree supervision, bounded
diagnostics, verified staging outputs and durable invocation observations.

The first supported execution profile is `windows-lpac-registry-read-batch-v1`. It uses a
Less Privileged AppContainer (LPAC) for resource isolation and a Job Object for
process lifetime. Unsupported platforms or unavailable isolation return a typed
refusal before running provider code. There is no unsandboxed fallback.

This is the M2 decision requested by the
[post-M1 boundary](CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md#post-m1-boundary).
The owner accepted this exact scope, including the stated staging-volume disk
limit, on 12 September 2026. This authorises bounded implementation and synthetic
proof described
here. Normal branch, PR and maintainer integration rules still apply.

M2 is complete only when the same production supervisor passes the operational
cases below. Contract-only code or a service which always refuses is partial.
M3 supplies production Framework admission and orchestration; M2's Editor-facing
service remains disabled by default, with a deny-all admission gate until that
separate owner integration exists.

## Scope and ownership

| Surface | Owner and responsibility |
| --- | --- |
| Provider/command identity, discovery and process execution | ExternalToolchain; discovery never grants execution |
| Immutable invocation request, status, logs and invocation record | ExternalToolchain; observations about one tool attempt |
| Admission, qualification, policy and human authorisation | Foundation/Framework; M2 consumes a trusted gate, never invents an affirmative decision |
| Staging directory and private journal custody | ExternalToolchain; no pack ownership or installation ownership ledger |
| M1 phase/execution receipts and artifact promotion | Their existing owners; read-only architectural boundary, no M2 writer |
| Editor system component | Connects the service and shuts it down; no launch button or provider-specific UI |
| Synthetic executable and operational harness | Test harness; exercises the real service and Windows backend |

In scope: native Windows batch executables using the declared argument convention;
staging-only file output; start, status, cancel and bounded log retrieval;
provider/target exclusion; safe recovery of invocation observations.

Out of scope: a shell or script interpreter route, interactive tools, installing
applications, downloads, secret use, arbitrary IPC, provider hot-loading, tool
qualification, asset import, Unity/Blender/game execution, deployment, saves,
release/signing/publication, Framework orchestration and the M5 full pipeline.
No existing game, save, tool installation or pinned engine file may be modified.
Synthetic fixtures contain only repository-owned source and generated test data.

## Inspected evidence and design consequence

The controlling [Capability Execution Contract](CAPABILITY_EXECUTION_CONTRACT.md)
requires M0 before source implementation and defines M2's execution API, V2
contracts, supervision, cancellation/status and durable records.

The existing [ExternalToolchain architecture](../../Gems/ExternalToolchain/docs/ARCHITECTURE.md)
suggests `AzFramework::ProcessWatcher` for future supervision. Inspection of
O3DE pin `68683f23fb747380d3efa2424bd5f30242e9c5a2` found:

- `ProcessWatcher_Win.cpp`, `ProcessLauncher::LaunchProcess`: job setup can fail
  and the process still launches; failed job assignment closes the job and
  resumes the process. That does not provide M2's fail-closed containment.
- `ProcessLaunchInfo::GetCommandLineParametersAsString`: the vector path joins
  arguments with spaces without wrapping every argument, so an argument
  containing spaces or an empty argument can lose its boundary.
- `ProcessWatcher.h`: no public per-launch security-capabilities or inherited
  handle allowlist contract is exposed.

M2 therefore uses a private Windows backend in the product Gem, without
editing the engine or using the pinned launcher's fallback behavior. Acceptance
of this decision supersedes only the earlier suggested ProcessWatcher choice.
It does not weaken the shared supervisor requirements.

Microsoft documents Job Objects as process-group lifetime/resource management;
they do not replace per-process security controls. See
[Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
LPAC adds explicit resource access restrictions; profile creation and process
security attributes have their own lifecycle. See
[Launch an AppContainer](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer).
These establish API design context, not proof that the proposed FOA-SDK backend
works. Its isolation must be tested on the exact Windows host.

## Additive API and compatibility

Keep the registration/discovery API at `HostApiVersion{1,1,0}`, its bus, provider
descriptors and status meanings unchanged. Add an independent
`ToolExecutionApiVersion{2,0,0}` and a separate execution bus.

Accepted public family:

- `ToolExecutionCommandV2`: exact provider ID/version and command ID, configured
  discovery probe identity, batch mode, argument convention, permitted input
  and output kinds, execution profile and hard resource limits.
- `ToolInvocationRequestV2`: attempt ID, immutable request fingerprint, exact
  execution-command binding, ordered typed arguments, input references,
  expected output declarations, environment declarations and deadlines.
- `ToolInvocationStatusV2`: monotonically increasing observation sequence,
  lifecycle stage, independent process outcome, output verification, cleanup
  and persistence states, safe diagnostic codes.
- `ToolOutputManifestV2`: invocation/request binding and bounded output entries
  with stable ID, relative path, kind, byte count and SHA-256.
- `ToolInvocationRecordV2`: request and execution-profile fingerprints, observed
  executable digest, timestamps, process identity/exit code when observed,
  terminal states, accepted output observations and redacted log references.

Use contract IDs `foa-tool-invocation-v2`, `foa-tool-output-manifest-v2` and
`foa-tool-invocation-record-v2`; version is exactly 2. Canonical request identity
has profile `foa-tool-invocation-canonical-json-v2`. Fix canonical keys/order and
fixtures in the implementation review. Sort sets without mutating input,
preserve argument order, reject duplicates/unknown versions, exclude the
object's own fingerprint, and separate an attempt ID from reusable semantic
request identity. Reject unknown JSON properties and duplicate keys.

Path arguments are typed root-ID/relative-path references, resolved only by the
host. Literal arguments are bounded non-sensitive data; absolute machine paths
must use typed references. No request or journal field accepts a raw shell
command, a secret value, an OS handle or arbitrary JSON extensions. An explicit
secret-reference request is refused with `SecretUseUnsupported` in this profile.

Existing ExternalToolHandoff V1, Unity interchange, M1 Core canonical values,
workspace/pack formats and deployment/adapter/release contracts remain unchanged.
Do not deserialize any V1 record into an executable request. ExternalToolchain
does not depend on TaintedGrailModdingSDK Core or Framework. A future M3 adapter
can reference M2 observations without M2 manufacturing a generic phase receipt.

## Admission and service lifecycle

The execution bus offers execution-command registration, `Submit`, `GetStatus`,
`Cancel`, `ReadLog` and bounded record enumeration. Execution registration
closes with the existing registration lifecycle and must match an existing
provider's exact ID/version/command. Old providers acquire no executable route.

A host-composed internal admission interface receives the exact request,
command, executable identity and resolved root identities. It returns an
opaque, one-use lease bound to their fingerprints, expiry and host instance.
There is no public bus method or Settings Registry boolean that mints a lease.
The production default implementation denies. Synthetic tests inject a
fixture-only gate which can authorise only the fixture executable digest and
new test roots; it is never linked into Editor targets.

Submit copies and validates bounded input, reserves a queue slot and returns
promptly. A worker acquires provider and target locks in deterministic order,
resolves discovery, validates filesystem identity, checks admission, persists
the accepted attempt, creates isolation and launches. Admission is rechecked
after locking and before resume. Changed configuration, executable, input,
grant expiry, duplicate active ID or conflicting root causes refusal.
A persisted request is never permission to replay.

Lifecycle stages are `Queued -> Preparing -> Running -> Draining ->
Verifying -> Finalizing -> Terminal`, with refusal/cancel/failure paths.
Process outcome, verification, cleanup and persistence are separate enums.
Exit code zero alone cannot yield overall success. Success requires all job
processes stopped, required outputs verified, cleanup complete and the terminal
record flushed. After root exit, the job has at most 100 ms to drain naturally,
within the existing five-second cleanup deadline. Remaining processes are then
terminated and recorded as an unsuccessful descendant-cleanup outcome. Timeout,
cancellation and execution errors request termination immediately. This avoids
misclassifying a briefly surviving Windows console host as a failed tool.
Cleanup or journal failure retains the original process outcome
and adds its own failure; it never rewrites a failed run into success.

Cancellation is idempotent and observable. A queued attempt is cancelled without
launch; a running attempt requests Job Object termination. Timeout uses a
monotonic clock and follows the same cleanup path. Once the worker has observed
process completion, a later cancel cannot retroactively change its outcome.
A single serialised state transition decides races and tests both orderings.

Shutdown rejects new submissions, cancels queued/running attempts, drains and
joins workers, finalises observations where possible and closes all host-owned
handles. No detached thread or callback may survive component deactivation.
An unconfirmed cleanup is reported explicitly and keeps its target unavailable
for this host session.

## Windows process and resource boundary

Required backend behavior:

1. Accept a configured absolute local native executable whose digest and file
   identity match the admission lease. Keep the checked executable open without
   write/delete sharing through creation. Verify its containing path components
   without following reparses; no executable/PATH search or shell association.
2. Build an explicit UTF-16 argument line from a vector, preserving empty
   arguments, whitespace, quotes, Unicode and trailing backslashes under
   `windows-msvc-argv-v1`. Other parsing conventions are unsupported.
   [Microsoft's argument rules](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170)
   are the interoperability reference; the native fixture verifies round trips.
3. Supply an explicit working directory and complete allowlisted environment.
   Never inherit the Editor environment. Host-generated system/scratch variables
   are individually declared; PATH, COMSPEC, loader overrides and provider-chosen
   temporary directories are absent. Sensitive configuration is not resolved
   into this profile. Close stdin and expose only stdout/stderr handles.
4. Create a unique per-attempt LPAC with no network, COM or secret capabilities.
   Grant read/execute only on staged fixture/tool files and read-only input
   snapshots; grant write access only to that attempt's output and scratch roots.
   Change ACLs only on newly created service-owned staging objects, never on
   source installations, inputs or user directories.
5. Record the OS-created profile directory and profile identity as additional
   declared private scratch resources. They are not artifact outputs. LPAC
   profile storage is not silently excluded from the write-boundary claim.
   The host journal is outside every child-accessible writable root.
6. Set up the job and security/handle attributes before creation. Create hidden
   and suspended; check every containment call; assign to the job and verify
   isolation before resume. Disable breakaway and set kill-on-last-job-close.
   Any setup or assignment failure terminates the suspended child and reports
   failure; no partial setup is accepted.
7. Drain both pipes concurrently. Observe root exit and zero active job processes
   separately. A descendant surviving the bounded natural drain is terminated and makes
   cleanup observable; it is not silently detached.
8. Remove only the recorded, owned profile/scratch resources after confirmed job
   completion. Failed profile deletion is a cleanup failure and recoverable
   retained resource. Never reuse an existing profile by name after collision.

The backend uses explicit application name, Unicode environment and extended
startup attributes. See [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
[UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)
and [DeleteAppContainerProfile](https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-deleteappcontainerprofile).
No broad capabilities or OS fallback may be added to make an incompatible tool
start. An unavailable LPAC prerequisite is `IsolationUnavailable`.

Threat model: malformed providers and child output, accidental or deliberate
child attempts to escape staging, hanging/forking/flooding children, interrupted
hosts, corrupt journals, stale approvals and filesystem aliases. The OS kernel
and host composition are trusted; an administrator or injected in-process
malware is outside this isolation claim. AppContainer is not a virtual machine.
The exact profile must prove denied file/network access with synthetic sentinels
before it is advertised as supported.

## Filesystem, manifests and locks

Resolve roots from host configuration, never provider-selected deployment paths.
Use fresh invocation staging on a local fixed volume supporting the required
security semantics. Reject UNC/mapped network roots, device paths, alternate
streams, traversal, reserved names, hard-link aliases and any reparse component.
Hold directory identities during preparation and verify handle-resolved paths
again before reading, copying, hashing or removing anything. Paths are never
validated only by a string prefix.

Copy approved regular-file inputs into read-only snapshots, under the input
byte/count limit and matching the declared digests. Copy approved executable
and declared companion files into a separate read-only tool subtree; executable
digest verification occurs on the launched copy too. Tool redistribution is
not implied by staging; M2 proof uses only the synthetic native fixture.

Output verification occurs after every child has stopped. Require a manifest
bound to this invocation and request; enforce exact expected IDs, relative paths,
kinds, sizes and computed digests. Reject missing/extra outputs, case aliases,
links/reparses, duplicate entries and malformed/truncated/oversized manifests.
Enumerate only the attempt's output root under a fixed entry/depth budget.
Files in declared scratch are discarded as scratch, never promoted to artifacts.

Provider exclusion and target-root identity locks cover preparation through
verification and cleanup. Persisted journals have an exclusive host lock so
two Editor processes cannot share one store. Lock conflicting target roots
across host instances using exclusive host-owned lock files and canonical
volume/file identities; include overlaps and aliases in negative tests.
No busy wait, implicit retry or arbitrary unbounded queue.

## Logs, limits and persistence

All numbers below are accepted host validation or OS resource ceilings for this profile, not
measured performance results. A request may lower them; raising them requires
a reviewed profile change.

| Resource | Ceiling |
| --- | --- |
| Concurrent attempts / queued attempts | 2 / 16; one active attempt per provider |
| Arguments / encoded command line | 128 / 16,384 UTF-16 code units including executable and terminator |
| Environment entries / aggregate encoded environment | 32 / 16,384 UTF-16 code units |
| Input/output/companion file entries | 64 each; 1 GiB accepted aggregate across staged inputs, companions and outputs |
| Manifest / journal record | 256 KiB / 256 KiB |
| Output-tree entries / depth | 1,024 / 16 |
| Process count / committed job memory | 8 / 1 GiB |
| Execution timeout / cleanup deadline | 1,800 s / 5 s |
| Captured stdout / stderr | 256 KiB each after redaction |
| Total pipe bytes drained | 64 MiB; exceeding it cancels with OutputLimitExceeded |
| ReadLog page | 16 KiB; monotonic cursor and explicit truncation |
| Retained records / journal bytes / captured log bytes | 1,024 / 256 MiB / 512 MiB; a full store refuses new attempts |
| Submit/GetStatus/Cancel target | p95 <= 10 ms on the recorded test host |

The artifact byte ceiling is an input/output acceptance limit, not an OS disk
quota. This profile cannot guarantee a hard disk-consumption bound for a child
while it runs. Bounded periodic staging checks can request cancellation on
observed excess or low free space, but detection can lag. Disk exhaustion must
fail journal/output operations honestly and trigger cleanup. Acceptance of M0
must include this residual staging-volume risk; if a hard per-attempt disk quota
is required, an isolation/storage amendment is needed before implementation.
Do not describe periodic checks or post-run hashing as quota enforcement.

Redaction occurs before storage or notifications and carries overlap between
pipe chunks. Mask configured sensitive markers and resolved machine-path
variants; strip terminal control sequences; replace invalid text safely.
Drop further captured text after the cap while continuing bounded draining.
The profile has no secrets and does not promise to recognise arbitrary secrets
invented by the child. Never persist a raw environment, resolved command line,
absolute machine paths or unredacted child logs.

Store observations in a host-private per-user directory, outside source,
workspace packs, game data, the Asset Processor cache and child write roots.
The store uses an exclusive writer, create-new attempt IDs and versioned
snapshots committed through a flushed temporary file and same-volume atomic
replacement. Readers retain the last valid snapshot if a later write is corrupt.
Unknown versions are refused; no automatic migration or execution replay.

Maintain a distinct private recovery inventory for exact staging/profile
ownership and resolved paths. It is not part of shareable canonical receipts,
is never returned by the execution bus and is never uploaded as a fixture.
Persist recovery intent before creating external resources, then their observed
identities. Do not use PID alone to identify a process or cleanup target.

After a crash, nonterminal attempts become `Interrupted`; inspect only recorded
owned resources and never assume cleanup succeeded or relaunch an old request.
A malformed record is quarantined without following paths from it. Automatic
cleanup is limited to ownership/identity-proven service-created resources.
Ambiguous leftovers remain retained and block that target. Disk-full, denied
journal writes and recovery failures have explicit outcomes. No receipt hash
claims authenticity or grants permission.

## Exact accepted implementation paths

The accepted decision permits the following focused M2 source review unit;
any additional path needs a scoped amendment before editing it.

New public API and execution implementation:

```text
Gems/ExternalToolchain/Code/Include/ExternalToolchain/ToolExecutionTypes.h
Gems/ExternalToolchain/Code/Include/ExternalToolchain/ToolExecutionBus.h
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionContracts.cpp
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionService.h
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionService.cpp
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionAdmission.h
Gems/ExternalToolchain/Code/Source/Execution/ToolProcessBackend.h
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionJournal.h
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionJournal.cpp
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionRedactor.h
Gems/ExternalToolchain/Code/Source/Execution/ToolExecutionRedactor.cpp
Gems/ExternalToolchain/Code/Source/Execution/Platform/Windows/ToolProcessBackend_Windows.cpp
Gems/ExternalToolchain/Code/Source/Execution/Platform/Windows/ToolSandbox_Windows.h
Gems/ExternalToolchain/Code/Source/Execution/Platform/Windows/ToolSandbox_Windows.cpp
Gems/ExternalToolchain/Code/Source/Execution/Platform/Common/ToolProcessBackend_Unsupported.cpp
```

Build ownership and host lifecycle:

```text
Gems/ExternalToolchain/Code/CMakeLists.txt
Gems/ExternalToolchain/Code/externaltoolchain_api_files.cmake
Gems/ExternalToolchain/Code/externaltoolchain_execution_core_files.cmake
Gems/ExternalToolchain/Code/externaltoolchain_execution_host_files.cmake
Gems/ExternalToolchain/Code/externaltoolchain_execution_tests_files.cmake
Gems/ExternalToolchain/Code/externaltoolchain_execution_fixture_files.cmake
Gems/ExternalToolchain/Code/Source/Editor/ExternalToolchainEditorSystemComponent.h
Gems/ExternalToolchain/Code/Source/Editor/ExternalToolchainEditorSystemComponent.cpp
```

Tests and read-only validation integration:

```text
Gems/ExternalToolchain/Code/Tests/ToolExecutionContractTests.cpp
Gems/ExternalToolchain/Code/Tests/ToolExecutionServiceTests.cpp
Gems/ExternalToolchain/Code/Tests/ToolExecutionJournalTests.cpp
Gems/ExternalToolchain/Code/Tests/ToolExecutionOperationalTests.cpp
Gems/ExternalToolchain/Code/Tests/ToolExecutionFixture.cpp
Gems/ExternalToolchain/Tools/validate_external_toolchain_foundation.py
Gems/ExternalToolchain/Tools/validate_tool_execution.py
Gems/ExternalToolchain/Tools/tests/test_validate_tool_execution.py
Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py
Gems/TaintedGrailModdingSDK/Tools/validate_ci_runner_policy.py
.github/workflows/tainted-grail-sdk-pr-validation.yml
```

Documentation:

```text
CURRENT_TASK.md
CHANGELOG.md
Gems/ExternalToolchain/README.md
Gems/ExternalToolchain/docs/ARCHITECTURE.md
docs/tainted-grail-sdk/TOOL_EXECUTION_M2_DESIGN.md
docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md
docs/tainted-grail-sdk/DATA_FORMATS.md
docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md
docs/tainted-grail-sdk/README.md
```

The existing discovery settings, provider bus/types, V1 files, engine
pin, project/workspace formats and installer/runtime paths are read-only.
Execution remains disabled in existing Settings Registry defaults.

The legacy ExternalToolchain foundation validator currently reads a removed
in-checkout engine.json and fails in the product-only checkout before reaching
its contract checks. Its narrowly allowed repair is to use the existing
product/project registration authority; retain every discovery, API-version and
disabled-execution assertion. Add negative coverage in the listed validator test
file. This is a baseline harness defect, not a failing M2 implementation.

Add `ExternalToolchain.Execution.Core.Static` for pure V2 contracts, depending
only on the public API/AzCore; `ExternalToolchain.Execution.Host.Static` owns
service/journal/backend and platform system libraries. Existing discovery Core
remains process-free. Editor.Private links the execution host. The two test
targets `ExternalToolchain.Execution.Tests` and
`ExternalToolchain.Execution.Operational.Tests` link production objects once;
`ExternalToolchain.Execution.Fixture` is a test-only native executable.
Reuse the existing test Main. No test gate or fixture is linked into Editor.
No new third-party package or upstream engine edit is proposed.

## Acceptance tests and evidence

| Layer | Required proof for implementation |
| --- | --- |
| L0 static | Exact path audit, dependency/source policy, V1/M1 unchanged guards, strict V2 validator and negative validator tests |
| L1 contracts/service | Canonical round trips; at/over limits; stale/expired/mismatched admission; cancellation races; queue/lock behavior; split-chunk redaction; fault-injected cleanup and journal failures |
| L2 exact pinned host | Configure/build both new libraries, both test targets, fixture, ExternalToolchain Editor, SDK Editor and Editor executable; run new tests and existing ExternalToolchain/M1/interchange regressions |
| L3 Editor lifecycle | Start/close Editor with service active but execution disabled; no default launch, hang, leaked callback or worker; UI interaction is otherwise not applicable |
| L4 synthetic process operations | Real fixture through production backend: exact argv/environment/cwd, exit 0/nonzero, crash, timeout, cancellation, stdout/stderr flood, descendant survival, host crash, containment refusal, resource limits, verified output and durable recovery |
| L4 isolation negatives | Child denied writes to outside sentinel/read-only input/journal; denied network; forbidden inherited handle absent; reparse/hard-link/alias rejection; failed job/LPAC setup prevents fixture marker; profile cleanup and recovery observed |
| L4 concurrency/persistence | Two hosts contend for same provider/target; overlapping roots and aliases; record truncation/disk-full/write denial; terminal receipt failure cannot become success; restart never replays |
| Performance | Record host/OS/build and measured API p95, launch/cancel latency, pipe drain limits, memory/handle/worker counts and bounded retained disk use |
| Game/deployment/release | NOT_APPLICABLE; no Fall of Avalon runtime sign-off |

Do not skip an isolation or process-tree test and call M2 complete. Synthetic
filesystem/ACL/network probes must use newly created fixture resources only.
Their private logs and OS-created profile paths stay outside the repository.
The read-only CI job must require the native fixture and nonzero matching tests;
unsupported workers report BLOCKED, not a passing no-op.

M2 requires an applicability-based receipt for this head. Do not reuse M1's
Windows UI exclusion: M2 changes Editor lifetime and executes processes.
If the existing receipt schema cannot represent these required operational rows,
stop and propose the exact harness amendment; do not edit receipt policy silently.

## Current validation state and next action

Implementation status: IMPLEMENTED. The owner approved the original scope and
amendment B, then requested a commit and pull request on 12 September 2026.
The V2 API, supervisor, admission boundary, Windows backend, private journal,
fixture, tests and Editor lifecycle integration now exist. Configure and the
changed host/Editor target builds passed at the locked engine revision.

The adopted production profile contains exactly registryRead. The original
zero-capability baseline failed Winsock initialization (10107) and fixture child
creation (5) on Windows 11 Home build 26200. Amendment A's corrected experimental
profile passed all 21 native cases in three consecutive runs. Amendment B adopts
that capability under a distinct production profile identity, with command and
request fingerprint rejection for the old identity. Its final production-source
validation belongs to the exact-head receipt and PR, separate from those earlier
experimental results.

A bounded 100 ms natural job-drain period fixes a separate completion race:
job-scoped diagnostics identified conhost.exe briefly surviving the tool's exit.
The native regression runs 16 rapid successful invocations; persistent
descendants still require termination and prevent success. The child case also
requires exit zero and the fixture's actual successful child-creation marker.
No incidental console-host termination can substitute for that proof.

Initial Editor probes exposed an absent cache and an incomplete shader cache.
The same-volume materialized project and completed asset preflight resolved
startup. Normal quit exposed the pinned Windows Editor's fast termination before
component deactivation. M2 joins on Qt aboutToQuit as well as deactivation,
disconnecting its callback when the component stops. The process trace observed
activation, joined shutdown and exit zero; this is lifecycle evidence only.

The host verifies effective LPAC isolation with successful token queries for
AppContainer identity, exact package SID, exactly one enabled registryRead
capability and low integrity,
then successful AccessCheck calls against in-memory descriptors: its own package
identity grants access, while ALL APPLICATION PACKAGES does not. A native
negative control rejects ordinary AppContainer tokens, the wrong package SID,
and missing, unknown or extra capabilities. The test independently derives the
registryRead SID using the Windows API before comparing it with the production
constant. Rejected control processes remain suspended and are never resumed.
This avoids relying on TokenIsLessPrivilegedAppContainer, which returns invalid
information class on the tested host. An API error is never treated as denial.

Provider and overlapping-target exclusion uses named kernel mutexes derived
from provider identity and pinned directory identities. Including ancestor
identities conservatively serializes targets on the same volume; the two-worker
ceiling is not a claim that such targets run concurrently. This provides the
accepted exclusion/lifetime behavior without persistent per-target lock files.
The private journal still has an exclusive writer lock file.

Next action: submit the implemented M2 scope and exact-head evidence for
maintainer review. A subsequent milestone requires a new owner instruction.
If LPAC needs broader privileges/capabilities, file mutations outside the named
roots, a new dependency, a different platform design or extra source paths,
return with that specific amendment. Do not relax isolation or extend to M3.

## Amendment A: bounded registry-read compatibility experiment

Status: PASSED. The owner accepted the fixture-only experiment ("YES") on
12 September 2026. All 21 native cases passed in three consecutive experimental
runs after the bounded-drain correction. The zero-capability source and native
binaries were restored afterward; the Editor binaries were unchanged by the
experiment. This does not approve adoption into the production profile.

Authorize a fixture-only comparison between the existing zero-capability LPAC
and an LPAC with exactly the Windows `registryRead` capability. The purpose is
to determine whether common Windows registry access enables Winsock
initialization and native child creation on this host. Microsoft documents that
LPAC needs this capability for common registry access in
[Launch an AppContainer](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer).
That is a compatibility hypothesis, not proof that it resolves both failures.

The experiment used the existing approved Windows sandbox, backend and
fixture/test files only. It retained low integrity, exact package identity,
ALL APPLICATION PACKAGES opt-out, Job Object limits, inherited-handle allowlist,
staging-only writes and no network or COM capabilities. Registry access was
limited by Windows ACLs for that capability; the probe did not collect or log
registry contents. No ACLs on existing files or registry keys changed.
Only repository-owned native fixture executables and new temporary roots ran. Production Editor admission remains disabled.

Required observations: exact token capabilities, successful network API
initialization followed by explicit network denial, actual descendant creation
followed by process-tree cleanup, unchanged outside/input/journal write denial,
no unexpected inherited handle, and profile/resource cleanup. Record API errors
as errors, not isolation passes. If one extra capability is insufficient, retain
the failed result and request a new specific amendment. Adopting an amended
production execution profile remains a separate explicit decision informed by
this experiment; no capability is added by this document alone.


Observed result: the experimental token had exactly one enabled registryRead
capability, the expected package SID and low integrity. Effective-access controls
rejected ordinary AppContainer, zero-capability and wrong-package alternatives.
WSAStartup returned zero and the network probe received WSAEACCES (10013).
The fixture created a child, the supervisor terminated surviving processes, and
cleanup completed. Outside/input/journal write, inherited-handle, memory,
cancellation, crash, restart, quarantine and record tests all passed in each of
the three final runs. The initial failing runs and diagnostic-build errors remain
in private evidence and are not discarded. These results apply to the tested
host and exact experimental artifacts, not an unexecuted production profile.

## Amendment B: adopt the verified registry-read batch profile

Status: ACCEPTED by the owner on 12 September 2026: "YES THEN COOMIT ALL
CHANGES AND OPWN A PR". This separately authorizes production adoption of the
tested registryRead profile, a DCO-signed commit and a pull request. It does not
authorize merge, deployment or any additional capability.

- Replace the zero-capability profile identity with
  windows-lpac-registry-read-batch-v1. Keep the public execution API at 2.0.0 and
  discovery at 1.1.0; update command/request/record fingerprint expectations.
  Old profile requests must refuse rather than silently gain a capability.
- Permit exactly the OS-derived registryRead capability SID with its enabled
  attribute. Require that exact set during suspended-process token verification;
  unknown, missing or extra capabilities refuse. No network or COM capability.
- Retain low integrity, package-identity checks, ALL APPLICATION PACKAGES opt-out,
  explicit handle inheritance, staged writes, Job limits, private records,
  explicit admission and default-disabled Editor execution.
- Use only the existing 43 approved paths. Update profile documentation and
  rejection tests, then rerun static, compiled, native and Editor lifecycle gates
  against the adopted source before any completion or handoff claim.

The capability allows access to registry resources whose Windows ACLs grant it;
it is broader than the original empty capability set. This proposal adds no registry-content collection, and grants no game, deployment
or release authority.
