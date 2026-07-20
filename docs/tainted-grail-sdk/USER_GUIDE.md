# User Guide

## Overview

The Tainted Grail Modding Editor and SDK is an O3DE-hosted authoring environment for governed FoA mod projects. It records exact game-build context, pack ownership, source provenance, evidence, canonical identities, independent governance decisions, permissions, prohibitions, blockers, adapter contracts, plans, evidence-return contracts, and read-only Phase 8 previews, reports, and verifier-result contracts.

The project is pre-alpha. It does not provide production runtime deployment or complete domain authoring tools.

## Before you begin

You need a supported O3DE source-build environment, Git LFS, this repository, writable authoring directories, and legally distributable project-owned data. A legitimate FoA installation is needed only when configuring a real game profile.

Keep the workspace, generated output, staging, and deployment directories separate from the game installation.

## Build and validate

Run focused validators from the repository root:

```shell
python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py
python Gems/TaintedGrailModdingSDK/Tools/validate_catalog_tests.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_contracts.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_work_order_plans.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_runtime_results.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_build_manifests.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_package_assembly_preview.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_staging_deployment_preview.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_deployment_work_orders.py
python Gems/TaintedGrailModdingSDK/Tools/validate_adapter_deployment_execution_results.py
```

These checks do not replace an O3DE configure/build and compiled test run.

After launching the Editor, open **Tools → Tainted Grail SDK**. Current panes are:

- **FOA Development Hub** (opens by default)
- **Tainted Grail SDK Status**
- **Tainted Grail Pack Manager**
- **Tainted Grail Source Intake**
- **Tainted Grail Catalog Browser**
- **Tainted Grail Catalog Governance**
- **Tainted Grail Item and Recipe Editor**
- **Tainted Grail Actor and Troop Editor**
- **Tainted Grail Economy Acquisition Coverage**
- **Tainted Grail Economy Cross-Pack Duplicates**
- **Tainted Grail Adapter Capability Matrix**
- **Tainted Grail Adapter Work-Order Plans**
- **Tainted Grail Adapter Runtime Result Evidence**
- **Tainted Grail Adapter Build Manifests**
- **Tainted Grail Package Assembly Preview**
- **Tainted Grail Staging and Deployment Preview**
- **Tainted Grail Deployment Confirmation and Work Orders**
- **Tainted Grail Deployment Execution Result Evidence**
- **Tainted Grail Post-Deployment Verification and Release Blockers**
- **Tainted Grail Independent Post-Deployment Verifier Results**
- **Tainted Grail Verifier Evidence Reconciliation and Release Decision**
- **Tainted Grail Release Artifact Provenance and Signing Intent**
- **Tainted Grail Release Assembly and Checksum Results**
- **Tainted Grail Release Signing Results**

## Recommended workflow

1. Start from **FOA Development Hub** and review the live workspace, game-profile, active-pack, validation, blocker, and persistence context. The Hub only orchestrates existing panes and never grants execution or release permission.
2. Use **Setup and readiness** to configure and save a workspace, exact game profile, active pack, and adapter capability context.
3. Create or load pack manifests.
4. Import sources and evidence.
5. Promote reviewed claims into canonical records.
6. Inspect records, relationships, and blockers.
7. Record maturity, confidence, risk, validation, and staleness decisions.
8. Allow or forbid one named usage through Catalog Governance.
9. Review economy coverage and cross-pack duplicate candidates.
10. Review adapter capability, version, permission, and proof readiness.
11. Review generated or refused canonical work-order plans.
12. Review any externally supplied runtime-result envelope as candidate evidence only.
13. Review the reproducible adapter build definition.
14. Review the package-assembly preview against a project-owned staging inventory.
15. Review the staging/deployment preview against an accepted declared target inventory.
16. Review the typed confirmation, maintenance window, preflight evidence, deployment work-order steps, and operator checklist.
17. Review any separately supplied deployment execution-result envelope and candidate evidence without treating it as execution authority or promoted truth.
18. Review the deterministic post-deployment compatibility and release blockers.
19. Review any separately supplied independent-verifier observations without treating contract validity as certification or release approval.
20. Review the shared status pane before later downstream work.

## Workspace and exact game profile

The status pane configures workspace identity and root, output, staging, and deployment paths. A game profile records exact FoA installation, game version, branch, `Mono` or `IL2CPP` target, Unity version, BepInEx version and plugin path where applicable, managed assemblies, diagnostics, extracted data, and content scopes.

Workspace schema 1 uses lowercase namespaced stable IDs. Paths are canonicalised and checked before publication. Changing the active profile does not re-authorise older evidence, governance, adapter metadata, plans, runtime-result candidates, build manifests, package previews, staging/deployment previews, confirmations, deployment work orders, deployment execution-result envelopes, derived post-deployment reports, or independent-verifier envelopes.

## Pack manager

A pack declares stable identity, owner, semantic version, exact game/Core/adapter compatibility, dependencies, required mods, incompatibilities, save impact, content/assets/localisation, build configuration, release channel, and disabled runtime actions.

Synthetic catalog records require an existing pack owner. Pack manifests remain inside the workspace root.

## Source and evidence intake

Supported importers create source and evidence documents with SHA-256 fingerprints and exact profile/build binding. Parsing success is not factual validation or runtime permission.

Adapter evidence subjects are:

```text
adapter:<adapter-id>
adapter:<adapter-id>:capability:<capability>
```

Imported data does not automatically become a catalog record, validation decision, permission, build input, package output, deployment target, confirmation, rollback proof, deployment execution evidence, compatibility decision, independent verification, or release approval.

## Canonical catalog and governance

The catalog stores stable records, relationships, typed economy data, validation history, and governance history. Display names are discovery labels, not identity.

Promotion creates reviewed-but-unvalidated, staleness-unknown records. Validation does not grant permission. A usage allowance requires a separate evidence-backed permission decision and same-subject validation proof. Stale, failed, blocked, or superseded subjects lose allowed usage.

The adapter-facing economy usages are:

- `existing_item_grant`
- `existing_recipe_learn`
- `runtime_recipe_append`
- `custom_item_registration`
- `custom_recipe_registration`
- `vendor_or_loot_injection`
- `quest_or_contract_reward_injection`

## Item and recipe authoring

The Item and Recipe Editor authors typed profiles, stations, ingredients, outputs, by-products, unlocks, and acquisition relationships on canonical records. Its action matrices and station/learnability rows are read-only. The pane cannot grant permission or invoke FoA runtime behavior.

## Economy analysis panes

**Tainted Grail Economy Acquisition Coverage** derives vendor, loot, reward, learnability, and crafting coverage. `covered`, `partial`, `blocked`, and `missing` describe research readiness only.

**Tainted Grail Economy Cross-Pack Duplicates** uses exact subject references and exact recipe duplicate keys across distinct owner packs. Matching is case-sensitive and never uses display-name similarity, aliases, localisation, tags, asset paths, or fuzzy inference. `review_required`, `partial`, and `blocked` are review states. The pane does not merge records, reject packs, or choose a winner.

## Adapter capability matrix

The typed capabilities are:

- `item_grant`
- `recipe_learn`
- `recipe_append`
- `custom_item_registration`
- `custom_recipe_registration`
- `vendor_mutation`
- `loot_mutation`
- `reward_mutation`
- `persistence`
- `cleanup`
- `rollback`

Rows are `unsupported`, `version_mismatch`, `permission_missing`, `proof_missing`, or `supported`. Evaluation is support → version → permission → proof. `supported` means contract readiness only and never authorises execution.

The adapter declaration registry is transient. With no declaration, all eleven capabilities fail closed as `unsupported`.

## Adapter work-order plans

**Tainted Grail Adapter Work-Order Plans** derives one candidate per exact pack and adapter declaration. A plan is **generated** only when all eleven capability rows are supported and each exact payload independently passes identity, permission, evidence, validation, relationship, target, and blocker checks. Otherwise it is **refused** as a whole.

Generated plans contain stable IDs, ordered typed steps, exact subjects/targets, sorted arguments, input evidence, permission events/evidence, validation proof, and byte-deterministic **canonical JSON**.

`ExecutionAllowed` is false on every plan and step. **Execution is prohibited.** The pane cannot save, export, dispatch, deploy, launch, or execute a plan.

## Adapter runtime-result evidence

**Tainted Grail Adapter Runtime Result Evidence** validates externally supplied metadata against one exact canonical plan.

Typed outcomes are `not_attempted`, `succeeded`, `failed`, and `skipped`. The contract checks exact step identities, sequence, capability, subject binding, typed failures, cleanup and rollback summaries, safe relative log references, and lowercase SHA-256 fingerprints.

Accepted metadata returns candidate source and evidence documents by value. Nothing is persisted or promoted, and nothing is automatically registered, validated, permitted, or published into the catalog. The ordinary Developer Preview state has zero registered runtime-result envelopes.

## Adapter build manifests

**Tainted Grail Adapter Build Manifests** records a reproducible definition for one exact work-order plan:

- source commit and O3DE revision;
- builder, compiler, configuration, target framework, deterministic/CI/path-map declarations;
- resolved materials and fingerprints;
- BepInEx plugin metadata and hard/soft dependencies;
- expected package outputs and redistribution decisions.

Statuses are `plan_mismatch`, `toolchain_unresolved`, `input_missing`, `fingerprint_missing`, `path_invalid`, `redistribution_blocked`, and `ready`.

`BuildAllowed` remains false even for `ready`. The pane is non-editable and **nothing is built or packaged**. It does not invoke a compiler, copy files, create archives, download dependencies, deploy content, launch FoA, or execute an adapter.

## Package assembly preview

Open **Tainted Grail Package Assembly Preview** after a build manifest has a separate accepted evidence-backed review and a project-owned staging inventory has been supplied to the transient registry.

The preview compares the exact reviewed manifest with the inventory and produces a deterministic **derived layout** containing staging path, package path, role, media type, byte size, project ownership, redistribution state, and exact output digest.

Statuses are:

- `manifest_not_ready`
- `manifest_unreviewed`
- `inventory_binding_mismatch`
- `inventory_untrusted`
- `output_missing`
- `fingerprint_missing`
- `path_invalid`
- `collision`
- `redistribution_blocked`
- `ready`

An `output_missing` result includes explicit omissions. A `collision` lists every staging entry targeting the same package path. Undeclared, unowned, unsafe, digest-less, or non-redistributable outputs fail closed.

`AssemblyAllowed`, `ArchiveAllowed`, and `DeploymentAllowed` are false for every state. The pane is read-only: **nothing is copied or archived**, and no staging directory is scanned or mutated. It cannot save/export a preview, copy/delete files, create an archive, deploy content, launch FoA, or execute an adapter.

The ordinary Developer Preview state has zero registered package-preview inputs.

## Staging and deployment preview

Open **Tainted Grail Staging and Deployment Preview** after an exact ready package preview and a separately accepted evidence-backed target inventory have been supplied to the transient registry.

The pane compares one reviewed package layout with one exact target inventory and derives **additions, replacements, removals, and unchanged paths**. It also reports exact ownership/type/multiplicity conflicts, backup requirements, and one inverse rollback step for every hypothetical change.

Statuses are:

- `package_not_ready`
- `target_unreviewed`
- `inventory_binding_mismatch`
- `inventory_untrusted`
- `path_invalid`
- `conflict`
- `backup_incomplete`
- `rollback_incomplete`
- `ready`

A replacement requires one managed project-owned target entry for the same pack, matching role/media type, a different exact fingerprint, and explicit replaceability. An obsolete same-pack managed entry becomes a removal only when explicitly removable. Foreign, unmanaged, multiplicity, role, media-type, and replaceability/removability problems remain conflicts; the preview never selects a winner.

Every replacement and removal requires an exact current fingerprint and a safe deterministic path beneath the separate backup root. Rollback actions are typed as `remove_added`, `restore_replaced`, and `restore_removed`.

`StagingMutationAllowed`, `DeploymentMutationAllowed`, `RollbackExecutionAllowed`, and `LaunchAllowed` are false for every state. The pane is non-editable and **nothing is copied or deleted**. It does not scan or hash the target, create a backup, restore a file, mutate staging/deployment directories, launch FoA, or execute an adapter.

The ordinary Developer Preview state has zero registered staging/deployment-preview inputs.

## Deployment confirmation and work orders

Open **Tainted Grail Deployment Confirmation and Work Orders** after one exact `ready` staging/deployment preview has a named evidence-backed confirmation and a typed preflight set supplied to the transient registry.

The confirmation binds to the exact preview ID and lowercase SHA-256 fingerprint. It records `confirmed` or `rejected`, the named reviewer, issue and expiry timestamps, evidence, and one scope:

- `additions_only`;
- `additions_and_replacements`;
- `full_preview`.

A narrow scope that does not cover the preview produces `scope_mismatch`. Evaluation must occur after issue and before expiry; otherwise the result is `confirmation_expired`.

The maintenance-window contract records an exact preview-bound UTC start/end, operator group, and evidence. Invalid intervals produce `maintenance_window_invalid`; an otherwise valid request evaluated outside the window produces `outside_maintenance_window`.

Required typed preflight kinds are `package_integrity`, `target_inventory`, `rollback_readiness`, and `operator_readiness`, plus `backup_readiness` whenever backups are required. A missing kind produces `preflight_missing`; duplicate, failed, stale, anonymous, evidence-free, or preview-mismatched preflight metadata produces `preflight_failed`.

The service derives deterministic non-executable steps for preflight verification, window confirmation, backups, additions, replacements, removals, deployment verification, and rollback preservation. It also derives an operator-facing checklist with `contract_satisfied`, `operator_action_required`, and `blocked` states.

Statuses are:

- `preview_not_ready`
- `confirmation_missing`
- `confirmation_rejected`
- `confirmation_binding_mismatch`
- `scope_mismatch`
- `confirmation_expired`
- `maintenance_window_invalid`
- `outside_maintenance_window`
- `preflight_missing`
- `preflight_failed`
- `work_order_incomplete`
- `review_ready`

`review_ready` means the metadata is complete enough for operator review only. `AcknowledgementRecorded` remains false. `ExecutionAllowed`, `CopyAllowed`, `DeleteAllowed`, `BackupAllowed`, `RestoreAllowed`, `DeploymentAllowed`, and `LaunchAllowed` remain false on every work order; **nothing is copied, deleted, backed up, restored, deployed, or launched**.

The pane is non-editable and contains no registration, acknowledgement, save, export, copy, delete, backup, restore, deployment, launch, or adapter-execution control. The ordinary Developer Preview state has zero registered deployment confirmation/work-order inputs.

## Deployment execution-result evidence

Open **Tainted Grail Deployment Execution Result Evidence** to inspect metadata supplied by a separately reviewed executor for one exact current `review_ready` deployment work order.

The contract binds the exact work-order ID, canonical JSON and fingerprint, preview and target-inventory identities, pack, profile, game version, branch, and runtime target. The executor metadata must have an accepted evidence-backed review with stable identity, strict semantic version, fingerprint, reviewer, and UTC review time.

Typed step, backup, and rollback outcomes are `not_attempted`, `succeeded`, `failed`, and `skipped`. Every canonical work-order step requires exactly one result. Every backup step requires one backup outcome. Every add, replace, and remove step requires one target verification and one typed rollback result.

Target-verification states are `not_checked`, `matched`, and `mismatched`. Exact deployed fingerprints and presence states remain distinct from the executor's operational outcome. A failed execution can still be contract-valid evidence when the failure, attempted step, rollback state, and safe log references are complete and exact.

Envelope statuses are:

- `work_order_not_ready`
- `executor_unreviewed`
- `work_order_binding_mismatch`
- `envelope_invalid`
- `step_binding_mismatch`
- `backup_binding_mismatch`
- `verification_binding_mismatch`
- `rollback_binding_mismatch`
- `failure_log_binding_mismatch`
- `accepted`

`accepted` means only that the supplied result metadata is structurally valid. Candidate source/evidence documents are returned by value with confidence `unrated`; **nothing is executed or promoted automatically**. The pane does not register sources, import or promote evidence, validate claims, grant permission, publish a release, invoke an executor, mutate files, launch FoA, or call an adapter.

The ordinary Developer Preview state has zero registered deployment execution-result envelopes.

## Post-deployment verification and release blockers

Open **Tainted Grail Post-Deployment Verification and Release Blockers** to inspect the deterministic report derived from one exact current work order, its deployment execution-result envelope, and the candidate evidence returned from that envelope.

The report preserves exact work-order/result fingerprints and profile/game/branch/runtime context. It aggregates completed, failed, and incomplete steps; backup completeness; `matched`, `mismatched`, and `not_checked` target states; rollback requirements and outcomes; failures; and referenced diagnostics.

Report statuses are:

- `evidence_rejected`
- `evidence_incomplete`
- `verification_incomplete`
- `compatibility_blocked`
- `rollback_incomplete`
- `release_blocked`
- `review_ready`

Every blocker keeps stable identity, type, severity, exact subject, step or rollback identity, evidence IDs, diagnostic references, and separate compatibility/release effects. `review_ready` remains a human-review state only. `HumanReviewRequired` stays true, while `VerifierExecuted`, `EvidencePromoted`, `ReleasePublished`, `LaunchPerformed`, and `AdapterCalled` stay false.

The report does not run an independent verifier, access a target filesystem, clear adverse execution evidence, promote evidence, sign an archive, publish a release, launch FoA, or call an adapter. The ordinary Developer Preview state has zero reports because it has zero deployment execution-result envelopes.

## Independent post-deployment verifier results

Open **Tainted Grail Independent Post-Deployment Verifier Results** to inspect metadata supplied by one separately reviewed verifier for an exact current structurally eligible post-deployment report.

The verifier review requires stable review/verifier identities, strict semantic version, lowercase SHA-256 fingerprint, a named reviewer, evidence, UTC review time, and the exact required capabilities: `target_presence`, `target_fingerprint`, and `target_absence` as demanded by the work-order changes.

Every canonical add, replace, and remove step requires exactly one independent check with exact sequence, step, target path, expected presence, and expected fingerprint. Check outcomes are:

- `not_run`
- `matched`
- `mismatched`
- `failed`
- `inconclusive`

Envelope statuses are:

- `report_not_ready`
- `verifier_unreviewed`
- `report_binding_mismatch`
- `envelope_invalid`
- `check_coverage_incomplete`
- `failure_diagnostic_binding_mismatch`
- `observation_mismatch`
- `accepted`

`accepted` means the supplied contract is exact and every supplied observation matches. A complete mismatch remains structurally valid and returns candidate evidence under `observation_mismatch`; it is not silently discarded or treated as success. Failed and inconclusive checks require typed same-check failures and safe fingerprinted diagnostics.

The Editor does not discover or execute a verifier, read or hash a target file, mutate deployment state, launch FoA, call an adapter, import or promote evidence, sign an archive, or publish a release. The ordinary Developer Preview state has zero registered independent-verifier envelopes.

## Workspace layout

```text
MyWorkspace/
├── workspace.tgworkspace.json
├── Packs/
├── Sources/
├── Catalog/
├── Content/
├── Assets/
├── Localisation/
├── Build/
├── Staging/
├── Deployment/
└── Reports/
```

There is no durable adapter declaration, work-order plan, runtime-result, build-manifest, staging-inventory, package-preview, target-inventory, deployment-preview, backup, rollback-plan, confirmation, maintenance-window, preflight, deployment-work-order, checklist, deployment-execution-result, post-deployment-report, independent-verifier-result, or returned-candidate-evidence file in this workflow.

## Safe-use rules

- Never treat display names as identity.
- Do not reuse native identities for custom records.
- Do not assume imported or adapter-reported data is true because it parsed.
- Do not assume validation grants permission.
- Do not treat a duplicate candidate as an automatic merge instruction.
- Do not treat `supported`, generated canonical JSON, `ready` build manifests, `ready` package previews, `ready` staging/deployment previews, `review_ready` deployment work orders or post-deployment reports, `accepted` execution-result envelopes, or `accepted`/`observation_mismatch` verifier envelopes as authority to execute, build, assemble, deploy, restore, certify compatibility, promote evidence, launch, sign, publish, release, or modify saves.
- Keep proprietary game files, private paths, credentials, and non-redistributable data out of project fixtures and public evidence.

## Troubleshooting

### Adapter matrix is not supported

Check the declaration, runtime target, semantic version, exact adapter/capability evidence, governed catalog subject, allowed usage, validation proof, and blockers.

### Work-order candidate is refused

Inspect every failed capability and exact subject/payload reason. Do not bypass whole-plan refusal by copying a partial step set.

### Runtime-result envelope is rejected

Confirm exact plan JSON and identity, one result per canonical step, typed outcome/failure shape, matching cleanup/rollback summaries, safe log locators, and lowercase SHA-256 values.

### Build manifest is not ready

Resolve plan binding, toolchain declarations, required materials, fingerprints, package paths, and redistribution decisions. `ready` still does not authorise a build.

### Package preview is not ready

- For `manifest_unreviewed`, provide an accepted evidence-backed review bound to the exact manifest fingerprint.
- For `inventory_binding_mismatch`, align manifest ID/fingerprint, pack, and package root.
- For `inventory_untrusted`, remove undeclared or non-project-owned included entries and fix role/media-type drift.
- For `output_missing`, supply exactly one included staging entry for every expected manifest output.
- For `fingerprint_missing`, supply lowercase `sha256:<64 hex>` output digests.
- For `path_invalid`, use safe relative staging paths and keep outputs beneath the package root.
- For `collision`, resolve all duplicate target paths instead of choosing one automatically.
- For `redistribution_blocked`, exclude restricted content or correct the reviewed redistribution policy.

### Staging/deployment preview is not ready

- For `package_not_ready`, resolve the upstream package-preview status and exact fingerprint first.
- For `target_unreviewed`, provide an accepted evidence-backed review bound to the exact target inventory fingerprint.
- For `inventory_binding_mismatch`, align package-preview ID/fingerprint, pack, target root, and target inventory.
- For `inventory_untrusted`, fix entry identities, owner-pack IDs, role/media-type declarations, and duplicate entry IDs.
- For `path_invalid`, keep every target beneath the reviewed target root and every backup beneath a separate safe backup root.
- For `conflict`, resolve duplicate target entries, foreign ownership, unmanaged content, role/media drift, or non-replaceable/non-removable targets without selecting a winner automatically.
- For `backup_incomplete`, provide the exact current target fingerprint and a contained backup path for every replacement/removal.
- For `rollback_incomplete`, ensure every addition, replacement, and removal has exactly one typed inverse step.
- Remember that `ready` remains preview-only and does not authorise copying, deletion, backup, restoration, deployment, launch, or execution.

### Deployment work order is not review ready

- For `preview_not_ready`, resolve every staging/deployment-preview conflict and blocker first.
- For `confirmation_missing` or `confirmation_rejected`, provide one named evidence-backed `confirmed` decision.
- For `confirmation_binding_mismatch`, bind the confirmation to the exact preview ID and fingerprint.
- For `scope_mismatch`, use a scope that covers all additions, replacements, and removals.
- For `confirmation_expired`, use valid UTC issue/expiry/evaluation timestamps and evaluate before expiry.
- For `maintenance_window_invalid`, provide a preview-bound increasing UTC window, named operator group, and evidence.
- For `outside_maintenance_window`, evaluate inside the accepted UTC window.
- For `preflight_missing`, provide every required typed preflight kind exactly once.
- For `preflight_failed`, correct failed, duplicate, stale, anonymous, evidence-free, or preview-mismatched preflight records.
- For `work_order_incomplete`, restore exact backup/add/replace/remove and inverse rollback coverage.
- Remember that `review_ready` records no acknowledgement and authorises no execution or filesystem mutation.

### Deployment execution-result envelope is rejected

- For `work_order_not_ready`, regenerate the exact `review_ready` work order with every execution and mutation flag false.
- For `executor_unreviewed`, supply an accepted evidence-backed executor review with stable identity, strict version, fingerprint, reviewer, and UTC review time.
- For `work_order_binding_mismatch`, bind the exact work-order ID, canonical JSON, fingerprint, preview, pack, and target inventory.
- For `envelope_invalid`, correct contract version, result identity, profile/game/branch/runtime context, capture time, and lowercase fingerprints.
- For `step_binding_mismatch`, supply exactly one typed result for every canonical step with exact sequence, kind, paths, fingerprints, attempted state, and outcome shape.
- For `backup_binding_mismatch`, preserve the exact backup step, source fingerprint, backup path, and successful backup fingerprint.
- For `verification_binding_mismatch`, report one exact `not_checked`, `matched`, or genuinely `mismatched` target verification for every mutation step.
- For `rollback_binding_mismatch`, report one exact inverse rollback result for every addition, replacement, and removal, including `not_attempted`.
- For `failure_log_binding_mismatch`, bind failures and safe fingerprinted logs back to the same exact step or rollback result.
- Remember that `accepted` is contract validity only; nothing is executed, promoted, validated, permitted, or published automatically.

### Post-deployment report is blocked

- For `evidence_rejected`, resolve the upstream execution-result contract before building the report.
- For `evidence_incomplete`, restore the exact work-order/result/evidence-return bindings and candidate counts/identities.
- For `verification_incomplete`, supply complete execution-result target-verification metadata; the report itself does not perform checks.
- For `compatibility_blocked`, inspect mismatched targets, failed or incomplete steps, and their exact blocker subjects.
- For `rollback_incomplete`, preserve the required inverse result and its exact outcome for every mutation step.
- For `release_blocked`, inspect failures, diagnostics, backup state, rollback results, and existing adverse observations.
- Remember that `review_ready` still requires human review and is not compatibility certification or release permission.

### Independent-verifier envelope is rejected or mismatched

- For `report_not_ready`, use the exact current report backed by accepted execution evidence and a `review_ready` work order.
- For `verifier_unreviewed`, supply accepted evidence-backed verifier review with stable identity, strict version, fingerprint, reviewer, UTC review time, and every required capability.
- For `report_binding_mismatch`, bind the exact report ID, typed status, canonical JSON, work-order/result fingerprints, and profile/game/branch/runtime context.
- For `envelope_invalid`, correct contract version, stable identities, timestamps, fingerprints, canonical report JSON, and runtime context.
- For `check_coverage_incomplete`, supply exactly one exact check for every canonical add, replace, and remove step, with no unknown, duplicate, missing, or extra checks.
- For `failure_diagnostic_binding_mismatch`, use unique stable same-check failures and safe fingerprinted diagnostic references with no orphan or cross-check bindings.
- For `observation_mismatch`, review the independently supplied mismatch, failed check, or inconclusive check; structurally valid adverse evidence is intentionally preserved and is not `accepted`.
- Remember that `accepted` proves supplied contract shape and all-matched metadata only; the Editor did not run a verifier or make a release decision.

## Getting help

Follow [SUPPORT.md](../../SUPPORT.md). Security issues follow [SECURITY.md](../../SECURITY.md).
