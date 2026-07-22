# O3DE to Unity Conversion and Runtime Bridge Research

Status: research intake and reconciliation

Original intake baseline: `92aa29960bab92d646c464ae48b8cf09d881a436`

Porting-method reconciliation baseline: `8fb3f0a729e4be4e513ba896ba52708a73d03eae`

Canonical-interchange conclusion baseline: `504e10b27e46fceae4d68af200118edca27b4d1b`

Current canonical-interchange reconciliation baseline: `3d50df1ea3aaf97a5148ae4ebb5c5ade8336e6d2`

Observation dates: 20 July 2026 at intake; 22 July 2026 at all reconciliations

## Authority boundary

This topic records evidence, accepted research conclusions, and unresolved design questions. It grants no
implementation authority. The normative design independently authorises one narrow Gate 0 contract-only
precursor: inert Core value objects, canonical serialization, pure validation, focused tests, and repository
boundary validation. Research does not create or expand that authority.

In particular, this track does not approve a service, persistence, filesystem access, process launch, Unity or
Blender version, native extension, game-install inspection, BepInEx or Harmony execution, deployment, game
launch, runtime mutation, save access, live IPC, evidence promotion, signing, or publication.

The normative authority remains
`docs/tainted-grail-sdk/EDITOR_TOOLCHAIN_UNITY_INTERCHANGE_DESIGN.md` and
`docs/tainted-grail-sdk/EXTERNAL_TOOL_INTERCHANGE_GATE_0.md`. Gate 0 may proceed before Phase 9 only within its
Core-only boundary. The Phase 9 prerequisite continues to control Gates 1 and later, including Gate 5.

## Preserved inputs

The original supplied report is preserved unchanged at
`inputs/FOA_SDK_O3DE_TO_UNITY_CONVERSION_AND_RUNTIME_BRIDGE_RESEARCH_REPORT.md`.

SHA-256 at intake:

```text
b17850a12efe97dbd92a8bdf9cfcd155204105c49e230c51cf9b10aceba9c048
```

The later canonical interchange Deep Research result is preserved as a normalized substantive intake at
[`inputs/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_DEEP_RESEARCH.md`](inputs/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_DEEP_RESEARCH.md).

Conversation-local citation tokens are not durable. Preserved inputs are not accepted decisions, and opaque
conversation-local citations do not grant durable evidence. Durable sources and scoped claims live in
[`SOURCE_REGISTER.md`](SOURCE_REGISTER.md) and [`CLAIM_REGISTER.md`](CLAIM_REGISTER.md).

## Accepted research conclusions

### Porting method

[`FOA_SDK_PORTING_METHODS_RESEARCH.md`](FOA_SDK_PORTING_METHODS_RESEARCH.md) selects a canonical file-backed
interchange package with separate shared domain documents and format-specific payloads. It preserves
Blender-first qualification, Unity Editor-only interchange, and separate Mono and IL2CPP runtime routes.

### Canonical Schema 1 and identity

[`areas/02-authoring-interchange-and-assets/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md`](areas/02-authoring-interchange-and-assets/CANONICAL_INTERCHANGE_SCHEMA_AND_IDENTITY_RESEARCH.md)
answers the minimum schema and identity question.

The conclusion is reconciled to current `main` through
[`areas/02-authoring-interchange-and-assets/CURRENT_MAIN_RECONCILIATION_2026-07-22.md`](areas/02-authoring-interchange-and-assets/CURRENT_MAIN_RECONCILIATION_2026-07-22.md).
That addendum records the later ExtensionRequestBus, optional Road Atlas and Avalon AI Tool Gems, path-policy
hardening, and plugin-validation work without treating those changes as Gate 5 authority.

The accepted direction includes:

- one authoritative `manifest.tginterchange.json`;
- a directory-only Schema-1 package;
- typed engine-neutral domain documents;
- ordered contained payload inventory;
- `PackageId`, `AssetId`, `RevisionFingerprint`, `BindingId`, and first-class mapping events;
- native IDs as bindings rather than canonical identity;
- `foa-interchange-canonical-json-v1` with ASCII semantic tokens and byte-preserved UTF-8 display text;
- right-handed `+X` right, `+Y` forward, `+Z` up, metres, radians, and seconds;
- column vectors, matrix-on-left multiplication, and column-major storage;
- explicit transformations, losses, provenance, legal state, migration, errors, blockers, and fixtures;
- external Framework-owned qualification evidence.

The accepted conclusion remains research-only. It does not authorize Gate 5 implementation.

## Research lanes

| Area | Purpose |
| --- | --- |
| `01-architecture-and-repository-state` | Exact repository baseline, existing ownership, and contradictions |
| `02-authoring-interchange-and-assets` | Package contracts, identities, transforms, materials, animation, losses, and Asset Processor handoff |
| `03-external-toolchain-host` | Generic provider discovery and separately gated process supervision |
| `04-blender-provider` | Blender discovery, native extension candidates, compatibility, and round trips |
| `05-unity-editor-interchange` | Synthetic or user-owned Unity Editor project interchange only |
| `06-runtime-target-profile` | Pinned route observations versus unselected active operator target, loading evidence, and diagnostic boundaries |
| `07-foa-runtime-adapters` | Separate Mono/IL2CPP route, build, deployment, execution, cleanup, rollback, and evidence-return research |
| `08-security-privacy-legal-and-licensing` | Trust, provenance, content, path, log, dependency, licence, and redistribution boundaries |
| `09-qualification-and-experiments` | Synthetic fixtures, compatibility matrices, deterministic comparisons, and negative tests |

## Claim states

- `unverified` — no durable supporting source has been registered;
- `source-supported` — a durable source supports the scoped statement;
- `repository-observed` — the exact repository baseline supports the statement;
- `fixture-verified` — an accepted, reproducible fixture supports the statement;
- `contradicted` — an accepted source or invariant conflicts with the statement;
- `superseded` — a later accepted record replaces the statement.

`source-supported` and `repository-observed` are evidence states, not qualification or permission states.

## Current process state

Completed research sequence:

```text
preserve Deep Research input
→ reconcile to current main
→ correct registers and citations
→ resolve canonicalization, spatial, migration, and error-code decisions
→ import accepted research conclusion
```

The next decision is not provider or host implementation. The process advances to:

[`areas/02-authoring-interchange-and-assets/GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md`](areas/02-authoring-interchange-and-assets/GATE_5_CONTRACT_IMPLEMENTATION_DESIGN_QUESTION.md)

That question defines the exact contract-only design needed before any Gate 5 code can be considered. Phase 9
entry and explicit Gate 5 authority must be confirmed separately.

Current governance finding: optional authoring Tool Gem work is active, but the controlling gate map still
contains no explicit Phase 9 entry acceptance opening Gate 5.

## Controlling order

See [`gates/IMPLEMENTATION_GATE_MAPPING.md`](gates/IMPLEMENTATION_GATE_MAPPING.md).

The order remains:

```text
accepted research conclusion
→ Phase 9/Gate 5 authority decision
→ Gate 5 contract-only implementation
→ Blender qualification
→ O3DE round trips
→ Unity Editor-only interchange
→ later separately gated runtime mapping
```
