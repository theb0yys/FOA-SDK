# Release Process

## Status

The project is pre-alpha and has not published a supported binary release. This process defines the controls required before tags and packages are produced.

## Release principles

A release must be traceable to a reviewed `main` commit, reproducible from documented inputs, explicit about compatibility and limitations, free of proprietary content and secrets, validated against security/schema boundaries, and accompanied by checksums, migration guidance, and rollback information.

A green build alone does not make a release safe.

## Versioning and channels

Use Semantic Versioning. While below `1.0.0`, breaking changes still require explicit migration and release notes.

Channels are `development`, `alpha`, `beta`, `release-candidate`, and `stable`. Pack release channels do not automatically indicate SDK release readiness.

## Release inputs

Record:

- source commit/tag and O3DE revision;
- supported operating systems, compilers, configuration, and dependency locks;
- TG SDK schema versions;
- supported FoA profile, Core, and adapter versions;
- exact build manifest, package preview, staging/deployment preview, deployment work order, **generated package contents**, and test/validation results;
- exact target-inventory review, additions, replacements, removals, conflicts, backups, rollback steps, confirmation, maintenance window, preflight evidence, operator checklist, licence, notices, and redistribution decisions.

## Release readiness checklist

### Governance

- release scope approved;
- changelog and roadmap updated;
- licence/notices reviewed;
- no unresolved blocking review or high-severity security issue.

### Code and CI

- release commit is on `main` and `foa-development` is synchronized;
- focused and repository validators pass;
- supported host builds and compiled tests pass;
- warnings/flaky failures are understood and documented.

### Schemas and compatibility

- all durable versions documented;
- migrations tested or unsupported versions explicitly rejected;
- compatibility matrix and pack/Core/adapter constraints published;
- save and deployment impact documented.

### Security and privacy

- dependency/vulnerability review complete;
- no credentials, private paths, proprietary game content, or unapproved third-party binaries;
- import limits and path containment tested;
- generated packages inspected;
- security notes prepared where required.

### Documentation

- README/User Guide match the release;
- Data Formats match serialized output;
- build/install instructions verified;
- limitations, migration, upgrade, troubleshooting, and rollback updated.

### Windows manual UI evidence

Developer Preview and later Windows claims require completed **Windows manual UI evidence** for the **exact reviewed `main` commit**.

Confirm every checklist item passed; Editor launch and activation log are confirmed; required screenshots are present; screenshot hashes, dimensions, and sizes verify; the tester completed the privacy attestation and runtime-boundary attestation; no game files, saves, credentials, or private paths are visible; and the verifier passed for the exact commit.

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

## Controlled pipeline

The eventual pipeline is:

```text
validate → configure → build → test → package → inspect → checksum → publish
```

Until each mutation step has separate reviewed tooling, maintainers document every manual command/input and do not infer authority from a read-only preview or work order.

## Package and deployment boundaries

Release packages may contain only project-owned or legally redistributable output. Do not package game binaries/assets, extracted proprietary data, private workspace content, personal logs/paths, source trees or work-order plans not intended for distribution, or third-party binaries without redistribution rights.

The current build-manifest, package-preview, staging/deployment-preview, and deployment-work-order contracts do not copy, replace, delete, back up, restore, create archives, deploy content, record acknowledgement, launch FoA, or publish releases.

## Artefacts

A public release may include source tag/archive, editor/SDK package, schema package, checksums, SBOM when available, licence/notices, release notes, compatibility matrix, migration, and rollback instructions.

Manual UI evidence is not a redistributable product payload.

## Checksums

Publish SHA-256 **checksums** for every distributed binary archive and generated release package. Checksums prove integrity, not runtime safety or trustworthiness.

## Release notes

Include summary, user-visible changes, security notes, schema/migration changes, compatibility changes, known limitations, upgrade/rollback steps, checksums, and artefact list. Do not describe planned features as shipped.

## Signing

Do not claim signed releases until signing identity, key custody, verification commands, rotation/revocation, and compromise response are implemented and verifiable.

## Publishing

Only a maintainer may publish. Verify the tag points to the approved commit, artefacts were produced in a clean environment, checksums match, the release page is reviewed, and no restricted/private file is attached.

## Post-release, rollback, and hotfixes

After publishing, verify downloads/checksums, monitor issues/security reports, preserve logs/results, and begin a patch release for confirmed release-blocking defects.

A release may be withdrawn for security exposure, data/save corruption, unsafe deployment, incorrect runtime permission, severe compatibility failure, or restricted/private content. Preserve an audit record where safe, identify affected versions, provide safe removal/rollback, and publish a corrected release when available.

Emergency hotfixes still require a scoped change, regression test, DCO/provenance, maintainer approval, changelog/advisory update, and post-release review.
