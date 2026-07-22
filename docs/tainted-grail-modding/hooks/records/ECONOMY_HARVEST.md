# Candidate Hook Family — Harvest Yield

The three caller registrations are prefixes. Each may invoke a live quantity-adjustment helper before vanilla item creation and may also emit diagnostics when the lane is enabled.

## Location pickup action start

```yaml
hook_id: tg.hook.economy.pick-item-action-on-start
title: Location pickup action start
status: candidate
hook_class: method-patch
domain: items-harvest-and-economy
purpose: inspect and optionally adjust _itemSpawningData before PickItemAction.OnStart
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Actions, type: PickItemAction, member: OnStart, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before location pickup starts, thread: Unity main thread expected but unverified, frequency: interaction event, inputs: action instance and reflected _itemSpawningData, outputs: diagnostics and possible runtime quantity mutation through HarvestYieldLive, side_effects: may alter item quantity before vanilla pickup }
safety: { nullability: missing required field returns without mutation, failure_mode: caught exception and skip, save_risk: unknown, performance_risk: low, story_risk: bounded }
compatibility: { known_conflicts: pickup, item-spawn, quantity, and OnStart patches, load_order: prefix mutation order is significant, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/HarvestYieldDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [0c1d9b7e9c7b71b1be130ac886bbb7dcc21201e6, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: live pickup mutation, inventory/save claims, promotion }
limitations: [Exact overload and spawning-data type are unresolved, downstream helper semantics are Batch 003 work]
```

## Pickable interaction start

```yaml
hook_id: tg.hook.economy.pickable-start-interaction
title: Pickable interaction start
status: candidate
hook_class: method-patch
domain: items-harvest-and-economy
purpose: inspect and optionally adjust _itemData before Pickable.StartInteraction
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Pickables, type: Pickable, member: StartInteraction, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before scene-pickable interaction, thread: Unity main thread expected but unverified, frequency: interaction event, inputs: Pickable instance and reflected _itemData, outputs: diagnostics and possible quantity mutation, side_effects: may alter gathered item quantity }
safety: { nullability: missing required field returns without mutation, failure_mode: caught exception and skip, save_risk: unknown, performance_risk: low, story_risk: bounded }
compatibility: { known_conflicts: pickable interaction, harvest, and item-data patches, load_order: prefix mutation order is significant, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/HarvestYieldDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [0c1d9b7e9c7b71b1be130ac886bbb7dcc21201e6, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: live harvest mutation, inventory/save claims, promotion }
limitations: [Exact overload and item-data ownership are unresolved, repeated interaction behavior is untested]
```

## Regrowable interaction start

```yaml
hook_id: tg.hook.economy.regrowable-start-interaction
title: Regrowable interaction start
status: candidate
hook_class: method-patch
domain: items-harvest-and-economy
purpose: inspect and optionally adjust _regrownItemData before Regrowable.StartInteraction while recording the hero additional-plants capability when resolvable
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Regrowables, type: Regrowable, member: StartInteraction, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before regrowable interaction, thread: Unity main thread expected but unverified, frequency: interaction event, inputs: Regrowable instance, reflected _regrownItemData, reflective Hero.Current.Development.CanGatherAdditionalPlants lookup, outputs: diagnostics and possible quantity mutation, side_effects: may alter gathered item quantity and performs additional reflection }
safety: { nullability: missing item field returns; capability lookup catches all failures, failure_mode: caught exception and skip, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: regrowable, perk, harvest, and StartInteraction patches, load_order: prefix mutation order is significant, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/HarvestYieldDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [0c1d9b7e9c7b71b1be130ac886bbb7dcc21201e6, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: live harvest mutation, hero-state inspection at runtime, promotion }
limitations: [Exact overload and regrowth lifecycle are unresolved, the Hero.Current capability lookup is a second unverified reflection chain]
```

No record in this family is promoted beyond `candidate`.