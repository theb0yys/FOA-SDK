# Tainted Grail Modding Editor and SDK Documentation

This directory contains the public documentation for the TG SDK layer in this O3DE fork.

## Start here

### For users

- [Open and Test the Actual Editor](OPEN_AND_TEST_EDITOR.md) — shortest Windows path from a fresh pull to the dedicated O3DE project, source build, trusted clickable entry, native log, and synthetic workspace.
- [Developer Preview 0](DEVELOPER_PREVIEW_0.md) — Windows x64 prerequisites, configure/build/validate commands, deterministic fixture, persistence smoke, controlled Editor launch, redacted diagnostics, manual UI evidence tooling, and current limitations.
- [Developer Preview Exact-Head Verification](DEVELOPER_PREVIEW_EXACT_HEAD_VERIFICATION.md) — explicit
  reviewed-range whitespace checking, external receipts, O3DE configure/build, compiled tests, authoritative
  receipt/UI re-verification, and Windows evidence finalization.
- [Windows Manual UI Smoke](DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md) — real Windows checklist, screenshot-evidence initializer/verifier, exact-commit binding, privacy attestation, and evidence handling.
- [Developer Preview Troubleshooting](DEVELOPER_PREVIEW_TROUBLESHOOTING.md) — missing Editor output, path-policy and clickable-entry failures, absent TG SDK panes, native log locations, diagnostics failures, and verification guidance.
- [User Guide](USER_GUIDE.md) — build, open, configure, and use the foundation editor tools.
- [Catalog Guide](CATALOG_GUIDE.md) — canonical search, record inspection, evidence promotion, relationships, validation, and blockers.
- [Governance Engine Guide](GOVERNANCE_ENGINE_GUIDE.md) — independent maturity, confidence, risk, validation, staleness, permission, prohibition, and supersession decisions.
- [Item and Recipe Editor Guide](ITEM_RECIPE_EDITOR_GUIDE.md) — typed item and recipe profiles, ingredients, outputs, stations, acquisition relationships, and governed action lanes.
- [Recipe Station Evidence View](RECIPE_STATION_EVIDENCE_VIEW.md) — read-only station visibility and learnability research, evidence health, governance, and blockers.
- [Economy Acquisition Coverage](ECONOMY_ACQUISITION_COVERAGE.md) — read-only vendor, loot, reward, learnability, and crafting coverage derived from canonical relationships, exact evidence binding, governance, and blockers.
- [Economy Cross-Pack Duplicate Report](ECONOMY_CROSS_PACK_DUPLICATES.md) — exact cross-pack subject and recipe-key candidates, evidence health, governance state, blockers, and read-only review results.
- [FoA Adapter Contracts](FOA_ADAPTER_CONTRACTS.md) — typed adapter identity, semantic versions, capabilities, permission/proof gates, and the read-only compatibility matrix.
- [FoA Adapter Work-Order Plans](FOA_ADAPTER_WORK_ORDER_PLANS.md) — deterministic canonical plans and refusals from reviewed catalog records, with execution permanently disabled.
- [FoA Adapter Runtime Result Evidence](FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md) — typed attempted plan/step outcomes, failures, cleanup/rollback, logs, fingerprints, and candidate evidence return with no execution path.
- [FoA Adapter Build Manifests](FOA_ADAPTER_BUILD_MANIFESTS.md) — reproducible build definitions, resolved fingerprints, BepInEx metadata, dependency and package-layout declarations, redistribution gates, and build prohibition.
- [FoA Adapter Package Assembly Preview](FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md) — reviewed manifests, project-owned staging inventory, derived package layout and output digests, omissions, collisions, redistribution blockers, and assembly prohibition.
- [FoA Adapter Staging and Deployment Preview](FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md) — reviewed target inventory, deterministic additions/replacements/removals, conflicts, backup requirements, inverse rollback steps, and deployment prohibition.
- [FoA Adapter Deployment Confirmation and Work Orders](FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md) — named confirmation, typed scope and expiry, maintenance windows, preflight evidence, deterministic non-executable steps, and operator checklist.
- [FoA Adapter Deployment Execution Results](FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md) — exact attempted work-order steps, backup/restore outcomes, deployed fingerprints, target verification, rollback results, safe logs, and candidate evidence with no executor.
- [FoA Post-Deployment Verification Reports](FOA_POST_DEPLOYMENT_VERIFICATION_REPORTS.md) — deterministic candidate-evidence aggregation, target and rollback analysis, explicit compatibility/release blockers, and no independent verifier or publication path.
- [FoA Independent Post-Deployment Verifier Results](FOA_POST_DEPLOYMENT_VERIFIER_RESULTS.md) — separately reviewed verifier identity, exact report/check binding, independent observations, failures, safe diagnostics, and candidate evidence with no verifier execution.
- [FoA Verifier Evidence Reconciliation and Release Decision](FOA_VERIFIER_EVIDENCE_RECONCILIATION.md) — preserved report blockers, adverse verifier observations, separate compatibility/release/human-review axes, explicit dispositions, and no automatic approval or publication.
- [FoA Release Artifact Provenance and Signing Intent](FOA_RELEASE_ARTIFACT_PROVENANCE_SIGNING_INTENT.md) — exact approved reconciliation/package binding, declared checksums, provenance, legal review, signing intent, publication targets, and no performed release operations.
- [FoA Release Assembly and Checksum Results](FOA_RELEASE_ASSEMBLY_CHECKSUM_RESULTS.md) — exact ready-artifact binding, reviewed external assembler/checksummer results, archive identity, per-content checksum observations, failures, safe diagnostics, and no SDK release operations.
- [FoA Release-Signing Result Evidence](FOA_RELEASE_SIGNING_RESULTS.md) — exact accepted assembly/archive and signing-intent binding, reviewed signer metadata, typed outcomes, signature artifacts, failures, safe diagnostics, and no SDK signing or verification.
- [Data Formats](DATA_FORMATS.md) — workspace, pack, source, evidence, issue, catalog, validation, governance-history, and typed-domain contracts.
- [Support](../../SUPPORT.md) — where and how to ask for help.
- [Security](../../SECURITY.md) — private reporting and secure-use guidance.
- [Privacy](PRIVACY.md) — local data handling and telemetry requirements.
- [Accessibility](ACCESSIBILITY.md) — user-interface accessibility standard.
- [Glossary](GLOSSARY.md) — shared project terminology.

### For contributors

- [Contributing](../../CONTRIBUTING.md) — contribution lifecycle and DCO requirements.
- [Development Guide](DEVELOPMENT_GUIDE.md) — repository setup, build, validation, and implementation workflow.
- [CI, Runner, and Local Validation Policy](CI_AND_LOCAL_VALIDATION.md) — current manual-only workflow state, authoritative local command, self-hosted runner boundary, token handling, and restoration conditions.
- [Developer Preview 0 Design](DEVELOPER_PREVIEW_0_DESIGN.md) — approved scope, acceptance criteria, safety boundary, and implementation sequence.
- [Windows Installer and Prebuilt Artifact Workflow Design](WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md) — approved prebuilt O3DE SDK layout, exact redistribution review, MSI/ZIP identity, failure, rollback, and non-release boundary.
- [Installing the Prebuilt Windows SDK](INSTALLING_PREBUILT_SDK.md) — checksum verification, MSI and portable ZIP use, repair, upgrade, uninstall, and troubleshooting.
- [Actor and Troop Editor Design](ACTOR_TROOP_EDITOR_DESIGN.md) — active population implementation,
  completed Core, schema-2 persistence, Framework candidate-publication, population-authoring test-source,
  immutable action-lane, and registered Actor/Troop pane units, with the deterministic fixture and full
  local-validation integration next and runtime or spawn authority still deferred.
- [Editor Toolchain and Unity Interchange Design](EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md) — located O3DE,
  Blender, and Unity components; provider qualification; deterministic interchange; loss reporting; and
  staged delivery gates.
- [External Tool Interchange Gate 0](EXTERNAL_TOOL_INTERCHANGE_GATE_0.md) — Core-only disabled handoff and
  result envelopes, canonical binding, fail-closed validation, and the boundary before any process work.
- [O3DE-to-Unity Bridge Research Archive](../../Research/o3de-to-unity-conversion-and-runtime-bridge/README.md)
  — supplied report, durable source and claim registers, research areas, and gate reconciliation.
- [Dedicated Editor Entry Architecture](DEDICATED_EDITOR_ENTRY.md) — product-host project, project-owned icons, trusted generated Windows shortcut, and separation from engine testing projects.
- [Canonical Path and Executable Trust Policy](PATH_POLICY.md) — canonical resolution, component containment, workspace/pack persistence boundaries, source-build provenance, and diagnostic overrides.
- [Atomic Workspace Transition and Schema Contract](WORKSPACE_ATOMICITY_AND_SCHEMA.md) — schema-0 migration, durable schema 1, candidate construction, cross-binding validation, atomic publication, rollback, and failure-stage tests.
- [Core and Framework Build Graph](CORE_FRAMEWORK_BUILD_GRAPH.md) — internal static targets, unique source ownership, dependency direction, test linkage, enforcement, and runtime boundary.
- [Economy Acquisition Coverage Contract](ECONOMY_ACQUISITION_COVERAGE.md) — Core analysis ownership, lane/status semantics, evidence and blocker handling, determinism, non-mutation, and runtime boundary.
- [Economy Cross-Pack Duplicate Contract](ECONOMY_CROSS_PACK_DUPLICATES.md) — exact signal semantics, pack gating, candidate health, determinism, non-mutation, and runtime boundary.
- [FoA Adapter Contracts](FOA_ADAPTER_CONTRACTS.md) — Core registry and compatibility ownership, typed capability vocabulary, SemVer policy, proof ordering, and planning boundary.
- [FoA Adapter Work-Order Plans](FOA_ADAPTER_WORK_ORDER_PLANS.md) — all-capability gating, exact payload/proof rebuilding, stable identities, canonical JSON, refusal semantics, and execution prohibition.
- [FoA Adapter Runtime Result Evidence](FOA_ADAPTER_RUNTIME_RESULT_EVIDENCE.md) — exact return bindings, typed outcomes/failures/recovery, safe log fingerprints, candidate evidence, and no-execution enforcement.
- [FoA Adapter Build Manifests](FOA_ADAPTER_BUILD_MANIFESTS.md) — exact plan/toolchain/material binding, canonical build definitions, package containment, redistribution policy, determinism, and no-build enforcement.
- [FoA Adapter Package Assembly Preview](FOA_ADAPTER_PACKAGE_ASSEMBLY_PREVIEW.md) — accepted manifest reviews, staging-inventory trust, exact layout/digest derivation, omissions, collisions, redistribution policy, determinism, and no-assembly enforcement.
- [FoA Adapter Staging and Deployment Preview](FOA_ADAPTER_STAGING_DEPLOYMENT_PREVIEW.md) — exact package/target binding, change classification, conflicts, backup and rollback completeness, determinism, and no-deployment enforcement.
- [FoA Adapter Deployment Confirmation and Work Orders](FOA_ADAPTER_DEPLOYMENT_CONFIRMATION_WORK_ORDERS.md) — exact preview confirmation, scope, time/window, preflight, work-order coverage, checklist, determinism, and no-execution enforcement.
- [FoA Adapter Deployment Execution Results](FOA_ADAPTER_DEPLOYMENT_EXECUTION_RESULTS.md) — exact work-order/executor binding, typed outcomes, backups, verification, rollback, failures/logs, candidate evidence, and no-executor enforcement.
- [FoA Post-Deployment Verification Reports](FOA_POST_DEPLOYMENT_VERIFICATION_REPORTS.md) — exact current-work-order/evidence binding, deterministic status precedence, explicit blockers, non-mutation, and no-verifier/no-release enforcement.
- [FoA Independent Post-Deployment Verifier Results](FOA_POST_DEPLOYMENT_VERIFIER_RESULTS.md) — deterministic report serialization, reviewed-verifier capabilities, exact final-state check coverage, failure/diagnostic binding, and no-verifier/no-release enforcement.
- [FoA Verifier Evidence Reconciliation and Release Decision](FOA_VERIFIER_EVIDENCE_RECONCILIATION.md) — exact immutable binding, preserved blockers, typed human dispositions, deterministic compatibility/release decisions, non-mutation, and no-publication enforcement.
- [FoA Release Artifact Provenance and Signing Intent](FOA_RELEASE_ARTIFACT_PROVENANCE_SIGNING_INTENT.md) — exact content/checksum/provenance/legal/signing/publication declarations, deterministic fail-closed status, and no file or release operations.
- [FoA Release Assembly and Checksum Results](FOA_RELEASE_ASSEMBLY_CHECKSUM_RESULTS.md) — contract-validated external archive/checksum observations, deterministic fail-closed binding, candidate evidence, and no file or release operations.
- [FoA Release-Signing Result Evidence](FOA_RELEASE_SIGNING_RESULTS.md) — exact upstream/signing-intent binding, reviewed signer capabilities, typed outcomes and artifacts, deterministic candidate evidence, transient registry, and no signing, verification, or publication.
- [Architecture](ARCHITECTURE.md) — layers, responsibilities, invariants, and data flow.
- [Governance Reliability Baseline](GOVERNANCE_HARDENING.md) — typed transitions, shared record/relationship logic, intrinsic audit atomicity, publish-after-save persistence, and required failure tests.
- [Code Quality](CODE_QUALITY.md) — mandatory C++, Qt, persistence, UI, testing, and evidence standards.
- [Review and Merge Policy](REVIEW_AND_MERGE_POLICY.md) — design review, self-review, pull-request review, and merge gates.
- [Release Process](RELEASE_PROCESS.md) — versioning, release evidence, packaging, and rollback.
- [Maintainer Checklist](MAINTAINER_CHECKLIST.md) — recurring repository and release responsibilities.
- [Legal and Content Policy](LEGAL_AND_CONTENT_POLICY.md) — redistribution and proprietary-content boundaries.

### Project governance

- [Governance](../../GOVERNANCE.md)
- [Code of Conduct](../../CODE_OF_CONDUCT.md)
- [Roadmap](../../ROADMAP.md)
- [Changelog](../../CHANGELOG.md)

## Documentation authority

When documents conflict, use this order:

1. security policy and licence obligations;
2. governance and review policy;
3. architecture and data-format contracts;
4. code-quality standard;
5. user and development guides;
6. examples and issue discussions.

A merged architecture decision or schema migration may supersede an older section. The change must update the affected documentation in the same pull request.

## Current product status

The project is pre-alpha. Current implemented editor workflows cover:

- internal `Core.Static` and `Framework.Static` build targets with one-way dependencies, unique production-source ownership, and tests linked to production libraries instead of recompiling them;
- durable workspace schema 1 with validated schema-0 migration and atomic all-or-nothing workspace transitions;
- workspace, exact game profiles, packs, source/evidence intake, canonical catalog, validation, governance, and blockers;
- typed item/recipe authoring, station/learnability evidence, economy acquisition coverage, and exact cross-pack duplicate candidates;
- typed transient adapter declarations, capability compatibility, deterministic work-order plans, and typed runtime-result evidence candidates;
- deterministic transient adapter build-manifest definitions with exact toolchain/material/output declarations and `BuildAllowed` disabled;
- deterministic transient package-assembly previews with accepted manifest reviews, project-owned staging inventories, exact package layouts and output digests, omissions, collisions, redistribution blockers, and all assembly/archive/deployment permissions disabled;
- deterministic transient staging/deployment previews with accepted target reviews, exact target inventories, additions, replacements, removals, unchanged paths, conflicts, backups, rollback steps, and all mutation/launch permissions disabled;
- typed transient deployment confirmations and work orders with exact scope, expiry, maintenance windows, preflight evidence, deterministic operator steps/checklists, and all execution/mutation permissions disabled;
- typed transient deployment execution-result envelopes with exact work-order/executor binding, attempted steps, backup/restore outcomes, target verification, rollback, safe logs, and candidate evidence returned without automatic promotion;
- deterministic post-deployment reports with exact current-work-order/candidate-evidence binding, target-verification and rollback analysis, explicit compatibility/release blockers, mandatory human review, and no independent verifier or release publication path;
- typed transient independent-verifier result envelopes with exact canonical-report binding, reviewed verifier capabilities, one check per mutation step, independent observations, typed failures, safe diagnostics, and candidate evidence returned without automatic promotion;
- deterministic transient verifier-evidence reconciliations with preserved blockers, separate compatibility/release/human-review axes, typed dispositions, canonical JSON, and no automatic approval, signing, or publication;
- deterministic transient release-artifact envelopes with exact approved reconciliation and ready-package binding, declared checksums/provenance/legal review/signing intent/publication targets, and every file, archive, signing, upload, publication, adapter, launch, and deployment operation disabled;
- typed transient release-assembly/checksum result envelopes with exact artifact and archive binding, one checksum observation per content row, reviewed external assembler/checksummer metadata, failures, safe diagnostics, and no file, archive, signing, upload, publication, launch, adapter, or deployment operation;
- typed transient release-signing result envelopes with exact accepted assembly/archive and signing-intent binding, reviewed signer metadata, typed outcomes, signature artifacts, failures, safe diagnostics, candidate evidence, and no key, signing, verification, upload, publication, launch, adapter, deployment, or save operation;
- Developer Preview validation, deterministic synthetic fixtures, controlled Editor launch, diagnostics, and exact-commit Windows manual UI evidence tooling.

Automatic GitHub Actions triggers are currently suspended; the local validation command is the documented test gate and no automated per-commit result is claimed.

The actual Windows screenshot pass remains pending. Trusted identity/time providers, actual verifier execution and target access, acknowledgement, toolchain resolution, compilation, file-backed staging/target inventory, package copying/archiving, backup/restore, release-artifact hashing, key custody, actual signing and signature verification, release upload/publication, runtime adapters, production deployment, live executor capture, and remaining specialised domain tools are not complete.

## Documentation contribution rules

Documentation changes must describe current behavior honestly, distinguish implemented from planned behavior, avoid proprietary content and private paths, use stable links, and include migration notes for breaking format changes.
