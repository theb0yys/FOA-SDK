# Runtime-adapter plug-ins

This category contains optional target-specific adapters that consume reviewed work orders and return runtime observations as new evidence.

## Implemented packages

- [`Mono`](Mono/README.md) — project-owned BepInEx 5 source package, deterministic non-executable build plan, exact-profile execution-eligibility gate, and typed external runtime-result evidence for the pinned Mono route.
- [`IL2CPP`](IL2CPP/README.md) — independent project-owned BepInEx 6 source package with exact generated-interop manifests, deterministic non-executable build planning, a three-evidence-class external review gate, and typed IL2CPP result evidence.

Mono and IL2CPP are separate package families. They must not share target-specific binaries, dependencies, compatibility claims, build results, generated interop, or runtime evidence merely because they implement similar logical operations.

A package manifest declares capabilities and compatibility only. It does not grant process-launch, deployment, injection, save-mutation, signing, publication, or gameplay-mutation authority. Those actions require separate reviewed work orders, exact-profile validation, explicit permission, execution results, and rollback evidence.

Every package directory requires a schema-valid `plugin.json` at its root.
