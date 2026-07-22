# Canonical Source Facts — Economy Helper Semantics

These facts describe the pinned upstream Tainted Economy source. They are not facts about an exact current game profile and do not authorize runtime diagnostics or mutation.

## Shared identity

```yaml
scope: pinned upstream source only
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
runtime_family: Mono source family
project: mods/TaintedEconomy/src/AvalonEconomy.csproj
project_blob: 7fa22d90de1c79320ccf03a22b567824c903ea67
licence: NOASSERTION at repository root
```

## Fact 1: economy classifier dry run

```yaml
fact_id: tg.fact.items-economy.economy-classifier-dry-run
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/EconomyClassifierDryRun.cs
source_blob: f48082becd31e408dadeb6acb14bbbeab0a3a3b8
primary_effect: diagnostics-only in the selected source
```

The helper reads the first four `TradeUtils.Price` Harmony arguments as seller, buyer, item, and count. It calls `VendorPriceLivePricing.TryEvaluate` to compute a proposed price, classifies an item with reflected booleans and vendor-pricing heuristics, and writes a diagnostic row when enabled.

It does not assign a game price or write an item in this file. That no-mutation fact does not validate its argument contract, classification, reflected values, regional evaluation, output volume, or diagnostic writer.

## Fact 2: crafting-cost dry run

```yaml
fact_id: tg.fact.items-economy.crafting-cost-dry-run
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/CraftingCostDryRun.cs
source_blob: 67e5506a111eec35b57ff19493d2f16e281fa7e1
primary_effect: diagnostics-only in the selected source
```

The helper reflectively reads a recipe's `Ingredients`, then each ingredient's `Count` or `count`. Parsed values are multiplied, rounded away from zero, and clamped to at least one for a proposed diagnostic string. The selected source writes no recipe or ingredient member.

The ingredient enumeration has no local row cap. Reflection cost, collection size, diagnostic string size, and output retention remain unresolved.

## Fact 3: shared quantity dry run

```yaml
fact_id: tg.fact.items-economy.quantity-dry-run
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/QuantityDryRun.cs
source_blob: 4af8fd1b2924b29258c06c53c4d469d7fedb85ea
primary_effect: diagnostics-only in the selected source
```

The helper reads the first available `quantity`, `Quantity`, `count`, or `Count` member through `EconomyReflection`, defaults missing text to `1`, computes a rounded proposed quantity clamped to at least one, and emits a diagnostic row. It does not call a setter or write the selected item object.

`ContainerLootDryRun` and `HarvestYieldDryRun` are thin callers of this helper. Their caller gates do not prove the reflected member is the correct game quantity field.

## Fact 4: container-loot dry-run delegation

```yaml
fact_id: tg.fact.items-economy.container-loot-dry-run-delegation
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/ContainerLootDryRun.cs
source_blob: 02dad135a0b321e8b5019303dc77b572d483668c
primary_effect: gated delegation to QuantityDryRun
```

When its configuration gate is enabled, the helper delegates the item row, source method, context, configured multiplier, and logging choice to `QuantityDryRun`. It contains no independent reflection or mutation logic.

## Fact 5: harvest-yield dry-run delegation

```yaml
fact_id: tg.fact.items-economy.harvest-yield-dry-run-delegation
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/HarvestYieldDryRun.cs
source_blob: fc65c843d441a38decd20496037721af11736019
primary_effect: gated delegation to QuantityDryRun
```

When enabled, the helper delegates harvest item data and context to `QuantityDryRun`. It contains no independent setter or rollback path.

## Fact 6: reward-Wealth dry run

```yaml
fact_id: tg.fact.items-economy.reward-wealth-dry-run
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/RewardWealthDryRun.cs
source_blob: 8afb4c6183c900320fc3a81ed9c0e88141669720
primary_effect: diagnostics-only in the selected source
```

The helper treats any currency-type text containing `Wealth`, case-insensitively, as Wealth. Positive parsed amounts are multiplied and rounded away from zero, then written to diagnostics as a proposal. Non-Wealth, unparsed, zero, and negative values are skipped or optionally logged.

It does not write the story step in this file. Substring currency classification, amount semantics, story identity, output handling, and comparison with `RewardWealthLive` remain unverified.

## Fact 7: harvest-yield live is a thin mutation delegator

```yaml
fact_id: tg.fact.items-economy.harvest-yield-live-delegation
status: source-fact
source_path: mods/TaintedEconomy/src/Patches/HarvestYieldLive.cs
source_blob: b08b96d14280c4e898471faa604f89f980135d37
primary_effect: gated delegation to QuantityLive
blocked_dependency: tg.blocker.items-economy.quantity-live-reflective-write
```

The helper checks the harvest live gate, calls `QuantityLive.TryAdjustRuntimeQuantity`, optionally logs a successful adjustment, and catches exceptions. It has no independent quantity-member selection, conversion, write, rollback, or protected-item policy.

Therefore its caller gate and exception handling do not validate the mutation. All mutation safety remains blocked by the `QuantityLive` and protected-item-guard records.

## Dependency map

| Fact helper | Direct semantic dependencies |
|---|---|
| Economy classifier dry run | `VendorPriceLivePricing`, `EconomyReflection`, diagnostic writer |
| Crafting-cost dry run | `EconomyReflection`, diagnostic writer |
| Quantity dry run | `EconomyReflection`, diagnostic writer |
| Container-loot dry run | `QuantityDryRun` |
| Harvest-yield dry run | `QuantityDryRun` |
| Reward-Wealth dry run | `EconomyReflection`, diagnostic writer |
| Harvest-yield live | `QuantityLive`, `EconomyReflection` for logging |

## Non-inheritance and prohibitions

These facts prove source control flow only. They do not establish correct game members, exact types, safe hot-path cost, bounded output, privacy, mutation safety, persistence semantics, or combined-mod compatibility.

They authorize documentation and offline synthetic analysis only. They do not authorize runtime reflection, game-object mutation, diagnostic capture, save access, deployment, or evidence promotion.
