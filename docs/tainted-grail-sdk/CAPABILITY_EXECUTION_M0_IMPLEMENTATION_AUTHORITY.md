# Capability Execution M0 Implementation Authority

Status: repository-owner-authorised M0 decision for maintainer audit; effective only after this document is merged to `main`

Decision owner: `@theb0yys`

Decision date: 15 August 2026

Controlling architecture: [Capability Execution Contract](CAPABILITY_EXECUTION_CONTRACT.md)

Authorised implementation batch: **M1 — Additive Core Contracts only**

## Decision

This record satisfies the M0 governance and implementation-authority prerequisite for one bounded implementation batch:

```text
M1 — Additive Core Contracts
```

After this record is merged to `main`, a separate implementation branch may add the pure Core contract, canonicalisation, validation, compiled-test, static-validator, documentation, and read-only CI surfaces named below.

This record does not implement M1. It does not authorise M2 or any later batch.

The authorised M1 result is a deterministic, side-effect-free contract family that can describe capability semantics, provider bindings, semantic requests, independent decisions, immutable phase plans, artifact ownership, target-mutation claims, rollback plans, phase receipts, and complete execution receipts. It cannot resolve a provider, launch a process, read or write a file, persist state, build, package, deploy, launch Fall of Avalon, verify a live target, sign, publish, mutate a save, promote evidence, or grant permission.

## Authority Boundary

### Authorised now, after M0 merge

M1 may:

1. add new Core-only C++ value types and typed vocabularies;
2. add deterministic canonical JSON projections and SHA-256 fingerprints;
3. add pure validation and exact upstream-binding checks;
4. add a dedicated Core-only compiled test target;
5. add a repository validator that enforces the M1 source family and forbidden dependencies;
6. wire that validator and test target into existing read-only validation;
7. document the new non-durable contract family and its compatibility boundary;
8. update current-task and changelog records for the exact M1 review unit.

### Not authorised

M1 may not:

- add or invoke an executor;
- add process supervision, shell execution, `ProcessWatcher`, `QProcess`, `CreateProcess`, IPC, sockets, or network access;
- add ExternalToolchain execution APIs or provider invocation;
- add Framework orchestration, repositories, persistence, loaders, writers, registries, or workspace schemas;
- add Editor commands, panes, buttons, menus, toolbars, or Qt dependencies;
- inspect executables, tool installations, game installations, deployment targets, saves, Unity projects, or protected files;
- build, package, copy, replace, delete, back up, restore, deploy, launch, verify, sign, upload, publish, or mutate runtime state;
- alter catalog state, grant permission, clear blockers, register promoted evidence, or make release decisions;
- modify existing adapter, deployment, runtime-result, release, canonical-interchange, or External Tool Interchange V1 semantics;
- begin M2 process supervision, M3 orchestration, M4 planner adaptation, M5 synthetic execution, M6 heightmap runtime work, or any later migration batch.

A passing M1 contract test does not prove an executable capability.

## Implementation Branch And Review Unit

M1 implementation must use a separate branch based on the accepted `main` head after this M0 decision merges:

```text
implementation/capability-execution-m1-core-contracts
```

The M1 branch must contain one reviewable implementation unit. It enters `main` only through a maintainer-audited pull request. This decision does not authorise direct-main work, self-merge, workflow control, or automatic progression to M2.

## Owner Systems

| Surface | Owner system | Write authority in M1 |
|---|---|---|
| Capability semantics and typed state | `capability-execution` | Yes, Core-only |
| Canonical requests, plans, receipts, validation, fingerprints | `capability-execution` | Yes, Core-only |
| Artifact identity and target-mutation claims | `artifact-ownership` | Yes, contract values only |
| Rollback descriptions | `artifact-ownership` | Yes, plan values only |
| Phase and execution receipts | `execution-receipts` | Yes, contract values only |
| Runtime observations | `runtime-verification` | Reference shape only; no verifier or live observation |
| Existing adapter/build/package/deployment/release contracts | Their current owners | Read-only compatibility boundary |
| ExternalToolchain | `external-toolchain` | No production changes in M1 |
| Framework, Editor, providers, plug-ins, installer, runtime adapters | Existing owners | No production changes in M1 |

Core remains dependent only on `AZ::AzCore`. The M1 compiled test target may additionally depend on `AZ::AzTest`. No Framework, Qt, AzToolsFramework, ExternalToolchain, provider, runtime, deployment, filesystem, or process dependency may enter the production contract family.

## Exact Authorised M1 Paths

M1 product-source and supporting changes are limited to the following paths.

### New Core production files

```text
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionContracts.h
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionContracts.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionCanonical.h
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionCanonical.cpp
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionValidation.h
Gems/TaintedGrailModdingSDK/Code/Source/CapabilityExecutionValidation.cpp
```

### Build ownership and dedicated Core-only compiled tests

```text
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake
Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_capability_execution_tests_files.cmake
Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt
Gems/TaintedGrailModdingSDK/Code/Tests/CapabilityExecutionContractTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CapabilityExecutionCanonicalTests.cpp
Gems/TaintedGrailModdingSDK/Code/Tests/CapabilityExecutionValidationTests.cpp
```

The dedicated target name is:

```text
TaintedGrailModdingSDK.CapabilityExecution.Tests
```

It must link `TaintedGrailModdingSDK.Core.Static` and `AZ::AzTest` only, use `NO_UNITY`, run with `--no-tests=error`, and not recompile production sources.

### Static enforcement and validation integration

```text
Gems/TaintedGrailModdingSDK/Tools/validate_capability_execution_contracts.py
Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_capability_execution_contracts.py
Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py
.github/workflows/tainted-grail-sdk-pr-validation.yml
Gems/TaintedGrailModdingSDK/Tools/validate_ci_runner_policy.py
```

`validate_ci_runner_policy.py` may change only when the read-only CI policy validator requires an exact declaration for the new Core-only job. The workflow remains `contents: read`, performs no repository mutation, and may only configure, build, test, and retain bounded evidence for the exact event head.

### Documentation and continuation state

```text
CURRENT_TASK.md
CHANGELOG.md
docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md
docs/tainted-grail-sdk/DATA_FORMATS.md
docs/tainted-grail-sdk/README.md
docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md
```

No other path is authorised. If implementation requires another source, test, validator, workflow, schema, plug-in, provider, Framework, Editor, installer, runtime-adapter, research, or process path, M1 stops and requests an M0 amendment before writing it.

Existing helper files may be read and called when their public contract already permits it, but may not be modified under this decision. In particular, changes to existing deterministic JSON, fingerprint, path-validation, adapter, deployment, release, interchange, Foundation, or ExternalToolchain helpers require separate authority.

## Canonical M1 Contract Family

The M1 namespace is:

```text
TaintedGrailModdingSDK::CapabilityExecution
```

The contract identity is:

```text
Contract ID: foa-capability-execution-v1
Canonical profile: foa-capability-execution-canonical-json-v1
Contract version: 1
```

M1 must define the following public value families. Exact member spelling is fixed by the reviewed M1 pull request, but the semantic groups and their separation are mandatory.

### Capability and provider semantics

- `CapabilityDescriptorV1`;
- `CapabilityProviderBindingV1`;
- typed phase, side-effect, rollback-support, terminal-phase, and contract-kind vocabularies.

### Semantic request and independent decisions

- `CapabilityExecutionRequestV1`;
- `CapabilitySupportDecisionV1`;
- `CapabilityQualificationDecisionV1`;
- `CapabilityEnvironmentDecisionV1`;
- `CapabilityPolicyDecisionV1`;
- `CapabilityAuthorizationReceiptV1`.

Support, qualification, environment, policy, human authorisation, execution outcome, verification, assessment, evidence promotion, and release decision must use independent typed states. No field, helper, default, or validator may imply one state from another.

### Artifacts, targets, and rollback

- `ArtifactReferenceV1`;
- `ExpectedArtifactV1`;
- `ArtifactRecordV1`;
- `TargetMutationClaimV1`;
- `RollbackPlanV1` and typed rollback steps;
- stable owner, producer, custodian, lifecycle, media type, digest, byte-size, and redistribution fields where applicable.

### Immutable plans

- `CapabilityPhasePlanV1`;
- `CapabilityExecutionPlanV1`;
- exact provider-binding, command, input, expected-output, target-mutation, configuration, environment, rollback, canonical JSON, and fingerprint bindings.

### Receipts and observations

- `FailureRecordV1`;
- `DiagnosticReferenceV1`;
- `TargetObservationV1`;
- `CapabilityPhaseReceiptV1`;
- `RollbackReceiptV1`;
- `CapabilityExecutionReceiptV1`;
- a typed phase-extension reference containing contract identity, canonical JSON, and fingerprint.

M1 defines shapes and validation only. It contains no registry, singleton, service locator, executor interface, EBus, process abstraction, filesystem abstraction, persistence service, provider resolver, or UI surface.

## Canonicalisation And Fingerprint Rules

M1 canonical projections must:

- use UTF-8 and fixed property order;
- use exact, case-sensitive typed tokens;
- use locale-independent numeric formatting;
- sort set-like collections on copies;
- preserve explicitly ordered phase, step, and attempt sequences;
- reject duplicate stable IDs and duplicate set-like values;
- never mutate caller input during validation or serialisation;
- reject unknown enum values and unsupported contract versions;
- use lowercase `sha256:<64-hex-digits>` fingerprints;
- exclude an object's own fingerprint from its canonical projection;
- require embedded upstream canonical JSON and fingerprints to match the supplied upstream values exactly;
- exclude capture timestamps, display labels, transient UI state, and log locations from semantic request and plan identity;
- include event timestamps in receipt identity where the receipt contract declares them as observed event data;
- distinguish semantic identity from event-attempt identity;
- define and test explicit maximum string, collection, nesting, and canonical-byte bounds;
- fail closed before expensive nested work when an earlier structural bound fails.

M1 must not claim authenticity, trust, provider qualification, execution, success, deployment, or runtime proof from a SHA-256 value.

## Validation Rules

Validation must require, as applicable:

- stable namespaced capability, provider, binding, request, plan, phase, artifact, mutation, rollback, receipt, failure, diagnostic, and extension IDs;
- exact contract and canonical-profile identity;
- exact provider, command, phase, profile, configuration, environment, input, output, target, and upstream fingerprint binding;
- unique phase identities and deterministic sequence coverage;
- each required phase represented exactly once;
- the requested terminal phase reachable from the declared ordered phase set;
- each phase input resolved from request inputs or an earlier expected output;
- no output consumed before it is produced;
- no target mutation without exact target root, safe relative target path, expected preimage state, desired artifact, owner, and rollback declaration;
- no replacement or removal without an exact preimage fingerprint and owner binding;
- no `EXACT_RESTORE` declaration without a corresponding immutable backup artifact and restore fingerprint;
- no receipt without exact plan and phase-plan fingerprint binding;
- no phase extension without exact extension contract ID, canonical bytes, and fingerprint;
- outcome shape consistency for attempted, succeeded, failed, skipped, cancelled, cleanup, and rollback observations;
- safe relative artifact, target, backup, and diagnostic locators only;
- no absolute path, URI, drive-qualified path, UNC path, traversal, reserved-name, ambiguous separator, or private-machine locator in canonical values;
- no secret, credential, signing-key material, environment dump, unrestricted command line, raw process handle, native runtime pointer, or game object handle;
- explicit bounded cardinality and aggregate canonical size.

Provider selection and policy evaluation do not occur in M1. M1 validates supplied decision and binding records without claiming that a provider exists, is installed, is qualified, is permitted, or can execute.

## Threat And Failure Analysis

| Threat or failure | Required M1 control | Required negative proof |
|---|---|---|
| Existing V1 contracts become executable by reinterpretation | New namespace and version-forward contract family; no edits to legacy V1 source or validators | Exact legacy file-diff guard plus existing V1 validator and compiled-test regression |
| Support, qualification, policy, authorisation, outcome, or promotion collapse into one state | Separate structs and enums with no implication helpers | Construct contradictory combinations and prove they remain representable or are rejected only by explicit contract rules |
| Canonical output changes with input ordering, locale, or mutation | Fixed order, set sorting on copies, locale-independent formatting, non-mutation | Permuted-input byte equality, duplicate rejection, locale guard, and before/after input equality |
| Duplicate or ambiguous identities bind the wrong phase/provider/artifact | Stable IDs, uniqueness checks, exact upstream bindings | Duplicate ID, duplicate phase, duplicate artifact, and mismatched fingerprint failures |
| Path traversal or private-machine data enters canonical contracts | Safe relative locator validation and explicit forbidden syntax | Absolute, drive, UNC, URI, traversal, mixed separator, reserved-name, and control-character rejection |
| Unbounded strings, collections, nesting, or canonical JSON cause memory/CPU exhaustion | Named maximum constants, checked arithmetic, early short-circuit, bounded aggregate bytes | At-limit acceptance, one-over-limit rejection, multiplication-overflow and nested-cardinality tests |
| Provider ambiguity is hidden by the contract | Binding records identify one exact provider/command; M1 contains no resolver or fallback | Multiple candidate metadata cannot be converted into a selected binding by any M1 helper |
| Artifact ownership is ambiguous | Separate semantic owner, producer, storage custodian, target owner, provenance, digest, and lifecycle fields | Missing, conflicting, or cross-owner target mutation rejection |
| Deployment preimage drift is ignored | Target mutation claims bind exact preimage presence, fingerprint, and owner | Missing/stale/mismatched preimage rejection |
| Rollback is incomplete or directionally wrong | Typed support level, exact inverse steps, backup identity, restore fingerprint, deterministic inverse sequence | Missing inverse, duplicate inverse, wrong action, wrong backup, and wrong restore fingerprint rejection |
| A receipt is accepted for another plan/provider/phase | Exact request, plan, phase-plan, provider, configuration, environment, and extension bindings | Cross-plan, cross-phase, stale provider, stale config, and stale extension rejection |
| Contract acceptance is presented as operational success | Validation status and phase outcome remain independent; documentation says contract-valid only | Failed/partial/not-attempted receipts can remain structurally valid without being successful |
| Secrets or executable commands leak into durable values | No secret field, no executable path, no free-form shell command, bounded/redacted diagnostics | Forbidden-fragment validator and representative secret/path/command-line rejection |
| Capture time changes semantic idempotency | Request/plan semantic fingerprints exclude capture time; attempt/receipt identity remains separate | Same semantic request with different capture metadata has equal semantic key and distinct receipt identity where required |
| Side effects enter Core through a dependency or helper | Core-only source-family validator and dedicated Core-only test target | Reject Qt, AzToolsFramework, ExternalToolchain, filesystem, process, socket, persistence, deployment, launch, signing, publication, save, and evidence-promotion symbols/includes |
| M1 silently creates persistence or migration obligations | No parser, loader, writer, suffix, registry, reflection persistence, or workspace field | Source-family validator rejects persistence and file APIs; data-format docs label M1 non-durable |
| Existing consumers silently switch to M1 | No adapter/facade or production consumer changes in M1 | Build-graph and exact source-set validator proves no existing production caller was modified |

Any threat control that cannot be implemented and tested inside the authorised paths blocks M1. It is not deferred silently.

## Migration And Compatibility Policy

### Existing contracts

M1 is additive. It must not modify the canonical bytes, field semantics, enum vocabularies, validators, authority flags, result meanings, or tests of:

- `ExternalToolHandoffV1`;
- `UnityConversionRequestV1`;
- `ExternalToolExecutionResultV1`;
- `UnityConversionResultV1`;
- canonical interchange Schema 1;
- existing adapter declarations, work-order plans, build manifests, package previews, staging/deployment previews, deployment work orders, runtime/deployment results, verifier results, reconciliation, release assembly, or release-signing contracts.

Existing `*Allowed` values remain false where their current contracts require it.

### New M1 contracts

- Contract version 1 is introduced as a new Core-only C++ and canonical-JSON contract family.
- M1 introduces no durable file suffix, workspace schema, pack schema, registry, loader, writer, or persisted migration.
- No existing reader or writer is redirected to M1.
- No legacy object is automatically converted into an M1 request, plan, artifact, rollback plan, or receipt.
- Compatibility facades and legacy mappings belong to M4 and require separate authority.
- Unknown contract versions are rejected.
- Once M1 is merged, a breaking semantic or canonical change requires a new contract/canonical profile version or an explicitly reviewed migration; it may not silently rewrite V1.
- Downgrade remains unaffected because no persisted M1 state exists.
- If M1 implementation discovers that persistence, reflection serialization, a file suffix, a registry, or a consumer migration is required, implementation stops for an M0 amendment.

### Source compatibility

Public names are new and isolated under `TaintedGrailModdingSDK::CapabilityExecution`. Existing public names are not renamed, aliased, overloaded, or reinterpreted. Existing production source may include the new headers only in later authorised batches.

## Required Proof For M1

M1 cannot be reported complete until every applicable row below is directly executed against the final reviewed head.

### Preflight and authority

- M0 record present on `main` and cited by the M1 pull request.
- `.codex/scripts/Get-AgentSkillPlan.ps1` run for the exact M1 request and paths.
- `.codex/scripts/Get-AgentTestPlan.ps1` run for the exact M1 request and paths.
- `.codex/scripts/Get-AgentPerformancePlan.ps1` run for the exact M1 request and paths.
- `.codex/scripts/Get-AgentBuildDeployPlan.ps1` run for the exact M1 request and paths.
- pre-edit and post-edit deep review completed.
- evidence pack completed with runtime sign-off explicitly not performed.

### Static and contract validation

```text
git diff --check <accepted-m0-main> HEAD
powershell -ExecutionPolicy Bypass -File .codex/skills/tests/Validate-AgentSkills.ps1
python Gems/TaintedGrailModdingSDK/Tools/validate_capability_execution_contracts.py
python Gems/TaintedGrailModdingSDK/Tools/validate_core_framework_build_graph.py
python Gems/TaintedGrailModdingSDK/Tools/validate_external_tool_interchange_contracts.py
python -m unittest Gems.TaintedGrailModdingSDK.Tools.tests.test_validate_capability_execution_contracts
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going --static-only --skip-source-policy
```

The new validator must enforce the exact six-file production family, exact test manifest, unique Core ownership, dedicated Core-only test target, read-only CI job, required contract fragments, and forbidden side-effect/dependency fragments.

### Pinned O3DE configure and builds

Using the exact `o3de.lock.json` commit and an external `FOA_BUILD_ROOT`:

```text
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py prerequisites --engine-root <pinned-o3de-root> --build-dir <foa-build-root>
python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py configure --engine-root <pinned-o3de-root> --build-dir <foa-build-root>
cmake --build <foa-build-root> --config profile --target TaintedGrailModdingSDK.CapabilityExecution.Tests --parallel 2
ctest --test-dir <foa-build-root> -C profile --output-on-failure --no-tests=error -R "TaintedGrailModdingSDK\\.CapabilityExecution\\.Tests"
cmake --build <foa-build-root> --config profile --target TaintedGrailModdingSDK.CanonicalInterchange.Tests TaintedGrailModdingSDK.Catalog.Tests Editor AssetProcessorBatch --parallel 2
ctest --test-dir <foa-build-root> -C profile --output-on-failure --no-tests=error -R "TaintedGrailModdingSDK\\.(CanonicalInterchange|Catalog)\\.Tests"
```

A zero-test match, missing executable, wrong O3DE revision, wrong project, stale build, or wrong head is a failure.

### Full exact-head validation

```text
python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going --engine-root <pinned-o3de-root> --ctest-build-dir <foa-build-root>
```

The final M1 head also requires the repository's automatic read-only PR validation, the dedicated Core-only compiled job, reviewed-range whitespace evidence, Windows prerequisites, and an exact-head validation receipt satisfying the merge policy.

### Mandatory compiled test coverage

Compiled tests must include:

- every typed enum token and unknown-token rejection;
- minimal valid objects for every contract family;
- complete request, phase plan, execution plan, artifact, rollback, phase receipt, and execution receipt examples;
- deterministic canonical output and known fingerprints;
- permuted set-like inputs producing identical bytes;
- ordered phases/steps preserving order;
- complete input non-mutation;
- duplicate IDs and values;
- malformed and unsupported versions;
- missing and mismatched upstream canonical bindings;
- cross-profile, cross-provider, cross-command, cross-phase, cross-plan, cross-artifact, and cross-owner mismatches;
- safe-relative-path positive and adversarial cases;
- bound and one-over-bound cases for strings, collections, nesting, and canonical bytes;
- checked arithmetic and aggregate-size overflow;
- policy-axis independence;
- terminal-phase reachability and phase dependency ordering;
- exact target preimage, backup, inverse rollback, and receipt binding;
- structurally valid failed, partial, skipped, cancelled, and not-attempted observations remaining distinct from operational success;
- legacy V1 regression tests remaining byte- and behaviour-compatible.

### Performance proof

M1 performance risk is **medium**: pure bounded validation and canonical serialisation, with no UI/event/hot-path integration.

Required proof:

- document worst-case collection and canonical-byte bounds;
- state the asymptotic complexity of validation and canonicalisation;
- prove no filesystem, reflection, process, network, repeated global scan, or hidden cache exists;
- prove checked aggregate-size arithmetic and early bound rejection;
- include deterministic maximum-bound fixtures and one-over-bound rejection;
- record Profile-build command, representative cardinality, and result;
- report any missing performance guard as `PARTIAL` or `BLOCKED`.

A hardware-specific microbenchmark is not required unless implementation introduces superlinear work or the performance preflight classifies the final change as high risk.

### Not applicable to M1

The following are explicitly `NOT_APPLICABLE` for M1 and must not be claimed:

- ExternalToolchain process-supervisor execution;
- provider discovery or execution proof;
- Framework orchestration or durable repository proof;
- Editor/UI interaction evidence;
- Unity conversion;
- package assembly;
- deployment, backup, restore, or rollback execution;
- Fall of Avalon launch or runtime verification;
- save compatibility;
- installer lifecycle;
- signing or publication;
- evidence promotion or release approval.

Runtime sign-off status is:

```text
runtime sign-off not performed
```

## M1 Acceptance Mapping

| Capability-execution gate | M1 disposition |
|---|---|
| CEC-G0 Authority | Must pass through this merged M0 decision |
| CEC-G1 Contract purity | Must pass fully |
| CEC-G2 Backward compatibility | Must pass fully |
| CEC-G3 Provider resolution | Contract shape and exact binding only; resolver execution remains not applicable |
| CEC-G4 Policy separation | Must pass at contract level |
| CEC-G5 Supervisor security | Not applicable; no supervisor exists |
| CEC-G6 Preview/execute parity | Plan/receipt binding shape only; execution remains not applicable |
| CEC-G7 Artifact ownership | Must pass at contract level |
| CEC-G8 Idempotency | Semantic-key inputs and identity rules must pass; execution reuse remains not applicable |
| CEC-G9 Rollback | Plan and receipt invariants must pass; rollback execution remains not applicable |
| CEC-G10 Receipt/evidence separation | Must pass at contract level; no evidence promotion exists |
| CEC-G11 Synthetic spine | Not applicable until M5 |
| CEC-G12 Editor reachability | Not applicable until an Editor capability is authorised |
| CEC-G13 Heightmap runtime | Not applicable until M6 |
| CEC-G14 Release security | Not applicable until M8 |
| CEC-G15 Full-system regression | Core/downstream build and test regression required; runtime rows remain not applicable |

M1 is complete only when every applicable row is `PASSED`; unavailable required proof is `BLOCKED` or `PARTIAL`, never waived or inferred.

## Stop Conditions

M1 must stop before writing or continue only through an amended M0 decision if any of these occurs:

- another source or process path is required;
- an existing V1 contract, canonical byte, validator, enum, or authority flag must change;
- a Framework, Editor, ExternalToolchain, provider, plug-in, installer, runtime-adapter, game, save, signing, publication, or deployment dependency is proposed;
- a durable schema, parser, loader, writer, reflection format, registry, suffix, or migration becomes necessary;
- provider selection, policy evaluation, authorisation, execution, cancellation, persistence, or evidence promotion would enter M1;
- exact owner, consumer, version, bound, compatibility, or validation behaviour is unresolved;
- a required compiled test, validator, CI job, exact-head receipt, or downstream regression cannot be executed;
- protected files or external destinations become relevant;
- the implementation cannot preserve the Core-only build boundary;
- the proposed contract cannot represent the canonical architecture without collapsing independent decision axes.

No substitute implementation is authorised when a stop condition is reached.

## Post-M1 Boundary

A merged M1 result grants no authority to start M2.

The next possible transition is a new, separately authorised M0 decision for:

```text
M2 — ExternalToolchain Process Supervisor
```

That later decision must identify its own exact paths, threat model, process and secret boundary, provider contract version, cancellation and cleanup design, persistence/receipt policy, test harness, and required proof. M1 completion alone does not satisfy those requirements.
