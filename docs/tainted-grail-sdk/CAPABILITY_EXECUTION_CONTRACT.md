# Capability Execution Contract And Shared Production Spine

Status: accepted architecture and process direction for maintainer audit; implementation remains separately authorised by batch

Owner systems: `capability-execution`, `artifact-ownership`, `execution-receipts`, `runtime-verification`

## Decision

FOA-SDK keeps its existing capability assessment, adapter planning, build-manifest, package-preview, staging/deployment-preview, deployment-work-order, result-evidence, verification, reconciliation, and release metadata systems as the **control plane**.

Actual side effects are introduced only through a separately reviewed additive **execution plane**:

```text
canonical authoring
-> capability planning
-> immutable phase plans
-> Build
-> Package
-> Deploy
-> Launch
-> Verify
-> assessment
-> reconciliation
-> human promotion when required
```

This document is an architecture and process lock. It does not add an executor, process launcher, deployment command, game launch path, verifier, signer, publisher, save mutation, evidence promotion, or runtime permission.

## Why This Contract Exists

The repository already contains mature, deterministic, fail-closed contracts for capability compatibility, work-order planning, build definitions, package layouts, target diffs, backups, rollback descriptions, deployment confirmations, execution-result observations, independent verification metadata, release provenance, assembly results, and signing results.

Those contracts should not be discarded or made executable by changing existing flags. The missing component is a shared execution plane that consumes exact immutable planner output and returns exact receipts.

Without one canonical spine, terrain, roads, AI, economy, population, assets, external conversion, runtime adapters, and release tooling could each invent incompatible provider resolution, staging, deployment, rollback, evidence, and result semantics.

## Non-Negotiable Invariants

1. Existing inert V1 contracts remain inert.
2. Future executable contracts version forward; V1 validators are not relaxed.
3. Existing `*Allowed` flags are not production execution switches.
4. Preview and execute are separate operations bound to one canonical plan fingerprint.
5. Support, qualification, environment, policy, authorisation, outcome, assessment, promotion, and release decision remain independent.
6. Provider resolution is deterministic and phase-specific.
7. Domain systems may own native materialisation and expected domain observations only.
8. Build, package, deployment, launch, rollback, receipts, and evidence handoff use one shared spine.
9. Artifact identity, ownership, producer, custodian, digest, lifecycle, and redistribution state are explicit.
10. Idempotency binds semantic request, inputs, providers, configuration, plans, and target preimages.
11. Rollback is planned and validated before execution.
12. Execution receipts are observations; they do not promote evidence or grant permission.
13. Assessment and reconciliation remain downstream of independent observations.
14. Human promotion and final release authority remain separate where governance requires them.
15. Missing authority, proof, provider qualification, artifact ownership, preimage, backup, rollback, or validation fails closed.

## Architecture Ownership

| Responsibility | Canonical owner |
|---|---|
| Stable capability semantics | TG SDK Core |
| Canonical requests, plans, receipts, validation, fingerprints | TG SDK Core |
| FoA exact-profile qualification | TG SDK Core and Framework |
| Evidence and governance decisions | Framework/Foundation |
| Execution policy | Framework |
| Human authorisation bound to immutable plans | Framework |
| Generic provider identity, commands, configuration, discovery | ExternalToolchain |
| Generic process supervision | ExternalToolchain execution API, added only in M2 |
| Pipeline orchestration | TG SDK Framework |
| Artifact repository and target ownership ledger | TG SDK Framework |
| Domain-native conversion/materialisation | Domain provider |
| Build implementation | Build provider |
| Package implementation | Package provider |
| Deployment and rollback implementation | Deployment provider |
| Game launch | Runtime launch provider |
| Independent verification | Verification provider |
| User interaction | Thin Editor commands and panes |
| Candidate evidence projection | Existing evidence services |
| Assessment and reconciliation | Existing verification/reconciliation services |
| Final promotion and release decision | Human governance |

Core remains free of Qt, filesystem mutation, process launch, network access, private game assemblies, and runtime libraries.

## Existing Service Disposition

### Capability And Adapter Contracts

| Current surface | Required disposition |
|---|---|
| `AdapterCapability` | Remain the V1 economy/runtime capability vocabulary; do not turn it into the universal capability vocabulary |
| `AdapterDeclaration` | Remain a legacy declaration and compatibility input to future provider bindings |
| `AdapterContractRegistry::{RegisterDeclaration,FindByAdapterId,GetDeclarations}` | Become compatibility facades over a future capability-provider binding registry after consumer migration |
| `AdapterCompatibilityService::BuildCapabilityMatrix` | Remain owner of the compatibility projection, consuming separate support, qualification, environment, policy, and authorisation decisions |
| `FoARuntimeAdapterRoutes::{GetCanonicalRoutes,ValidateRoute}` | Remain V1 route-identity and validation owners |
| `FoARuntimeAdapterRoutes::{FindRoute,Qualify}` | Become compatibility facades over typed provider lookup and qualification decisions |
| Route/request/result `m_*Allowed` fields | Remain serialized V1 false values and retire from production control flow |

### Domain Planning

| Current surface | Required disposition |
|---|---|
| `AdapterWorkOrderPlanningService::BuildPlans` | Remain owner of exact domain mutation intent and deterministic refusal |
| `SerializeCanonicalPlan` | Remain canonical-plan owner |
| `AdapterWorkOrderPlan` | Remain a legacy domain-plan input to the shared execution request |
| Plan and step `m_executionAllowed` | Remain V1 false values and retire from production control flow |

### Build

| Current surface | Required disposition |
|---|---|
| `AdapterBuildManifestService::BuildManifest` | Remain the pure Build preview implementation |
| `SerializeCanonicalManifest` | Remain canonical Build-plan owner |
| `AdapterBuildManifest` | Remain the canonical V1 build definition consumed by a future Build phase adapter |
| `m_buildAllowed` | Remain false in V1 and retire from production control flow |
| Future `BuildPhaseService::Preview` | Delegate to the existing manifest service |
| Future `BuildPhaseService::Execute` | Dispatch the exact accepted manifest to a reviewed Build provider |

### Package

| Current surface | Required disposition |
|---|---|
| `AdapterPackageAssemblyPreviewService::BuildPreview` | Remain the pure Package preview implementation |
| `SerializeCanonicalPreview` | Remain canonical Package-plan owner |
| `AdapterPackageAssemblyPreviewRegistry` | Become a compatibility view over durable execution/artifact repositories |
| Package `m_*Allowed` fields | Remain false in V1 and retire from production control flow |
| Future `PackagePhaseService::Execute` | Assemble and verify the exact preview through a reviewed provider |

### Deployment And Authorisation

| Current surface | Required disposition |
|---|---|
| `AdapterStagingDeploymentPreviewService::BuildPreview` | Remain owner of target diff, backup requirements, and inverse rollback planning |
| `AdapterDeploymentWorkOrderService::BuildWorkOrder` | Remain owner of explicit confirmation, scope, expiry, maintenance-window, preflight, and deterministic operator work order |
| Preview/work-order registries | Become compatibility views over durable execution state |
| Deployment/work-order `m_*Allowed` fields | Remain false in V1 and retire from production control flow |
| Future `DeploymentPhaseService::Execute` | Perform exact backup/copy/replace/delete/verification steps |
| Future `DeploymentPhaseService::Rollback` | Consume the preplanned inverse sequence and verified backups |

### Runtime And Deployment Results

| Current surface | Required disposition |
|---|---|
| `AdapterRuntimeResultEnvelope` | Remain the legacy Runtime phase receipt extension |
| `AdapterDeploymentExecutionResultEnvelope` | Remain the Deployment phase receipt extension |
| Bound result registration | Become a compatibility facade over the durable execution repository |
| Unbound result registration | Retire after callers migrate |
| Runtime/deployment evidence services | Remain candidate-evidence projection owners |

### Verification And Reconciliation

| Current surface | Required disposition |
|---|---|
| `AdapterPostDeploymentVerificationService::BuildReport` | Remain assessment owner after independent observations exist |
| `AdapterPostDeploymentVerifierResultEnvelope` | Remain the Verify phase receipt extension |
| Registry-bound verifier evidence overload | Remain owner |
| Registry-free fail-closed overload | Retire after caller migration |
| `AdapterVerifierEvidenceReconciliationService::BuildReconciliation` | Remain governance and human-disposition owner |
| Future `VerificationPhaseService::Preview/Execute` | Define and perform independent checks before assessment |

The required distinction is:

```text
VERIFY
provider observes installed/runtime state

ASSESS
existing report service compares observations with expected state

RECONCILE
existing reconciliation service preserves blockers and human disposition
```

### Release

| Current surface | Required disposition |
|---|---|
| `AdapterReleaseArtifactProvenanceService::BuildEnvelope` | Remain provenance/legal/signing-intent/publication-target validation owner |
| `AdapterReleaseAssemblyResultEnvelope` | Remain Release Assemble receipt extension |
| `AdapterReleaseAssemblyEvidenceService` | Remain assembly evidence owner |
| `AdapterReleaseSigningResultEnvelope` | Remain Release Sign receipt extension |
| `AdapterReleaseSigningEvidenceService` | Remain signing evidence owner |
| Future release assemble/sign execute services | Invoke isolated reviewed providers; publication remains a separate optional phase |

### ExternalToolchain And Interchange

| Current surface | Required disposition |
|---|---|
| Provider/command descriptors and registry | Remain generic provider identity and registration owners |
| Configuration service | Remain layered configuration owner; enabled does not mean authorised |
| Discovery service | Remain bounded installation-discovery owner; installed does not mean qualified or executable |
| Existing request bus | Remain registration/configuration/discovery API |
| Future execution request bus/process supervisor | New additive M2 execution owner |
| Gate 0 `ExternalToolHandoffV1`, Unity request, and result contracts | Freeze permanently inert |
| Existing V1 canonicalisers and validators | Never relax |
| Future executable external-tool contracts | Version-forward additive contracts |
| `CanonicalInterchangeManifestV1` | Remain neutral asset/package, provenance, licensing, transformation, and loss schema |

## Canonical Contract Model

The additive contract family is `foa-capability-execution-v1`. Names below define ownership and required fields; exact C++ layout requires a separately reviewed M1 design.

### Capability Descriptor

Defines stable capability identity, accepted input and output contract IDs, required and optional phases, side-effect classes, terminal phase, exact-profile requirement, runtime requirement, save impact, and rollback requirement.

Examples include:

```text
terrain.heightmap.materialize
terrain.native.build
road-atlas.native.build
avalon-ai.package.build
economy.item.register
population.actor.register
asset.model.import
pack.build
pack.deploy
game.launch
game.runtime.verify
```

### Capability Provider Binding

Binds one capability phase to exact provider ID, provider version constraint, command ID, accepted/produced contract IDs, exact-profile constraints, qualification evidence, side effects, cancellation/resume support, rollback support, and binding fingerprint.

A provider is resolved per phase, not once for the complete operation.

### Capability Execution Request

Binds request, workspace, pack, exact profile fingerprint, capability, terminal phase, ordered input artifacts, preferred provider bindings, canonical options, and semantic request fingerprint.

It describes desired outcome. It contains no execute-now Boolean.

### Independent Decisions

Each decision is canonical, fingerprinted, evidence-bound, and independently reasoned:

```text
support: SUPPORTED | UNSUPPORTED
qualification: QUALIFIED | UNQUALIFIED | STALE | UNKNOWN
environment: AVAILABLE | UNAVAILABLE | DRIFTED | UNKNOWN
policy: ALLOWED | CONFIRMATION_REQUIRED | DENIED
authorisation: NOT_REQUIRED | PENDING | GRANTED | EXPIRED | REVOKED | SCOPE_MISMATCH
```

No state implies another.

### Phase Plan

Every phase plan binds phase identity, provider binding, provider and command identity/version/fingerprint, ordered inputs, expected outputs, target mutation claims, configuration and environment fingerprints, rollback plan, canonical JSON, and phase-plan fingerprint.

### Execution Plan

The complete plan binds the semantic request, all independent decisions, ordered phase plans, canonical JSON, and plan fingerprint.

Execution must consume this exact immutable plan.

### Phase And Execution Receipts

Every phase receipt binds exact phase-plan fingerprint, provider identity/version/fingerprint, outcome, timestamps, output artifacts, target observations, failures, diagnostics, rollback/cleanup results, and a phase-specific extension contract.

The complete execution receipt binds plan, authorisation receipt, phase receipts, rollback receipt, assessment reference, and final receipt fingerprint.

Existing adapter runtime, deployment execution, verifier, release assembly, and release signing envelopes remain phase-specific extensions rather than duplicated generic data.

## Lifecycle States

Overall execution states:

```text
DRAFT
VALIDATED
RESOLVED
QUALIFIED
PLANNED
AWAITING_AUTHORIZATION
READY
EXECUTING
VERIFYING
SUCCEEDED
REJECTED
RESOLUTION_FAILED
QUALIFICATION_FAILED
POLICY_DENIED
AUTHORIZATION_EXPIRED
ENVIRONMENT_DRIFTED
FAILED
PARTIAL
CANCELLATION_REQUESTED
CANCELLED
ROLLBACK_REQUIRED
ROLLING_BACK
ROLLED_BACK
ROLLBACK_FAILED
SUPERSEDED
ARCHIVED
```

Phase states:

```text
NOT_PLANNED
PENDING
READY
RUNNING
SUCCEEDED
FAILED
SKIPPED
BLOCKED
CANCELLED
ROLLBACK_PENDING
ROLLED_BACK
ROLLBACK_FAILED
```

Key transition rules:

1. Validation requires canonical request validity.
2. Resolution requires exactly one eligible provider binding for every required phase.
3. Qualification requires current exact-profile evidence.
4. Planning requires deterministic phase plans and rollback declarations.
5. Ready requires allowed policy or a valid exact-plan authorisation receipt.
6. Execution revalidates provider/environment fingerprints and target preimages.
7. A replan creates a new fingerprint and invalidates prior authorisation.
8. Deployment success followed by failed verification is not success.
9. Rollback never rewrites the original failure into success.
10. Target or provider drift fails closed and requires a new preview.

## Deterministic Provider Resolution

For every phase:

1. Validate capability and terminal phase.
2. Enumerate semantic capability-provider bindings.
3. Resolve referenced ExternalToolchain providers and commands.
4. Require finalised registration and host API compatibility.
5. Require exact version, platform, profile, input, and output compatibility.
6. Require current qualification evidence.
7. Require valid configuration and installed discovery state.
8. Require declared side effects and rollback/cancellation properties.
9. Apply explicit request selection, then exact workspace/project default.
10. Otherwise require exactly one eligible binding.
11. Multiple eligible bindings return `AMBIGUOUS`.
12. No eligible binding returns deterministic unsupported/unqualified/environment reasons.
13. Persist selected provider, command, executable observation, configuration, and environment fingerprints into the plan.

There is no heuristic best-provider selection and no automatic fallback after execution begins.

## Policy Separation

Use side-effect classes rather than broad execution Booleans:

| Side effect | Example | Normal policy |
|---|---|---|
| `READ_ONLY` | Catalog or target inspection | Allowed |
| `WORKSPACE_WRITE` | Neutral authoring revision | Allowed or configured |
| `STAGING_WRITE` | Build/package output | Allowed |
| `PROCESS_LAUNCH` | Compiler, Unity, game launch | Policy or confirmation |
| `INSTALLATION_MUTATION` | Deploy into FoA installation | Confirmation required |
| `RUNTIME_MUTATION` | Register item or spawn actor | Capability-specific confirmation |
| `SAVE_MUTATION` | Persistent game-state edit | Denied until separately proven |
| `SECRET_USE` | Signing key | Dedicated isolated approval |
| `NETWORK_PUBLICATION` | Release upload | Dedicated approval |
| `DESTRUCTIVE_DELETE` | Remove installed/release files | Exact ownership and confirmation |

Provider `Enabled`, discovery `Installed`, compatibility `Supported`, plan `Ready`, result `Accepted`, or test `Passed` is not policy or authorisation.

## Shared Build -> Package -> Deploy -> Launch -> Verify Spine

An optional domain materialisation phase converts canonical authoring data into pack-owned native build inputs. Everything afterward uses the shared phases.

| Phase | Preview owner | Execute owner | Receipt |
|---|---|---|---|
| Build | Existing build-manifest service through a phase adapter | Reviewed Build provider | Generic receipt plus build extension |
| Package | Existing package-preview service | Reviewed Package provider | Generic receipt plus artifact inventory |
| Deploy | Existing staging preview and work-order services | Reviewed Deployment provider | Generic receipt plus deployment envelope |
| Launch | New launch-plan builder | Reviewed runtime launch provider | Generic receipt plus process/runtime extension |
| Verify | New expected-observation planner | Independent verifier provider | Generic receipt plus verifier envelope |
| Assess | Existing post-deployment report service | Pure assessment | Verification report |
| Reconcile | Existing reconciliation service | Human governance | Reconciliation envelope |

Editor panes call one Framework execution service. They do not call process, filesystem, deployment, game, signing, or publication APIs directly.

## External Process Supervisor

The future ExternalToolchain execution API must use argument vectors and a reviewed host-owned supervisor. It requires:

- executable identity from bounded discovery;
- no shell or implicit PATH search;
- explicit working directory;
- allowlisted environment;
- secret handles rather than secret values;
- bounded and redacted stdout/stderr;
- timeout, cancellation, and child-process-tree cleanup;
- declared input/output roots;
- no undeclared writes;
- local filesystem only by default;
- symlink/reparse-point boundary checks;
- provider and target concurrency locks;
- exit code and process identity capture;
- output-manifest validation before success;
- no provider-selected deployment destination.

## Artifact Ownership

Every artifact records:

- stable artifact ID;
- role and media type;
- storage root and safe relative path;
- SHA-256 and byte size;
- semantic owner pack;
- producer execution, phase, and provider;
- source manifest and fingerprint;
- lifecycle;
- redistribution state.

Ownership dimensions stay separate:

1. semantic owner;
2. producer;
3. storage custodian;
4. target mutation owner;
5. source provenance/licensing.

Every target mutation claim records exact target root/path, operation, expected preimage presence/fingerprint/owner, desired artifact/fingerprint, backup artifact, and rollback action.

No replacement/removal occurs without exact preimage and owner binding. Installed ownership changes only after successful target verification.

## Idempotency

The operation key binds:

```text
canonical semantic request
+ exact profile fingerprint
+ ordered input artifact fingerprints
+ ordered provider-binding fingerprints
+ provider configuration fingerprints
+ phase-plan fingerprints
+ target inventory/preimage fingerprint
```

Timestamps, UI labels, transient IDs, and log paths are excluded.

Required behaviour:

- duplicate running invocation attaches to the existing execution;
- completed verified output may be reused;
- artifact or target drift requires a new preview;
- retryable failure creates a new attempt in the same lineage;
- non-retryable failure requires a new request/plan;
- same identity with different canonical fingerprint is a collision;
- matching deployed bytes and owner are a verified no-op;
- matching bytes with different owner are a conflict;
- provider/configuration or target-preimage change invalidates the plan.

## Rollback

Rollback support levels are:

```text
NONE
CLEANUP_ONLY
COMPENSATING
EXACT_RESTORE
```

Rules:

1. Plan rollback during preview.
2. Back up before replacement/removal.
3. Verify backup hash before mutation.
4. Execute rollback in strict inverse order.
5. Verify target still matches the deployed fingerprint before restoring.
6. Never overwrite later third-party changes automatically.
7. Restore only from the backup bound to the original preimage.
8. Record every attempted, succeeded, failed, and skipped rollback row.
9. Successful rollback does not change the original execution outcome.
10. `ROLLBACK_FAILED` requires manual review.

Signing and publication are irreversible unless separately proven compensation exists.

## Results, Evidence, Assessment, And Promotion

Every receipt records exact plan/provider/configuration/environment bindings, attempted state, timestamps, exit code when applicable, outputs, target observations, failures, diagnostics, cleanup, rollback, and phase extension.

The required evidence path is:

```text
execution receipt
-> contract validation
-> candidate source/evidence documents
-> assessment
-> reconciliation
-> human promotion when required
```

Executors may not:

- register promoted evidence;
- clear blockers;
- grant permission;
- mark release approved;
- publish automatically;
- convert contract acceptance into operational success.

## Migration Batches

### M0 — Governance And Implementation Authority

Accept exact source paths, owners, versioning, threat/failure analysis, validation lanes, and current-task authority. No source implementation begins before M0.

### M1 — Additive Core Contracts

Add pure capability, provider-binding, plan, artifact, rollback, and receipt contracts with Core-only dependencies and unchanged V1 canonical fixtures.

### M2 — ExternalToolchain Process Supervisor

Add separate execution API, invocation V2 contracts, bounded supervisor, cancellation/status, and durable invocation records. Default remains fail-closed.

### M3 — Framework Orchestrator And Repositories

Add provider binding/qualification/policy services, execution planning/orchestration, durable execution repository, artifact repository, target ownership ledger, and evidence projection.

### M4 — Existing Planner Adaptation

Wrap existing build, package, deployment, work-order, and assessment services as pure phase previews/assessment. Do not add side effects to them.

### M5 — Isolated Synthetic Spine

Prove Build -> Package -> Deploy -> Launch -> Verify -> Rollback against a harmless sandbox provider and target before touching the game installation.

### M6 — Heightmap Vertical Slice

Connect terrain canonical documents through native materialisation and the shared production spine. Require exact profile, deployment, runtime observation, and rollback proof.

### M7 — Ordered Domain Migration

Migrate Road Atlas, Avalon AI, items/recipes, actors/troops, then model/texture/animation/audio/material importers. Each domain supplies only native materialisation and domain verification.

### M8 — Release Execution

Add isolated provider-backed assembly, checksum, signing, and optional publication. Secrets remain opaque handles and publication remains separately authorised.

### M9 — Facade Retirement And Reachability CI

Stop production reads of V1 permission flags, retire unbound/fail-closed compatibility callers after migration, preserve V1 deserialisation/canonical fixtures, and fail CI for orphan or preview-only capabilities represented as complete.

## Acceptance Gates

| Gate | Required proof |
|---|---|
| CEC-G0 Authority | Accepted decision and exact current-task implementation authority |
| CEC-G1 Contract purity | Core-only compile boundary and deterministic canonical contracts |
| CEC-G2 Backward compatibility | Existing V1 canonical bytes and validator behaviour unchanged |
| CEC-G3 Provider resolution | Exact deterministic selection; unsupported/ambiguous fail closed |
| CEC-G4 Policy separation | Tests prove no decision axis implies another |
| CEC-G5 Supervisor security | No shell; bounded args/env/output; cancellation, path, process-tree, and secret isolation |
| CEC-G6 Preview/execute parity | Executor consumes exact plan; all drift rejected |
| CEC-G7 Artifact ownership | Every artifact/mutation has immutable owner, producer, digest, and lifecycle |
| CEC-G8 Idempotency | Duplicate, resume, retry, no-op, and collision behaviour verified |
| CEC-G9 Rollback | Failure injection proves inverse-order exact restoration |
| CEC-G10 Receipt/evidence separation | Receipts become candidate evidence only; no automatic promotion |
| CEC-G11 Synthetic spine | Complete isolated Build -> Package -> Deploy -> Launch -> Verify -> Rollback proof |
| CEC-G12 Editor reachability | Every executable capability has a command, provider, and terminal phase |
| CEC-G13 Heightmap runtime | Exact-profile build, deploy, load, verify, and rollback proof |
| CEC-G14 Release security | Assembly/checksum/signature verification with no secret leakage |
| CEC-G15 Full-system regression | Exact-head build, tests, Editor, compatibility, rollback, and runtime gates |

## Retirement Conditions

| Legacy surface | Retirement condition |
|---|---|
| V1 `m_*Allowed` production reads | No production consumer remains; V1 still serialises required false values |
| Registry-free or unbound result/evidence overloads | All callers use exact bound repositories and evidence registry |
| Singleton preview/request/result registries | Durable repositories and compatibility tests are complete |
| Direct pane-owned plan construction | All commands route through the Framework execution service |
| Preview-only completion claims | Reachability and terminal-phase validation are enforced |
| External Tool Interchange V1 as execution candidate | Never; V1 stays permanently inert |
| Per-domain build/deploy/launch infrastructure | Never introduced |

## Prohibited Shortcuts

The following are architecture violations:

1. Flip existing V1 `Allowed` flags.
2. Add filesystem/process side effects to Core preview or evidence services.
3. Let Qt widgets invoke tools or mutate installations directly.
4. Treat discovery as execution permission.
5. Use one provider decision for all phases.
6. Fall back to another provider after execution starts without a new preview.
7. Recompute a plan during execution.
8. Deploy without exact preimage, ownership, backup, and rollback.
9. Create separate execution spines for terrain, roads, AI, economy, population, assets, or release.
10. Treat result registration as proof or promotion.
11. Treat rollback success as original execution success.
12. Relax Gate 0 V1 rather than version forward.
13. Pass free-form shell commands.
14. Place secrets/signing keys in plans, logs, fingerprints, or receipts.

## Required Process Route

Every affected task follows:

```text
request
-> repository governance and research authority
-> owner and blast-radius classification
-> this canonical contract
-> .codex/workflows/foa_capability_execution_contract.md
-> compatibility, test, performance, and artifact/deployment gates
-> evidence pack
-> maintainer-audited pull request
-> next researched stop/process
```

The next researched implementation stop is **M0 — Governance And Implementation Authority**. Documentation acceptance does not authorise M1 or any later batch.
