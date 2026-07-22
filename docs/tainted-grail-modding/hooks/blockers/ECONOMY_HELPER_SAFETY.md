# Research Blockers — Economy Helper Safety

These blockers are independent of the Batch 002 caller records. A caller's configuration gate, `try/catch`, lane disablement, or candidate status does not resolve a helper's reflected-member identity, mutation atomicity, performance, persistence, or rollback behavior.

## Generic economy reflection

```yaml
blocker_id: tg.blocker.items-economy.generic-reflection-helper
status: research-blocker
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_path: mods/TaintedEconomy/src/Diagnostics/EconomyReflection.cs
source_blob: 4697cb7cd98297d96569626c6b0bb3f0c6d3cc69
```

### Source behavior

- Searches instance public and non-public properties, then fields, by caller-supplied fallback names.
- A throwing property getter or field read returns `null` immediately; later names and a same-name alternative are not attempted.
- Performs reflection without a member cache.
- `Text` may invoke arbitrary `IFormattable.ToString` or `ToString` implementations.
- Item-row construction probes many names, GUIDs, flags, tags, prices, quantities, levels, and qualities.
- A static missing-field warning set is not explicitly cleared by the plug-in cleanup shown in the selected source.

### Blocking evidence

1. exact-profile member maps for every caller;
2. deterministic ambiguity and throwing-getter fixtures;
3. proof of bounded allocations and execution on UI/hot paths;
4. output-field classification, redaction, row/byte bounds, retention, and support-sharing rules;
5. lifecycle handling for static diagnostic caches;
6. IL2CPP separation; Mono reflection results must not be projected across runtimes.

## Vendor price evaluation

```yaml
blocker_id: tg.blocker.items-economy.vendor-price-helper
status: research-blocker
source_path: mods/TaintedEconomy/src/Patches/VendorPriceLivePricing.cs
source_blob: 5e7409cf644e06e353d2b306b5920b19ef1e0d00
```

### Source behavior

- Treats Harmony argument indexes 0, 1, 2, and 3 as seller, buyer, item, and count.
- Recognizes Hero only when the runtime full type name equals `Awaken.TG.Main.Heroes.Hero` exactly.
- Classifies item categories using reflected booleans plus quality, tags, names, and substring patterns.
- Preserves vanilla for selected quest, favorite, non-sellable, hidden, and non-droppable cases when those values are resolved.
- Multiplies base and optional regional factors, rounds away from zero, preserves vanilla on overflow, and either clamps below-one results to one or preserves vanilla.
- Can log reflected item summaries from the price path.

### Blocking evidence

1. exact `TradeUtils.Price` overload and argument contract;
2. subclass/proxy and null seller/buyer direction fixtures;
3. canonical item classification and protected-sale policy;
4. integer, count, rounding, and clamp invariants;
5. bounded hot-path reflection, string construction, and logging;
6. order/commutativity tests with other price postfixes;
7. transaction and save/economy consequence analysis.

## Regional vendor pricing

```yaml
blocker_id: tg.blocker.items-economy.regional-vendor-price-helper
status: research-blocker
source_path: mods/TaintedEconomy/src/Patches/RegionalVendorPricePricing.cs
source_blob: c0d4ea116c7fbd74f1a7ed5c82a1a2ea0b09278e
```

### Source behavior

- Builds a lower-cased merchant identity from type, object/template/location/spec summaries, names, scene identity, and tags.
- Parses the configured rule string on every call.
- Uses first-match case-insensitive substring matching; `*` matches every identity.
- Clamps each buy/sell multiplier to `0.1` through `10` and counts malformed rules.
- Emits a cleaned and truncated identity hint, not a canonical location key.

### Blocking evidence

1. canonical merchant/location identity contract;
2. false-positive and false-negative corpus for substring matching;
3. explicit precedence and duplicate-pattern rules;
4. cached parsing or proven hot-path bounds;
5. locale, Unicode, delimiter, wildcard, and malformed-config fixtures;
6. diagnostic exposure review for location/name/tag identity.

## Generic live quantity mutation

```yaml
blocker_id: tg.blocker.items-economy.quantity-live-reflective-write
status: research-blocker
source_path: mods/TaintedEconomy/src/Patches/QuantityLive.cs
source_blob: 22c556f6c427c87ce5fd39b06730307b57c78ef8
```

### Source behavior

- Selects the first writable `quantity`, `Quantity`, `count`, or `Count` field/property, including non-public members.
- Supports integral, floating, and decimal member types, but rounds through a whole-number decimal before conversion.
- Clamps all adjusted quantities to at least one.
- For value types, requires an owner and writable owner field for write-back.
- Writes the item member first, then writes the owner field when required. Those operations are not one transaction.
- Protects only items accepted by `ProtectedItemMutationGuard`.

### Blocking evidence

1. exact item-row schemas and ownership for every caller;
2. member-selection allowlist rather than fallback-name guessing;
3. atomic write/rollback across item and owner storage;
4. value-type, reference-type, nullable, floating, overflow, NaN, infinity, and setter-exception fixtures;
5. persistence timing and repeated-callback idempotence;
6. protected-item policy proven against canonical IDs;
7. combined mutation ordering with other loot/harvest mods.

## Protected-item mutation guard

```yaml
blocker_id: tg.blocker.items-economy.protected-item-heuristic
status: research-blocker
source_path: mods/TaintedEconomy/src/Patches/ProtectedItemMutationGuard.cs
source_blob: cf43455ef6c60fb7e9798bd52232d96a17562201
```

### Source behavior

- Preserves items when resolved flags indicate quest, important, hidden, or non-droppable state.
- Missing or unreadable flags are not treated as protected.
- Builds a normalized text identity from type, template GUID/name, display name, flags, tags, and quality.
- Preserves when that text contains selected item-name or `relic`/`artifact`/`unique`/`legendary` patterns.

### Blocking evidence

1. canonical immutable protected-item IDs and classes;
2. fail-closed behavior for unresolved protection data;
3. false-positive/false-negative tests across localized names and tags;
4. explicit policy for hidden, non-droppable, quest, unique, stackable, currency, and progression items;
5. evidence that the guard runs before every destructive removal or quantity mutation.

## Live reward-Wealth mutation

```yaml
blocker_id: tg.blocker.items-economy.reward-wealth-reflective-write
status: research-blocker
source_path: mods/TaintedEconomy/src/Patches/RewardWealthLive.cs
source_blob: f646f9759e3e9942c4366b59a1ff4971655f211a
```

### Source behavior

- Treats currency-type text containing `Wealth` as Wealth.
- Mutates only positive parsed amounts.
- Selects the first `amount` or `Amount` field/property, including non-public members.
- Supports `int`, `float`, `double`, and `decimal` targets.
- Writes the story-step object before the original `Execute` method runs.
- Preserves vanilla on unsupported type, overflow, below-one, unchanged, missing member, or caught exception.

### Blocking evidence

1. exact currency enum/type and exact story-step member identity;
2. prohibition of substring currency classification;
3. exact numeric type, rounding, and maximum policy;
4. rollback when original story execution fails or is skipped;
5. one-time versus reused step-instance lifecycle;
6. save, achievement, quest, analytics, and economy side effects;
7. ordering with other prefixes and story-step mutations.

## Shared disposition

All six selected helpers remain blocked as runtime guidance and supported extension surfaces. They may be documented and tested with synthetic fixtures, but they may not be promoted from caller evidence, historical builds, logs, or configuration defaults.

No blocker authorizes runtime reflection, game-object mutation, story execution, inventory/loot mutation, diagnostic capture, save access, deployment, or evidence promotion.
