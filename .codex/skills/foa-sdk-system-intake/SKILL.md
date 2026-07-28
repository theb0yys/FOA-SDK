---
name: foa-sdk-system-intake
description: Use whenever a task touches FOA-SDK Foundation, authoring, toolchain, conversion, runtime-integration, integration, or support systems, public contracts, owned-truth boundaries, or the governed test harness. It assigns one primary owner, defines forbidden domains, and maps publisher, consumer, persistence, UI, and proof obligations before implementation.
---

# FOA-SDK System Intake

Use after research authority and impact classification when a task introduces, moves, connects, or materially changes a system.

## Research To Read

Start with:

- root governance and process
- `docs/systems/SYSTEM_INDEX.md`
- protected-files policy
- public architecture and data-format documentation
- task-relevant `Research/`
- owner-specific README, local `AGENTS.md`, implementation, and tests
- test, performance, artifact, migration, and release gates

Do not infer ownership from a folder or product name alone.

## Owner Mapping

Map the work to exactly one primary owner.

The standard decomposition is:

- Foundation owns product truth, durable models, services, and governed state transitions.
- Contracts and schemas own stable public shapes and versioned interchange.
- Facades, providers, and adapters expose controlled access without taking ownership of the source truth.
- UI presents snapshots and forwards commands; it does not compute or own domain truth.
- Plug-ins extend through ExtensionAPI and do not acquire mutable Foundation internals.
- Installer owns reviewed installation lifecycle source and evidence, not runtime permission.
- Test harnesses prove evidence and never become product, gameplay, migration, or runtime sovereign.

## Required System Definition

For every system, record:

- canonical system key
- architecture group
- primary owner
- owned truth
- forbidden domain
- published contracts or snapshots
- read-only inputs
- accepted commands or work orders
- persistence and serialization ownership
- canonical-interchange responsibilities
- UI, facade, provider, or adapter routes
- external dependencies and exact identities
- degraded or absent-provider behavior
- tests, performance guards, artifacts, and runtime proof required

## Authority Rules

- One system owns each source of truth.
- Shared context is published through reviewed contracts rather than duplicated ownership.
- Consumers read published values and do not cross-write neighboring private state.
- UI, AI, integrations, adapters, installers, diagnostics, and tests must not become sovereign over Foundation truth.
- Evidence, claims, validation, permission, and promotion remain separate.
- Missing providers, profiles, evidence, or permissions fail closed.

## Architecture Rules

- O3DE remains the governed authoring host.
- Fall of Avalon remains a separate Unity runtime target.
- Neutral canonical handoff crosses the engine boundary.
- Unity owns Unity-native metadata, GUIDs, imports, and generated native output.
- External processes run through bounded provider and toolchain contracts.
- Candidate conversion results return as evidence and are not auto-promoted.
- Mono and IL2CPP remain separate adapter paths.
- No declaration, plug-in, work order, installer selection, or evidence record grants runtime mutation, save, deployment, signing, publication, catalog mutation, or evidence-promotion authority.

## Test Obligations

Require the applicable proof for:

- local invariants and range or enum legality
- stable identity and exact-reference behavior
- publisher and consumer boundaries
- persistence, serialization, and round trips
- malformed, stale, missing, and future-version inputs
- lifecycle and transient rebuild
- degraded missing-provider or missing-data behavior
- cross-system consequence and observability
- duplicate registration and event immunity
- performance and soak risk
- UI, O3DE host, conversion, installer, adapter, migration, and harness surfaces

Missing lanes must be added with the change or reported as partial or blocked.

## Hard Stops

Stop when:

- research or owner evidence is absent
- multiple systems claim the same truth
- the forbidden domain cannot be stated
- publisher or consumer direction is unknown
- protected data would be copied, committed, or modified
- a public or persisted shape changes without compatibility authority
- new dependencies or external profiles lack exact identity and licence or provenance review
- a support, UI, integration, adapter, installer, or test layer would seize Foundation authority
- runtime proof is claimed without exact-install evidence

## Validation

Use owner-specific gates. Report:

- owner and system key
- owned truth and forbidden domain
- publisher and consumer contracts
- persistence and interchange impact
- UI, provider, adapter, installer, or external-process routes
- required tests and performance proof
- artifact and external-write implications
- complete, partial, or blocked intake status

If no existing owner-specific gate proves the change, report the gap instead of inventing one. State `runtime sign-off not performed` when exact runtime evidence is absent.
