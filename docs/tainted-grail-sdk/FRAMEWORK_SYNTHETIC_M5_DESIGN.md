# M5 isolated execution workflow

The owner requested the isolated synthetic spine. Classification: Critical/Runtime
for native process and file transactions; no game runtime authority is claimed.
Primary owner: capability-execution. The deployment provider owns target writes.

## Scope and threat boundary

Run BUILD, PACKAGE, DEPLOY, LAUNCH, VERIFY and ROLLBACK through the existing
Framework and M2 supervisor. A dedicated harmless native provider transforms a
small synthetic document, packages it, unpacks it, interprets the deployed document
and independently checks its result. The host deployment provider owns exactly one
payload in an exclusively created private target, one immutable backup and a canary.
No caller-supplied installation path or arbitrary mutation list is accepted.

M3 staging remains the default. The later phases require an explicit synthetic
target instance, exact synthetic capability/profile and exact bounded mutation.
All process launches retain M2's existing LPAC profile. Children receive immutable
input copies and staging outputs, never write access to the target. Deployment
applies verified output bytes in the host; launch and verification consume copies
hashed from the actual target. Verification is a separate supervised invocation.

Preview binds preimage, owner, backup, desired bytes and inverse operation using
unchanged M1 values. Live qualification, policy and confirmation remain required, including a fresh check immediately before the host deployment write.
Target writes pin ancestor and original payload/backup/canary identities, reject reparse points/hard links,
exclude concurrent writers and compare exact bytes. A durable intent precedes each
write. A crash leaving unknown partial bytes is quarantined, never overwritten.
Rollback accepts only the owned expected postimage (or a positively observed
unchanged preimage), restores exact backup bytes and retains the original outcome.
Cancellation stops forward execution but permits the already confirmed inverse.
Reopen never replays forward work; recovery requires a fresh preview and confirmation.

M1, M2 and M3 serialized contracts are unchanged. Synthetic target state uses its
own versioned, bounded private journal and cannot be interpreted as game evidence.
No catalog promotion, save access, installer operation, release or publication.

## Files and acceptance

Changes are limited to the Framework's opt-in phase/transaction integration,
Source/ExecutionSynthetic, its native provider and tests/build manifests, focused
source-policy checks, Editor acceptance, CURRENT_TASK and affected guides.

Required evidence: unchanged M1/M3 tests; actual supervised six-phase success;
byte-for-byte restoration and untouched canary; failed and cancelled launch;
provider/policy/target drift refusal; corrupt backup and interrupted state recovery;
no replay on reopen. Run exact-pin affected builds and native tests, plus the
Foundation lifecycle in a disposable Editor project. Record skipped/failed gates.
No game runtime sign-off is included.

Bounds: one payload <=64 KiB, a fixed six-phase plan, one target per service,
M2 bounded output/timeouts, constant-size transaction metadata. The fixed payload transaction and rejection checks at the 64 KiB read limit
must finish within 1 second on the local Profile build; full workflow
within 60 seconds excluding host startup. Generated evidence stays outside source.

Status: applicable local validation PASSED on 13 September 2026. Static validation
ran 960 Python cases with 33 explicit skips, ten additional tooling tests and four
exact-pin source-policy selections. The affected Windows Profile targets built.
Compiled Catalog, M1, Framework contract and operational selections passed 641
tests with two explicit Catalog symlink skips. All 19 M5 native tests passed.
The fixed payload apply/rollback took 45.240 ms against the one-second budget.

The actual disposable Editor passed success, cancellation during launch, deliberate
hard interruption after deployment, and fresh-confirmation recovery. Success produced
six artifacts. Normal shutdown joined workers and restored the target in 0.610 s,
1.110 s and 0.391 s for success, cancellation and recovery respectively.
Manual visual acceptance was NOT_RUN; this change adds service integration, not
operator controls or a pane. A window-capture helper initially failed independently
of the workflow and was removed from this service-lifecycle harness. All native
workflow, context-veto, shutdown, crash and recovery assertions remain required.
Private logs retain both failed harness attempts and final passing executions.
Exact-commit receipt and maintainer review are separate handoff obligations.

## Host integration and validation commands

`FrameworkSyntheticTarget::Create` accepts an existing private workspace root and
creates a fresh `synthetic-target` child. It refuses an existing target. `Reopen`
checks the exact context and original file identities. The six phase bindings come
from `PrepareSyntheticWorkflow`; this function verifies the M3 store context and
initializes only the fixed seed and staging directories. It grants no policy or
qualification. The host supplies reviewed qualifications and a confirmation-required
policy to the explicit synthetic `ConfigureFrameworkExecution` overload. Then it
previews, confirms and submits through the existing service. The original staging
overloads retain their signatures and restrictions.

Build the affected targets in the pinned Windows Profile build, including
`TaintedGrailModdingSDK.Synthetic.Provider`, `FrameworkExecution.Static`,
`Framework.Static`, `FrameworkExecution.Operational.Tests` and `Editor` (SDK target
prefix omitted on the latter names). CTest's existing Framework native lane sets
`FOA_M5_PROVIDER` to the actual built executable. Its `FrameworkSyntheticNative.*`
cases exercise production transactions and real supervised processes.

The disposable Editor acceptance script is
`Gems/TaintedGrailModdingSDK/Tools/editor_tests/framework_synthetic_lifecycle_smoke.py`.
Supply private paths using its documented environment variables; use a disposable
project and the exact pinned engine path. Run success, cancel, crash and recover
in that order, with crash/recover sharing their owned target. Crash exits the test
Editor after deployment without destructor cleanup; recovery proves fresh admission
and exact restoration without replaying the interrupted execution.

The synthetic journal is separate from the unchanged M3 store. Up to 64 immutable
restore observations are retained by fingerprint, with a separate latest-observation
copy for diagnostics. Recovery never fills in a missing successful receipt or
changes the original Framework execution outcome. Unknown partial writes, replaced
files, changed canaries, corrupt backups and storage failures remain quarantined.
New failed attempts require explicit retry; unresolved crash histories are retained.
Use a fresh disposable target/store for further forward work after a quarantined
crash history. A full observation store refuses new deployment before mutation,
while still allowing read-only reopen.

The six commands operate on a repository-owned text fixture, not on game assets:
build transforms a seed into a value document; package wraps that document; deploy
unwraps and installs its verified bytes; launch interprets the deployed value;
verify independently checks the value and launch observation; rollback validates
the backup and the host restores it. The LPAC child never receives target write
access. Failure, hang and bounded one-second delay options exist only for explicit synthetic fault-injection plans.
