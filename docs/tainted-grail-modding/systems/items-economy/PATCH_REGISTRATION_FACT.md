# Canonical Source-System Fact — Economy Patch Registrar

```yaml
fact_id: tg.fact.items-economy.upstream-patch-registrar
status: source-verified
fact_class: mod-system-architecture
domain: items-and-economy
scope: pinned upstream source only
statement: the upstream TaintedEconomy source centralizes caller-supplied Harmony registration through a guarded TryPatch helper
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono source family
  loader: BepInEx/Harmony exact versions unknown
source:
  repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  commit: d7e740e7f167b73152b53409e483dab07d80d048
  path: mods/TaintedEconomy/src/Patches/PatchRegistration.cs
  blob: f6997889aa3468e8692602423b8ce8481648ad26
licence: NOASSERTION at repository root
```

## Verified facts

The selected file establishes all of the following about the upstream mod's registration architecture:

1. callers supply the diagnostic lane, target type name, target method name, patch method name, patch type, and prefix/postfix choice;
2. the target type is resolved with `AccessTools.TypeByName(typeName)`;
3. the target method is resolved with `AccessTools.Method(targetType, methodName)` without an explicit parameter list;
4. the patch method is resolved from the caller-provided patch type and method name;
5. missing target type, target method, or patch method returns `false` after a warning;
6. registration exceptions are caught, logged, and return `false`;
7. successful registration returns `true` and logs the caller-supplied target identity.

## Canonical disposition

This file is **not a hook record** because it defines no native target of its own. Target identity is supplied by its callers. Treating the helper as evidence for every caller would collapse multiple unrelated targets into one false record and would hide overload ambiguity.

The canonical result is therefore a system fact about the upstream economy mod's registration pattern. Every caller of `PatchRegistration.TryPatch` receives an independent semantic disposition.

## Safety and compatibility implications

The helper is fail-open at registration time: absent targets or registration exceptions return `false` rather than throwing from this helper. Callers may disable a lane when all primary targets are absent.

That behavior is useful but incomplete. The name-only method lookup can select the wrong overload or become ambiguous when the native API changes. A caller cannot advance beyond `candidate` until its exact parameter list, return type, patch style, downstream behavior, and target assembly fingerprint are recorded.

## Batch 002 caller completion

The first eight callers are processed. They create twenty source-only candidates and no promotions:

| Caller | Result |
|---|---|
| `CraftingCostLive.cs` | [Ingredient count getter](../../hooks/records/ECONOMY_CRAFTING_RECIPE.md) |
| `RecipeCostDiagnosticsPatch.cs` | [Recipe-grid Refresh and ClickSlot](../../hooks/records/ECONOMY_CRAFTING_RECIPE.md) |
| `HarvestYieldDiagnosticsPatch.cs` | [Three harvest interaction prefixes](../../hooks/records/ECONOMY_HARVEST.md) |
| `RewardPayoutDiagnosticsPatch.cs` | [Four reward/story routes](../../hooks/records/ECONOMY_REWARDS.md) |
| `ServiceCostPatches.cs` | [Six service-cost getters](../../hooks/records/ECONOMY_SERVICE_COSTS.md) |
| `ContainerLootLive.cs` | [SearchAction initialization and live display prefix](../../hooks/records/ECONOMY_CONTAINER_LOOT.md) |
| `VendorPriceDiagnosticsPatch.cs` | [TradeUtils.Price](../../hooks/records/ECONOMY_VENDOR_PRICE.md) |
| `ContainerLootDiagnosticsPatch.cs` | [SearchAction display diagnostics postfix](../../hooks/records/ECONOMY_CONTAINER_LOOT.md) |

The complete ledger is [Semantic Hook Batch 002](../../hooks/BATCH_002.md).

## Remaining boundary

The caller records still invoke downstream reflection, pricing, quantity, protected-item, dry-run, diagnostics, and rule helpers. Those helpers are not proven by the registrar or caller records and form a later semantic batch.