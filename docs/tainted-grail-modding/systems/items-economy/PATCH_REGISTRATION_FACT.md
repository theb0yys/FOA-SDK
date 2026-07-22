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

The canonical result is therefore a system fact about the upstream economy mod's registration pattern. Every caller of `PatchRegistration.TryPatch` must receive an independent semantic disposition:

- a candidate hook record with its exact caller-supplied target;
- a canonical system fact when the caller describes source architecture rather than a native hook;
- a research blocker when the target or behavior cannot be bounded;
- or an explicit rejection when no hook exists.

## Safety and compatibility implications

The helper is intentionally fail-open at registration time: absent targets or registration exceptions disable that diagnostic lane rather than throwing from this helper. That is useful, but incomplete.

The name-only method lookup can select the wrong overload or become ambiguous when the native API changes. A caller cannot advance beyond `candidate` until its exact parameter list, return type, patch style, downstream behavior, and target assembly fingerprint are recorded.

## Next extraction queue

The first caller queue is:

- `mods/TaintedEconomy/src/Patches/CraftingCostLive.cs`;
- `mods/TaintedEconomy/src/Patches/RecipeCostDiagnosticsPatch.cs`;
- `mods/TaintedEconomy/src/Patches/HarvestYieldDiagnosticsPatch.cs`;
- `mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs`;
- `mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs`;
- `mods/TaintedEconomy/src/Patches/ContainerLootLive.cs`;
- `mods/TaintedEconomy/src/Patches/VendorPriceDiagnosticsPatch.cs`;
- `mods/TaintedEconomy/src/Patches/ContainerLootDiagnosticsPatch.cs`.

No target from those files is promoted or implied by this fact record.