# M1 - Shared execution core contracts

Status: implemented; final exact-head validation and PR handoff in progress.
Request: implement the first Build and Test Runner slice, M1.
Primary owner: capability-execution. Supporting owners: artifact-ownership, execution-receipts and runtime-verification (observation shapes only).
Classification: Significant, introducing an additive public contract family; medium performance risk.

Authority: merged CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md and the owner's current instruction. Base: cbd8409ae3cc1dde61772a20c3f40928b7d46cf8. Working branch: codex/capability-execution-m1-core-contracts, using the current agent branch prefix.

Scope: the exact M0 six-file Core family, dedicated three-file compiled test target, strict source validator and its tests, read-only validation integration and the authorised documentation paths.
No production consumers, persistence, executors, provider resolution, policy evaluation, UI or external processes are introduced.

Design: independent typed decisions and observations; bounded versioned value records; explicit upstream canonical bytes and digests; immutable phase and complete plans; ordered artifact dependencies and inverse rollback declarations; exact contextual receipt validation. Canonical output uses fixed keys, copy-free sorting through indices/pointers for sets, preserved sequences and checked byte/cardinality budgets. It excludes capture metadata and each object's own fingerprint. Existing V1 contracts are unchanged.

Proof: M0 preflight helpers, source/compatibility guards, compiled enum/canonical/negative/boundary and complete-chain tests, pinned configure/build, dedicated CTest, legacy catalog/interchange regression, full validation and read-only CI. Editor/UI and operational/runtime evidence are NOT_APPLICABLE to M1.

Pre-edit review: owners and producer/consumer boundaries mapped; source allowlist fixed by M0; semantic and event identity separated; no protected inputs required. The existing test-plan helper has a PowerShell line-continuation parser defect; its identical selection logic was reproduced in memory with that syntax corrected, without editing the helper.

M1 completion does not start M2. Runtime sign-off not performed.

Implemented: 25 versioned value types, 23 typed enums, checked scalar/collection/canonical budgets, exact contextual plan/provider/artifact/rollback/receipt bindings, separate authorization intent and exact-plan observations, a Core-only compiled test target and read-only local/CI guards.

Executed before commit: 48 new compiled tests passed; Core, Framework, Catalog.Tests, CanonicalInterchange.Tests, SDK Editor, consumer Tool Gems, Editor and AssetProcessorBatch built against the exact O3DE pin. Legacy CTest passed after precreating its output XML file; no source or generated test command was changed. Final full validation is pending.

Evidence limitation: M0 declares Editor/UI execution NOT_APPLICABLE, while the existing validation-receipt tool requires a passed windows-ui row or explicit maintainer risk acceptance for merge-ready finalization. M1 does not edit that tool or invent UI proof. This mismatch will remain explicit in the exact-head receipt and PR until the maintainer resolves it.

Post-edit review: no existing production consumers, canonical helpers or persisted formats changed; the six-file family has unique Core ownership. All changed paths are in the M0 allowlist. Runtime, game files, saves, installer state and release artifacts were not accessed.
