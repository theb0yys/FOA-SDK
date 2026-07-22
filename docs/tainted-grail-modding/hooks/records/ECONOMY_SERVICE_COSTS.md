# Candidate Hook Family — Service-Cost Getters

All six records are postfixes on name-resolved getters. Each callback may emit a dry-run row and may replace the returned `int` cost when the live lane is enabled. Registration is skipped entirely when both service-cost dry-run and live configuration are disabled.

## Gems base wealth cost

```yaml
hook_id: tg.hook.economy.gems-base-service-cost-getter
title: Gems base Wealth service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale the Wealth service cost returned by GemsBaseUI<T>.ServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: "GemsBaseUI`1", member: get_ServiceCost, signature: expected int getter on an open/closed generic type; exact closure rules unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after service-cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native int result, configured Wealth multiplier, outputs: dry-run row and optional adjusted int result, side_effects: possible price mutation }
safety: { nullability: instance type read assumes non-null callback instance, failure_mode: fail-open when live disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: gem-service UI and cost getter patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs, mods/TaintedEconomy/src/Plugin.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26, 73bc0b7101e02363c27895e04453971a02f96b4b], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline generic-target verification, prohibited: service-price mutation and promotion }
limitations: [Generic target resolution and exact getter ownership are unresolved]
```

## Gems base cobweb cost

```yaml
hook_id: tg.hook.economy.gems-base-cobweb-service-cost-getter
title: Gems base Cobweb service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale the Cobweb cost returned by GemsBaseUI<T>.CobwebServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: "GemsBaseUI`1", member: get_CobwebServiceCost, signature: expected int getter on a generic type; exact closure rules unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after cobweb-cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native int result, configured Cobweb multiplier, outputs: dry-run row and optional adjusted result, side_effects: possible price mutation }
safety: { nullability: instance type read assumes non-null, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: gem-service and cost getter patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline generic-target verification, prohibited: service-price mutation and promotion }
limitations: [Generic target resolution and currency semantics are unresolved]
```

## Sharpening wealth cost

```yaml
hook_id: tg.hook.economy.sharpening-service-cost-getter
title: Sharpening Wealth service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale the Wealth service cost returned by SharpeningUI<T>.ServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: "SharpeningUI`1", member: get_ServiceCost, signature: expected int getter on a generic type; exact closure rules unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after sharpening cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native result, Wealth multiplier, outputs: dry-run row and optional adjusted result, side_effects: possible sharpening price mutation }
safety: { nullability: instance type read assumes non-null, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: sharpening and service-cost patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline generic-target verification, prohibited: service-price mutation and promotion }
limitations: [Generic target resolution and final transaction semantics are unresolved]
```

## Sharpening cobweb cost

```yaml
hook_id: tg.hook.economy.sharpening-cobweb-service-cost-getter
title: Sharpening Cobweb service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale the Cobweb service cost returned by SharpeningUI<T>.CobwebServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: "SharpeningUI`1", member: get_CobwebServiceCost, signature: expected int getter on a generic type; exact closure rules unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after sharpening cobweb-cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native result, Cobweb multiplier, outputs: dry-run row and optional adjusted result, side_effects: possible sharpening price mutation }
safety: { nullability: instance type read assumes non-null, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: sharpening and service-cost patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline generic-target verification, prohibited: service-price mutation and promotion }
limitations: [Generic target resolution and currency semantics are unresolved]
```

## Weight-reduction wealth cost

```yaml
hook_id: tg.hook.economy.weight-reduction-service-cost-getter
title: Weight-reduction Wealth service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale WeightReductionUI.ServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: WeightReductionUI, member: get_ServiceCost, signature: expected int getter; metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after weight-reduction cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native result, Wealth multiplier, outputs: dry-run row and optional adjusted result, side_effects: possible service-price mutation }
safety: { nullability: instance type read assumes non-null, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: weight-reduction and service-cost patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: service-price mutation and promotion }
limitations: [Exact getter and transaction lifecycle are unresolved]
```

## Weight-reduction cobweb cost

```yaml
hook_id: tg.hook.economy.weight-reduction-cobweb-service-cost-getter
title: Weight-reduction Cobweb service-cost getter
status: candidate
hook_class: method-patch
domain: services-items-and-economy
purpose: observe or scale WeightReductionUI.CobwebServiceCost
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Gems, type: WeightReductionUI, member: get_CobwebServiceCost, signature: expected int getter; metadata unverified }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: after weight-reduction cobweb-cost getter, thread: Unity main thread expected but unverified, frequency: UI read and potentially repeated, inputs: UI instance, native result, Cobweb multiplier, outputs: dry-run row and optional adjusted result, side_effects: possible service-price mutation }
safety: { nullability: instance type read assumes non-null, failure_mode: fail-open when disabled or unchanged, save_risk: unknown, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: weight-reduction and service-cost patches, load_order: result mutation is order-sensitive, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [66a97f43161051ef5e498587c4fd4128b18f26bc, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: service-price mutation and promotion }
limitations: [Exact getter and currency semantics are unresolved]
```

No record in this family is promoted beyond `candidate`.