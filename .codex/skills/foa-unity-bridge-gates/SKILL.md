---
name: foa-unity-bridge-gates
description: Use for Unity provider discovery, external toolchain execution, canonical handoff, Unity batch conversion, candidate evidence, BepInEx Mono or IL2CPP adapters, exact game profiles, runtime integration, or deployment review. It preserves the O3DE/Unity/runtime boundary and blocks silent authority promotion.
---

# FOA-SDK Unity Bridge Gates

Use whenever work crosses from FOA-SDK authoring into external Unity conversion, runtime-adapter preparation, packaging, or exact-install verification.

## Research To Read

Read:

- FOA-SDK architecture
- external toolchain and Unity provider research
- canonical handoff and conversion contracts
- exact Unity and Fall of Avalon profile evidence
- adapter contracts and Mono/IL2CPP separation
- packaging and deployment-review documents
- existing process, malformed-input, timeout, conversion, adapter, and evidence tests

## Lawful Boundary

- FOA-SDK and O3DE own neutral authoring data, reviewed work orders, validation, packaging previews, and candidate evidence.
- The external Unity conversion project owns Unity imports, `.meta` files, GUIDs, import settings, and Unity-native generated output.
- Fall of Avalon owns final runtime interpretation.
- Only the documented ExternalToolchain, Unity provider, conversion project, packaging, and runtime-adapter layers may touch their assigned surfaces.
- No editor, plug-in, work order, conversion result, or installer selection grants runtime mutation, deployment, save, signing, publication, or evidence-promotion authority.

## Gate Order

Run gates in this order:

1. Exact external-tool and runtime-profile discovery.
2. Canonical handoff serialization and validation.
3. Bounded external-process planning and supervision.
4. Unity import and batch conversion.
5. Typed conversion-result and external-process evidence return.
6. Candidate-evidence review without auto-promotion.
7. Mono or IL2CPP adapter contract and build.
8. Packaging preview and inventory.
9. Explicit deployment review, rollback, and destination authority.
10. Exact-install Fall of Avalon runtime proof.

Do not promote a later gate because an earlier gate “mostly works.”

## Canonical Handoff Rules

- Handoff data is neutral, deterministic, versioned, and reviewable.
- O3DE does not author Unity-native files.
- Unity owns generated metadata and native outputs.
- Work orders are plans; they do not imply execution permission.
- Results return as candidate evidence and remain separate from reviewed evidence, claims, validation, and permission.
- Exact source identity, profile, tool version, input hashes, output inventory, diagnostics, and status are recorded.

## External Process Rules

- Discover tools through reviewed providers; do not scan the whole machine.
- Bound process arguments, working directory, timeout, cancellation, output capture, and exit handling.
- Preserve stdout, stderr, logs, result paths, hashes, and timestamps.
- A process launch is not proof of successful conversion.
- Failure, timeout, cancellation, missing output, or malformed result must fail closed.

## Adapter Rules

- Mono and IL2CPP are separate target paths.
- Exact-install evidence must determine the applicable profile.
- Adapters consume reviewed contracts and artifacts; they do not acquire authoring authority.
- Initial proof may be no-op or identity-binding only when that scope is explicitly authorised.
- Runtime mutation, live reload, hot reload, IPC, save changes, or direct in-game authoring remain separately governed and are not implied by adapter compilation.

## Deployment Rules

- Packaging preview is not deployment.
- Deployment requires explicit current-task authority, exact destination, inventory, rollback, backup, hash, and timestamp evidence.
- No silent write to a game installation is allowed.
- External destinations remain outside source control.
- A successful copy is not runtime proof.

## Hard Stops

Stop when:

- Unity-native files are authored outside Unity
- O3DE directly mutates runtime or the game installation
- Mono and IL2CPP are conflated
- exact tool, Unity, game, or adapter profile is unknown
- external-process scope is unbounded
- handoff or result formats are unversioned or nondeterministic
- candidate evidence is automatically promoted
- deployment lacks explicit approval, exact destination, and rollback
- runtime proof lacks exact-install evidence

## Validation

Run the applicable lanes:

- canonical serialization and deterministic round trip
- malformed, stale, missing, and future-version handoff input
- work-order planning and execution-permission separation
- tool discovery and provider compatibility
- process timeout, cancellation, exit code, stdout/stderr, and missing-output behavior
- Unity batch conversion and output inventory
- candidate evidence return and no-auto-promotion assertions
- Mono or IL2CPP adapter build and contract tests
- packaging preview and inventory
- deployment-review assertions
- exact-install runtime proof when authorised

Report each gate separately as passed, failed, partial, blocked, or not run.

## Runtime Proof

Without lawful exact-install Fall of Avalon evidence for the exact adapter and artifact, state `runtime sign-off not performed`.
