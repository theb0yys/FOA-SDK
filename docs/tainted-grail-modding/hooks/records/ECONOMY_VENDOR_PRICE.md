# Candidate Hook — Vendor Price Calculation

```yaml
hook_id: tg.hook.economy.trade-utils-price
title: Vendor price calculation
status: candidate
hook_class: method-patch
domain: vendors-items-and-economy
purpose: observe TradeUtils.Price and optionally replace the returned price through configured live pricing rules
profile: { game_version: unknown, branch: unknown, runtime: Mono, loader: BepInEx v5 Mono exact version unknown, framework: Harmony exact version unknown }
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Locations.Shops
  type: TradeUtils
  member: Price
  signature: unknown; registrar resolves by name only and callback interprets argument positions as seller, buyer, item, and optional count
patch: { style: postfix, ordering: unspecified, cleanup: owning Plugin.OnDestroy calls Harmony.UnpatchSelf }
context:
  lifecycle_point: after native price calculation
  thread: Unity main thread expected but unverified
  frequency: potentially hot-path in shop UI
  inputs: Harmony argument array, native int result, live pricing configuration, reflected seller/buyer/item context
  outputs: native or adjusted int price plus optional diagnostics and classifier dry-run rows
  side_effects: can change purchase/resale price and emit diagnostics
safety: { nullability: argument positions and reflected values are guarded; live exceptions are caught, failure_mode: fail-open to vanilla price when live adjustment fails, save_risk: unknown, performance_risk: high, story_risk: bounded }
compatibility: { known_conflicts: vendor, price, barter, regional-economy, and TradeUtils.Price patches, load_order: result mutation is order-sensitive, version_stability: low because overload identity is unresolved }
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths: [mods/TaintedEconomy/src/Patches/VendorPriceDiagnosticsPatch.cs, mods/TaintedEconomy/src/Patches/PatchRegistration.cs, mods/TaintedEconomy/src/Plugin.cs, mods/TaintedEconomy/src/AvalonEconomy.csproj]
  source_blobs: [95043833be680eab2bae2aa5a5674c8213cf7f72, f6997889aa3468e8692602423b8ce8481648ad26, 73bc0b7101e02363c27895e04453971a02f96b4b, 7fa22d90de1c79320ccf03a22b567824c903ea67]
  evidence_ids: []
validation: { static: extracted, target_resolution: not performed, load: not established, behavior: not established, combined_mods: not established }
permissions: { allowed: documentation, offline overload verification, synthetic pricing fixtures, prohibited: live price mutation, shop execution, save claims, promotion }
limitations:
  - Exact overload, argument positions, count semantics, and direction inference are unresolved.
  - VendorPriceLivePricing and classifier helpers require downstream semantic review.
  - Diagnostic volume under repeated UI price reads is unverified.
```

This record remains `candidate`.