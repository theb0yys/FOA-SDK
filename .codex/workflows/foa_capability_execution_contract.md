# FOA-SDK Capability Execution Contract Gate

This workflow is mandatory for any task involving capability execution, adapters, ExternalToolchain provider invocation, build manifests, package assembly, staging, deployment, launch, verification, rollback, runtime or deployment result envelopes, release assembly/signing, artifact ownership, or execution receipts.

The canonical architecture authority is:

- `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`
- `DECISIONS.md` — Capability Execution Contract And Shared Production Spine

This workflow does not authorise implementation. It governs how a separately authorised implementation must be researched, designed, migrated, validated, and handed off.

## Gate C0: Exact Authority

Before editing:

1. Name the current user-authorised transition: design, contract, preview, executor, provider, migration, validation, or documentation.
2. Identify the owner systems through `docs/systems/SYSTEM_INDEX.md`.
3. Read the existing owner contracts and their tests.
4. Identify whether the current service remains an owner, becomes a preview adapter, becomes a compatibility facade, or is retired from production control flow.
5. Stop when implementation authority, exact profile evidence, provider qualification, artifact ownership, rollback requirements, or validation lanes are missing.

A proposal, roadmap item, prior analysis, `Next researched task:`, ready preview, accepted result envelope, or existing false permission flag does not authorise execution.

## Gate C1: Preserve The Control Plane

The following existing service families remain control-plane owners unless a separately accepted decision says otherwise:

- adapter capability declarations and compatibility assessment;
- domain work-order planning;
- build-manifest planning;
- package-assembly preview;
- staging/deployment preview;
- deployment confirmation and work-order planning;
- runtime and deployment result contracts;
- post-deployment assessment and verifier evidence;
- evidence reconciliation and human release decision;
- release provenance, assembly-result, and signing-result evidence;
- ExternalToolchain provider registration, configuration, and discovery;
- canonical interchange schemas and validation.

Do not move filesystem, process, deployment, launch, signing, publication, game, or save side effects into pure Core planning or evidence services.

## Gate C2: Version Forward

- Existing inert V1 contracts remain inert.
- Existing `BuildAllowed`, `ExecutionAllowed`, `DeploymentAllowed`, mutation, launch, signing, publication, and equivalent fields remain `false` where required by V1.
- Do not relax V1 validators, widen V1 outcome enums, or reinterpret a V1 `ready`/`accepted` status as execution permission.
- Executable behaviour requires a new reviewed contract version and migration/compatibility decision.

## Gate C3: Preview And Execute Pairs

Every side-effecting phase requires:

```text
semantic request
-> deterministic preview
-> immutable canonical phase plan
-> plan fingerprint
-> policy decision
-> human authorisation when required
-> environment and preimage revalidation
-> execute exact plan
-> phase receipt
```

Preview and execute are separate methods. Execution must not regenerate the plan from mutable UI state or silently repair a stale preview.

## Gate C4: Shared Production Spine

Use one shared spine:

```text
optional domain materialisation
-> Build
-> Package
-> Deploy
-> Launch
-> Verify
-> assessment
-> reconciliation
-> human promotion when required
```

Domain systems may own native materialisation and expected domain observations. They must not own private build, package, deployment, launch, rollback, receipt, or evidence-promotion infrastructure.

## Gate C5: Deterministic Provider Resolution

Resolve a provider separately for every phase. Require:

- finalised provider registration;
- exact provider and command identity;
- host API and semantic-version compatibility;
- exact platform and profile compatibility;
- accepted input/output contract compatibility;
- current qualification evidence;
- valid configuration and installed discovery state;
- declared side effects, cancellation, resume, and rollback support;
- explicit selection or exactly one eligible provider.

Multiple eligible providers are `AMBIGUOUS`. No eligible provider is `UNSUPPORTED` or `UNQUALIFIED` with exact reasons. Do not use heuristic best-provider selection or automatic fallback after execution begins.

## Gate C6: Separate Decisions

Keep these axes independent:

- support;
- qualification;
- environment readiness;
- policy;
- human authorisation;
- execution outcome;
- verification observation;
- assessment;
- evidence promotion;
- release decision.

One state never grants another. ExternalToolchain `Enabled`, discovery `Installed`, compatibility `Supported`, preview `Ready`, result `Accepted`, or test `Passed` does not grant execution or promotion authority.

## Gate C7: Artifact Ownership And Idempotency

Every input, output, backup, installed target, log, receipt, and signature artifact requires stable identity, SHA-256, byte size, media type, semantic owner, producer execution/phase/provider, storage custodian, lifecycle, and redistribution state where applicable.

The idempotency key binds canonical request, exact profile, ordered input fingerprints, provider bindings, configuration, phase plans, and target preimage. Duplicate running work attaches; verified completed work may be reused; drift requires a new preview; identity/fingerprint collision fails closed.

No replacement or removal occurs without exact target preimage and owner binding.

## Gate C8: Rollback Before Execution

Rollback is designed during preview:

- build/package normally provide cleanup;
- deployment additions provide exact removal;
- replacement/removal require immutable verified backups and exact restore;
- launch may terminate only the process started and owned by the execution;
- runtime/save mutation requires separately proven compensating behaviour;
- signing/publication are declared irreversible unless exact compensation exists.

Rollback runs in strict inverse order and never converts the original failed outcome into success. Target drift before rollback blocks automatic overwrite.

## Gate C9: Receipts, Evidence, Assessment, And Promotion

Execution produces immutable phase receipts bound to exact plan and provider fingerprints.

Existing adapter runtime, deployment execution, independent verifier, release assembly, and release signing envelopes remain phase-specific receipt extensions.

The required path is:

```text
execution receipt
-> contract validation
-> candidate source/evidence projection
-> assessment
-> reconciliation
-> human promotion when required
```

Executors do not register promoted evidence, clear blockers, grant permission, approve release, or publish automatically.

## Gate C10: Migration Batches

Follow the ordered batches in the canonical contract:

- M0 governance and implementation authority;
- M1 additive Core execution contracts;
- M2 ExternalToolchain process supervisor;
- M3 Framework execution/artifact repositories and orchestrator;
- M4 adaptation of existing planners into phase previews;
- M5 isolated synthetic shared-spine proof;
- M6 heightmap vertical slice;
- M7 ordered domain migration;
- M8 release execution;
- M9 compatibility-facade retirement and reachability CI.

Do not skip directly from documentation to a domain executor.

## Gate C11: Acceptance Gates

Map the affected batch to the canonical CEC-G0 through CEC-G15 acceptance gates. Missing required proof is `PARTIAL` or `BLOCKED`, not `PASSED`.

Static review, source inspection, compilation, package preview, or result-envelope validation are not process execution, target persistence, Editor behaviour, Unity conversion, deployment, save compatibility, or Fall of Avalon runtime proof.

## Prohibited Shortcuts

Stop if the proposed change would:

- flip an existing V1 permission flag;
- add side effects to Core preview/evidence services;
- let widgets invoke tools or mutate targets directly;
- treat discovery as execution permission;
- use one provider decision for every phase;
- switch providers after execution begins without a new plan;
- recompute the plan during execution;
- deploy without exact preimage, ownership, backup, and rollback;
- create a domain-private execution spine;
- treat receipt registration as proof or promotion;
- treat rollback as original success;
- pass free-form shell commands;
- put secrets or signing keys in plans, logs, configuration fingerprints, or receipts.

## Required Handoff

Report:

- exact authority and implementation batch;
- existing services classified as owner/preview adapter/facade/retirement;
- selected systems and providers;
- immutable plan and compatibility status;
- policy and authorisation status;
- artifact ownership and idempotency status;
- rollback and receipt/evidence status;
- acceptance gates run, failed, skipped, or blocked;
- `runtime sign-off not performed` unless exact-install runtime proof actually ran;
- next researched stop/process.
