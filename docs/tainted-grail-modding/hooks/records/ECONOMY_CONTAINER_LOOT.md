# Candidate Hook Family — Container Loot

This family contains one initialization postfix and two separate patches on `SearchAction.ShowContainerContents`. The live prefix runs before the original; the diagnostics postfix runs after the original. Their relationship inside this plug-in is structurally clear, but ordering against other mods is not.

## Search-action initialization live lane

```yaml
hook_id: tg.hook.economy.search-action-on-initialize-live
title: Search-action initialization live lane
status: candidate
hook_class: method-patch
domain: containers-loot-and-economy
purpose: adjust generated container runtime rows after SearchAction.OnInitialize when quantity or rule live lanes are enabled
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Actions, type: SearchAction, member: OnInitialize, signature: unknown; registrar resolves by name only }
patch: { style: postfix, ordering: unspecified, cleanup: Harmony.UnpatchSelf plus ContainerLootLive.Reset clears source-owned hash sets and rule cache }
context: { lifecycle_point: after container search action initialization, thread: Unity main thread expected but unverified, frequency: per search-action instance, inputs: SearchAction instance and reflected _itemsInsideContainer rows, outputs: adjusted quantities, removed rows, preserved protected rows, and optional diagnostics, side_effects: mutates generated container contents and stores instance identity hashes }
safety: { nullability: missing/non-enumerable rows return unchanged and exceptions are caught, failure_mode: fail-open, save_risk: write unknown and potentially high, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: loot generation, container initialization, item quantity, protected-item, and SearchAction patches, load_order: postfix and row-mutation order are significant, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ContainerLootLive.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs, mods/TaintedEconomy/src/Plugin.cs], source_blobs: [583dbcef5d18b01d449f72214a8eef7cf83cfe91, f6997889aa3468e8692602423b8ce8481648ad26, 73bc0b7101e02363c27895e04453971a02f96b4b], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and synthetic rule review, prohibited: live loot mutation, save access, deployment, promotion }
limitations:
  - Exact OnInitialize overload and persistence semantics are unresolved.
  - Downstream QuantityLive and ProtectedItemMutationGuard helpers require Batch 003 review.
  - Runtime hash identities can be reused; lifecycle behavior across scene transitions is unverified.
```

## Show-container live prefix

```yaml
hook_id: tg.hook.economy.search-action-show-container-contents-live-prefix
title: Show-container live prefix
status: candidate
hook_class: method-patch
domain: containers-loot-and-economy
purpose: apply container rules immediately before SearchAction.ShowContainerContents when initialization did not already process the instance
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Actions, type: SearchAction, member: ShowContainerContents, signature: unknown; callback treats first two arguments as location and open-transfer context }
patch: { style: prefix, ordering: runs before original and before the same plug-in's diagnostics postfix; order versus third-party prefixes unknown, cleanup: Harmony.UnpatchSelf plus ContainerLootLive.Reset }
context: { lifecycle_point: before container contents are shown, thread: Unity main thread expected but unverified, frequency: container-open event, inputs: SearchAction instance, Harmony arguments, reflected _itemsInsideContainer rows, configured rules, outputs: removed or quantity-adjusted rows, side_effects: changes contents before display and records the instance as processed }
safety: { nullability: arguments and rows are guarded; exceptions are caught, failure_mode: fail-open, save_risk: write unknown and potentially high, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: container UI, loot, transfer, and ShowContainerContents prefixes, load_order: high sensitivity because row mutation occurs before original, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ContainerLootLive.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [583dbcef5d18b01d449f72214a8eef7cf83cfe91, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: runtime row removal, quantity mutation, save claims, promotion }
limitations: [Exact arguments and original method semantics are unresolved, rule matching and stable-roll behavior require downstream review]
```

## Show-container diagnostics postfix

```yaml
hook_id: tg.hook.economy.search-action-show-container-contents-diagnostics-postfix
title: Show-container diagnostics postfix
status: candidate
hook_class: method-patch
domain: containers-loot-and-economy
purpose: observe container rows after SearchAction.ShowContainerContents and emit diagnostics/dry-run candidates
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Actions, type: SearchAction, member: ShowContainerContents, signature: unknown; callback treats first two arguments as location and open-transfer context }
patch: { style: postfix, ordering: after original; therefore also after this plug-in's live prefix, order versus third-party postfixes unknown, cleanup: Harmony.UnpatchSelf; diagnostic output cleanup separate }
context: { lifecycle_point: after container contents are shown, thread: Unity main thread expected but unverified, frequency: container-open event, inputs: SearchAction instance, Harmony arguments, reflected _itemsInsideContainer rows, outputs: one diagnostic/dry-run row per non-null item row, side_effects: reflection and diagnostic output }
safety: { nullability: absent arguments, missing rows, and null item rows are guarded; exceptions are caught, failure_mode: diagnostics-only, save_risk: none directly, performance_risk: medium, story_risk: none }
compatibility: { known_conflicts: container diagnostics and ShowContainerContents postfixes, load_order: observed rows may include mutations from prefixes/original/earlier postfixes, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/ContainerLootDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs, mods/TaintedEconomy/src/Patches/ContainerLootLive.cs], source_blobs: [14d724ab099588ac9b1291bc0f53df57dc798f6b, f6997889aa3468e8692602423b8ce8481648ad26, 583dbcef5d18b01d449f72214a8eef7cf83cfe91], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and synthetic diagnostics review, prohibited: runtime capture, raw output publication, promotion }
limitations: [The record observes final rows but cannot attribute each mutation without combined patch-order evidence]
```

No record in this family is promoted beyond `candidate`.