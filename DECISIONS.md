# Durable Decisions

Record accepted FOA-SDK architecture and process decisions here. Active work belongs in `CURRENT_TASK.md`; durable decisions belong here.

## Repository State Is Authority

- **Decision:** Repository files, governing documents, current task state, durable decisions, recent diffs, and relevant code comments are authoritative. Conversation history is background only unless the repository owner explicitly overrides it for the current task.
- **Rationale:** This prevents context drift and preserves auditable continuation state.
- **Scope:** All repository work.
- **Status:** Accepted.

## Research Authority Is Required

- **Decision:** Implementation stops when exact controlling research, ownership, compatibility, validation, or next-process authority is missing, unclear, contradictory, outdated, or unproven.
- **Rationale:** FOA-SDK must not invent game facts, runtime assumptions, native identities, permissions, or architecture.
- **Scope:** Code, tests, documentation, process, packaging, adapters, and release work.
- **Status:** Accepted.

## Context-Only Process Port

- **Decision:** The Waning Realm agent operating model is ported to FOA-SDK without behavioural redesign. Only project-context substitutions are permitted.
- **Rationale:** The requested outcome is process parity, not a new workflow.
- **Scope:** Root agent policy integration and `.codex/` process assets.
- **Status:** Accepted.

## Capability Execution Contract And Shared Production Spine

- **Decision:** FOA-SDK retains its existing capability assessment, work-order planning, build-manifest, package-preview, staging/deployment-preview, deployment-work-order, result-evidence, verification, reconciliation, and release metadata services as the control plane. Actual side effects must be introduced only through a separately reviewed additive execution plane governed by [`docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`](docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md).
- **Decision:** The canonical production path is one shared `Build -> Package -> Deploy -> Launch -> Verify` spine. Domain systems may own native materialisation and domain verification semantics, but they must not create private build, deployment, launch, rollback, receipt, or evidence-promotion paths.
- **Decision:** Existing inert V1 contracts and their `BuildAllowed`, `ExecutionAllowed`, `DeploymentAllowed`, mutation, launch, signing, publication, and equivalent flags remain inert. Future executable contracts version forward; validators are not relaxed and flags are not flipped to activate behaviour.
- **Decision:** Preview and execute are distinct operations over one immutable fingerprinted plan. Support, qualification, environment readiness, policy, human authorisation, execution outcome, assessment, and evidence promotion remain separate state axes.
- **Decision:** Provider resolution is deterministic and phase-specific. Artifact identity and ownership, idempotency, rollback planning, execution receipts, candidate evidence, assessment, reconciliation, and human promotion remain explicit boundaries.
- **Rationale:** This activates existing reviewed planning and evidence infrastructure without discarding it, prevents contract drift, and prevents terrain, roads, AI, economy, population, asset, and release tooling from developing incompatible execution systems.
- **Scope:** Capability contracts, adapters, ExternalToolchain providers, build, package, deployment, launch, verification, rollback, results, evidence, release assembly/signing, and all future domain execution work.
- **Status:** Accepted architecture and process direction; implementation remains batch-scoped and requires explicit current-task authority, review, validation, and maintainer merge.

## Capability Execution M0 Authority For M1 Core Contracts

- **Decision:** The repository owner authorises the M0 governance and implementation boundary recorded in [`docs/tainted-grail-sdk/CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md`](docs/tainted-grail-sdk/CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md).
- **Decision:** After that record is merged to `main`, exactly one implementation batch is authorised: **M1 — Additive Core Contracts** on `implementation/capability-execution-m1-core-contracts`, limited to the exact paths named by the M0 record.
- **Decision:** M1 may add Core-only value types, typed vocabularies, canonical JSON, fingerprints, pure validation, a dedicated Core-only compiled test target, static enforcement, read-only CI coverage, and matching documentation. It may not add a registry, resolver, persistence, Framework service, UI, provider invocation, process supervisor, side effect, deployment, launch, runtime action, signing, publication, or evidence promotion.
- **Decision:** Existing adapter, deployment, release, canonical-interchange, and External Tool Interchange V1 source, canonical bytes, validators, outcomes, and false authority flags are compatibility-locked and may not change under M1.
- **Decision:** Any need to touch an unlisted path, change V1, add persistence or a consumer, introduce another dependency, or cross into M2 or later work is a hard stop requiring an amended M0 decision.
- **Rationale:** M1 establishes the neutral language required by every later phase while preserving the reviewed control plane, preventing premature execution authority, and ensuring later providers and executors share one contract foundation.
- **Scope:** M1 contract, canonicalisation, validation, build ownership, Core-only tests, static validator, read-only CI, data-format documentation, compatibility proof, and exact-head evidence only.
- **Status:** Repository-owner authorised for maintainer audit; effective as implementation authority only after the M0 decision pull request is merged to `main`.
