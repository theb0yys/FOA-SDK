# Candidate Hook Family — Avalon Mounts Runtime Targets

All seven records are source-only candidates from the pinned Mono project. The source proves target spelling and callback intent, not exact-profile target availability or supported behavior.

## Player desired movement fallback

```yaml
hook_id: tg.hook.mounts.vmount-get-player-desired-movement
title: VMount player desired movement fallback
status: candidate
hook_class: method-patch
domain: mounts-movement-input
purpose: replace a zero native mount movement result with clamped PlayerInput.MoveInput for a controller-owned mount view
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Fights.Mounts
  type: VMount
  member: GetPlayerDesiredMovement
  signature: expected Vector2 GetPlayerDesiredMovement(); source lookup uses an empty parameter list; return metadata unverified
patch: { style: postfix, ordering: unspecified, cleanup: owning Plugin.OnDestroy calls Harmony.UnpatchSelf; static callback references are not explicitly cleared }
context:
  lifecycle_point: after native desired-movement calculation
  thread: Unity main thread expected but unverified
  frequency: hot-path or frame-adjacent; exact call frequency unknown
  inputs: VMount instance, native Vector2 result, World.Any<PlayerInput>().MoveInput, controller ownership check
  outputs: original result or clamped fallback vector
  side_effects: mutates movement result and logs first successful fallback
safety: { nullability: controller and PlayerInput are guarded; zero vectors return unchanged, failure_mode: fail-open, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: mount input, player-input, camera, and movement patches on the same method, load_order: unspecified, version_stability: low }
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/WolfMount/WolfMountInputFallbackPatch.cs, mods/avalon-mounts/src/AvalonMounts.csproj]
  source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 212fc43b9c857ad25fa2d41fecae8aca63c0c821, 2f683eba7f696b9e78ace7797a5a81a9e7f36128]
  evidence_ids: []
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline exact-profile verification, prohibited: deployment, runtime input mutation, evidence promotion }
limitations:
  - Exact return type and native input ownership are not independently verified.
  - Static controller/logger references are initialized but not explicitly nulled on unload.
```

## Hero movement-system initialization probe

```yaml
hook_id: tg.hook.mounts.hero-movement-system-init-probe
title: Hero movement-system initialization probe
status: candidate
hook_class: method-patch
domain: mounts-lifecycle-diagnostics
purpose: observe HeroMovementSystem.Init and record whether the concrete movement system is MountedMovement
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Heroes.MovementSystems
  type: HeroMovementSystem
  member: Init
  signature: expected Init(VHeroController); return type unverified
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf; diagnostic file retention is separate }
context:
  lifecycle_point: after hero movement-system initialization
  thread: Unity main thread expected but unverified
  frequency: lifecycle event
  inputs: HeroMovementSystem instance, VHeroController
  outputs: appended CSV row and one-time log when MountedMovement is observed
  side_effects: creates diagnostics directory and appends native_horse_transition_probe.csv
safety: { nullability: controller and runtime type names are guarded, failure_mode: diagnostics-only, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: movement-system initialization patches and filesystem restrictions, load_order: unspecified, version_stability: low }
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/Diagnostics/NativeHorseTransitionProbe.cs, mods/avalon-mounts/src/AvalonMounts.csproj]
  source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 7459480772a1fb7660af722dddd91ee1497d2ea5, 2f683eba7f696b9e78ace7797a5a81a9e7f36128]
  evidence_ids: []
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and privacy review, prohibited: runtime capture, filesystem writes, publication of unreviewed output, evidence promotion }
limitations:
  - Output redaction, retention, row bounds, and deletion behavior are unresolved.
  - Exact Init overload and callback frequency require profile verification.
```

## Running velocity getter

```yaml
hook_id: tg.hook.mounts.vmount-running-velocity-getter
title: VMount running velocity getter
status: candidate
hook_class: method-patch
domain: mounts-movement
purpose: multiply the native VMount.RunningVelocity getter result by the configured running multiplier
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Fights.Mounts, type: VMount, member: get_RunningVelocity, signature: expected float getter; metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after velocity getter, thread: unknown, frequency: hot-path, inputs: native float result and resolved config multiplier, outputs: unchanged or multiplied float result, side_effects: runtime movement mutation and one-time log }
safety: { nullability: config resolver accepts nullable config, failure_mode: fail-open when multiplier is approximately one, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: other velocity getter patches and movement overhauls, load_order: result multiplication is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/Diagnostics/NativeHorseSpeedUpgradePatches.cs], source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 028056ab83dd2e5b47b9a14254281dc58a81931c], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: runtime movement mutation and promotion }
limitations: [Exact property type and getter frequency are unverified, multiplier composition with other postfixes is unresolved]
```

## Turning velocity getter

```yaml
hook_id: tg.hook.mounts.vmount-turning-velocity-getter
title: VMount turning velocity getter
status: candidate
hook_class: method-patch
domain: mounts-movement
purpose: multiply the native VMount.TurningVelocity getter result by the configured turning multiplier
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Fights.Mounts, type: VMount, member: get_TurningVelocity, signature: expected float getter; metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after velocity getter, thread: unknown, frequency: hot-path, inputs: native float result and resolved config multiplier, outputs: unchanged or multiplied float result, side_effects: runtime movement mutation and one-time log }
safety: { nullability: config resolver accepts nullable config, failure_mode: fail-open when multiplier is approximately one, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: other turning getter patches and movement overhauls, load_order: result multiplication is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/Diagnostics/NativeHorseSpeedUpgradePatches.cs], source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 028056ab83dd2e5b47b9a14254281dc58a81931c], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: runtime movement mutation and promotion }
limitations: [Exact property type and getter frequency are unverified, multiplier composition with other postfixes is unresolved]
```

## Recall teleport decision

```yaml
hook_id: tg.hook.mounts.mount-hero-seeker-should-teleport-horse
title: Mount recall teleport decision
status: candidate
hook_class: method-patch
domain: mounts-travel-and-recall
purpose: change a false ShouldTeleportHorse result to true while the recall-teleport upgrade is enabled
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Fights.Mounts, type: MountHeroSeeker, member: ShouldTeleportHorse, signature: expected bool ShouldTeleportHorse(); metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after native teleport decision, thread: unknown, frequency: event or polling unknown, inputs: native bool result and config gate, outputs: native result or true, side_effects: changes teleport eligibility }
safety: { nullability: config is guarded, failure_mode: fail-open when disabled or native result is already true, save_risk: unknown, performance_risk: low, story_risk: bounded }
compatibility: { known_conflicts: mount recall, world streaming, teleport, follower, and travel mods, load_order: final bool result is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/NativeHorse/NativeHorseRecallUpgradePatches.cs], source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 9d602eb2f3faf25ff363e7d76d7ae319d538e9c6], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: teleport mutation, game launch, promotion }
limitations: [World and scene safety are unverified, native reasons for a false result are not preserved by the enabled callback]
```

## Target forward speed

```yaml
hook_id: tg.hook.mounts.vmount-get-target-forward-speed
title: VMount target forward speed
status: candidate
hook_class: method-patch
domain: mounts-movement
purpose: multiply the native target-forward-speed result by a clamped configured multiplier
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Fights.Mounts, type: VMount, member: GetTargetForwardSpeed, signature: expected float GetTargetForwardSpeed(); metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after target speed calculation, thread: unknown, frequency: hot-path, inputs: native float result and config multiplier, outputs: unchanged or multiplied result, side_effects: runtime movement mutation }
safety: { nullability: config is guarded, failure_mode: fail-open when disabled or multiplier is approximately one, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: mount locomotion and speed patches, load_order: multiplication is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/NativeHorse/NativeHorseRecallUpgradePatches.cs], source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 9d602eb2f3faf25ff363e7d76d7ae319d538e9c6], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: runtime speed mutation and promotion }
limitations: [Exact call frequency and locomotion-pipeline ordering are unverified]
```

## Mounted location marker

```yaml
hook_id: tg.hook.mounts.mount-element-mount-location-marker
title: Mounted location marker enablement
status: candidate
hook_class: method-patch
domain: mounts-map-and-ui
purpose: enable an existing LocationMarker element after MountElement.Mount(Hero)
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Fights.Mounts, type: MountElement, member: Mount, signature: expected Mount(Hero); return type unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf; marker state restoration is not implemented in the callback }
context: { lifecycle_point: after mounting, thread: Unity main thread expected but unverified, frequency: event, inputs: MountElement instance and config gate, outputs: none, side_effects: resolves ParentModel LocationMarker and calls SetEnabled(true) }
safety: { nullability: missing marker returns unchanged, failure_mode: fail-open, save_risk: unknown, performance_risk: low, story_risk: bounded }
compatibility: { known_conflicts: map-marker, mount-state, UI, and visibility mods, load_order: marker final state may be order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/avalon-mounts/src/Plugin.cs, mods/avalon-mounts/src/NativeHorse/NativeHorseMountedMarkerUpgradePatches.cs], source_blobs: [896e0f4e63c57b1800eb3ac09468cefe0ca8509d, 005d9b4f7557588a9c951768a081db4c6596f8cd], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: marker mutation, save claims, promotion }
limitations: [Prior marker state is not captured or restored, persistence and scene-transition behavior are unverified]
```

## Family boundary

The plugin-level configuration and diagnostics guard may prevent some registration attempts, but guard source is not runtime proof. Every record remains independently gated by exact-profile target resolution, behavior, unload, and collision evidence.