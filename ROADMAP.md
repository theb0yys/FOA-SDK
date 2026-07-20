# Tainted Grail Modding Editor and SDK Roadmap

This roadmap describes capability order, not promised dates. Work advances only when architecture, evidence, validation, documentation, tests, and build quality are sufficient.

## Guiding rule

Every authoring feature uses the shared workspace, pack ownership, source/evidence, canonical catalog, validation, risk, staleness, permission, prohibition, and blocker infrastructure. Domain tools do not create private identity systems, grant their own permissions, or bypass blockers.

## Phase 0 — Project foundation

Status: implemented, continuing hardening.

- O3DE editor Gem and host-tool registration.
- Editor-only product boundary.
- Focused repository validation and public governance/documentation.
- Two-branch development model: `main` and `foa-development`.

## Phase 1 — Workspace and exact game profile

Status: implemented, continuing hardening.

- Workspace identity and canonical paths.
- Exact FoA version, branch, runtime target, Unity/BepInEx versions, assemblies, diagnostics, and extracted-data locations.
- Durable workspace schema 1 with legacy migration and atomic all-or-nothing publication.
- Blockers for incomplete identity and unsafe paths.

## Phase 2 — Mod and content-pack projects

Status: implemented, continuing hardening.

- Pack-owned stable identities and semantic versions.
- Core, adapter, game, mod, DLC, and incompatibility declarations.
- Save-impact, content, asset, localisation, build, and release declarations.
- Durable pack manifests with runtime actions disabled.

## Phase 3 — Source and evidence intake

Status: implemented, continuing hardening.

- SHA-256 fingerprinting and exact profile/build binding.
- Structured JSON/CSV extraction and bounded generic-artifact intake.
- Durable source/evidence documents and import issues.
- No automatic catalog promotion, validation, or permission.

## Phase 4 — Canonical catalog browser and record inspector

Status: implemented, continuing hardening.

- Durable `Catalog/catalog.tgcatalog.json` bound to workspace/profile/version/branch.
- Stable records, first-class relationships, validation history, and governance history.
- Exact-reference/GUID, identity, pack, evidence, governance, blocker, and supersession search.
- Reviewed claim promotion without display-name identity merging.
- Native exact-reference uniqueness, synthetic pack ownership, and save-before-publish transactions.

## Phase 5 — Validation, maturity, risk, and permission engine

Status: implemented, continuing hardening.

- Independent maturity, confidence, operational risk, validation, staleness, permission, prohibition, and supersession states.
- Usage-specific allow, forbid, and clear decisions.
- Validation and governance history bound to exact subjects, profiles, evidence, reviewers, and proof.
- Stale, failed, blocked, and superseded subjects lose allowed usage.
- Dedicated **Tainted Grail Catalog Governance** pane and rollback/persistence hardening tests.

## Phase 6 — Domain authoring tools

Status: active development.

All domain tools use canonical IDs, source/evidence provenance, governance decisions, blockers, and the shared catalog transaction boundary.

### Economy and content

#### Item and Recipe Editor

Status: implemented, continuing hardening and evidence coverage.

- Typed item/recipe profiles, stations, ingredients, outputs, by-products, unlocks, and acquisition relationships.
- Separate native, synthetic, append, registration, vendor/loot, reward, asset, and localisation lanes.
- Read-only action matrices and station/learnability evidence rows.
- Transactional catalog persistence, exact evidence checks, and fail-closed blockers.
- No item or recipe authoring command invokes FoA runtime code.

#### Economy acquisition coverage dashboard

Status: implemented, continuing hardening and Windows UI verification.

- Pure Core vendor, loot, reward, learnability, and crafting coverage.
- Deterministic `covered`, `partial`, `blocked`, and `missing` states.
- Exact relationship/evidence/governance/blocker handling and input non-mutation.
- Read-only Editor dashboard with no permission, persistence, deployment, launch, or save action.

#### Economy cross-pack duplicate report

Status: implemented, continuing hardening and Windows UI verification.

- Exact case-sensitive cross-pack grouping by economy subject reference and recipe duplicate key.
- Deterministic `review_required`, `partial`, and `blocked` health.
- No display-name, alias, localisation, tag, asset-path, case-folded, or fuzzy matching.
- Read-only review candidates only: no merge, rejection, winner selection, or runtime action.

#### Remaining economy work

- Extend typed planning contracts as later domain tools become implemented.
- Keep runtime behavior in separately reviewed adapters.

### Actors and population

- Actor and Troop Editor.
- Spawn and Encounter Editor.
- Templates, identities, pools, routes, lifecycle, uniqueness, density, cleanup, and rollback research.

### World and societies

- World, Road, and Route Editor.
- Faction and Authority Editor.
- Regions, scenes, locations, roads, factions, cultures, territory, authority, and dispositions.

### Narrative and state

- Quest and State Inspector.
- State keys, decisions, consequences, overlays, and separate read/mutation permissions.

### Assets and localisation

- Asset and Localisation Manager.
- Addresses, bundles, source/cooked assets, localisation keys, evidence, and compatibility.

## Phase 7 — FoA adapter contracts

Status: active development.

The runtime-result evidence envelope remains an observation-only return contract and never authorises another execution.

### Adapter capability contract foundation

Status: implemented, continuing hardening and Windows UI verification.

- Typed lowercase namespaced adapter IDs, strict semantic versions, exact `Mono`/`IL2CPP` targets, and eleven capabilities.
- Exact adapter/capability evidence and same-subject catalog permission/validation proof.
- Deterministic `unsupported`, `version_mismatch`, `permission_missing`, `proof_missing`, and `supported` states.
- Transient registry and read-only **Tainted Grail Adapter Capability Matrix**.
- No runtime adapter implementation, BepInEx/Harmony execution, process access, deployment, game launch, save mutation, or work-order execution.

### Deterministic work-order plan generation

Status: implemented, continuing hardening and Windows UI verification.

- Whole-plan refusal unless all eleven capability rows are `supported`.
- Independent exact identity, permission, proof, payload, relationship, target, and blocker rebuilding.
- Stable plan/step IDs, deterministic order, canonical JSON, and `ExecutionAllowed: false`.
- Read-only **Tainted Grail Adapter Work-Order Plans** pane.
- No plan persistence, export, dispatch, adapter call, code generation, deployment, launch, telemetry, save mutation, or execution.

### Runtime-result evidence envelope

Status: implemented, continuing hardening and Windows UI verification.

- Typed attempted-plan and attempted-step identities bound to exact canonical work-order plans.
- Typed `not_attempted`, `succeeded`, `failed`, and `skipped` outcomes; failures; cleanup/rollback; safe logs; and SHA-256 fingerprints.
- Candidate source/evidence documents returned by value without automatic registration, persistence, validation, or permission.
- Transient `AdapterRuntimeResultRegistry` and read-only **Tainted Grail Adapter Runtime Result Evidence** pane.
- No actual FoA adapter implementation or execution path is introduced or authorised by this contract.

## Phase 8 — Build, package, deploy, and test

Status: active development.

### Reproducible adapter build manifests

Status: implemented, continuing hardening and Windows UI verification.

- Pure Core reproducible build definitions bound to exact plans, packs, adapters, profiles, source/O3DE revisions, toolchains, materials, dependencies, BepInEx metadata, and expected outputs.
- Safe package-relative paths, SHA-256 material fingerprints, redistribution gates, deterministic canonical JSON, and `BuildAllowed: false`.
- Deterministic `plan_mismatch`, `toolchain_unresolved`, `input_missing`, `fingerprint_missing`, `path_invalid`, `redistribution_blocked`, and `ready` states.
- Read-only **Tainted Grail Adapter Build Manifests** pane.
- No compilation, packaging, deployment, launch, or execution is introduced or authorised.

### Deterministic package-assembly preview

Status: implemented, continuing hardening and Windows UI verification.

- Compare one reviewed build manifest with one project-owned staging inventory.
- Require exact manifest/review/inventory/pack/fingerprint/package-root binding.
- Derive a sorted package layout with exact staging paths, package paths, roles, media types, byte sizes, ownership, redistribution state, and output digests.
- Emit explicit omissions for missing manifest outputs and collisions for multiple entries targeting the same output.
- Fail closed for unreviewed manifests, untrusted inventory, undeclared outputs, missing fingerprints, unsafe paths, collisions, and redistribution blockers.
- Deterministic `manifest_not_ready`, `manifest_unreviewed`, `inventory_binding_mismatch`, `inventory_untrusted`, `output_missing`, `fingerprint_missing`, `path_invalid`, `collision`, `redistribution_blocked`, and `ready` states.
- Canonical JSON with `AssemblyAllowed: false`, `ArchiveAllowed: false`, and `DeploymentAllowed: false`.
- Transient registry and read-only **Tainted Grail Package Assembly Preview** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and thirteen-pane Windows UI coverage.
- No file copying, archive creation, deployment, launch, or execution is implemented or authorised.

### Deterministic staging and deployment preview

Status: implemented, continuing hardening and Windows UI verification.

- Compare one exact ready package layout with one accepted evidence-backed declared target inventory.
- Require exact package-preview ID/fingerprint, pack, target root, target inventory ID/fingerprint, and review binding.
- Derive deterministic additions, replacements, removals, unchanged paths, exact conflicts, backup requirements, and inverse rollback steps.
- Preserve previous/desired fingerprints, byte sizes, roles, media types, project ownership, managed/replaceable/removable state, and target entry identities.
- Fail closed for non-ready packages, unreviewed targets, binding drift, untrusted entries, unsafe target/backup paths, ownership/type/multiplicity conflicts, incomplete backups, and incomplete rollback.
- Deterministic `package_not_ready`, `target_unreviewed`, `inventory_binding_mismatch`, `inventory_untrusted`, `path_invalid`, `conflict`, `backup_incomplete`, `rollback_incomplete`, and `ready` states.
- Canonical JSON with `StagingMutationAllowed: false`, `DeploymentMutationAllowed: false`, `RollbackExecutionAllowed: false`, and `LaunchAllowed: false`.
- Transient registry and read-only **Tainted Grail Staging and Deployment Preview** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and fourteen-pane Windows UI coverage.
- **No copying, deletion, deployment, launch, or execution** is implemented or authorised.

Exit criteria:

- a non-ready package layout or unreviewed target inventory cannot produce a ready deployment preview;
- exact package, target, fingerprint, ownership, role, media-type, and path bindings fail closed;
- additions, replacements, removals, conflicts, backups, and rollback are explicit and deterministic;
- a replacement or removal cannot be described without an exact current fingerprint and contained backup path;
- every addition, replacement, and removal has one typed inverse rollback step;
- equivalent inputs produce byte-identical canonical JSON;
- preview generation does not mutate package or target metadata;
- all staging, deployment, rollback-execution, and launch permissions remain false, including for `ready`;
- the Editor pane remains non-editable and exposes no registration, copy, delete, backup, restore, deploy, launch, or execute action;
- the Windows manual UI checklist includes all fourteen panes and the default zero-staging/deployment-input state.

### Typed deployment confirmation and work-order contract

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact ready staging/deployment preview and lowercase SHA-256 fingerprint to one named evidence-backed confirmation.
- Require typed confirmation scope, expiry, maintenance window, preflight evidence, and operator checklist.
- Scope values are `additions_only`, `additions_and_replacements`, and `full_preview`; a narrower scope cannot omit reviewed replacements or removals.
- Require exact UTC issue, expiry, evaluation, maintenance-start, maintenance-end, and preflight-check timestamps without consulting the host clock.
- Require exactly one passed `package_integrity`, `target_inventory`, `rollback_readiness`, and `operator_readiness` preflight record, plus `backup_readiness` when backups exist.
- Derive deterministic typed steps for preflight verification, maintenance-window confirmation, backups, additions, replacements, removals, post-deployment verification, and rollback preservation.
- Derive an operator-facing checklist with `contract_satisfied`, `operator_action_required`, and `blocked` states while `AcknowledgementRecorded` remains false.
- Deterministic `preview_not_ready`, `confirmation_missing`, `confirmation_rejected`, `confirmation_binding_mismatch`, `scope_mismatch`, `confirmation_expired`, `maintenance_window_invalid`, `outside_maintenance_window`, `preflight_missing`, `preflight_failed`, `work_order_incomplete`, and `review_ready` states.
- Canonical JSON with `ExecutionAllowed: false`, `CopyAllowed: false`, `DeleteAllowed: false`, `BackupAllowed: false`, `RestoreAllowed: false`, `DeploymentAllowed: false`, and `LaunchAllowed: false`.
- Transient registry and read-only **Tainted Grail Deployment Confirmation and Work Orders** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and fifteen-pane Windows UI coverage.
- **No copy, delete, backup, restore, deployment, launch, or adapter call** is implemented or authorised.

Exit criteria:

- only a confirmed, evidence-backed, exact-preview-bound named reviewer decision can reach `review_ready`;
- confirmation scope covers every addition, replacement, and removal in the preview;
- evaluation occurs after issue and before expiry inside one valid evidence-backed UTC maintenance window;
- every required preflight kind appears exactly once, passes, and is checked between confirmation issue and evaluation;
- every backup, addition, replacement, removal, and rollback inverse has exact deterministic work-order coverage;
- operator checklist acknowledgements remain pending and cannot be authored by the read-only pane;
- equivalent reviewed inputs produce byte-identical canonical JSON;
- work-order generation does not mutate previews, confirmations, windows, preflight evidence, changes, backups, or rollback rows;
- all execution, copy, delete, backup, restore, deployment, and launch permissions remain false, including for `review_ready`;
- the Editor pane remains non-editable and exposes no registration, acknowledgement, copy, delete, backup, restore, deploy, launch, or execute action;
- the Windows manual UI checklist includes all fifteen panes and the default zero-deployment-work-order-input state.

### Typed deployment execution-result and verification envelope

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact `review_ready` deployment work order, canonical JSON, preview, pack, target inventory, and lowercase SHA-256 fingerprints.
- Require one accepted evidence-backed review of the separately supplied executor, with stable identity, strict semantic version, fingerprint, reviewer, and UTC review time.
- Preserve exact attempted step identities, typed outcomes, attempted state, observed target presence, and deployed fingerprints.
- Preserve exact backup/restore outcomes, target-verification states, rollback results, failures, and safe fingerprinted log references.
- Keep operational success separate from structural contract validity: a failed execution can still be accepted as contract-valid candidate evidence.
- Deterministic `work_order_not_ready`, `executor_unreviewed`, `work_order_binding_mismatch`, `envelope_invalid`, `step_binding_mismatch`, `backup_binding_mismatch`, `verification_binding_mismatch`, `rollback_binding_mismatch`, `failure_log_binding_mismatch`, and `accepted` states.
- Return candidate source/evidence documents by value for work-order binding, executor review, steps, backup/restore outcomes, target verification, rollback, failures, and logs.
- Transient registry and read-only **Tainted Grail Deployment Execution Result Evidence** pane.
- Production-linked tests, focused validator and negative tests, CI integration, public documentation, and sixteen-pane Windows UI coverage.
- **No executor, deployment command, launch path, adapter call, or automatic evidence promotion** is implemented or authorised.

Exit criteria:

- only a complete exact work-order and accepted reviewed-executor binding can reach `accepted`;
- every canonical work-order step has exactly one typed result with exact identity, order, paths, and fingerprints;
- every backup step has one exact backup result, and every mutation step has one verification and rollback result;
- matched/mismatched verification and attempted/not-attempted/succeeded/failed/skipped outcomes remain distinct;
- failures and safe log references bind to the same exact steps or rollback results;
- accepted envelopes return candidate documents without registering, persisting, promoting, validating, permitting, or publishing them;
- result validation and evidence return do not mutate work orders, envelopes, results, or registries;
- the Editor pane remains non-editable and exposes no registration, import, promotion, deployment, launch, or execution action;
- the Windows manual UI checklist includes all sixteen panes and the default zero-deployment-execution-result state.

### Post-deployment verification and release-blocker report

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact current `review_ready` deployment work order, accepted execution-result envelope, and returned candidate evidence set.
- Fail closed for canonical-work-order drift, preview/pack/target drift, result or work-order fingerprint drift, typed-count drift, duplicate or malformed candidate identities, and exact profile/game/branch/runtime mismatch.
- Aggregate work-order step outcomes, backup completeness, target `matched`/`mismatched`/`not_checked` states, rollback requirements and outcomes, failures, and safe referenced diagnostics.
- Emit stable typed compatibility and release blockers with exact subjects, step/rollback identities, evidence IDs, and log-reference IDs.
- Deterministic `evidence_rejected`, `evidence_incomplete`, `verification_incomplete`, `compatibility_blocked`, `rollback_incomplete`, `release_blocked`, and `review_ready` states.
- Keep `HumanReviewRequired: true`; `VerifierExecuted`, `EvidencePromoted`, `ReleasePublished`, `LaunchPerformed`, and `AdapterCalled` remain false.
- Read-only **Tainted Grail Post-Deployment Verification and Release Blockers** pane with candidate-evidence, step, verification, rollback, failure/diagnostic, compatibility-blocker, and release-blocker columns.
- Public contract documentation and seventeen-pane Windows manual UI coverage.
- **No independent verifier, executor, adapter call, FoA launch, evidence promotion, release archive, signing, or publication path** is implemented or authorised.

Exit criteria:

- only the exact current review-ready work order and accepted exact-bound evidence return can be aggregated as accepted execution evidence;
- evidence rejection and incomplete binding remain distinct from operational failures and target mismatches;
- unchecked and mismatched target states block compatibility and release review without claiming independent verification;
- failed/skipped steps, incomplete backups, rollback incompleteness, successful rollback of the attempted candidate, failures, and missing diagnostics remain explicit release blockers;
- equivalent inputs produce identical report identities, statuses, counts, sorted candidate/diagnostic IDs, blocker IDs, blocker ordering, and associations;
- report construction does not mutate the work order, execution-result envelope, evidence return, or registries;
- `review_ready` remains an operator-review state, not compatibility certification, execution authority, or release permission;
- the Editor pane remains non-editable and exposes no verifier, import, promotion, deployment, rollback, launch, archive, signing, publication, or adapter action;
- the Windows manual UI checklist includes all seventeen panes and the default zero-post-deployment-report state.

### Independent post-deployment verifier contract

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact current structurally eligible post-deployment report, typed status, deterministic canonical JSON, execution-result identity/fingerprint, work-order identity/fingerprint, and exact profile/game/branch/runtime context.
- Require one accepted evidence-backed review of the separately supplied verifier with stable identity, strict semantic version, lowercase SHA-256 fingerprint, named reviewer, UTC review time, and unique required capabilities.
- Require exactly one check for every canonical add, replace, and remove work-order step with exact sequence, target path, expected presence, and expected fingerprint.
- Preserve typed `not_run`, `matched`, `mismatched`, `failed`, and `inconclusive` outcomes, attempted and observation-recorded state, observed presence/fingerprint, and UTC check time.
- Require reciprocal same-check failures and safe fingerprinted diagnostics for failed and inconclusive checks.
- Deterministic `report_not_ready`, `verifier_unreviewed`, `report_binding_mismatch`, `envelope_invalid`, `check_coverage_incomplete`, `failure_diagnostic_binding_mismatch`, `observation_mismatch`, and `accepted` states.
- Keep complete observation mismatches structurally contract-valid while distinguishing them from all-matched `accepted` results.
- Return candidate source/evidence documents by value for report binding, verifier review, checks, failures, and diagnostics without automatic promotion.
- Transient `AdapterPostDeploymentVerifierResultRegistry` and read-only **Tainted Grail Independent Post-Deployment Verifier Results** pane.
- Public contract documentation and eighteen-pane Windows manual UI coverage.
- **No verifier discovery or execution, target filesystem access, deployment mutation, FoA launch, adapter call, evidence promotion, archive signing, or release publication** is implemented or authorised.

Exit criteria:

- only an exact current eligible report and accepted capability-complete verifier review can reach a structurally valid result;
- every canonical mutation step has exactly one exact independent check and no unknown, duplicate, missing, or extra check can pass;
- matched observations reproduce the exact expected target state while mismatched observations actually differ;
- not-run checks remain distinct from failed and inconclusive attempts;
- failures and diagnostics are stable, safe, unique, reciprocal, same-check bound, and non-orphaned;
- structurally valid observation mismatches return candidate evidence but never become `accepted`;
- equivalent inputs produce identical canonical report JSON, statuses, issue ordering, candidate identities, and evidence ordering;
- validation and evidence return do not mutate work orders, execution results, reports, verifier envelopes, or registries;
- the Editor pane remains non-editable and exposes no verifier, filesystem, deployment, launch, adapter, promotion, archive, signing, publication, or release action;
- the Windows manual UI checklist includes all eighteen panes and the default zero-independent-verifier-envelope state.

### Verifier evidence reconciliation and release-decision envelope

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact post-deployment report, one structurally valid `accepted` or `observation_mismatch` verifier evidence return, and one named human release review to exact report JSON, work-order/execution/verifier fingerprints, profile/game/branch/runtime context, pack, preview, target inventory, and candidate source/evidence identities.
- Keep contract validity, compatibility assessment, release decision, and human-review state as separate typed axes.
- Preserve every existing report blocker and every adverse verifier observation; a matched observation cannot silently clear a blocker and an all-matched verifier result cannot automatically approve release.
- Emit deterministic preserved, supporting, contradictory, and new findings for every report blocker and verifier check.
- Require explicit evidence-backed human dispositions for preserved blockers, contradictions, mismatches, failures, inconclusive checks, and not-run checks.
- Derive deterministic `unassessed`, `clear`, `blocked`, and `inconclusive` compatibility states independently from `pending`, `hold`, `rejected`, and `approved` release decisions.
- Deterministic contract status precedence is `report_not_ready`, `verifier_evidence_invalid`, `binding_mismatch`, `review_missing`, `review_invalid`, `disposition_incomplete`, `decision_inconsistent`, and `accepted`.
- Return reconciliation binding, compatibility, human review, release decision, and every finding as candidate evidence by value without automatic registration or promotion.
- Transient `AdapterVerifierEvidenceReconciliationRegistry` and read-only **Tainted Grail Verifier Evidence Reconciliation and Release Decision** pane.
- Public contract documentation and nineteen-pane Windows manual UI coverage.
- **No verifier execution, target access, file mutation, deployment, rollback execution, FoA launch, adapter call, evidence promotion, archive assembly, checksum generation, signing, upload, or release publication** is implemented or authorised.

Exit criteria:

- both all-matched and structurally valid adverse verifier evidence remain representable and exact-bound;
- every upstream report blocker remains visible with its original effects and associations;
- every contradictory or adverse finding receives one explicit human disposition before the review can be complete;
- compatibility and release decisions remain independent and deterministic;
- approval is impossible while compatibility is unclear, release blockers remain, verifier evidence is adverse, or required dispositions are unresolved or rejected;
- equivalent inputs produce identical findings, disposition coverage, status, axes, canonical JSON, and candidate evidence ordering;
- reconciliation does not mutate reports, verifier evidence, work orders, execution results, reviews, or registries;
- the Editor pane remains non-editable and exposes no review-authoring, verifier, filesystem, deployment, launch, adapter, promotion, archive, signing, publication, or release action;
- the Windows manual UI checklist includes all nineteen panes and the default zero-verifier-reconciliation-request state.

### Release-artifact provenance and signing-intent contract

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact accepted reconciliation with `clear` compatibility, `approved` release decision, and complete review to one exact `ready` package layout.
- Require one declared release-content row for every package-layout entry with exact identity, path, role, media type, byte size, project ownership, redistribution eligibility, provenance, legal disposition, and expected checksum.
- Treat lowercase SHA-256 values as declarations bound to existing reviewed package-layout output digests; no file is opened or hashed.
- Require stable evidence-backed provenance for every content row and one exact approved legal/redistribution disposition per content.
- Require one reviewed `unsigned` or `sign_externally` intent; external signing intent preserves typed identity kind, signer, locator, and fingerprint without loading a key or producing a signature.
- Require at least one reviewed typed publication-target declaration without authenticating, uploading, or publishing.
- Deterministic status precedence is `reconciliation_not_approved`, `package_not_ready`, `binding_mismatch`, `content_invalid`, `checksum_declaration_invalid`, `provenance_incomplete`, `legal_disposition_incomplete`, `signing_intent_invalid`, `publication_target_invalid`, and `ready`.
- Return exact binding, content declarations, signing intent, and publication targets as candidate evidence by value without automatic registration or promotion.
- Transient `AdapterReleaseArtifactRegistry` and read-only **Tainted Grail Release Artifact Provenance and Signing Intent** pane.
- Focused repository validator, public contract documentation, and twenty-pane Windows manual UI coverage.
- **No file read/hash/copy, checksum generation, archive assembly, signing, upload, publication, FoA launch, adapter call, or deployment mutation** is implemented or authorised.

Exit criteria:

- only an exact approved reconciliation and exact ready package layout can reach `ready` metadata;
- every reviewed package-layout entry has exactly one declaration and no missing, duplicate, or extra content can pass;
- every expected checksum equals the exact reviewed package-layout digest while remaining explicitly ungenerated and unverified;
- provenance and legal dispositions are complete, exact-bound, evidence-backed, named, and time-bounded;
- unsigned intent cannot carry a signing identity and external-signing intent cannot omit reviewed identity metadata;
- publication targets remain declarations only and cannot trigger network or release actions;
- equivalent inputs produce identical sorted content, provenance, disposition, target, blocker, status, and canonical JSON output;
- envelope construction does not mutate reconciliation, package, request, evidence, or registries;
- all file, checksum-generation, archive, signing, upload, publication, launch, adapter, and deployment flags remain false, including for `ready`;
- the Editor pane remains non-editable and exposes no registration, review-authoring, file, archive, signing, upload, publication, launch, adapter, or deployment action;
- the Windows manual UI checklist includes all twenty panes and the default zero-release-artifact-envelope state.

### Release-assembly and checksum-result envelope

Status: implemented, continuing hardening and Windows UI verification.

- Bind one exact ready release-artifact envelope and its canonical SHA-256 fingerprint to one separately supplied external assembler/checksummer result.
- Require an accepted evidence-backed review for an external tool declaring content-checksum, archive-assembly, and archive-checksum capabilities.
- Preserve exactly one observation per declared content with exact identity, package path, expected checksum, attempted/recorded state, outcome, observed checksum, timestamp, failures, and diagnostics.
- Preserve one archive identity, safe relative reference, format, attempt/outcome state, presence, size, fingerprint, timestamp, failures, and diagnostics.
- Keep valid adverse outcomes representable: contract acceptance does not convert mismatched, failed, inconclusive, not-observed, failed-archive, or skipped-archive results into successful release evidence.
- Validate stable failure/diagnostic identity, content bindings, safe references, fingerprints, and referential integrity.
- Return exact binding, review, archive, checksum, failure, and diagnostic claims as candidate evidence by value without automatic registration or promotion.
- Transient `AdapterReleaseAssemblyResultRegistry` and read-only **Tainted Grail Release Assembly and Checksum Results** pane.
- Focused repository validator, public contract documentation, and twenty-one-pane Windows manual UI coverage.
- **No file read/hash/copy, checksum generation, archive assembly, signing, upload, publication, FoA launch, adapter call, or deployment mutation** is implemented or authorised.

Exit criteria:

- only an exact ready release-artifact canonical envelope and fingerprint can be accepted;
- every declared content has exactly one exact-bound checksum observation and no missing, duplicate, or extra observation can pass;
- assembler/checksummer review is accepted, evidence-backed, named, time-bounded, fingerprinted, and capability-complete;
- checksum and archive flags, outcomes, fingerprints, timestamps, failures, and diagnostics are self-consistent;
- failures and diagnostics remain safe, exact-bound supplied metadata and referenced diagnostic content is never opened;
- candidate evidence is returned only for a structurally valid result and is never automatically registered or promoted;
- result validation does not mutate the release artifact, result envelope, evidence registry, filesystem, or transient registries;
- all SDK file, hash, archive, signing, upload, publication, launch, adapter, and deployment flags remain false;
- the Editor pane remains non-editable and exposes no registration, review-authoring, file, archive, signing, upload, publication, launch, adapter, or deployment action;
- the Windows manual UI checklist includes all twenty-one panes and the default zero-release-assembly-result state.

### Release-signing result envelope

Status: implemented, continuing hardening and exact-head host/UI validation.

- Bind one exact accepted release-assembly/checksum result and successful supplied archive to one separately supplied external signing result.
- Require one ready release artifact with reviewed `sign_externally` intent and exact signer identity metadata.
- Require an accepted evidence-backed external signer review with strict tool version, lowercase SHA-256 fingerprint, named reviewer, UTC review time, and archive-signing plus signature-artifact-fingerprint capabilities.
- Preserve typed `not_attempted`, `succeeded`, `failed`, and `skipped` outcomes, attempted state, completion/capture times, signature artifacts, failures, retryable state, and safe diagnostics.
- Require exact release-artifact, assembly-result, archive, reconciliation, package, manifest, pack, profile, game, branch, runtime, and signing-intent binding.
- Keep structurally valid failed outcomes representable as adverse evidence instead of rewriting operational failure as contract failure or success.
- Deterministic `assembly_result_not_accepted`, `signing_not_requested`, `signer_unreviewed`, `assembly_binding_mismatch`, `signing_intent_binding_mismatch`, `envelope_invalid`, `signing_outcome_mismatch`, `signature_artifact_binding_mismatch`, `failure_diagnostic_binding_mismatch`, and `accepted` states.
- Return exact binding, signer review, signing outcome, signature-artifact, failure, and diagnostic claims as candidate evidence by value without automatic registration, trust, or promotion.
- Transient `AdapterReleaseSigningResultRegistry` and read-only **Tainted Grail Release Signing Results** pane with registry rows labelled `contract status=not evaluated`.
- Focused repository validator, local-validation integration, public contract documentation, and twenty-two-pane Windows manual UI coverage.
- **No archive access, key loading, identity resolution, signing, signature verification, artifact writing, upload, publication, FoA launch, adapter call, deployment mutation, or save mutation** is implemented or authorised.

Exit criteria:

- only an exact accepted release-assembly/checksum result for a `sign_externally` artifact can produce accepted signing evidence;
- signer review and required capabilities are complete, evidence-backed, named, time-bounded, fingerprinted, and exact-bound;
- signing outcomes, attempted state, timestamps, signature artifacts, and failures are self-consistent without erasing adverse results;
- signature artifacts and diagnostics use stable identities, safe relative references, exact fingerprints, exact archive bindings, unique Windows path identities, and reciprocal references;
- candidate evidence is returned only for a structurally valid result and is never automatically registered, persisted, promoted, trusted, uploaded, or published;
- validation does not mutate the release artifact, assembly result, accepted assembly evidence, signing result, or transient registries;
- all SDK file, archive, key, identity, cryptographic, copy/write, upload, publication, launch, adapter, deployment, and save flags remain false;
- the Editor pane remains non-editable, explicitly separates registry display from contract acceptance, and exposes no operational action;
- the Windows manual UI checklist includes all twenty-two panes and the default zero-release-signing-result state.

### Remaining Phase 8 release work

Controlled pipeline:

```text
validate → generate → build → package → deploy → launch → capture → attach evidence
```

Remaining Phase 8 work includes controlled package assembly, trusted filesystem inventory and hashing, trusted
identity/time providers, acknowledgement and key custody, actual backup/restore and deployment implementations,
actual independent verifier execution and target access, cryptographic signing and verification, publication
results, and separately reviewed runtime adapters.

## Phase 9 — Ecosystem and automation

- Importer plugin SDK and domain-tool extensions.
- Headless validation and packaging.
- Public schema packages, CI fixtures, compatibility matrices, and migration tooling.
- Documentation and examples for third-party adapters.

## Cross-cutting requirements

Every phase preserves exact identity, pack ownership, evidence provenance, independent governance axes, fail-closed validation and permission, editor/runtime separation, durable schema versioning, legal redistribution boundaries, accessibility, usable errors, public documentation, and executable tests.

## Not roadmap commitments

This document does not promise release dates, support every game patch, guarantee compatibility with private mods, or authorise runtime actions that have not passed the required validation and permission gates.
