# Candidate Adapter Family — Avalon Companions API v1 Dialogue Bridge

The selected controller is called once per frame, but it attempts resolution only when unregistered and at least two unscaled seconds have elapsed since the previous attempt. Batch 004 verifies the optional API's exact pinned **source** identity: assembly source name `AvalonCompanions`, source version `0.2.17`, assembly version `0.2.17.0`, API version `1`, and the two public static signatures. No built assembly fingerprint or live resolution is claimed.

## Register dialogue command

```yaml
hook_id: tg.hook.mounts.avalon-companions-register-dialogue-command
title: Avalon Companions dialogue command registration adapter
status: candidate
hook_class: adapter
domain: mounts-companions-dialogue-and-interop
purpose: register a wolf Mount / Dismount dialogue command with the optional Avalon Companions interop API
profile:
  game_version: 1.23.401
  branch: unknown
  runtime: Mono
  loader: BepInEx 5.4.23.3 host profile; plug-in load not established
  framework: AvalonCompanions 0.2.17 source, assembly version 0.2.17.0, API v1; built assembly fingerprint absent
target:
  assembly: AvalonCompanions source project; exact built AvalonCompanions.dll fingerprint absent
  namespace: AvalonCompanions
  type: AvalonCompanionsInteropApi
  member: RegisterDialogueCommand
  signature: public static bool RegisterDialogueCommand(string ownerId, string commandId, string targetTemplateGuid, string label, Func<bool> canExecute, Action execute)
patch:
  style: resolve
  ordering: resolution retries no more often than every two unscaled seconds until registered
  cleanup: paired owner-scoped unregistration is attempted on gate loss and controller disposal
context:
  lifecycle_point: optional interop discovery after plug-in startup
  thread: Unity main thread expected because Tick is called from Plugin.Update; unverified
  frequency: frame check with two-second retry throttle; one registration attempt per retry window
  inputs: loaded assembly set, exact API type name, exact method signature, fixed owner/command/template/label, gate delegate, execution delegate
  outputs: registered bool state
  side_effects: installs an external dialogue command and retains the reflected unregister MethodInfo
safety:
  nullability: absent type or methods skips registration; reflection invocation exceptions are caught
  failure_mode: fail-open
  save_risk: unknown
  performance_risk: medium
  story_risk: bounded
compatibility:
  known_conflicts: command ID or owner collisions, incompatible Avalon Companions API versions, duplicate registrations, assembly reload/unload, other wolf dialogue providers
  load_order: optional API may load before or after Avalon Mounts; retry loop is intended to accommodate late availability
  version_stability: source API is exact at v1/0.2.17; runtime stability remains low without a built fingerprint and behavior evidence
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths:
    - mods/avalon-mounts/src/WolfMount/AvalonCompanionsDialogueBridge.cs
    - mods/avalon-mounts/src/Plugin.cs
    - mods/avalon-mounts/src/Safety/DiagnosticsOnlyGuard.cs
    - mods/avalon-companions/src/AvalonCompanionsInteropApi.cs
    - mods/avalon-companions/src/AvalonCompanions.csproj
  source_blobs:
    - cc1b0165303e3bf1de09d5ac249820e70566afe9
    - 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
    - 5609bcff8082d362bdf6356a224d01e291e88f20
    - 07c9aec4e2a32aca2e9922e07f1f13d0f7c17c7d
    - bb617d2df55f3e28d665696a06e75b7bee8dcd67
  evidence_ids: []
validation:
  static: exact API v1 source type, assembly source identity, source/assembly version, method signature, fixed identifiers, retry interval, and bool-result interpretation verified
  target_resolution: not performed against a fingerprinted AvalonCompanions.dll
  load: not established
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation, offline API compatibility fixtures, synthetic reflection and state-machine tests
  prohibited: loading third-party assemblies, runtime registration, dialogue mutation, mount execution, deployment, evidence promotion
limitations:
  - Duplicate registration, owner isolation, bool return semantics, delegate lifetime, and stale-callback behavior are not independently verified.
  - AppDomain assembly enumeration and LINQ allocation occur on each retry until success.
  - A successful registration is trusted from a bool result; no query verifies ownership afterward.
  - Registration itself has no known save write, but the supplied execution delegate reaches the separately prohibited wolf-seat controller.
```

## Unregister dialogue commands

```yaml
hook_id: tg.hook.mounts.avalon-companions-unregister-dialogue-commands
title: Avalon Companions dialogue command cleanup adapter
status: candidate
hook_class: adapter
domain: mounts-companions-dialogue-and-interop
purpose: remove commands owned by Avalon Mounts when the runtime gate closes or the controller is disposed
profile:
  game_version: 1.23.401
  branch: unknown
  runtime: Mono
  loader: BepInEx 5.4.23.3 host profile; plug-in load not established
  framework: AvalonCompanions 0.2.17 source, assembly version 0.2.17.0, API v1; built assembly fingerprint absent
target:
  assembly: AvalonCompanions source project; MethodInfo expected from the loaded type that supplied registration
  namespace: AvalonCompanions
  type: AvalonCompanionsInteropApi
  member: UnregisterDialogueCommands
  signature: public static bool UnregisterDialogueCommands(string ownerId)
patch:
  style: resolve
  ordering: invoked when gateAvailable becomes false or Dispose is called, but only when local registered state is true
  cleanup: local registered state and retained MethodInfo are cleared in finally even when invocation fails
context:
  lifecycle_point: gate loss and plug-in/controller teardown
  thread: Unity main thread expected but unverified
  frequency: event
  inputs: fixed owner ID kane.tgfoa.avalon-mounts
  outputs: external owner-scoped command removal attempt; returned bool is ignored by the bridge
  side_effects: mutates external dialogue-command registration state
safety:
  nullability: null MethodInfo results in no invocation; local state is still cleared
  failure_mode: fail-open
  save_risk: none
  performance_risk: low
  story_risk: bounded
compatibility:
  known_conflicts: owner-ID reuse, external API unload/reload, stale MethodInfo, unregister semantics broader or narrower than expected
  load_order: cleanup depends on the target assembly and API remaining invokable
  version_stability: source API is exact at v1/0.2.17; runtime stability remains low
evidence:
  source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
  source_commit: d7e740e7f167b73152b53409e483dab07d80d048
  source_paths:
    - mods/avalon-mounts/src/WolfMount/AvalonCompanionsDialogueBridge.cs
    - mods/avalon-mounts/src/Plugin.cs
    - mods/avalon-companions/src/AvalonCompanionsInteropApi.cs
    - mods/avalon-companions/src/AvalonCompanions.csproj
  source_blobs:
    - cc1b0165303e3bf1de09d5ac249820e70566afe9
    - 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
    - 07c9aec4e2a32aca2e9922e07f1f13d0f7c17c7d
    - bb617d2df55f3e28d665696a06e75b7bee8dcd67
  evidence_ids: []
validation:
  static: exact API v1 source signature, cleanup trigger, invocation, return-value mismatch, exception handling, and local finally-state verified
  target_resolution: not performed against a fingerprinted AvalonCompanions.dll
  load: not established
  behavior: not established
  combined_mods: not established
permissions:
  allowed: documentation and synthetic cleanup fixtures
  prohibited: runtime API invocation, dialogue mutation, deployment, evidence promotion
limitations:
  - Local state is cleared even when external unregistration fails, so the controller cannot retry cleanup from that state.
  - The source API returns bool, but the bridge ignores it; successful external removal is not confirmed.
  - Exact owner scoping, collision behavior, delegate release, and stale-callback behavior require API-owner evidence.
```

## Shared source behavior

The bridge does not directly mount or dismount. It supplies `canExecute` and `execute` delegates owned by the Avalon Mounts plug-in. The execution delegate rechecks the wolf-seat runtime gate before calling the seat controller.

## Batch 004 decision

- Canonical source fact: [`Avalon Companions API v1`](../../systems/mounts/AVALON_COMPANIONS_API_V1_FACT.md).
- Exact-profile fixture specification: [`batch-004-avalon-companions-api.json`](../fixtures/batch-004-avalon-companions-api.json).
- Decision register: [`Batch 004 promotion and prohibition decisions`](../decisions/BATCH_004_PROMOTION_PROHIBITION.md).

Both adapter records remain `candidate`. Runtime registration and cleanup are `prohibited-current-source` until a fingerprinted built assembly, exact resolution, duplicate/owner-isolation behavior, gate-race behavior, load-order/reload behavior, and retryable cleanup all pass the reviewed fixture.
