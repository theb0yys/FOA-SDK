# Candidate Hook Family — Crafting Cost and Recipe UI

## Ingredient count getter

```yaml
hook_id: tg.hook.economy.ingredient-count-getter
title: Crafting ingredient count getter
status: candidate
hook_class: method-patch
domain: items-crafting-and-economy
purpose: multiply the native Ingredient.Count getter result when the live crafting-cost lane is enabled
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Crafting.Recipes, type: Ingredient, member: get_Count, signature: expected int getter; exact metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: owning Tainted Economy Plugin.OnDestroy calls Harmony.UnpatchSelf }
context: { lifecycle_point: after ingredient-count read, thread: unknown, frequency: potentially hot-path in crafting UI and execution, inputs: Ingredient instance, native int result, configured multiplier, outputs: count clamped to at least one and at most int.MaxValue, side_effects: changes crafting cost and optional log }
safety: { nullability: no local instance dereference except diagnostic summary, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: recipe, crafting-cost, and getter patches, load_order: result mutation is order-sensitive, version_stability: low because registrar resolves by name without parameter metadata }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/CraftingCostLive.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs, mods/TaintedEconomy/src/Plugin.cs, mods/TaintedEconomy/src/AvalonEconomy.csproj], source_blobs: [84543d2b23705e6133a904e9a9ebf0f4a47f5fee, f6997889aa3468e8692602423b8ce8481648ad26, 73bc0b7101e02363c27895e04453971a02f96b4b, 7fa22d90de1c79320ccf03a22b567824c903ea67], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: live crafting mutation, save claims, promotion }
limitations: [Exact getter identity and all call sites are unresolved, downstream inventory and crafting behavior are untested]
```

## Recipe grid refresh

```yaml
hook_id: tg.hook.economy.recipe-grid-refresh
title: Recipe grid refresh observer
status: candidate
hook_class: method-patch
domain: items-crafting-and-economy
purpose: observe recipe-grid refresh and emit bounded recipe-cost diagnostics or dry-run candidates
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Crafting.HandCrafting.RecipeView, type: RecipeGridUI, member: Refresh, signature: unknown; registrar resolves by name only }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf; diagnostic output cleanup separate }
context: { lifecycle_point: after recipe-grid refresh, thread: Unity main thread expected but unverified, frequency: UI event and potentially repeated, inputs: RecipeGridUI instance and reflected recipe members, outputs: up to 100 recipe observations per callback, side_effects: reflection and diagnostic output }
safety: { nullability: enumerable and selected-recipe paths are guarded, failure_mode: diagnostics-only with caught exceptions, save_risk: none, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: crafting UI replacement and Refresh patches, load_order: unspecified, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RecipeCostDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [d327e99b81b4f4e5295ea062d958d5b2727f8784, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and synthetic diagnostics review, prohibited: runtime capture, UI execution, promotion }
limitations: [Exact overload is unresolved, reflection member names and output privacy require separate verification]
```

## Recipe slot click

```yaml
hook_id: tg.hook.economy.recipe-grid-click-slot
title: Recipe grid slot click observer
status: candidate
hook_class: method-patch
domain: items-crafting-and-economy
purpose: observe a recipe slot click and record the selected recipe and craftable state
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Crafting.HandCrafting.RecipeView, type: RecipeGridUI, member: ClickSlot, signature: unknown; callback reads the first Harmony argument }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf; diagnostic output cleanup separate }
context: { lifecycle_point: after slot click, thread: Unity main thread expected but unverified, frequency: UI event, inputs: Harmony argument array and reflected Recipe and IsCraftable members, outputs: one recipe diagnostic row, side_effects: reflection and diagnostic output }
safety: { nullability: empty argument array and null slot/recipe are guarded, failure_mode: diagnostics-only with caught exceptions, save_risk: none, performance_risk: low, story_risk: none }
compatibility: { known_conflicts: crafting UI and ClickSlot patches, load_order: unspecified, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RecipeCostDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [d327e99b81b4f4e5295ea062d958d5b2727f8784, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and synthetic diagnostics review, prohibited: runtime capture, UI execution, promotion }
limitations: [Exact argument and overload identity are unresolved, reflected slot structure may change by profile]
```

No record in this family is promoted beyond `candidate`.