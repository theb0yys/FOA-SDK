# Framework Orchestrator and Repositories M3

Status: ACCEPTED for implementation by the owner on 12 September 2026. The M3 change implements this scope; its exact-head validation receipt records acceptance results.
Requested by the owner on 12 September 2026: "Next: M3 — the Framework Orchestrator and Repositories."
Primary owner: `capability-execution` in SDK Framework.
Supporting owners: `artifact-ownership`, `execution-receipts`, and `external-toolchain`.
Classification: Significant design; Critical/Runtime implementation.
Dependency: M2 PR #265, locally validated at `c41be264c7179c80c86ed64399f33465346a7b40`; the PR was merged on 12 September 2026. Local M2 evidence is retained separately from M3 validation.

## Decision to accept

Implement the M3 batch in the [Capability Execution Contract](CAPABILITY_EXECUTION_CONTRACT.md#m3--framework-orchestrator-and-repositories): deterministic provider resolution and qualification, independent policy and exact-plan authorization, immutable planning, asynchronous orchestration, durable execution and artifact repositories, target ownership records, and candidate-only evidence projection.

The first executable profile consumes M2's already reviewed `windows-lpac-registry-read-batch-v1` native batch route. It supports declared MATERIALIZE, BUILD and PACKAGE phases whose only side effects are READ_ONLY, PROCESS_LAUNCH and STAGING_WRITE. At least one required phase must exist, phases retain the M1 order, and the requested terminal phase must be reachable. Other phase/effect combinations return a typed unsupported result before submission. This accepts the M3 infrastructure without starting M4 planner adapters or M5 deployment/launch/verification execution.

The result must execute a supported, explicitly authorized staging job through the actual M2 production supervisor, retain verified output custody and receipts, and reopen them after restarting the Editor. A collection of always-denying stubs does not satisfy this scope.

## Authority and compatibility

The owner has requested M3. The shared contract's M0 batch gate additionally requires acceptance of exact source paths, owners, versioning, threat/failure analysis and validation lanes before source implementation. The owner accepted this concrete scope before M3 source implementation.

Controlling architecture is the [Capability Execution Contract](CAPABILITY_EXECUTION_CONTRACT.md). M1's [implementation authority](CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md) and M2's [accepted design](TOOL_EXECUTION_M2_DESIGN.md) establish the dependency boundaries. Their earlier exclusions of future M3 work describe those completed batches, not a prohibition on preparing this requested batch.

M1 Core contract IDs, canonical bytes, validation rules and dependencies remain unchanged. Framework adds strict readers for its persisted copies of those canonical values and must re-canonicalize and contextually validate them. This is a Framework consumer, not a Core filesystem/JSON-loader extension. The M1 source gate changes only to allow the exact new Framework consumer family and its tests; the Core purity checks remain enforced.

ExternalToolchain discovery 1.1.0, execution 2.0.0, LPAC profile, resource limits and admission lease protocol remain unchanged. Framework is a host-private consumer of M2, not a provider/plug-in privilege upgrade. Existing inert adapter, work-order, interchange, deployment, evidence and release V1 contracts remain inert.

## Ownership and call direction

| Surface | Owner and boundary |
| --- | --- |
| Descriptors, bindings, canonical requests, plans, receipts | M1 Core values; Framework uses contextual validation |
| Provider identity, command registration, configuration and discovery | ExternalToolchain; installed/enabled does not imply qualified or authorized |
| Binding selection and qualification decisions | Framework provider service; exact capability, phase, profile, provider and evidence |
| Policy and authorization | Framework policy service; host-originated session decisions, never reconstructed from stored grants |
| Process execution and isolation | Unchanged M2 supervisor and native backend |
| Execution attempts, artifacts and target ownership | Framework repositories with immutable identity and transaction references |
| Candidate evidence projection | Framework projection into existing SourceRecord/EvidenceRecord shapes; separate registration/promotion owners |
| Editor integration | Foundation-owned service lifetime and thin snapshot/command access |

One active Framework execution context belongs to an exact workspace, pack and profile. Context switching cannot move live work into the new workspace. It must reject while work cannot be stopped safely, or cancel and join the previous context before opening the next. The repository writer lock prevents two host sessions from concurrently owning the same store.

M3 uses a host-private M2 service configured by Framework; it does not connect another handler to the existing M2 request bus. The pre-existing Editor bus remains denied to direct provider callers. Only the Framework adapter can register its reviewed commands and supply its resolver/admission gate. Tests must prove that directly submitting an otherwise valid M2 invocation without a matching pending Framework attempt is denied.

## Provider binding and qualification

Registration is bounded and finalized before planning. A registration binds the exact M1 provider binding, provider descriptor, M2 command fingerprint, fixed argument/environment mapping and phase preview contract. Identity collisions with different bytes are errors; duplicate identical registrations are idempotent.

Resolution evaluates each requested phase separately. It applies exact request preferences, then an exact workspace default, then requires one eligible binding. Zero candidates and multiple candidates produce distinct deterministic diagnostics. Registration order, display labels and lexicographic preference are not selection rules. No fallback occurs after execution starts.

Qualification is supplied through a host-owned reviewed-evidence interface. Every observation binds the binding fingerprint, executable digest, exact profile, evidence IDs, observation identity and validity interval. Merely naming an evidence ID or constructing a structurally valid QUALIFIED record does not admit execution. Candidate, absent, expired, revoked or mismatched qualification fails closed. Evidence review is not inferred from successful discovery or execution.

Read-only current discovery/configuration, qualification freshness and root identities are rechecked during execution admission and immediately before M2 consumes its lease. Provider, command, executable, configuration, environment or root drift requires a new preview. Approved arguments cannot be changed after preview. No shell, PATH lookup, script interpreter, secret, network publication or runtime route is introduced.

## Planning, policy and authorization

Preview consumes a validated descriptor/request and pure phase-preview outputs. It assembles the selected bindings, independent decisions, ordered M1 phases, declared outputs, input digests, inventory/preimage fingerprints and rollback declarations. Preview does not launch a process or publish an artifact. Existing domain planners remain unchanged until M4.

Support, qualification, environment, policy and authorization remain separate results. Unsupported/unqualified/unavailable/policy-denied data may still be displayed as diagnostic snapshots, but cannot become an executable plan. The profile accepts only its declared staging effects; missing policy is DENIED.

The immutable plan contains PENDING or NOT_REQUIRED intent. A subsequent host confirmation binds the complete plan fingerprint, exact workspace/pack/profile, actor, policy revision, qualification revision and expiry. Confirmation is a command from the trusted host session; imported JSON, plugin declarations, receipts and cached history cannot call it into existence. The API does not claim to implement identity-provider authentication; the embedding trusted host owns actor authentication and supplies the confirmation event.

Execution needs current allowed policy and any required live confirmation. A NOT_REQUIRED decision is valid only under an explicit host policy covering the exact route/effects/context. It is not a bypass for missing policy. Every lease is single-use, short-lived and bound to M2's complete admission context, including host instance and pinned root identities. Expiry and revocation are checked at acquisition and consumption. Replanning invalidates prior grants.

## Orchestration and observability

A joined Framework worker coordinates phases; M2 retains ownership of its bounded process workers. At most one execution is active per Framework context, with a queue bound of 16. Public status/cancel calls return cached bounded snapshots and do not perform filesystem scans, hashing, canonicalization or synchronous waits on Editor event paths.

Before any M2 submission, persist the immutable plan and attempt intent. The bounded Framework queue is session-local; queued work does not acquire durable execution authority or replay after restart. Bind the M2 request fingerprint and attempt ID to the pending phase. Only after that durable write and admission registration may M2 receive the invocation. Poll bounded status at a documented interval without holding repository or UI locks. Phase failure/cancellation stops later phases; it cannot be converted to success by cleanup.

Success requires the exact M2 record to be terminal, exit-zero, output-verified, cleanup-confirmed and durable, followed by Framework output verification/custody and durable receipt commit. A process exit code alone is insufficient. M2 invocation records become phase-extension observations, not fabricated phase-success receipts. Attempted, blocked, interrupted, timed-out and cancelled cases preserve their distinctions.

A service snapshot exposes plan/execution identity, lifecycle state, current phase, structured refusal/failure, cancellation availability and receipt/artifact references. Logs remain bounded/redacted under M2. No new end-user pane or domain launch button is included; Foundation provides the production service access point for subsequent adapters/commands.

## Repositories and failure atomicity

New private envelope ID: `foa-framework-execution-store-v1`, version 1. This is separate from workspace, pack, catalog, game/save and M1 interchange formats. Roots are host-selected local private directories outside source and game installations; public/persisted records contain symbolic roots and safe relative locators only.

The repository stores immutable descriptors/requests/selected bindings/plans, append-only attempt observations, final M1 receipts, artifact custody and ownership transactions. Exact canonical bytes and their SHA-256 values are retained. Hashes detect changes; they do not establish authenticity or authorization. Strict bounded readers reject unknown versions, duplicate/unknown fields, invalid enums, malformed references, missing lineage, identity collisions, unsupported paths and contextual mismatches. Failed reads leave prior in-memory state unchanged.

A single exclusive writer and durable transaction intent coordinate execution, artifacts and ownership. Temporary writes are flushed and atomically published on the supported Windows filesystem. Finalization verifies staged artifact bytes, persists the complete transaction, and publishes its committed marker last. Readers ignore incomplete transactions. After interruption, recovery can complete a fully verified prepared transaction or preserve it as quarantined; it never guesses ownership or reruns a tool. No success is published before all required records are durable.

A storage error disables further submission until explicit recovery/reopen. Corrupt, future-version, contradictory or unsafe state is reported; files are not silently deleted or migrated. Interrupted attempts remain history and require fresh policy/authorization for an explicit retry. Recovery first reconciles the referenced M2 journal/cleanup result; unknown cleanup remains quarantined and blocks reuse.

Bounds: at most 1,024 execution lineages, 64 attempts per lineage, M1's 9 phases and 64 entries per contract collection, 2 MiB per canonical value, 256 MiB metadata total, and 1 GiB per captured artifact with a 4 GiB aggregate store admission budget. Reaching a bound returns StoreFull before a new operation. There is no automatic eviction. Disk admission checks are not an OS quota; disk-full during staging/finalization must be handled as a failed operation. An explicit future retention design would be a separate change.

Rollback of this feature means disabling the Framework service and retaining its private store for inspection. Older builds ignore the separate store; they cannot replay it. No workspace or pack migration is promised or required. A future incompatible store version must reject or undergo a separately specified migration.

## Artifacts and target ownership

Artifact capture consumes only outputs of the exact successful M2 invocation. It rechecks root custody, safe locator, single-link/reparse restrictions, declared kind, size and digest before copying into Framework custody. Publish artifacts with create-new semantics; never overwrite an existing immutable artifact. Record semantic owner, producer execution/phase/provider, storage custodian, source extension, lifecycle and redistribution separately.

The target ledger stores plan reservations and verified ownership observations. It detects same-location/different-owner conflicts, expected-preimage drift and artifact identity collisions. It commits ownership only from exact contextually validated execution observations in the same durable transaction. Matching bytes with a different owner remain a conflict. Metadata registration is never permission to mutate a target.

M3's executable profile has no installation mutation or replacement/removal operation. Its ledger integration proves staging/artifact custody and reservation release. General target claim/preimage/verified-observation rules receive deterministic repository tests, while actual deploy/restore behavior remains M5. Unknown cleanup retains reservations/quarantine. Successfully cleaned failed attempts may release reservations without rewriting the failure outcome.

## Idempotency, retry and recovery

The operation key binds the stable M1 semantic request, exact profile, ordered inputs/bindings, configurations, phase plans and target inventory. Execution/attempt IDs, capture timestamps, UI labels and log locations are excluded. M1 stable contract IDs remain part of their existing canonical values; M3 does not rewrite M1 canonical semantics to remove them.

Duplicate active submissions attach to the existing execution. Completed results may be reused only after the current artifact/ownership/environment checks pass. Content or target drift requires a new preview. A retryable failure creates another attempt in the same lineage only through an explicit retry command with current admission; non-retryable failures require a new request/plan. Same identity with different canonical bytes is a collision. Restart recovers observations, never live grants or execution permission.

## Candidate evidence

The projection service consumes repository-bound, contextually validated receipts and emits candidate SourceRecord/EvidenceRecord values with exact receipt fingerprint, profile, subject and observed result. It preserves failed/partial/interrupted outcomes and limits claims to the operation actually observed. It performs no source/evidence registry write itself.

Existing Foundation review/persistence services remain the owners of any later registration. M3 never promotes evidence, clears blockers, assesses game compatibility, approves release or treats successful batch output as game-runtime evidence.

## Performance and lifetime acceptance

New M3 API budgets are acceptance thresholds to measure, not current claims: status/cancel p95 at most 10 ms at the 1,024-lineage limit; snapshot pages at most 64 entries; cancellation propagation at most 100 ms before M2's existing cleanup deadline; joined shutdown at most 6 seconds for the bounded native fixture. One maximum-size canonical preview must complete within 500 ms on the recorded test host off the UI thread. Record cold/warm repository reopen measurements at the metadata bound and fail the acceptance lane if reopen exceeds 10 seconds.

Large artifact IO occurs only on the worker. Storage copying and hashing check cancellation between bounded chunks. Tests include 100 repeated submit/cancel/reopen cycles and assert no monotonically growing handle count, duplicate active attempt or leaked worker. The exact host/configuration and measurements accompany the receipt; no cross-machine performance guarantee is inferred.

Foundation lifetime owns the service. Qt aboutToQuit cancels and joins it before the pinned Editor's early termination. Repeated close/deactivation is idempotent. The live Editor test opens a disposable synthetic context through the host service, runs a fixture job, checks snapshots, closes/reopens its store and verifies shutdown. It must not expose a general fixture-configuration or approval bypass through product UI.

## Validation matrix

| Lane | Required proof |
| --- | --- |
| Static and compatibility | Existing M1 canonical fixtures unchanged; explicit new consumer allowlist; no Core/process/persistence dependency leak; no V1 permission switch; source-policy and proposal/implementation scope audit |
| Provider and policy compiled tests | Deterministic exact selection/preferences/defaults; ambiguity; stale qualification; missing policy; expiry/revocation; scope mismatch; lease replay and direct-M2 bypass refusal |
| Repository compiled tests | Strict round trips; future/duplicate/unknown/malformed input; atomic read failure; collisions; writer exclusion; bounds; interrupted transactions; ownership conflicts; restart without grants |
| Orchestrator compiled tests | Exact preview/execute parity; multi-phase order; pending/active duplicate; verified reuse; input/output/environment drift; retry lineage; no downstream phase after failure; candidate-only projection |
| Windows operational | Actual M2 backend with host-owned synthetic executable; successful custody/reopen; cancellation, timeout, nonzero exit, missing/wrong outputs, persistence fault, hard restart, cleanup quarantine and resource/performance guards |
| Editor | Actual pinned Editor activation, context isolation/switch refusal, synthetic service workflow, normal quit, joined workers and exit zero |
| Regression | M1, M2, discovery, interchange and affected Foundation/Catalog tests; exact-pin configure/build and static gates |
| Later milestones | Full deployment spine, game/Unity launch, installed target rollback, runtime, release and signing NOT_APPLICABLE to M3 |

Two dedicated compiled targets separate deterministic Framework tests from actual Windows process/storage acceptance: `TaintedGrailModdingSDK.FrameworkExecution.Tests` and `TaintedGrailModdingSDK.FrameworkExecution.Operational.Tests`. The native lane requires the existing M2 fixture executable and fails if unavailable; zero matching tests are an error. CI uses the pinned external O3DE and bounded build parallelism. Current M2 remote checks are tracked separately and cannot be counted as M3 proof.

## Exact proposed implementation paths

The following 52 paths, plus the three necessary integration paths listed below, bound this implementation. Existing files outside this list, the M1 Core family, M2 isolation implementation, O3DE pin/dependencies, external engine, game/install/save data and unrelated SDK-client work are excluded. If a consequential additional boundary is needed, update the design before expanding implementation.

```text
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionTypes.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionCodec.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionCodec.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkProviderService.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkProviderService.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionPolicyService.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionPolicyService.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionRepository.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionRepository.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkArtifactRepository.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkArtifactRepository.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkTargetOwnershipLedger.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkTargetOwnershipLedger.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionService.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionService.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionEvidenceProjection.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkExecutionEvidenceProjection.cpp
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkToolExecutionAdapter.h
Gems/TaintedGrailModdingSDK/Code/Source/ExecutionFramework/FrameworkToolExecutionAdapter.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/FrameworkExecutionTestFixtures.h
Gems/TaintedGrailModdingSDK/Code/Tests/FrameworkExecutionProviderPolicyTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/FrameworkExecutionRepositoryTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/FrameworkExecutionOrchestratorTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/FrameworkExecutionOperationalTests.cpp
Gems/TaintedGrailModdingSDK/Code/Source/FoundationService.h
Gems/TaintedGrailModdingSDK/Code/Source/FoundationService.cpp
Gems/TaintedGrailModdingSDK/Code/Source/FoundationServiceConstruction.cpp
Gems/TaintedGrailModdingSDK/Code/Source/FoundationExecutionService.cpp
Gems/TaintedGrailModdingSDK/Code/Source/TaintedGrailModdingSDKSystemComponent.h
Gems/TaintedGrailModdingSDK/Code/Source/TaintedGrailModdingSDKSystemComponent.cpp
Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_files.cmake
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_execution_files.cmake
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_execution_tests_files.cmake
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_execution_operational_tests_files.cmake
Gems/TaintedGrailModdingSDK/Tools/validate_framework_execution.py
Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_framework_execution.py
Gems/TaintedGrailModdingSDK/Tools/validate_capability_execution_contracts.py
Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_capability_execution_contracts.py
Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py
Gems/TaintedGrailModdingSDK/Tools/validate_ci_runner_policy.py
Gems/TaintedGrailModdingSDK/Tools/editor_tests/framework_execution_lifecycle_smoke.py
.github/workflows/tainted-grail-sdk-pr-validation.yml
docs/tainted-grail-sdk/FRAMEWORK_EXECUTION_M3_DESIGN.md
docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md
docs/tainted-grail-sdk/DATA_FORMATS.md
docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md
docs/tainted-grail-sdk/README.md
Gems/TaintedGrailModdingSDK/README.md
docs/tainted-grail-sdk/ARCHITECTURE.md
CHANGELOG.md
CURRENT_TASK.md
```

## Implementation sequence

1. Accept this scope, including its private store/rollback, session authority and staging-only profile.
2. Implement strict repository readers/transactions and provider/policy services with negative tests.
3. Connect immutable planning and the asynchronous orchestrator to unchanged M2 production admission/execution.
4. Add verified artifact custody, ledger transactions, retry/recovery and candidate evidence projection.
5. Integrate Foundation/Editor lifetime, run actual native and Editor acceptance, and record exact-head results.
6. Submit the focused M3 change for maintainer review; do not merge M2 or M3 without a current explicit integration instruction.

M4 adapters and M5 synthetic deployment are not automatically started after completion. Runtime sign-off not performed.

### Required validation integration discovered during implementation

The existing recursive build-graph guard also needs its new FrameworkExecution owner
registered in `Gems/TaintedGrailModdingSDK/Tools/validate_core_framework_build_graph.py`
and covered in `Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_core_framework_build_graph.py`.
These two validation paths preserve unique source ownership for the accepted M3 family;
they add no product scope or external transition.

The existing `Gems/TaintedGrailModdingSDK/Code/Source/FoundationLocalSetupService.cpp`
refresh entry point also requires the same context guard: it can replace the active
profile independently of `LoadWorkspace`. This integration change prevents a live
Framework context from silently surviving a profile replacement.

### Implementation details

`FoundationService::ConfigureFrameworkExecution` installs the exact context and
host-owned bindings/qualification/policy. `GetFrameworkExecution` exposes internal
preview, confirm, submit, cached status/cancel/page and bounded history methods.
The service retains immutable shared plans so submission/confirmation do not copy
large plans while holding the status lock. Repository opening validates independent
transactions with at most sixteen joined readers; final publication and lineage
checks remain ordered. Unknown or interrupted state stays quarantined even when
an exact M2 cleanup observation is recovered. No automatic repair/replay is provided.

### Host integration workflow

The embedding host first selects its active workspace, pack and exact profile, then
constructs reviewed `HostBinding` records, current `Qualification` observations and
an explicit `HostPolicy`. Call `FoundationService::ConfigureFrameworkExecution`
with a new private local store root and a context matching those active identities.
Failure leaves the service unavailable and returns a structured setup error.

Call `Preview` off the UI event path with the M1 descriptor, semantic request and
optional exact default bindings. Retain the returned plan fingerprint. If the
policy requires confirmation, send the authenticated host confirmation event to
`Confirm` for that exact plan and bounded lifetime. `Submit` returns an execution
identity immediately; use `Status` or pages of at most 64 snapshots for progress,
and `Cancel` to request cancellation. Terminal `History` retains the M1 receipt
and the actual M2 observations. `ProjectCandidate` requires the exact host profile (checked against the plan fingerprint) and returns fully populated candidate records for
a separate review flow; it does not register them.

After reopening, configure current host policy and qualification again. Stored
confirmation receipts are history only. A verified completed submission may reuse
its artifacts after current custody, ownership and environment checks. A failed
attempt needs an explicit retry; an interrupted or quarantined transaction does
not replay. A damaged store remains intact for inspection and refuses writes.
Quarantine also prevents a workspace/profile switch. Normal Editor shutdown
cancels and joins the worker and supervisor.

No user-facing job pane is part of M3. The lifecycle smoke loads only its compiled
test DLL into the actual Editor and exercises the Foundation instance registered
by the production component. It does not add a fixture launcher to product UI.

Candidate source metadata uses the existing registry warning import status; evidence
remains in the separate candidate collection. The projection performs no registry write.
A changed profile/build is refused, and failed observations retain their failed outcome.
