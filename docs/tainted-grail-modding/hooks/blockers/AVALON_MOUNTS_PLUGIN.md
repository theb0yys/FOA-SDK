# Resolved Decomposition Blocker — Avalon Mounts Plugin Aggregation

```yaml
blocker_id: tg.blocker.hooks.avalon-mounts-plugin-aggregation
status: decomposed
resolution_batch: tg.semantic-hook-batch-002
domain: mounts-movement-input-and-diagnostics
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_path: mods/avalon-mounts/src/Plugin.cs
source_blob: 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
licence: NOASSERTION at repository root
```

## Historical decision

Batch 001 rejected `Plugin.cs` as a single hook record because it aggregates lifecycle, per-frame controller work, diagnostics, compatibility scanning, configuration guards, and seven independent Harmony targets. That decision remains correct: there is no canonical “Avalon Mounts plugin hook.”

## Batch 002 resolution

The aggregate source is now decomposed into:

- seven exact source-only target candidates in [MOUNTS_RUNTIME_TARGETS.md](../records/MOUNTS_RUNTIME_TARGETS.md);
- lifecycle, cleanup, per-frame, and compatibility source facts in [AVALON_MOUNTS_RUNTIME_FACTS.md](../../systems/mounts/AVALON_MOUNTS_RUNTIME_FACTS.md);
- a residual [diagnostics/output/privacy blocker](AVALON_MOUNTS_DIAGNOSTICS_PRIVACY.md).

Exact target records now exist for:

1. `VMount.GetPlayerDesiredMovement()`;
2. `HeroMovementSystem.Init(VHeroController)`;
3. `VMount.RunningVelocity` getter;
4. `VMount.TurningVelocity` getter;
5. `MountHeroSeeker.ShouldTeleportHorse()`;
6. `VMount.GetTargetForwardSpeed()`;
7. `MountElement.Mount(Hero)`.

## Meaning of resolved

`decomposed` means the selected aggregate file has an exact semantic disposition. It does **not** mean any target is signature-verified, load-verified, behavior-verified, compatibility-reviewed, executable, supported, or safe for deployment.

Every target remains `candidate`. The unresolved diagnostics/privacy work remains a live blocker. The two per-frame controller implementations remain downstream Batch 003 research inputs.

## Original blocking evidence that still applies

- exact game, branch, loader, Harmony, and assembly fingerprints;
- exact method signatures and generic/overload resolution;
- runtime guard behavior;
- bounded frame-loop cost;
- deterministic controller, marker, callback, static-reference, and file cleanup;
- combined mount, movement, input, teleport, map, companion, camera, and UI compatibility;
- behavior evidence separating diagnostics from mutation.

No runtime or evidence authority is granted by this resolution.