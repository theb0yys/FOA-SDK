# Research Blocker — Avalon Mounts Plugin Aggregation

```yaml
blocker_id: tg.blocker.hooks.avalon-mounts-plugin-aggregation
status: research-blocker
domain: mounts-movement-input-and-diagnostics
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_path: mods/avalon-mounts/src/Plugin.cs
source_blob: 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
licence: NOASSERTION at repository root
primary_disposition: research
```

## Decision

The selected file is rejected as a single hook record and retained as a research blocker. It aggregates too many independently owned lifecycle, reflection, patch, diagnostics, output, and mutation surfaces to describe honestly under one hook ID.

No target extracted below is promoted. Each target must be split into a complete record and joined with the corresponding patch implementation, configuration gate, diagnostics-only guard, exact runtime profile, and cleanup evidence.

## Source surfaces requiring decomposition

### Lifecycle and hot-path surfaces

- `Plugin.Awake` creates config and guard state, creates controllers, applies an input fallback, and runs diagnostics once.
- `Plugin.Update` calls two controller `Tick` methods every frame.
- `Plugin.OnDestroy` disposes both controllers and calls `Harmony.UnpatchSelf`.
- diagnostics output resolves beneath `BepInEx.Paths.ConfigPath`, with a relative fallback.

These are separate lifecycle, frame-frequency, cleanup, and privacy records. They cannot be folded into one mount hook.

### Dynamic Harmony targets

| Intended target | Lookup shape | Intended patch style | Immediate unresolved work |
|---|---|---|---|
| `Awaken.TG.Main.Fights.Mounts.VMount.GetPlayerDesiredMovement()` | `AccessTools.Method` with empty parameter list | postfix | return type, postfix signature, input ownership, per-frame frequency, collision testing |
| `Awaken.TG.Main.Heroes.MovementSystems.HeroMovementSystem.Init(VHeroController)` | exact one-parameter lookup | postfix | exact return type, lifecycle timing, diagnostics-only behavior |
| `VMount.RunningVelocity` getter | `AccessTools.PropertyGetter` | postfix | property type, multiplier semantics, hot-path cost |
| `VMount.TurningVelocity` getter | `AccessTools.PropertyGetter` | postfix | property type, multiplier semantics, hot-path cost |
| `Awaken.TG.Main.Fights.Mounts.MountHeroSeeker.ShouldTeleportHorse()` | empty-parameter method lookup | postfix | return type, teleport ownership, world/scene safety |
| `VMount.GetTargetForwardSpeed()` | empty-parameter method lookup | postfix | return type, movement pipeline ordering, hot-path cost |
| `Awaken.TG.Main.Fights.Mounts.MountElement.Mount(Hero)` | exact one-parameter lookup | postfix | return type, mount lifecycle, marker ownership and cleanup |

### Downstream source dependencies

The selected file calls patch methods and controllers defined elsewhere, including:

- `WolfMountInputFallbackPatch`;
- `NativeHorseTransitionProbe`;
- `NativeHorseSpeedUpgradePatches`;
- `NativeHorseRecallUpgradePatches`;
- `NativeHorseMountedMarkerUpgradePatches`;
- `WolfMountSeatBridgeController`;
- `AvalonCompanionsDialogueBridge`;
- `DiagnosticsOnlyGuard`;
- multiple probes and report writers.

Risk cannot be assigned from the registrar alone. The downstream code determines result mutation, controller disposal, data capture, object ownership, logging volume, and possible scene/save effects.

## Blocking evidence gaps

1. exact game version and branch for every target;
2. exact Mono loader and Harmony versions;
3. `TG.Main` and Unity assembly fingerprints;
4. exact method return types and patch callback signatures;
5. proof that every configuration and guard state fails closed as documented;
6. proof that `Update` work is bounded and allocation-safe;
7. proof that diagnostics output is redacted, bounded, and removable;
8. proof that every created controller, marker, callback, and patch is disposed or unpatched;
9. combined-mod tests for other mount, movement, input, teleport, companion, and camera modifications;
10. behavior evidence separating diagnostics-only observation from runtime mutation.

## Required resolution

This blocker closes only when the source is decomposed into at least:

- one plugin lifecycle record;
- one per-frame controller/update record;
- seven exact target records from the table above;
- one diagnostics-output/privacy record;
- downstream implementation reviews for each patch and controller;
- one compatibility record covering mount-adjacent mods and patch collisions.

Until then, the source remains useful research input but is not a catalogue hook, a supported example, or evidence of current mount capability.