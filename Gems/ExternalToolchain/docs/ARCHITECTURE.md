# External Toolchain Architecture

## Purpose

The host standardizes how independently packaged O3DE Tool Gems declare external applications and commands. O3DE Gems remain the packaging and activation mechanism; this Gem does not introduce a second module loader.

## Foundation lifecycle

```text
Editor loads enabled Gems
    ↓
Provider Editor system components activate
    ↓
Providers register validated descriptors
    ↓
Action Manager registration completes
    ↓
Host finalizes provider registration
    ↓
Diagnostics surface becomes stable
```

The foundation registry is transient. It writes no provider document and performs no process or asset operation.

## Contract invariants

1. Provider IDs are lowercase namespaced identifiers.
2. Provider versions use semantic versioning.
3. Provider minimum API versions must be compatible with host API `1.0.0`.
4. Platforms and command IDs are unique within a provider.
5. Interactive and batch commands require matching declared capabilities.
6. Provider enumeration is deterministic by provider ID.
7. Registration fails after finalization.
8. Provider Gems depend on the public API and require the host service.
9. This Gem exposes no runtime aliases.
10. No provider may infer execution permission from successful registration.

## Ordered implementation slices

1. **Host foundation** — provider contracts, registry, finalization, diagnostics and tests.
2. **Discovery and settings** — bounded probes and layered Settings Registry configuration.
3. **Process supervision** — host-owned `AzFramework::ProcessWatcher` integration.
4. **Command UI** — descriptor-generated Action Manager actions and command forms.
5. **Artifact handoff** — source-artifact manifests and Asset Processor bridge.
6. **Reference providers** — Blender interactive provider and a headless heightmap provider.
7. **Research gates** — Unity interchange, structured IPC, hot loading and third-party trust.

Each slice requires design review and must preserve the preceding boundaries.
