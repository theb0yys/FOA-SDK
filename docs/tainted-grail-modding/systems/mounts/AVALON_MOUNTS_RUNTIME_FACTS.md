# Canonical Source Facts — Avalon Mounts Runtime Structure

These are source-architecture facts from the pinned upstream project. They are not supported-runtime capability claims.

## Fact 1: plug-in lifecycle and cleanup

```yaml
fact_id: tg.fact.mounts.avalon-mounts-plugin-lifecycle
status: source-fact
profile: { runtime: Mono, target_framework: netstandard2.1, loader_family: BepInEx v5 Mono, exact_loader_version: unknown }
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/AvalonMounts.csproj]
source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 2f683eba7f696b9e78ace7797a5a81a9e7f36128]
```

`Plugin.Awake` constructs configuration and a diagnostics/runtime guard, creates two controller objects, attempts the wolf-input fallback, and runs a one-time diagnostics/upgrade registration pass. `Plugin.OnDestroy` disposes the two controller objects and calls `Harmony.UnpatchSelf`.

The source establishes an intended cleanup path. It does not prove that every controller-owned object, callback, marker state, static field, coroutine, file handle, or downstream subscription is released on every shutdown path.

## Fact 2: per-frame controller lane

```yaml
fact_id: tg.fact.mounts.avalon-mounts-frame-controller-lane
status: source-fact
source_paths:
  - mods/avalon-mounts/src/Plugin.cs
  - mods/avalon-mounts/src/WolfMount/WolfMountSeatBridgeController.cs
  - mods/avalon-mounts/src/WolfMount/AvalonCompanionsDialogueBridge.cs
source_blobs:
  - 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
  - c226dead88329eb55f3444f36c6fceecc2ed7e66
  - cc1b0165303e3bf1de09d5ac249820e70566afe9
```

`Plugin.Update` calls both controllers with the current `WolfMountSeatBridgeAvailable` guard value.

### Wolf seat controller

Every enabled frame performs a contract observation call, attachment-validity check, and shortcut `IsDown` poll. World enumeration and nearest-candidate ordering occur on attach attempts rather than every frame. Gate loss detaches an active seat. Disposal detaches and disposes the native mount contract.

The controller is not treated as safe merely because the gate is strict. Its downstream contract creates and mutates Unity/model objects, private fields, movement components, mount ownership, and native actions. Batch 003 therefore dispositions it as a research blocker.

### Avalon Companions dialogue bridge

Every frame checks local registration state and an unscaled-time retry deadline. While unregistered, it scans loaded assemblies and attempts exact reflective API resolution no more often than every two seconds. Gate loss and disposal attempt owner-scoped unregistration.

Batch 003 splits the external registration and cleanup methods into two source-only candidate adapter records. No exact Avalon Companions assembly/version or live behavior is established.

## Fact 3: compatibility detection is filename-token scanning

```yaml
fact_id: tg.fact.mounts.filename-token-compatibility-detection
status: source-fact
source_path: mods/avalon-mounts/src/Diagnostics/MountModDetector.cs
source_blob: ed46ff75641ba8247d4ef13e5adcacdaf46606cc
```

The compatibility detector recursively enumerates `*.dll` under `BepInEx.Paths.PluginPath` and performs case-insensitive filename substring checks for three families:

- Better Mounts-like names;
- True Third Person or movement-camera-like names;
- Smooth Horse Animation-like names.

It does not inspect plug-in GUIDs, versions, Harmony ownership, active patch targets, or metadata. Missing directories and enumeration exceptions return `false`. The result is therefore a warning heuristic with both false-positive and false-negative risk, not a compatibility guarantee.

## Linked records

- seven runtime target candidates: `../../hooks/records/MOUNTS_RUNTIME_TARGETS.md`;
- Avalon Companions register/unregister adapter candidates: `../../hooks/records/MOUNTS_AVALON_COMPANIONS_DIALOGUE_BRIDGE.md`;
- wolf seat frame-controller blocker: `../../hooks/blockers/WOLF_MOUNT_SEAT_CONTROLLER.md`;
- residual diagnostics/privacy blocker: `../../hooks/blockers/AVALON_MOUNTS_DIAGNOSTICS_PRIVACY.md`;
- original aggregation blocker history: `../../hooks/blockers/AVALON_MOUNTS_PLUGIN.md`.

## Prohibitions

These facts do not authorize game-folder scanning, runtime patching, per-frame execution, diagnostics capture, filesystem output, mount conversion, dialogue registration, deployment, or evidence promotion.
