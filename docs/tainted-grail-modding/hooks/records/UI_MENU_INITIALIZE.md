# Candidate Hook Family — Menu Initialization and Options Button Lookup

This source contains two Harmony method targets and one reflected field pattern. The Mono source is recorded separately from the repository's IL2CPP implementation; no target or behavior is inherited across runtimes.

## Main menu initialization

```yaml
hook_id: tg.hook.ui.main-menu-on-initialize
title: Main menu initialization observer
status: candidate
hook_class: ui
domain: ui-menus-and-input
purpose: run a postfix after VMenuUI initialization so a mod-owned button can be added beside the native options button
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.UI.Menu
  type: VMenuUI
  member: OnInitialize
  signature: unknown; source resolves by method name without a parameter list
patch:
  style: postfix
  ordering: unspecified
  cleanup: patch cleanup and cloned-object cleanup are not established by this source file
context:
  lifecycle_point: after main menu view initialization
  thread: Unity main thread expected but unverified
  frequency: event; may occur more than once per view lifecycle
  inputs: VMenuUI instance
  outputs: attempts to clone and configure a sibling button
  side_effects: creates a Unity GameObject, changes sibling order, clears click events, and installs a mod-owned callback
safety:
  nullability: target and postfix resolution are guarded; options button, parent, and cloned ButtonConfig are checked
  failure_mode: fail-open; missing targets or UI elements log and skip; AddManagerButton catches exceptions
  save_risk: none
  performance_risk: low
  story_risk: none
compatibility:
  known_conflicts: menu replacement mods, options-button mutations, sibling-order assumptions, and patches on OnInitialize
  load_order: unspecified
  version_stability: low until the method overload and native view shape are verified
validation:
  static: source target and guarded postfix behavior extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline target verification, bounded UI diagnostics planning
  prohibited: runtime UI mutation, asset copying, controller/input claims, deployment, evidence promotion
limitations:
  - The exact OnInitialize signature is unresolved because AccessTools.Method is called without parameter types.
  - Destruction of the cloned button during unload or view teardown is not proven here.
  - Localization, controller navigation, focus order, accessibility, and repeated initialization require separate review.
```

## Title-screen initialization

```yaml
hook_id: tg.hook.ui.title-screen-on-initialize
title: Title-screen initialization observer
status: candidate
hook_class: ui
domain: ui-menus-and-input
purpose: run a postfix after VTitleScreenUI initialization so a mod-owned button can be added beside the native options button
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.UI.TitleScreen
  type: VTitleScreenUI
  member: OnInitialize
  signature: unknown; source resolves by method name without a parameter list
patch:
  style: postfix
  ordering: unspecified
  cleanup: patch cleanup and cloned-object cleanup are not established by this source file
context:
  lifecycle_point: after title-screen view initialization
  thread: Unity main thread expected but unverified
  frequency: event; may occur more than once per view lifecycle
  inputs: VTitleScreenUI instance
  outputs: attempts to clone and configure a sibling button
  side_effects: creates a Unity GameObject, changes sibling order, clears click events, and installs a mod-owned callback
safety:
  nullability: target and postfix resolution are guarded; options button, parent, and cloned ButtonConfig are checked
  failure_mode: fail-open; missing targets or UI elements log and skip; AddManagerButton catches exceptions
  save_risk: none
  performance_risk: low
  story_risk: none
compatibility:
  known_conflicts: title-screen replacement mods, options-button mutations, sibling-order assumptions, and patches on OnInitialize
  load_order: unspecified
  version_stability: low until the method overload and native view shape are verified
validation:
  static: source target and guarded postfix behavior extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline target verification, bounded UI diagnostics planning
  prohibited: runtime UI mutation, asset copying, title-screen flow mutation, deployment, evidence promotion
limitations:
  - The exact OnInitialize signature is unresolved because AccessTools.Method is called without parameter types.
  - Destruction of the cloned button during unload or view teardown is not proven here.
  - New-game flow, localization, controller navigation, focus order, and accessibility require separate review.
```

## Options-button field lookup

```yaml
hook_id: tg.hook.ui.options-button-field
 title: Native options button field lookup
status: candidate
hook_class: reflection
domain: ui-menus-and-input
purpose: resolve the native options ButtonConfig field from VMenuUI or VTitleScreenUI as the template and placement anchor for a mod-owned button
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony AccessTools; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.UI.Menu or Awaken.TG.Main.UI.TitleScreen
  type: VMenuUI or VTitleScreenUI
  member: options
  signature: reflected field expected to contain Awaken.TG.Main.Utility.UI.ButtonConfig
patch:
  style: reflect
  ordering: after the corresponding view OnInitialize postfix
  cleanup: not-applicable to field lookup; cloned-object cleanup remains unresolved
context:
  lifecycle_point: during the mod-owned initialization postfix
  thread: Unity main thread expected but unverified
  frequency: event
  inputs: initialized view instance and concrete view type
  outputs: ButtonConfig value or null
  side_effects: reflection lookup itself is read-only; the caller may clone the returned GameObject
safety:
  nullability: missing field or incompatible value returns null and causes the caller to skip
  failure_mode: fail-open
  save_risk: none
  performance_risk: low
  story_risk: none
compatibility:
  known_conflicts: field rename/type change, stripped metadata, runtime-specific layout, and UI replacement mods
  load_order: not-applicable to lookup; caller patch ordering remains unspecified
  version_stability: low because the field is private-name reflection
validation:
  static: field name and expected cast extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline metadata verification, bounded read-only diagnostics planning
  prohibited: runtime reflection execution, UI cloning, cross-runtime inference, deployment, evidence promotion
limitations:
  - The source does not verify field visibility, declaring-type ownership, or metadata preservation.
  - Mono results must not be projected onto the separate IL2CPP source path.
```

## Evidence

- source repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- source commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- source path: `mods/foa-mod-manager/src/Patches/MenuButtonPatch.cs`
- source blob: `b289d4a510bf0cb1a0f146710f3892cd7b33ffc7`
- runtime family: Mono source tree only
- evidence IDs: none registered; source-only candidates

## Extracted source facts

The source attempts to patch `OnInitialize` on `VMenuUI` and `VTitleScreenUI` by method name. Each postfix asks for a field named `options`, expects a `ButtonConfig`, clones its GameObject under the same parent, prevents duplicate creation by object name, clears inherited click handlers, and initializes a mod-owned callback. Missing targets and structural mismatches log and skip.

The structural guards are worth retaining as a pattern. They do not resolve the missing exact signatures, unload cleanup, controller navigation, localization, or runtime-specific UI identity.