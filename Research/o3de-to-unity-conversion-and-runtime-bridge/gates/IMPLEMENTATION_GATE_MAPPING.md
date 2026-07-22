# Implementation Gate Mapping

Baseline: `92aa29960bab92d646c464ae48b8cf09d881a436`, observed 20 July 2026.

Controlling design:
`docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md`.

This file maps research topics to the existing implementation order. It does not add, approve, reorder, or
satisfy an implementation gate.

## Entry prerequisite

Research and design review may continue. Gate 0 is the sole implementation exception before Phase 9: inert
Core-only contracts, canonical helpers, pure validation, focused tests, and repository boundary validation.
Every authority/performed flag remains false, result outcomes remain `NotAttempted`, and no service consumes
the contracts. Gates 1 and later begin only after ordered first-party capability items 1–7 stabilize and the
Phase 9 entry gate is accepted. The active first-party vertical-slice order remains unchanged.

## Controlling authoring and interchange gates

| Gate | Existing focused unit | Research mapped here | Authority still absent after the gate unless separately granted |
| ---: | --- | --- | --- |
| 0 | Core-only `ExternalToolHandoffV1`, `UnityConversionRequestV1`, `ExternalToolExecutionResultV1`, and `UnityConversionResultV1`; canonical fingerprints; pure validators; focused tests; boundary validation | Disabled envelope shapes and exact upstream bindings from the supplied report; later-gate qualification/source bindings remain optional and schema stays unselected | Services, registries, filesystem, processes, providers, Framework, Editor, conversion, build, deployment, runtime, and evidence promotion |
| 1 | Separately reviewed host process supervision, disabled by default, with no provider execution approved | Generic request/result shape, argument vectors, cancellation, locks, logs, environment, staging, output policy, synthetic process fixtures | Blender/Unity execution, game access, deployment, runtime |
| 2 | Descriptor-generated provider and command UI over existing registration/configuration | Generic progress and result presentation | Provider-specific logic in Editor, hidden execution |
| 3 | Source-artifact manifests and Asset Processor handoff contracts with candidate evidence return | Any generic `ExternalToolHandoff` concept and source-manifest binding | Interchange schema, process execution, publication on failure |
| 4 | Read-only provider inventory, typed qualification contracts, and Blender Provider Gem discovery | Blender application/add-on identities, compatibility claims, licence and provenance records | Add-on installation or Blender launch |
| 5 | Interchange schema 1, canonical identity, transformation, loss, lock, package validation, pure migration dispatch, public structural schema, and synthetic fixtures | Accepted Schema-1 research plus proposed `CANONICAL_INTERCHANGE_GATE_5_DESIGN.md` and its decisions addendum | Any implementation until the pending authority record is accepted; all services, filesystem, providers, native hosts, runtime, deployment, save, signing, publication, evidence promotion, and compatibility claims |
| 6 | Separately reviewed Blender native-extension execution through the host supervisor into isolated staging | Exact Blender command and extension behavior; removal/disablement of unsafe DCCsi bootstrap/legacy launch behavior | O3DE publication or Unity execution |
| 7 | Reviewed `SceneProcessing` activation/binding, O3DE handoff, qualified reverse export, and Blender round-trip evidence | Asset Processor isolation, reverse export, deterministic/tolerance/loss fixtures | Unity provider or runtime behavior |
| 8 | Unity Provider Gem plus editor-only Unity interchange package for synthetic test projects | Unity Editor profile, final provider/package IDs, dedicated versus user-owned project model, GUID binding, package lock, editor-only assembly boundary | Unity process launch, runtime adapter build, game project access |
| 9 | Separately reviewed Unity native-extension execution through the host supervisor | Exact batch invocation, executable interrogation, project locks, import/build result envelope, cleanup in contained project/staging | Game deployment, BepInEx/Harmony, FoA API calls |
| 10 | O3DE-to-Unity and Unity-to-O3DE qualification, compatibility matrices, and loss reports | Cross-host identities, round trips, numeric tolerances, byte/canonical determinism classes | Proprietary FoA project compatibility or runtime support |
| 11 | Validators, public schemas, headless validation, documentation, and exact-head UI evidence | Public contract publication, negative fixtures, supported matrix, exact batch-versus-headless statements | Live IPC, runtime execution |
| 12 | Separately reviewed structured IPC, hot-loading, signing, and third-party trust research | Optional IPC while durable file documents remain authoritative; authenticity/signing research | Runtime-facing IPC or permission inherited from authoring IPC |
| 13 | Separately reviewed FoA mapping and runtime-adapter payload extensions | Target-profile schema, runtime payload mapping, and runtime-adapter design candidates | Automatic game discovery, build, deployment, game launch, mutation, or save access |

Every focused unit still requires its own design review when it introduces a durable schema, dependency,
process execution, licence obligation, runtime-facing behavior, or different security boundary.

## Active Gate 5 design state

The Schema-1 research conclusion is merged. The proposed design package is:

```text
docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN.md
docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_DESIGN_DECISIONS.md
docs/tainted-grail-sdk/CANONICAL_INTERCHANGE_GATE_5_CURRENT_MAIN_RECONCILIATION_2026-07-22.md
Research/o3de-to-unity-conversion-and-runtime-bridge/gates/GATE_5_IMPLEMENTATION_AUTHORITY_DECISION.md
```

The authority decision remains pending with Phase 9 entry, design acceptance, and Gate 5 implementation all
false. Merging design documents or passing documentation validation does not authorize source implementation.

## Supplied-report crosswalk

| Supplied report proposal | Correct placement | Reconciliation |
| --- | --- | --- |
| Durable generic outer handoff and process-result shapes | Gate 0 for disabled Core contracts; Gates 1 and 3 for any consumer or operation | Bind, do not replace, the interchange package; preserve non-executable plan/build flags |
| Unity-specific request/result shapes | Gate 0 for disabled Core contracts; Gates 8 and 9 for Unity provider/execution | Keep them Editor-interchange-only; no BepInEx, runtime-target, deployment, or game authority |
| Version-locked Unity conversion project and editor-only package | Gate 8 candidate | Exact project model, IDs, licence, package lock, `.meta`/GUID lifecycle, and output roots remain unqualified |
| `-batchmode`/`-executeMethod` execution | Gate 9 candidate | Exact selected Unity version must qualify flags, error behavior, cancellation, logs, and exit results |
| One-way FOA-SDK-to-Unity conversion | Gates 8–10 | Program acceptance also requires reverse Unity-to-O3DE behavior and complete loss evidence |
| C# deterministic compilation | Gate 9 or later runtime build gate, depending on output | Does not prove deterministic Unity imports or complete packages |
| BepInEx adapter compilation | After Gate 13 in the runtime lane | Must not be folded into the editor-only Unity extension |
| Manual local deployment and no-op plugin load | After Gate 13 and the existing deployment/executor gates | Not part of the first authoring slice and not authorized by research |
| AssetBundle/Addressables generation | Later runtime/asset design after exact target evidence | Remains absent from the first Unity authoring slice |
| Live preview, hot reload, or IPC | Gate 12 for authoring research; Gate 13 plus runtime review when game-facing | File-backed documents remain authoritative |
| Mono-first or IL2CPP implementation | Runtime-target decision after exact lawful evidence | A separately governed Mono package does not select a Gate 5 host or grant authoring-interchange authority |

## Runtime re-entry after Gate 13

Gate 13 does not itself permit runtime execution. A later runtime program must re-enter the already governed
pipeline in this order:

1. separately authorized, bounded exact-target evidence capture;
2. one exact Mono or IL2CPP target decision and adapter design;
3. synthetic/mock-host adapter build proof with lawful dependencies;
4. exact work-order and build-manifest binding, retaining current non-executable flags;
5. reviewed package assembly/inventory and applicable signing evidence;
6. staging/deployment preview against a reviewed target inventory;
7. explicit deployment confirmation, maintenance window, preflight evidence, and canonical work order;
8. separately approved executor with exact backup, addition, replacement, removal, and rollback coverage;
9. separately approved game launch or no-op load observation;
10. independent post-deployment verification and candidate evidence return;
11. separately reviewed bounded runtime capability;
12. independently gated save or persistence behavior, if ever proposed.

Success at one step grants no later authority. A successful compile, Unity exit code, file copy, plugin load, or
log entry does not establish compatibility, permission, safe mutation, cleanup, or rollback.

## Explicitly rejected reorderings

- Unity implementation does not bypass Blender qualification and the Blender/O3DE round trip.
- A no-op BepInEx plugin does not replace the synthetic process-supervisor fixture.
- A generic execution envelope does not turn `ExecutionAllowed: false` or `BuildAllowed: false` into authority.
- Provider discovery does not run Unity, Blender, package managers, registry searches, or game scans.
- Host-side worker cleanup does not own game deployment rollback.
- A runtime-adapter package or installer execution handoff does not authorize Gate 5 or alter its Core-only scope.
- Research approval does not authorise Gate 0, satisfy Gate 0 acceptance, satisfy Phase 9 entry, or satisfy any
  later implementation gate. Gate 0 authority comes only from the normative design amendment.