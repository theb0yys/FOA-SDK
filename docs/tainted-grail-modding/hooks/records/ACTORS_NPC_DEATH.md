# Candidate Hook — NPC Non-Critical Death Completion

```yaml
hook_id: tg.hook.actors.npc-death-noncritical-functions
title: NPC non-critical death completion observer
status: candidate
hook_class: method-patch
domain: actors-and-encounters
purpose: observe completion of the native NPC non-critical death path and forward the NPC plus damage outcome to a mod-owned handler
profile:
  game_version: unknown
  branch: unknown
  runtime: Mono
  loader: BepInEx v5 Mono; exact version unknown
  framework: Harmony from the local BepInEx installation; exact version unknown
target:
  assembly: TG.Main
  namespace: Awaken.TG.Main.Fights.NPCs
  type: NpcElement
  member: DeathNonCriticalFunctions
  signature: unknown; source callback expects access to DamageOutcome
patch:
  style: postfix
  ordering: unspecified
  cleanup: Harmony.UnpatchSelf from the owning plug-in OnDestroy path
context:
  lifecycle_point: after the original DeathNonCriticalFunctions invocation
  thread: unknown
  frequency: event
  inputs: NpcElement instance and DamageOutcome supplied to the postfix
  outputs: call to ThreatEventHooks.OnNpcDeath
  side_effects: downstream threat or encounter effects are outside this source file and remain unreviewed
safety:
  nullability: no local null guard for the NPC instance or damage outcome
  failure_mode: fail-open with respect to original execution; downstream callback exception containment is unknown
  save_risk: unknown
  performance_risk: low structural frequency; runtime cost unverified
  story_risk: bounded but unverified
compatibility:
  known_conflicts: unknown; any other patch on the same method is relevant
  load_order: unspecified
  version_stability: unknown until exact-profile target resolution
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths:
    - mods/wyrd-hunt/src/Patches/NpcDeathPatch.cs
    - mods/wyrd-hunt/src/Plugin.cs
    - mods/wyrd-hunt/src/WyrdHunt.csproj
  source_blobs:
    NpcDeathPatch.cs: 7e4915918da28cdcb3b257ea46cf31ae2d427c0f
    Plugin.cs: 1b302b6d7b6d014c1c4a7a991698cc09b1a83b1b
    WyrdHunt.csproj: 390132066dddd79c7631f37c72616f74a68c0f88
  evidence_ids: none registered; source-only candidate
validation:
  static: source target and postfix shape extracted
  target_resolution: not performed against an exact current TG.Main assembly
  load: not established for an FOA-SDK supported profile
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline review, exact-profile signature verification, bounded diagnostics planning
  prohibited: runtime patch execution, behavior claims, save mutation, encounter mutation, evidence promotion
limitations:
  - The original method signature and return type are not established by this source file.
  - The downstream ThreatEventHooks.OnNpcDeath implementation must be reviewed before risk can be classified.
  - Historical source compilation or deployment does not prove this exact target on a current profile.
```

## Extracted source facts

The source declares a Harmony postfix for `NpcElement.DeathNonCriticalFunctions`. The postfix accepts the patched instance and a `DamageOutcome`, then forwards both to `ThreatEventHooks.OnNpcDeath`. The owning plug-in applies assembly-wide patches during `Awake` and calls `UnpatchSelf` during `OnDestroy`.

These are source-architecture facts. They do not establish the exact native signature, current assembly identity, runtime behavior, or compatibility.