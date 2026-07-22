# Canonical Source Fact — Avalon Companions API v1

```yaml
fact_id: tg.fact.mounts.avalon-companions-api-v1
status: source-verified
scope: pinned upstream source only
profile_id: foa-1.23.401-mono-bepinex5-tf-0.1.33
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_paths:
  - mods/avalon-companions/src/AvalonCompanionsInteropApi.cs
  - mods/avalon-companions/src/AvalonCompanions.csproj
  - mods/avalon-companions/src/Patches/PetCompanionController.cs
source_blobs:
  - 07c9aec4e2a32aca2e9922e07f1f13d0f7c17c7d
  - bb617d2df55f3e28d665696a06e75b7bee8dcd67
  - ede145c35db4dc7ed495c8055893dbe8693bfff6
```

## Exact source contract

The pinned project declares:

- assembly name: `AvalonCompanions`;
- package/source version: `0.2.17`;
- assembly version: `0.2.17.0`;
- target framework: `netstandard2.1`;
- loader family: BepInEx v5 Mono;
- API type: `AvalonCompanions.AvalonCompanionsInteropApi`;
- API version constant: `1`.

The public source signatures are:

```csharp
public static bool RegisterDialogueCommand(
    string ownerId,
    string commandId,
    string targetTemplateGuid,
    string label,
    Func<bool> canExecute,
    Action execute);

public static bool UnregisterDialogueCommands(string ownerId);
```

Both methods delegate to `PetCompanionController`.

## Evidence boundary

This fact verifies source identity and signatures only. It does not establish:

- a built `AvalonCompanions.dll` fingerprint;
- installation or live load on the pinned game profile;
- exact duplicate registration behavior;
- owner and command collision policy;
- cross-owner isolation;
- delegate lifetime or stale-callback handling;
- unregister return/exception semantics;
- load-order, reload, or combined-mod compatibility.

## Decision

The API source contract is accepted as `source-verified`. The Avalon Mounts adapter records remain `candidate`, and runtime registration/cleanup is **prohibited-current-source** until the Batch 004 API fixture passes against an exact built assembly and combined-mod harness.

Fixture: [`../../hooks/fixtures/batch-004-avalon-companions-api.json`](../../hooks/fixtures/batch-004-avalon-companions-api.json)

This fact grants no authority to load assemblies, register callbacks, mutate dialogue or mounts, deploy plug-ins, or promote evidence.
