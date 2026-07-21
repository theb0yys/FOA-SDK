# Release Process

## Status

The project is pre-alpha and has not published a supported binary release. This process defines the controls required before tags and packages are produced.

Automatic GitHub Actions triggers are currently suspended because exact-head jobs could not acquire GitHub-hosted runners. Development uses the documented local validation gate, but **no public release may proceed until automatic hosted CI is safely restored and passes on the exact release commit**.

## Release principles

A release must be traceable to a reviewed `main` commit, reproducible from documented inputs, explicit about compatibility and limitations, free of proprietary content and secrets, validated against security/schema boundaries, and accompanied by checksums, migration guidance, and rollback information.

A green build alone does not make a release safe. A queued, skipped, waiting, absent, or manual-only workflow is not a green build.

## Versioning and channels

Use Semantic Versioning. While below `1.0.0`, breaking changes still require explicit migration and release notes.

Channels are `development`, `alpha`, `beta`, `release-candidate`, and `stable`. Pack release channels do not automatically indicate SDK release readiness.

## Release inputs

Record:

- source commit/tag and O3DE revision;
- supported operating systems, compilers, configuration, and dependency locks;
- TG SDK schema versions;
- supported FoA profile, Core, and adapter versions;
- exact build manifest, package preview, staging/deployment preview, deployment work order, deployment execution-result envelope, release-artifact envelope, accepted release-assembly/checksum result, release-signing result envelope, **generated package contents**, and test/validation results;
- exact target-inventory review, additions, replacements, removals, conflicts, backups, rollback steps, confirmation, maintenance window, preflight evidence, operator checklist, backup, restore, target-verification, rollback, and log evidence, licence, notices, and redistribution decisions.

## Release readiness checklist

### Governance

- release scope approved;
- changelog and roadmap updated;
- licence/notices reviewed;
- no unresolved blocking review or high-severity security issue.

### Code, local validation, and CI

- release commit is on `main` and `foa-development` is synchronized;
- `run_local_validation.py --keep-going` passes on the exact release commit;
- the complete command, tester, timestamp, exit result, and skipped checks are recorded;
- supported host builds and compiled tests pass on the exact release commit;
- automatic GitHub-hosted CI has been restored under the reviewed runner policy;
- every required automatic check starts, completes successfully, and points to the exact release commit;
- no unavailable, queued, skipped, approval-blocked, or stale workflow is configured as release proof;
- warnings and flaky failures are understood and documented.

### Schemas and compatibility

- all durable versions documented;
- migrations tested or unsupported versions explicitly rejected;
- compatibility matrix and pack/Core/adapter constraints published;
- save and deployment impact documented.

### Security and privacy

- dependency/vulnerability review complete;
- no credentials, runner registration tokens, private paths, proprietary game content, or unapproved third-party binaries;
- import limits and path containment tested;
- generated packages inspected;
- security notes prepared where required;
- no general-purpose public-repository self-hosted runner is used for untrusted pull-request code.

### Documentation

- README/User Guide match the release;
- Data Formats match serialized output;
- build/install instructions verified;
- CI, runner, and local-validation status is accurate;
- limitations, migration, upgrade, troubleshooting, and rollback updated.

### Windows manual UI evidence

Developer Preview and later Windows claims require completed **Windows manual UI evidence** for the **exact reviewed `main` commit**.

Confirm every checklist item passed; Editor launch and activation log are confirmed; all twenty-two TG SDK panes are present; required screenshots are present; screenshot hashes, dimensions, and sizes verify; the tester completed the privacy attestation and runtime-boundary attestation; no game files, saves, credentials, runner tokens, or private paths are visible; and the verifier passed for the exact commit.

Screenshots and evidence are review material and **must not be committed**.

### Package-preview gate

Before any future package assembler or release step is enabled:

- the build manifest has an accepted evidence-backed review;
- the staging inventory binds to the exact manifest fingerprint, pack, and package root;
- every included entry is project-owned and has a lowercase SHA-256 digest;
- every expected manifest output maps to exactly one staged output;
- there are no omissions, collisions, unsafe paths, undeclared outputs, or redistribution blockers;
- the package preview is `ready` while `AssemblyAllowed`, `ArchiveAllowed`, and `DeploymentAllowed` remain false;
- a human confirms every **redistributable output** and exclusion before a separately reviewed assembler can run.

A ready preview is not proof that files exist, were copied, or were archived.

### Release-assembly/checksum-result evidence gate

After release-artifact metadata is `ready`, the SDK may validate a separately supplied external assembler/checksummer result only when:

- the result binds to the exact release-artifact canonical JSON and SHA-256 fingerprint;
- the assembler/checksummer has an accepted evidence-backed review for content checksums, archive assembly, and archive checksums;
- every declared release content has exactly one observation bound to its identity, path, and expected checksum;
- archive outcome, identity, safe reference, size, fingerprint, timestamp, failures, and diagnostics are self-consistent;
- adverse or incomplete results remain visible and are not converted into successful release claims.

An accepted result proves only that the supplied metadata passed the contract. The SDK does not read or hash package files, assemble or open an archive, generate a checksum, sign, upload, or publish. See [FoA Release Assembly and Checksum Results](FOA_RELEASE_ASSEMBLY_CHECKSUM_RESULTS.md).

### Release-signing result evidence gate

After release-assembly/checksum evidence is contract-valid and accepted, the SDK may validate a separately supplied external signing result only when:

- the exact ready release artifact declares reviewed `sign_externally` intent;
- the result binds to the exact release-artifact fingerprint, accepted assembly-result identity/fingerprint, successful archive identity/path/format/size/fingerprint, and release context;
- the external signer has an accepted evidence-backed review with strict tool version, lowercase SHA-256 fingerprint, named reviewer, UTC review time, and archive-signing plus signature-artifact-fingerprint capabilities;
- the result preserves the exact approved signing-intent identity, identity kind, signer, locator, and identity fingerprint;
- attempted state, typed outcome, completion/capture timestamps, signature artifacts, failures, and diagnostic references are self-consistent;
- signature artifacts and diagnostics have stable identities, safe relative references, exact fingerprints, exact archive binding, unique Windows path identities, and reciprocal references;
- valid failed, skipped, or not-attempted outcomes remain explicit adverse evidence instead of being converted into successful release claims.

An `accepted` release-signing result proves only that the separately supplied metadata passed the exact-binding contract. The SDK does not open the archive, load keys or credentials, resolve or authenticate an identity, sign or verify data, copy or write signature artifacts, upload, publish, launch FoA, call an adapter, mutate deployment, or modify saves. See [FoA Release-Signing Result Evidence](FOA_RELEASE_SIGNING_RESULTS.md).

### Staging/deployment-preview gate

Before any future deployer or deployment work order can be enabled:

- the exact package preview is `ready`, fingerprinted, and retains all assembly/archive/deployment permissions as false;
- the declared target inventory has an accepted evidence-backed review bound to its exact ID and fingerprint;
- the package preview, target inventory, pack, target root, and package-preview fingerprint bind exactly;
- every target entry has a stable identity, exact path, role, media type, owner pack, project-owned state, managed state, and current fingerprint;
- **additions, replacements, removals, and conflicts** are reviewed explicitly rather than inferred from timestamps or names;
- replacements and removals have a complete **backup and rollback preview** with contained backup paths and exact pre-change fingerprints;
- every addition, replacement, and removal has one typed inverse rollback step;
- no foreign, unmanaged, multiplicity, role/media-type, replaceability, removability, path, backup, or rollback conflict remains;
- the staging/deployment preview is `ready` while `StagingMutationAllowed`, `DeploymentMutationAllowed`, `RollbackExecutionAllowed`, and `LaunchAllowed` remain false.

The current contract **does not mutate deployment**. A ready staging/deployment preview does not prove that target files exist, backups were created, rollback was tested, or deployment is safe to execute.

### Explicit deployment confirmation and work-order gate

Before any future deployment executor may consume a work order:

- one **explicit deployment confirmation** binds to the exact ready preview ID and lowercase SHA-256 fingerprint;
- the decision is `confirmed`, names the reviewer, carries evidence, and uses a typed scope that covers every addition, replacement, and removal;
- confirmation issue, expiry, and deterministic evaluation timestamps use exact UTC seconds and evaluation occurs before expiry;
- one reviewed **maintenance window** binds to the same preview, has an increasing UTC interval, names the operator group, and contains the evaluation time;
- exactly one passed `package_integrity`, `target_inventory`, `rollback_readiness`, and `operator_readiness` record is present, plus `backup_readiness` when backups exist;
- all **preflight evidence** is preview-bound, named, evidence-backed, and checked between confirmation issue and evaluation;
- every preview backup, addition, replacement, removal, and rollback inverse has exact deterministic work-order coverage;
- the **operator checklist** exposes contract-satisfied, blocked, and operator-action-required items while acknowledgements remain unrecorded;
- the work order is `review_ready` while `ExecutionAllowed`, copy, delete, backup, restore, deployment, and launch permissions remain false.

`review_ready` is not approval to execute. A future executor requires a separate design, implementation, review, validation, and evidence-return boundary; **execution remains separately prohibited**.

### Deployment execution-result evidence gate

Before any post-deployment compatibility or release claim can use executor-supplied results:

- one deployment execution-result envelope binds to the exact current `review_ready` work-order ID, canonical JSON, fingerprint, preview, pack, and target inventory;
- the separately supplied executor has an accepted evidence-backed review with stable identity, strict version, fingerprint, reviewer, and UTC review time;
- every canonical work-order step has exactly one typed attempted result with exact sequence, kind, paths, and previous/desired fingerprints;
- every backup step has one exact backup result, and successful backup fingerprints equal the exact pre-change fingerprints;
- every addition, replacement, and removal has one exact target verification and one typed rollback result;
- matched and mismatched target verification remain distinct, and successful rollback reports the exact inverse final presence and fingerprint;
- failures and safe fingerprinted log references bind to the same exact steps or rollback results;
- the envelope is structurally `accepted`; operational failures, skipped steps, mismatched targets, failed rollback, or unresolved diagnostics remain explicit release blockers rather than being hidden by contract acceptance;
- returned candidate evidence remains unpromoted until a separate reviewed source/evidence import, validation, permission, compatibility, and release decision is completed.

A structurally accepted deployment execution-result envelope is not proof of successful or safe deployment. The current contract does not invoke an executor, independently verify files, promote evidence, launch FoA, or publish a release.

## Controlled pipeline

The eventual pipeline is:

```text
validate → configure → build → test → package → inspect → checksum → publish
```

Until each mutation and verification step has separate reviewed tooling, maintainers document every manual command/input and do not infer authority from a read-only preview, work order, result envelope, or queued workflow.

### Prebuilt SDK installer implementation

The reviewed Windows installer workflow is the narrowly scoped implementation
of build-to-artifact copying and archive/MSI assembly for the **TG SDK product**.
It consumes O3DE's `INSTALL` target, binds a human redistribution review to the
exact inventory fingerprint, creates a new verified staging root, and produces
an unsigned development MSI and deterministic portable ZIP from that same root.

This implementation does not consume or execute an FoA adapter package-preview,
deployment work order, or release-artifact envelope. It grants no FoA package,
deployment, signing, upload, or publication authority. Public release remains
blocked until automatic hosted CI, exact reviewed `main`, Windows Editor UI,
legal, security, upgrade, checksum, and signing gates pass.

## Package and deployment boundaries

Release packages may contain only project-owned or legally redistributable output. Do not package game binaries/assets, extracted proprietary data, private workspace content, personal logs/paths, source trees or work-order plans not intended for distribution, or third-party binaries without redistribution rights.

The current build-manifest, package-preview, staging/deployment-preview, deployment-work-order, deployment execution-result, release-artifact, release-assembly/checksum-result, and release-signing-result contracts do not copy, replace, delete, back up, restore, create or open archives, load keys, sign or verify, write signature artifacts, deploy content, record acknowledgement, invoke an executor, promote candidate evidence, launch FoA, upload, or publish releases.

## Artefacts

A public release may include source tag/archive, editor/SDK package, schema package, checksums, SBOM when available, licence/notices, release notes, compatibility matrix, migration, and rollback instructions.

Manual UI evidence is not a redistributable product payload.

## Checksums

Publish SHA-256 **checksums** for every distributed binary archive and generated release package. Checksums prove integrity, not runtime safety or trustworthiness.

## Signing

The current release-signing result contract records and validates separately supplied signer metadata only. It does not load a key, perform signing, verify a signature, authenticate a signer, or establish cryptographic trust.

Do not claim signed releases until signing identity, key custody, signing and verification commands, rotation/revocation, compromise response, trusted execution, and independently reproducible verification evidence are implemented and verifiable.

## Publishing

Only a maintainer may publish. Verify the tag points to the approved commit, artefacts were produced in a clean environment, checksums match, the release page is reviewed, automatic hosted CI passed the exact commit, and no restricted/private file is attached.

## Post-release, rollback, and hotfixes

After publishing, verify downloads/checksums, monitor issues/security reports, preserve logs/results, and begin a patch release for confirmed release-blocking defects.

A release may be withdrawn for security exposure, data/save corruption, unsafe deployment, incorrect runtime permission, severe compatibility failure, or restricted/private content. Preserve an audit record where safe, identify affected versions, provide safe removal/rollback, and publish a corrected release when available.

Emergency hotfixes still require a scoped change, regression test, DCO/provenance, maintainer approval, changelog/advisory update, and post-release review.
