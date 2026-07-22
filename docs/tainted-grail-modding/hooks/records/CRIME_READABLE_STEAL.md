# Candidate Hook — Readable Steal Completion

```yaml
hook_id: tg.hook.crime.readable-steal-completion
title: Readable steal completion observer
status: candidate
hook_class: method-patch
domain: interaction-and-crime
purpose: observe a completed readable-object steal action and forward a mod-owned threat event only when Harmony reports that the original ran
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.UI.Popup
  type: VReadablePopupUI
  member: OnSteal
  signature: unknown
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the original OnSteal invocation
  thread: unknown
  frequency: event
  inputs: Harmony __runOriginal state
  outputs: call to ThreatEventHooks.OnReadableSteal when __runOriginal is true
  side_effects: downstream threat effects are outside this source file and remain unreviewed
safety:
  nullability: no patched instance or native value is dereferenced in this source
  failure_mode: fail-open with respect to the original; callback exception containment is unknown
  save_risk: unknown
  performance_risk: low structural frequency; runtime cost unverified
  story_risk: bounded but unverified
compatibility:
  known_conflicts: patches that skip or replace VReadablePopupUI.OnSteal can change __runOriginal behavior
  load_order: unspecified
  version_stability: unknown until exact-profile target resolution
validation:
  static: source target, postfix style, and __runOriginal guard extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline review, exact-profile signature verification, bounded diagnostics planning
  prohibited: runtime patch execution, crime-system mutation, threat mutation, behavior claims, evidence promotion
limitations:
  - The native OnSteal overload and return shape are not established by this source file.
  - Harmony __runOriginal semantics must be verified for the exact patch combination.
  - ThreatEventHooks.OnReadableSteal must be reviewed before save, story, and progression risks can be classified.
```

## Evidence

- source repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- source commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- source path: `mods/wyrd-hunt/src/Patches/ReadableStealPatch.cs`
- source blob: `4d58a71ebab00c050ce3907e25b9dac4085212d2`
- registration owner: `mods/wyrd-hunt/src/Plugin.cs`
- project source: `mods/wyrd-hunt/src/WyrdHunt.csproj`
- evidence IDs: none registered; source-only candidate

## Extracted source facts

The source declares a postfix for `VReadablePopupUI.OnSteal`. It returns without dispatch when Harmony reports that the original did not run; otherwise it calls `ThreatEventHooks.OnReadableSteal`. The owning plug-in applies assembly-wide patches during `Awake` and unpatches itself during `OnDestroy`.

The guard is a useful compatibility signal, but it is not evidence that every prefix/transpiler combination yields the intended crime event exactly once.