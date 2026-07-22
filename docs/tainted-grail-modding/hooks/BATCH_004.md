# Semantic Hook Batch 004

## Identity

- upstream repository: `theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods`
- upstream commit: `d7e740e7f167b73152b53409e483dab07d80d048`
- upstream root licence: `NOASSERTION`
- exact handbook profile: `foa-1.23.401-mono-bepinex5-tf-0.1.33`
- profile evidence state: `HostLiveLoadValidated`
- fixture execution state: `specification-only`
- authority: documentation, offline fixture specification, and fail-closed decision records only

Batch 004 converts the Batch 003 high-risk surfaces into profile-bound fixture manifests and explicit promotion-or-prohibition decisions. It does not treat a host-load profile, upstream source match, configuration gate, source compile, or synthetic fixture design as runtime behavior evidence.

## Selected source and profile inputs

| Input | Blob or identity | Use |
|---|---|---|
| `runtime/VERIFIED_PROFILES.md` | `foa-1.23.401-mono-bepinex5-tf-0.1.33` | exact profile binding and evidence ceiling |
| `EconomyReflection.cs` | `4697cb7cd98297d96569626c6b0bb3f0c6d3cc69` | reflection fixture and prohibition |
| `QuantityLive.cs` | `22c556f6c427c87ce5fd39b06730307b57c78ef8` | atomic quantity fixture and prohibition |
| `RewardWealthLive.cs` | `f646f9759e3e9942c4366b59a1ff4971655f211a` | story-step rollback fixture and prohibition |
| `VendorPriceLivePricing.cs` | `5e7409cf644e06e353d2b306b5920b19ef1e0d00` | overload, identity, classification, rounding, and ordering fixture |
| `RegionalVendorPricePricing.cs` | `c0d4ea116c7fbd74f1a7ed5c82a1a2ea0b09278e` | canonical merchant identity, precedence, cache, and Unicode fixture |
| `EconomyDiagnosticWriter.cs` | `020dd5c48f691690b5854446e56c0a5f4051bc3b` | path, cap, atomicity, redaction, and retention fixture |
| `EconomyDiagnosticRow.cs` | `6e0711c180db1a7c0e464914b0854987d5f4a840` | exact output-field inventory |
| `WolfMountSeatBridgeController.cs` | `c226dead88329eb55f3444f36c6fceecc2ed7e66` | frame/attach ownership fixture |
| `WolfNativeMountContract.cs` | `65a52e309da76d5bfd9b5627ee92def688031070` | object-graph and rollback fixture |
| `AvalonCompanionsDialogueBridge.cs` | `cc1b0165303e3bf1de09d5ac249820e70566afe9` | optional API consumer and cleanup fixture |
| `AvalonCompanionsInteropApi.cs` | `07c9aec4e2a32aca2e9922e07f1f13d0f7c17c7d` | exact API v1 source contract |
| `AvalonCompanions.csproj` | `bb617d2df55f3e28d665696a06e75b7bee8dcd67` | `AvalonCompanions` version `0.2.17` source identity |
| `PetCompanionController.cs` | `ede145c35db4dc7ed495c8055893dbe8693bfff6` | API delegation owner and combined-mod research input |

## Fixture deliverables

Batch 004 adds four JSON manifests under [`fixtures/`](fixtures/README.md):

1. economy reflection and mutation;
2. diagnostics writer and support output;
3. wolf native mount rollback;
4. Avalon Companions API v1 compatibility.

The manifests bind every case to the exact profile and pinned source, but require runtime assembly fingerprints that are not present in the repository. They therefore fail closed as `specification-only` and create no runtime evidence.

## Deterministic adverse findings

### Diagnostics output

The source contains two path-containment defects:

- configured roots are accepted with a prefix-only `StartsWith` test, so a sibling path whose text begins with the config-root text can pass;
- the session segment replaces invalid filename characters but does not reject `.` or `..`, allowing `Path.Combine` to resolve outside the intended diagnostics session directory.

The write sequence is also non-atomic: a CSV row can be appended before summary writing fails. With BepInEx row logging disabled, that durable row is not counted, so repeated attempts can exceed the intended row cap. There is no field, row, or session byte cap, no redaction policy, no retention/deletion contract, and no spreadsheet-formula neutralization.

### Economy mutation

- `QuantityLive` writes the selected item member before optional owner write-back; a later failure can leave partial state.
- `RewardWealthLive` classifies currency by substring and mutates a story-step object before native execution without rollback.
- vendor pricing depends on unresolved Harmony argument positions, exact runtime type-name equality, reflected string classification, first-match regional substrings, and per-evaluation rule parsing.
- generic reflection invokes non-public members without an exact-profile allowlist and aborts later fallbacks after a throwing getter.

### Wolf native mount rollback

The selected source can discard native actions, create and destroy Unity/model objects, write private fields, share native mount data, disable movement components by type-name heuristics, and change hero mount ownership. A previously null owned-mount state is not explicitly restored, discarded native actions are not reconstructed, and cleanup is not one transactional object-graph rollback.

### Avalon Companions API

The pinned source establishes:

```text
assembly source name: AvalonCompanions
source version: 0.2.17
assembly version: 0.2.17.0
API version constant: 1
RegisterDialogueCommand(string,string,string,string,Func<bool>,Action) -> bool
UnregisterDialogueCommands(string) -> bool
```

This is promoted only as a canonical **source API fact**. The adapter hook records remain candidates because no built `AvalonCompanions.dll` fingerprint, live resolution, duplicate/owner-isolation behavior, gate-race observation, load-order test, or reliable cleanup receipt exists. The bridge also clears its local registration state even when external unregistration fails.

## Decisions

The authoritative decision register is [`decisions/BATCH_004_PROMOTION_PROHIBITION.md`](decisions/BATCH_004_PROMOTION_PROHIBITION.md).

Batch result:

- exact-profile fixture manifests: 4;
- specified deterministic fixture cases: 33;
- canonical source facts added: 2;
- hook promotions: 0;
- current-source prohibitions: 7;
- permanent prohibitions: 0;
- executed runtime fixtures: 0;
- evidence promotions: 0.

`prohibited-current-source` means the reviewed implementation may not be presented as supported runtime guidance or used to promote a hook. It can be reopened by a new reviewed implementation plus the named fixture evidence. It is not a claim that the underlying modding goal is permanently impossible.

## Scope boundary

This batch performs no game inspection, assembly loading, dependency download, build, deployment, game launch, runtime reflection, Harmony registration, price/quantity/story mutation, diagnostics capture, filesystem write, mount conversion, dialogue registration, save access, publication, catalog mutation, or evidence promotion. It commits no binaries, game content, logs, saves, private paths, or decompiled game source.

## Next documented unit

Semantic Hook Batch 005 should be a source-repair design unit, not a promotion pass:

1. replace generic mutation with exact typed adapters and transactional rollback;
2. harden diagnostic path containment, session validation, atomic publication, byte caps, redaction, retention, and safe export;
3. redesign wolf mount conversion as a staged transaction with a complete inverse operation;
4. version the Avalon Companions API contract and make unregister failure retryable;
5. define a project-owned offline runner for the four Batch 004 manifests without adding runtime authority.
