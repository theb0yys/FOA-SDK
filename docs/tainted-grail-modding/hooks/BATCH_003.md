# Semantic Hook Batch 003

## Identity

- source repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- source commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- source root licence: `NOASSERTION`
- selected primary source files: 15
- supporting dependency files: 3
- authority: documentation and research classification only

Batch 003 reviews the downstream economy mutation/reflection helpers named by Batch 002 and the two Avalon Mounts controllers called from `Plugin.Update`. Caller configuration, caller hook status, and caller exception handling do not transfer validation or safety to these helpers.

## Results

| Selected source | Source blob | Primary result | Destination |
|---|---|---|---|
| `mods/TaintedEconomy/src/Diagnostics/EconomyReflection.cs` | `4697cb7cd98297d96569626c6b0bb3f0c6d3cc69` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/VendorPriceLivePricing.cs` | `5e7409cf644e06e353d2b306b5920b19ef1e0d00` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/RegionalVendorPricePricing.cs` | `c0d4ea116c7fbd74f1a7ed5c82a1a2ea0b09278e` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/QuantityLive.cs` | `22c556f6c427c87ce5fd39b06730307b57c78ef8` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/ProtectedItemMutationGuard.cs` | `cf43455ef6c60fb7e9798bd52232d96a17562201` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/RewardWealthLive.cs` | `f646f9759e3e9942c4366b59a1ff4971655f211a` | research blocker | `blockers/ECONOMY_HELPER_SAFETY.md` |
| `mods/TaintedEconomy/src/Patches/EconomyClassifierDryRun.cs` | `f48082becd31e408dadeb6acb14bbbeab0a3a3b8` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/CraftingCostDryRun.cs` | `67e5506a111eec35b57ff19493d2f16e281fa7e1` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/QuantityDryRun.cs` | `4af8fd1b2924b29258c06c53c4d469d7fedb85ea` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/ContainerLootDryRun.cs` | `02dad135a0b321e8b5019303dc77b572d483668c` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/HarvestYieldDryRun.cs` | `fc65c843d441a38decd20496037721af11736019` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/RewardWealthDryRun.cs` | `8afb4c6183c900320fc3a81ed9c0e88141669720` | canonical source-system fact | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/TaintedEconomy/src/Patches/HarvestYieldLive.cs` | `b08b96d14280c4e898471faa604f89f980135d37` | canonical delegation fact; dependency blocked | `../systems/items-economy/HELPER_SEMANTICS_FACTS.md` |
| `mods/avalon-mounts/src/WolfMount/WolfMountSeatBridgeController.cs` | `c226dead88329eb55f3444f36c6fceecc2ed7e66` | research blocker | `blockers/WOLF_MOUNT_SEAT_CONTROLLER.md` |
| `mods/avalon-mounts/src/WolfMount/AvalonCompanionsDialogueBridge.cs` | `cc1b0165303e3bf1de09d5ac249820e70566afe9` | two-record candidate adapter family | `records/MOUNTS_AVALON_COMPANIONS_DIALOGUE_BRIDGE.md` |

## Supporting dependency evidence

The controller review also reads, but does not disposition as primary Batch 003 selections:

- `mods/avalon-mounts/src/Plugin.cs` — blob `896e0f4e63c57b1800eb3ac09468cefe0ca8509d`;
- `mods/avalon-mounts/src/Safety/DiagnosticsOnlyGuard.cs` — blob `5609bcff8082d362bdf6356a224d01e291e88f20`;
- `mods/avalon-mounts/src/WolfMount/WolfNativeMountContract.cs` — blob `65a52e309da76d5bfd9b5627ee92def688031070`.

Those dependencies establish scheduling, gate input, runtime mutation, ownership, and cleanup behavior. They receive no inherited promotion.

## Batch counts

- selected primary source files: 15
- candidate adapter records: 2
- canonical source-system facts: 7
- research blockers: 7
- explicit rejections: 0
- promoted records: 0

## Key findings

1. `EconomyReflection.ReadMember` performs uncached non-public property/field lookup. A throwing getter aborts the fallback chain and returns `null`; the helper does not continue to later names or a same-name field.
2. `QuantityLive` can write public or non-public quantity fields/properties across integral and floating numeric types. It rounds to a whole-number decimal first, clamps to at least one, and has no transaction spanning the item write and optional owner-field write-back.
3. `ProtectedItemMutationGuard` is a heuristic. Missing protection flags are treated as not protected, while substring patterns such as `relic`, `artifact`, `unique`, and selected item names can create false positives.
4. `RewardWealthLive` identifies Wealth with a case-insensitive substring, reflectively writes `amount`/`Amount` before the native story step executes, and has no rollback if downstream execution later fails.
5. Vendor pricing relies on Harmony argument positions, exact runtime type-name equality for Hero direction, reflected item fields, display/tag substring classification, and first-match regional substring rules. Regional rules are reparsed on each evaluation.
6. Dry-run helpers do not write the reviewed game objects in their selected source, but they can reflect over runtime state and emit diagnostics. Their no-mutation fact does not establish bounded output, privacy, or hot-path cost.
7. `WolfMountSeatBridgeController.Tick` performs a per-frame gate check, mount-contract observation, attachment validation, and hotkey poll. Attach scans and materializes all `NpcHeroPetAlly` elements, then delegates invasive conversion and ownership changes to `WolfNativeMountContract`.
8. The mount contract creates Unity objects/components, writes private fields, adds/discards model elements, disables movement behaviours by type-name heuristics, changes mount ownership, and attempts cleanup. Previous-null ownership and discarded native actions do not have proven transactional restoration.
9. `AvalonCompanionsDialogueBridge` retries assembly/type/method resolution at most every two unscaled seconds until registration succeeds. It requires exact public static signatures and unregisters by owner ID on gate loss or disposal.

## Non-inheritance rule

A caller record may prove that a helper is gated, called, or wrapped in `try/catch`. It does not prove:

- the helper selected the correct reflected member;
- the helper mutation is atomic or reversible;
- the helper is allocation-safe on a hot or frame path;
- diagnostics are bounded or safe to share;
- cleanup restores prior game and mod state;
- or combined-mod ordering is compatible.

## Scope boundary

This batch performs no game inspection, assembly loading, build, deployment, game launch, runtime reflection, mutation, save access, diagnostics capture, plug-in registration, filesystem scanning, publication, catalog mutation, or evidence promotion.

## Next batch boundary

Semantic Hook Batch 004 should process the remaining high-risk dependencies exposed here:

1. exact-profile reflection and mutation fixtures for `EconomyReflection`, `QuantityLive`, `RewardWealthLive`, and vendor pricing;
2. bounded diagnostics-writer and retention semantics shared by the dry-run helpers;
3. the wolf native mount contract's ownership, component, movement-suspension, and rollback surfaces;
4. exact Avalon Companions API version/signature resolution and combined-mod registration behavior.
