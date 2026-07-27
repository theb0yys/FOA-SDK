---
name: foa-migration-release-gates
description: Use for O3DE pin changes, schema or persistence migration, Unity profile/version changes, Mono/IL2CPP adapter changes, dependency or package layout, installer upgrade, release lanes, save policy, or release readiness.
---

# FOA-SDK Migration and Release Gates

## Research To Read

Read migration/release research, roadmap, changelog, governance, release process, O3DE dependency boundary, data formats, installer and adapter docs, and relevant tests.

## Version Policy

Treat engine, Unity conversion, game profile, adapter, installer, schema, and package changes as controlled migrations, not routine version bumps. Verify exact installed or pinned versions before implementation claims. Keep incompatible reference/profile lanes separate and conditional logic at adapter boundaries.

## Persistence and Release Policy

Do not promise workspace, pack, interchange, installer, adapter, or save compatibility without tested migration/converter proof. State new-profile or new-workspace requirements explicitly when that is the authorised policy.

## QA Gates

Archive/freeze; empty-shell/prerequisite smoke; configure/compile; schema/data load; UI/Editor bring-up; conversion; packaging/installer; adapter profile; long-run stability; release candidate. Do not collapse them into `build passed`.

## Hard Stops

Stop if target versions/profiles are unknown, references are mixed, compatibility is promised without proof, runtime support is advertised without profile-specific tests, or runtime sign-off lacks evidence.

## Validation

Run the narrow gate for the touched subsystem and report every unexecuted release lane. Artifact-producing work follows the artifact/deployment gate.

## Runtime Proof

Static/build evidence is not runtime proof. State `runtime sign-off not performed` unless exact packaged evidence exists.
