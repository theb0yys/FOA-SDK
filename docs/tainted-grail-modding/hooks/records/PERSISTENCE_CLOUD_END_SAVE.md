# Candidate Hook Family — Cloud Service EndSave

The source enumerates four concrete cloud-service types and resolves `EndSave(string)` on each. Each type receives its own hook ID because compatibility and target availability may differ by platform or game build.

## Steam cloud service

```yaml
hook_id: tg.hook.persistence.steam-cloud-end-save
title: Steam cloud save completion observer
status: candidate
hook_class: method-patch
domain: saves-and-persistence
purpose: observe SteamCloudService EndSave completion and enqueue the completed slot identifier for a mod-owned backup workflow
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Saving.Cloud.Services
  type: SteamCloudService
  member: EndSave
  signature: EndSave(string); return type unknown
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the selected cloud service ends a save operation
  thread: unknown
  frequency: event
  inputs: string slotId
  outputs: Plugin.NotifySaveSlotWritten(slotId)
  side_effects: queues a slot identifier; downstream code may read slot files and write a plugin-owned ZIP backup
safety:
  nullability: blank slot identifiers are ignored by the downstream queue method
  failure_mode: fail-open target discovery; unresolved targets are omitted; callback exception containment is unknown
  save_risk: read, with high operational sensitivity around save completion and external backup creation
  performance_risk: medium
  story_risk: none directly; restore/use policy remains outside this hook
compatibility:
  known_conflicts: other EndSave patches, cloud-service replacements, and save lifecycle mods
  load_order: unspecified
  version_stability: unknown until exact-profile resolution and behavior validation
validation:
  static: exact type name, member name, one-string parameter lookup, and postfix extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline signature verification, bounded save-lifecycle diagnostics planning
  prohibited: save access, backup creation, restore operations, deployment, runtime execution, evidence promotion
limitations:
  - Return type, visibility, and exact native lifecycle guarantees are unresolved.
  - Slot completion ordering versus disk/cloud flush is not proven.
  - The downstream backup implementation must be reviewed and tested independently.
```

## Steam no-cloud service

```yaml
hook_id: tg.hook.persistence.steam-no-cloud-end-save
title: Steam no-cloud save completion observer
status: candidate
hook_class: method-patch
domain: saves-and-persistence
purpose: observe SteamNoCloudService EndSave completion and enqueue the completed slot identifier for a mod-owned backup workflow
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Saving.Cloud.Services
  type: SteamNoCloudService
  member: EndSave
  signature: EndSave(string); return type unknown
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the selected no-cloud service ends a save operation
  thread: unknown
  frequency: event
  inputs: string slotId
  outputs: Plugin.NotifySaveSlotWritten(slotId)
  side_effects: queues a slot identifier; downstream code may read slot files and write a plugin-owned ZIP backup
safety:
  nullability: blank slot identifiers are ignored by the downstream queue method
  failure_mode: fail-open target discovery; unresolved targets are omitted; callback exception containment is unknown
  save_risk: read, with high operational sensitivity around save completion and external backup creation
  performance_risk: medium
  story_risk: none directly; restore/use policy remains outside this hook
compatibility:
  known_conflicts: other EndSave patches, cloud-service replacements, and save lifecycle mods
  load_order: unspecified
  version_stability: unknown until exact-profile resolution and behavior validation
validation:
  static: exact type name, member name, one-string parameter lookup, and postfix extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline signature verification, bounded save-lifecycle diagnostics planning
  prohibited: save access, backup creation, restore operations, deployment, runtime execution, evidence promotion
limitations:
  - Return type, visibility, and exact native lifecycle guarantees are unresolved.
  - Slot completion ordering versus disk flush is not proven.
  - The downstream backup implementation must be reviewed and tested independently.
```

## Debug cloud service

```yaml
hook_id: tg.hook.persistence.debug-cloud-end-save
title: Debug cloud save completion observer
status: candidate
hook_class: method-patch
domain: saves-and-persistence
purpose: observe DebugCloudService EndSave completion and enqueue the completed slot identifier for a mod-owned backup workflow
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Saving.Cloud.Services
  type: DebugCloudService
  member: EndSave
  signature: EndSave(string); return type unknown
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the debug cloud service ends a save operation
  thread: unknown
  frequency: event
  inputs: string slotId
  outputs: Plugin.NotifySaveSlotWritten(slotId)
  side_effects: queues a slot identifier; downstream code may read slot files and write a plugin-owned ZIP backup
safety:
  nullability: blank slot identifiers are ignored by the downstream queue method
  failure_mode: fail-open target discovery; unresolved targets are omitted; callback exception containment is unknown
  save_risk: read, with high operational sensitivity around save completion and external backup creation
  performance_risk: medium
  story_risk: none directly; restore/use policy remains outside this hook
compatibility:
  known_conflicts: other EndSave patches, service substitutions, and save lifecycle mods
  load_order: unspecified
  version_stability: unknown until exact-profile resolution and behavior validation
validation:
  static: exact type name, member name, one-string parameter lookup, and postfix extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline signature verification, bounded save-lifecycle diagnostics planning
  prohibited: save access, backup creation, restore operations, deployment, runtime execution, evidence promotion
limitations:
  - Availability and production relevance of this debug implementation are unresolved.
  - Return type, visibility, and exact native lifecycle guarantees are unresolved.
  - The downstream backup implementation must be reviewed and tested independently.
```

## GOG cloud service

```yaml
hook_id: tg.hook.persistence.gog-cloud-end-save
title: GOG cloud save completion observer
status: candidate
hook_class: method-patch
domain: saves-and-persistence
purpose: observe GogCloudService EndSave completion and enqueue the completed slot identifier for a mod-owned backup workflow
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Saving.Cloud.Services
  type: GogCloudService
  member: EndSave
  signature: EndSave(string); return type unknown
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the selected GOG cloud service ends a save operation
  thread: unknown
  frequency: event
  inputs: string slotId
  outputs: Plugin.NotifySaveSlotWritten(slotId)
  side_effects: queues a slot identifier; downstream code may read slot files and write a plugin-owned ZIP backup
safety:
  nullability: blank slot identifiers are ignored by the downstream queue method
  failure_mode: fail-open target discovery; unresolved targets are omitted; callback exception containment is unknown
  save_risk: read, with high operational sensitivity around save completion and external backup creation
  performance_risk: medium
  story_risk: none directly; restore/use policy remains outside this hook
compatibility:
  known_conflicts: other EndSave patches, cloud-service replacements, and save lifecycle mods
  load_order: unspecified
  version_stability: unknown until exact-profile resolution and behavior validation
validation:
  static: exact type name, member name, one-string parameter lookup, and postfix extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline signature verification, bounded save-lifecycle diagnostics planning
  prohibited: save access, backup creation, restore operations, deployment, runtime execution, evidence promotion
limitations:
  - Return type, visibility, and exact native lifecycle guarantees are unresolved.
  - Slot completion ordering versus disk/cloud flush is not proven.
  - The downstream backup implementation must be reviewed and tested independently.
```

## Shared evidence

- source repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- source commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- source path: `mods/smart-save-backups/src/Patches/CloudServiceEndSavePatch.cs`
- source blob: `29aa7491292de7faa8e6d8df2662d51201b2296d`
- registration and downstream owner: `mods/smart-save-backups/src/Plugin.cs`
- project source: `mods/smart-save-backups/src/SmartSaveBackups.csproj`
- evidence IDs: none registered; source-only candidates

## Extracted source facts

The target enumerator resolves each type by full name, then requests `EndSave` with exactly one `string` parameter. Missing types or methods are silently omitted from the enumerable target set. The postfix forwards the slot identifier to a concurrent queue. The owning plug-in drains that queue during `Update`; downstream code may read the completed slot and create a plugin-owned ZIP archive.

Because this source sits on a save-completion boundary, source plausibility is nowhere near enough. Exact flush ordering, failure behavior, duplicate notifications, platform variants, save-mod collisions, and rollback must be proven before promotion.