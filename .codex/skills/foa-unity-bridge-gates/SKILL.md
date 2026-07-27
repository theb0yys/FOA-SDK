---
name: foa-unity-bridge-gates
description: Use for Unity provider discovery, external toolchain execution, canonical handoff, Unity batch conversion, candidate evidence, BepInEx Mono or IL2CPP adapters, exact game profiles, runtime integration, or deployment review.
---

# FOA-SDK Unity Bridge Gates

## Research To Read

Read architecture, external toolchain and Unity handoff research, adapter contracts, exact-profile evidence, packaging/deployment review docs, and current tests.

## Boundary

FOA-SDK/O3DE owns neutral authoring data, reviewed work orders, validation, and candidate evidence. Unity owns Unity-native imports, metadata, GUIDs, and generated native outputs. Fall of Avalon owns final runtime interpretation. Only the documented provider/conversion/adapter layers may touch those surfaces.

## Gate Order

1. Exact tool/runtime discovery.
2. Canonical handoff validation.
3. External-process supervision.
4. Unity import/conversion.
5. Candidate-result evidence return.
6. Adapter contract/build.
7. Packaging preview.
8. Explicit deployment review.
9. Exact-install runtime proof.

Do not promote a later gate because an earlier gate mostly works. Evidence return never auto-promotes authority.

## Hard Stops

Stop if Unity-native files are authored outside Unity; O3DE directly mutates runtime; Mono/IL2CPP are conflated; exact profile is unknown; external process is unbounded; candidate evidence is auto-promoted; deployment lacks explicit approval/rollback; or runtime proof lacks exact evidence.

## Validation

Run canonical serialization, malformed input, deterministic work-order, process timeout/cancellation, Unity batch, candidate evidence, adapter build, packaging, and exact-profile tests required by the layer.

## Runtime Proof

Without lawful exact-install evidence, state `runtime sign-off not performed`.
