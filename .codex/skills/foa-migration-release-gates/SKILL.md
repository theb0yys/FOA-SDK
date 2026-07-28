---
name: foa-migration-release-gates
description: Use for O3DE pin changes, schema or persistence migration, Unity profile or version changes, Mono or IL2CPP adapter changes, dependency or package-layout changes, installer upgrades, release lanes, save policy, or release readiness. It treats these as controlled migrations rather than routine version bumps.
---

# FOA-SDK Migration and Release Gates

Use whenever a change can alter compatibility, supported profiles, durable data, dependencies, packaging, installation, adapters, or release claims.

## Research To Read

Read:

- migration and release research
- `ROADMAP.md` and `CHANGELOG.md`
- governance and release process
- O3DE dependency boundary and `o3de.lock.json`
- data-format, schema, persistence, and canonical-interchange documents
- installer and packaging documents
- Unity provider and conversion-profile documents
- Mono and IL2CPP adapter contracts
- exact Fall of Avalon profile evidence
- existing migration, upgrade, rollback, package, adapter, and release tests

## Migration Principle

Treat changes to engine, Unity conversion, game profile, adapter, installer, schema, persistence, dependency, or package layout as platform migrations.

Do not use a direct “retarget, fix compile errors, and release” model when the change affects multiple governed surfaces.

## Version and Profile Authority

Before implementation:

1. Verify the exact pinned or installed version/profile.
2. Separate proven, strongly indicated, and unsupported targets.
3. Keep incompatible reference, conversion, installer, Mono, and IL2CPP lanes separate.
4. Place conditional behavior only at reviewed adapter or compatibility boundaries.
5. Do not scatter version checks through domain logic.
6. Do not plan against a version or profile that is not evidenced by repository or exact local proof.

## Durable Data and Compatibility

Do not promise compatibility for:

- workspaces
- packs
- schemas and manifests
- evidence and catalog records
- canonical interchange
- conversion results
- installer state
- adapter state
- saves or game-runtime data

unless a tested migration, converter, compatibility reader, or explicit rejection policy exists.

When the authorised policy requires a new workspace, new pack, new profile, reinstall, or unsupported downgrade, state that directly.

## Staged Migration Order

Use the applicable staged sequence:

1. Freeze and archive the known-good baseline.
2. Verify exact target versions, profiles, and external dependencies.
3. Bring up the minimal shell and prerequisites.
4. Restore configure and compilation.
5. Validate schemas, manifests, persistence, and canonical interchange.
6. Bring up Foundation and non-UI services.
7. Bring up Editor panes, UI, and assets.
8. Bring up external toolchain and Unity conversion.
9. Build packaging and installer lifecycle.
10. Bring up the exact Mono or IL2CPP adapter lane.
11. Run migration, compatibility, degraded-mode, and rollback tests.
12. Run long-duration, performance, and release-candidate gates.
13. Capture exact-head evidence and maintainer review.

Do not collapse the sequence into `build passed`.

## Release Readiness

A release candidate requires applicable proof for:

- exact source and dependency identity
- clean configure and required builds
- compiled tests with nonzero discovery
- schema, persistence, interchange, and migration behavior
- UI and Editor acceptance
- Unity conversion and output inventory
- package and installer install, repair, upgrade, rollback, and uninstall
- separate Mono and IL2CPP adapter status
- security, privacy, legal, and provenance review
- performance and soak evidence
- documentation, changelog, and known limitations
- exact-install runtime status
- exact-head receipt and maintainer approval

## Hard Stops

Stop when:

- target version or profile is unknown
- references from incompatible versions are mixed
- engine identity differs from the lock without an authorised lock migration
- compatibility is promised without converter or migration proof
- unsupported downgrade or new-profile behavior is hidden
- Mono and IL2CPP are treated as one runtime path
- installer upgrade or rollback behavior is unspecified
- a release claim relies only on local compilation
- runtime support is advertised without profile-specific evidence
- runtime sign-off lacks exact packaged evidence

## Validation

Run the narrow gate for the touched subsystem and the complete release sequence required by its blast radius. Report every unexecuted lane explicitly.

Artifact-producing work follows `.codex/workflows/foa_artifact_deploy_gate.md` and keeps generated output outside the source checkout.

Report:

- baseline and target versions or profiles
- migration stages completed
- compatibility and rejection policy
- tests and artifacts
- manual Editor, Unity, and installer rows
- Mono and IL2CPP status separately
- long-run and release-candidate status
- complete, partial, or blocked readiness

## Runtime Proof

Static review, configure, compilation, package generation, installer creation, and adapter build are not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless exact packaged evidence for the target profile exists.
