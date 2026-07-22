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
source_path: mods/avalon-mounts/src/Plugin.cs
source_blob: 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
```

`Plugin.Update` calls:

- `WolfMountSeatBridgeController.Tick(...)`;
- `AvalonCompanionsDialogueBridge.Tick(...)`.

Both calls are guarded by a runtime-availability value but still occupy a frame-frequency lane. Their internal allocations, polling, object lookup, input ownership, subscription state, and scene-transition behavior are outside the selected source and remain Batch 003 research inputs.

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
- residual diagnostics/privacy blocker: `../../hooks/blockers/AVALON_MOUNTS_DIAGNOSTICS_PRIVACY.md`;
- original aggregation blocker history: `../../hooks/blockers/AVALON_MOUNTS_PLUGIN.md`.

## Prohibitions

These facts do not authorize game-folder scanning, runtime patching, per-frame execution, diagnostics capture, filesystem output, deployment, or evidence promotion.