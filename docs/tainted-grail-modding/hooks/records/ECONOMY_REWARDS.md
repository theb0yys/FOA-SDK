# Candidate Hook Family — Reward and Wealth Routes

All four registrations are prefixes resolved by type and method name without parameter metadata.

## Story currency change

```yaml
hook_id: tg.hook.economy.story-change-currency-execute
status: candidate
hook_class: method-patch
domain: rewards-story-and-economy
purpose: observe SChangeCurrency.Execute and optionally adjust positive Wealth rewards through the live helper
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Stories.Steps, type: SChangeCurrency, member: Execute, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before story currency step execution, thread: unknown, frequency: story event, inputs: step instance and Harmony argument array, outputs: diagnostics and possible mutation of the step amount, side_effects: can change positive Wealth payout before story execution }
safety: { nullability: reflected members and argument access are guarded, failure_mode: caught exception and skip, save_risk: write unknown, performance_risk: low, story_risk: high }
compatibility: { known_conflicts: story-step, currency, reward, and economy patches, load_order: prefix mutation order is significant, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [5d84aef86e68472d2004356ef1f4dbe5eee6e07e, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: reward mutation, story execution, save claims, promotion }
limitations: [Exact Execute overload and amount storage are unresolved, downstream live helper is not reviewed in this batch]
```

## Story item-quantity change

```yaml
hook_id: tg.hook.economy.story-change-items-quantity-execute
status: candidate
hook_class: method-patch
domain: rewards-story-and-economy
purpose: observe item rows before SChangeItemsQuantity.Execute
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Stories.Steps, type: SChangeItemsQuantity, member: Execute, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before story item-quantity step, thread: unknown, frequency: story event, inputs: step instance, argument array, reflected item collections, outputs: diagnostic rows, side_effects: reflection and output only in selected source }
safety: { nullability: null rows and absent collections are guarded, failure_mode: diagnostics-only with caught exceptions, save_risk: none directly, performance_risk: medium, story_risk: bounded }
compatibility: { known_conflicts: story item-step and reward patches, load_order: unspecified, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [5d84aef86e68472d2004356ef1f4dbe5eee6e07e, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and synthetic diagnostics review, prohibited: runtime capture, item mutation, promotion }
limitations: [Exact Execute overload and item-row schema are unresolved]
```

## Alive-location death reward

```yaml
hook_id: tg.hook.economy.alive-location-death-reward-before-death
status: candidate
hook_class: method-patch
domain: rewards-combat-and-loot
purpose: observe invocation of AliveLocationDeathReward.OnBeforeDeath without invoking the loot table a second time
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Locations.Attachments.Elements, type: AliveLocationDeathReward, member: OnBeforeDeath, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before death reward processing, thread: unknown, frequency: death event, inputs: reward element instance and reflected spec, outputs: one diagnostic route row, side_effects: diagnostic output only in selected source }
safety: { nullability: reflected spec may be absent, failure_mode: diagnostics-only with caught exceptions, save_risk: none directly, performance_risk: low, story_risk: bounded }
compatibility: { known_conflicts: death, loot, reward, and OnBeforeDeath patches, load_order: unspecified, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [5d84aef86e68472d2004356ef1f4dbe5eee6e07e, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: runtime death/loot execution and promotion }
limitations: [Exact overload and reward lifecycle ordering are unresolved]
```

## Bounty wealth payment

```yaml
hook_id: tg.hook.economy.bounty-pay-wealth-execute
status: candidate
hook_class: method-patch
domain: rewards-crime-and-economy
purpose: observe SBountyPayWealth.Execute as a bounty gold-sink route
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target: { assembly: TG.Main, namespace: Awaken.TG.Main.Stories.Steps, type: SBountyPayWealth, member: Execute, signature: unknown; registrar resolves by name only }
patch: { style: prefix, ordering: unspecified, cleanup: Harmony.UnpatchSelf }
context: { lifecycle_point: before bounty payment execution, thread: unknown, frequency: story/crime event, inputs: step instance and argument array, outputs: one diagnostic route row, side_effects: diagnostic output only in selected source }
safety: { nullability: argument access is guarded, failure_mode: diagnostics-only with caught exceptions, save_risk: none directly, performance_risk: low, story_risk: high }
compatibility: { known_conflicts: bounty, crime, currency, and story-step patches, load_order: unspecified, version_stability: low }
evidence: { source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods, source_commit: d7e740e7f167b73152b53409e483dab07d80d048, source_paths: [mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs], source_blobs: [5d84aef86e68472d2004356ef1f4dbe5eee6e07e, f6997889aa3468e8692602423b8ce8481648ad26], evidence_ids: [] }
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation and offline verification, prohibited: bounty/currency execution, save claims, promotion }
limitations: [Exact Execute overload and story consequences are unresolved]
```

No record in this family is promoted beyond `candidate`.