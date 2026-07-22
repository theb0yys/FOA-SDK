# Verified Runtime and Loader Profiles

## Purpose

A modding instruction is valid only for the exact game, branch, runtime, loader, framework and package profile that produced its evidence. Mono and IL2CPP are separate routes. A result on one route must never be projected onto the other.

This page records the currently pinned FOA-SDK observations. It does not select or inspect an operator's active installation and does not grant build, deployment, game-launch or runtime authority.

## Current pinned observations

| Profile ID | Game | Unity | Runtime route | Loader | Framework | Evidence state | Permitted handbook use |
|---|---|---|---|---|---|---|---|
| `foa-1.23.401-mono-bepinex5-tf-0.1.33` | `1.23.401` | `6000.0.64f1` | Mono | BepInEx `5.4.23.3` | Tainted Framework `0.1.33` | `HostLiveLoadValidated` | Explain the pinned Mono route, local project references, load-log checks and framework-compatible planning. |
| `foa-1.23.401-il2cpp-bepinex6-tf-0.1.36` | `1.23.401` | `6000.0.64f1` | IL2CPP | BepInEx `6.0.0-be.735` | Tainted Framework `0.1.36` | `PackageInstallValidated` | Explain the pinned package/install route and why IL2CPP requires a distinct adapter and build/package chain. |

The evidence states are deliberately different. The IL2CPP row is not represented as live behavior validation. The Mono row does not authorize use on IL2CPP.

## Active-install verification

Before using either route, an operator must establish a local profile record containing:

- exact game version;
- distribution and branch;
- Unity version where observed;
- runtime kind: `Mono` or `IL2CPP`;
- loader name, version and source;
- framework name and version, when used;
- game executable and managed/native layout observations;
- exact mod package identity;
- installation timestamp supplied by the operator;
- load-log fingerprint or approved redacted evidence reference;
- compatibility notes and known conflicts.

The handbook must not infer the active route from a folder name alone.

## Mono route

The pinned Mono development model uses:

- a C# class-library mod;
- `netstandard2.1` for the current upstream template baseline;
- local compile-time references to the operator's BepInEx, Unity and game-managed assemblies;
- BepInEx 5 plug-in lifecycle;
- Harmony/HarmonyX patching where a verified target is required;
- deployment beneath the applicable BepInEx plug-in directory;
- `BepInEx/LogOutput.log` or the exact configured log target for load confirmation.

### Mono constraints

- Local game and loader assemblies are references only and must never be committed or packaged.
- A successful compile is not proof that the plug-in loads.
- A load message is not proof that a patch target resolved or behaved correctly.
- Reflection and Harmony target records must include exact signatures and failure behavior.
- Unpatch, event unsubscription and object cleanup must be designed before release.

## Batch 004 fixture binding

Semantic Hook Batch 004 binds four fixture manifests to `foa-1.23.401-mono-bepinex5-tf-0.1.33`:

- [economy reflection and mutation](../hooks/fixtures/batch-004-economy-profile.json);
- [diagnostic writer](../hooks/fixtures/batch-004-diagnostic-writer.json);
- [wolf native mount rollback](../hooks/fixtures/batch-004-wolf-mount-rollback.json);
- [Avalon Companions API v1](../hooks/fixtures/batch-004-avalon-companions-api.json).

The profile supplies exact game, Unity, runtime, loader and framework identity. It does **not** supply the missing `TG.Main.dll`, `BepInEx.dll`, `0Harmony.dll`, optional-mod assembly, target-resolution, behavior, rollback, diagnostics, or combined-mod fingerprints required by those manifests.

All four fixture sets therefore remain `specification-only`. Binding a manifest to this profile does not upgrade a hook, helper, controller, or adapter. The current decisions are recorded in [Batch 004 promotion and prohibition decisions](../hooks/decisions/BATCH_004_PROMOTION_PROHIBITION.md).

## IL2CPP route

The pinned IL2CPP route is a separate package and adapter concern. It may require generated interop material, IL2CPP-specific loader APIs, different reflection/type lookup, and different package layout.

### IL2CPP constraints

- Do not reuse a Mono DLL or Mono-only target record.
- Do not claim BepInEx 5 instructions for the BepInEx 6 route.
- Do not infer method/type availability from managed Mono assemblies.
- Interop generation, adapter compilation, package assembly, deployment and live execution remain separately reviewed capabilities.
- Current pinned evidence supports package-install planning, not a general live-behavior guarantee.
- Batch 004's Mono fixture manifests must not be projected onto this route.

## Profile status vocabulary

| Status | Meaning |
|---|---|
| `Observed` | Identity or layout was recorded, but no load behavior was proved. |
| `PackageInstallValidated` | The reviewed package installed or was qualified for the exact profile. |
| `HostLiveLoadValidated` | The loader/framework was observed loading on the exact host profile. |
| `TargetResolved` | A named hook target resolved with the recorded signature. |
| `BehaviorValidated` | The intended behavior was exercised and observed. |
| `CompatibilityReviewed` | Conflicts, ordering, cleanup and supported scope were reviewed. |
| `Stale` | A game, loader, framework or evidence change invalidated the profile. |
| `Blocked` | Required identity, licence, source, package or behavior evidence is missing or adverse. |

## Compatibility rule

Every public hook, tutorial and example must name one or more supported profile IDs or state `profile-unverified`. It must also state:

- minimum evidence state;
- unsupported routes;
- known conflicts;
- stale conditions;
- cleanup and rollback expectations.

## Prohibited inference

The following are invalid:

- "works on Tainted Grail" without an exact profile;
- "BepInEx compatible" without a major/version and runtime route;
- "Unity 6 compatible" as a substitute for exact game target evidence;
- projecting a target signature between Mono and IL2CPP;
- treating package installation as behavior verification;
- treating a historical mod release manifest as current compatibility proof;
- treating a fixture specification as an executed fixture result;
- treating host live-load validation as a game-target, mutation, cleanup, or combined-mod result.
