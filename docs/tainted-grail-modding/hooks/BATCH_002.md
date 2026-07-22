# Semantic Hook Batch 002

## Identity

- source repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- source commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- source root licence: `NOASSERTION`
- authority: documentation and research classification only
- promotion ceiling: `candidate`

Batch 002 closes the aggregate Avalon Mounts decomposition blocker and processes the first eight caller files that use the Tainted Economy name-based patch registrar. No source-only record is promoted by this work.

## Selected source dispositions

### Avalon Mounts

| Source | Blob | Result |
|---|---|---|
| `mods/avalon-mounts/src/Plugin.cs` | `896e0f4e63c57b1800eb3ac09468cefe0ca8509d` | seven exact target records plus lifecycle/frame facts |
| `mods/avalon-mounts/src/WolfMount/WolfMountInputFallbackPatch.cs` | `212fc43b9c857ad25fa2d41fecae8aca63c0c821` | callback evidence for movement-input target |
| `mods/avalon-mounts/src/Diagnostics/NativeHorseTransitionProbe.cs` | `7459480772a1fb7660af722dddd91ee1497d2ea5` | diagnostics-only callback evidence and privacy blocker input |
| `mods/avalon-mounts/src/Diagnostics/NativeHorseSpeedUpgradePatches.cs` | `028056ab83dd2e5b47b9a14254281dc58a81931c` | running and turning return-value mutation evidence |
| `mods/avalon-mounts/src/NativeHorse/NativeHorseRecallUpgradePatches.cs` | `9d602eb2f3faf25ff363e7d76d7ae319d538e9c6` | teleport-decision and target-speed mutation evidence |
| `mods/avalon-mounts/src/NativeHorse/NativeHorseMountedMarkerUpgradePatches.cs` | `005d9b4f7557588a9c951768a081db4c6596f8cd` | mounted-marker mutation evidence |
| `mods/avalon-mounts/src/Diagnostics/MountModDetector.cs` | `ed46ff75641ba8247d4ef13e5adcacdaf46606cc` | compatibility-detection source fact |
| `mods/avalon-mounts/src/AvalonMounts.csproj` | `2f683eba7f696b9e78ace7797a5a81a9e7f36128` | Mono/BepInEx-v5 project-family fact |

Results:

- seven complete source-only candidate hook records: `records/MOUNTS_RUNTIME_TARGETS.md`;
- plugin lifecycle, per-frame, cleanup, and compatibility facts: `../systems/mounts/AVALON_MOUNTS_RUNTIME_FACTS.md`;
- residual diagnostics/output/privacy blocker: `blockers/AVALON_MOUNTS_DIAGNOSTICS_PRIVACY.md`;
- the Batch 001 aggregate blocker is marked decomposed, not promoted.

### Tainted Economy registrar callers

| Caller source | Blob | Exact target records |
|---|---|---:|
| `mods/TaintedEconomy/src/Patches/CraftingCostLive.cs` | `84543d2b23705e6133a904e9a9ebf0f4a47f5fee` | 1 |
| `mods/TaintedEconomy/src/Patches/RecipeCostDiagnosticsPatch.cs` | `d327e99b81b4f4e5295ea062d958d5b2727f8784` | 2 |
| `mods/TaintedEconomy/src/Patches/HarvestYieldDiagnosticsPatch.cs` | `0c1d9b7e9c7b71b1be130ac886bbb7dcc21201e6` | 3 |
| `mods/TaintedEconomy/src/Patches/RewardPayoutDiagnosticsPatch.cs` | `5d84aef86e68472d2004356ef1f4dbe5eee6e07e` | 4 |
| `mods/TaintedEconomy/src/Patches/ServiceCostPatches.cs` | `66a97f43161051ef5e498587c4fd4128b18f26bc` | 6 |
| `mods/TaintedEconomy/src/Patches/ContainerLootLive.cs` | `583dbcef5d18b01d449f72214a8eef7cf83cfe91` | 2 |
| `mods/TaintedEconomy/src/Patches/VendorPriceDiagnosticsPatch.cs` | `95043833be680eab2bae2aa5a5674c8213cf7f72` | 1 |
| `mods/TaintedEconomy/src/Patches/ContainerLootDiagnosticsPatch.cs` | `14d724ab099588ac9b1291bc0f53df57dc798f6b` | 1 |

The caller files produce twenty records grouped under:

- `records/ECONOMY_CRAFTING_RECIPE.md`;
- `records/ECONOMY_HARVEST.md`;
- `records/ECONOMY_REWARDS.md`;
- `records/ECONOMY_SERVICE_COSTS.md`;
- `records/ECONOMY_CONTAINER_LOOT.md`;
- `records/ECONOMY_VENDOR_PRICE.md`.

## Counts

- selected primary source files: 16
- source-only candidate hook records: 27
  - Avalon Mounts: 7
  - Tainted Economy callers: 20
- canonical source-system facts: 3
  - Avalon Mounts plugin lifecycle and cleanup
  - Avalon Mounts per-frame controller lane
  - filename-token compatibility detection
- residual research blockers: 1
  - Avalon Mounts diagnostics output, redaction, retention, and privacy
- explicit rejections: 0
- promoted hooks: 0

## Shared findings

1. Every dynamic target is registered through source-level type/member spelling, not an exact current assembly fingerprint.
2. All records are Mono-family because both projects target `netstandard2.1`, reference the local BepInEx v5 layout, and reference `TG.Main.dll`; exact loader and Harmony versions remain unknown.
3. Tainted Economy's registrar resolves methods by name without parameter types. Overloads are unresolved until exact-profile metadata verification.
4. `SearchAction.ShowContainerContents` has a live prefix and a diagnostics postfix from separate caller files. Their before/after-original relationship is known, but combined ordering with other mods is not.
5. Several economy callbacks are mixed lanes: they may mutate when a live configuration is enabled and also emit diagnostics when the diagnostic lane is enabled.
6. Avalon Mounts separates configuration/guard checks from callbacks, but source checks alone do not prove runtime gates, cleanup, or compatibility.

## Promotion blockers

Every candidate remains blocked from promotion until it has:

- exact game version, branch, loader, Harmony version, and assembly fingerprints;
- exact declaring type, overload, parameter list, return type, and generic closure rules;
- successful target resolution and registration evidence;
- bounded load and unload evidence, including `UnpatchSelf` and owned-object cleanup;
- controlled behavior evidence for both enabled and disabled configurations;
- combined-mod and load-order testing;
- explicit save, story, performance, UI, filesystem, and privacy review.

## Next batch boundary

Semantic Hook Batch 003 should process downstream mutation helpers referenced by the economy callbacks and the two Avalon Mounts frame-ticked controllers. A caller record does not prove the safety or semantics of a helper it invokes.