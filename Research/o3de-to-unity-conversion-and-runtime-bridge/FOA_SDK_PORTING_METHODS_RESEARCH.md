# FOA-SDK O3DE to Unity to Tainted Grail Porting Methods Research

Status: research reconciliation; no implementation authority

Repository baseline: `8fb3f0a729e4be4e513ba896ba52708a73d03eae` (`main`), observed 22 July 2026

Previous deep-research baseline: `195f5c15f0c59d47bd54e661b37d7457af9d1d95`, observed 22 July 2026

## Authority boundary

This document records source-backed findings, contradictions, unresolved questions, experiment designs, and a recommended research direction. It does not authorize a schema, service, provider, filesystem operation, process launch, Unity or Blender execution, game-install inspection, runtime adapter build, deployment, game launch, runtime mutation, save access, catalog mutation, evidence promotion, signing, or publication.

Normative architecture, roadmap, gate, legal, security, and accepted implementation documents remain controlling.

## Baseline reconciliation

The deep-research report was written against `195f5c15f0c59d47bd54e661b37d7457af9d1d95`. The current default-branch head is `8fb3f0a729e4be4e513ba896ba52708a73d03eae`.

Exact comparison:

https://github.com/theb0yys/FOA-SDK/compare/195f5c15f0c59d47bd54e661b37d7457af9d1d95...8fb3f0a729e4be4e513ba896ba52708a73d03eae

The range is eight commits ahead and changes only the Suite Wizard catalogue discovery/selection interface, installer documentation, and focused tests. It does not change the O3DE-to-Unity interchange design, the research gate mapping, the Mono/IL2CPP route contracts, or runtime authority boundaries. The report therefore required a baseline correction, but the intervening commits do not invalidate its porting-method conclusion.

Current product identity and separation are recorded at:

- https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/README.md#L5-L56
- https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Plugins/README.md#L5-L66

## Executive finding

The accepted research direction is:

```text
FOA-SDK canonical authoring and domain data
→ versioned engine-neutral interchange package
→ Blender-first synthetic qualification
→ O3DE round-trip qualification
→ Unity Editor-only synthetic or user-owned test-project qualification
→ target-neutral runtime payload mapping
→ separately selected Mono or IL2CPP adapter route
→ existing governed build, package, deployment, verification, and evidence-return pipeline
```

The primary method is a canonical interchange package combined with separate shared domain documents and format-specific payloads. Direct O3DE-native to Unity-native conversion is not the primary architecture because it would make native importer behavior the de facto identity and schema authority. Live IPC remains later research and cannot replace durable file-backed authority.

## Confirmed ownership model

- O3DE is the governed authoring host.
- The target game and Unity runtime remain outside the editor repository.
- `Gems/ExternalToolchain` owns generic external-tool discovery and any later separately reviewed supervision.
- TG SDK Core owns typed contracts, canonical serialization, and intrinsic validation.
- Framework owns workspace/profile checks, evidence binding, staging, persistence, and candidate-before-publish orchestration.
- Editor panes remain thin presentation and command surfaces.
- `Plugins/Authoring` owns optional editor-side domains.
- `Plugins/Integrations` owns import/export and approved tool/acquisition bridges.
- `Plugins/RuntimeAdapters` owns separate target-specific runtime packages.
- Mono and IL2CPP remain separate packages, dependencies, binaries, tests, compatibility claims, and evidence.

Repository basis:

- https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/README.md#L28-L60
- https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Plugins/README.md#L37-L85
- https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Research/o3de-to-unity-conversion-and-runtime-bridge/gates/IMPLEMENTATION_GATE_MAPPING.md#L13-L41

## Recommended porting method

### Canonical identity remains engine-neutral

The canonical model uses stable logical identities such as `PackageId`, `AssetId`, `RevisionFingerprint`, and `BindingId`. O3DE asset identifiers, Unity GUIDs, Blender object identifiers, paths, and display names are native bindings or observations, not canonical identity.

Rename, duplicate, fork, merge, split, and one-to-many mappings must be represented explicitly. No host may silently replace a canonical identity because a native path or GUID changed.

### The interchange package is the first durable handoff

The package boundary remains one versioned `manifest.tginterchange.json` plus contained payloads. It must bind:

- producer and consumer profiles;
- stable identities and exact native bindings;
- relative payload paths, sizes, media types, and SHA-256 observations;
- coordinate, unit, matrix, pivot, hierarchy, and time-basis declarations;
- mesh, texture, material, skeleton, animation, collider, and metadata declarations;
- provenance, authorship, licence, modification, attribution, and redistribution state;
- ordered transformations and typed losses;
- exact toolchain and package locks;
- validation and qualification evidence.

Outer Gate 0 handoff/result envelopes may bind this package later, but they do not replace its identity, transformation, provenance, loss, or lock contracts.

### Shared domain documents remain separate from native payloads

Road Atlas, Avalon AI, actor, troop, relationship, governance, and future domain records should remain canonical FOA-SDK documents. The interchange manifest binds those records to format-specific scene or asset payloads without embedding domain authority into Blender, Unity, FBX, glTF, or runtime plug-in serialization.

### Blender qualification precedes Unity execution

Blender remains the first external authoring qualification route. This proves that the package is not secretly a Unity-owned schema and establishes the generic process, staging, reverse-export, loss, and round-trip requirements before Unity-specific import hooks and project mutation are introduced.

A current Blender release is only a candidate until an exact version, extension set, command surface, licensing state, synthetic fixture set, and cleanup policy are accepted.

### Unity remains editor-only during interchange qualification

The Unity route operates only in a synthetic or user-owned disposable test project. It must bind an exact Unity Editor version, package manifest, lock file, editor-only assemblies, generated roots, `.meta` files, GUID behavior, importer/postprocessor exposure, command-line arguments, logs, project locking, cancellation, and reverse export.

The Unity package must not contain, build, deploy, or execute a Tainted Grail runtime adapter.

### Runtime work re-enters through separate target-specific routes

A successful Unity import, Unity process exit, C# compile, file copy, or plug-in load does not establish gameplay compatibility or grant runtime authority.

The runtime lane begins only after authoring qualification and a separate runtime-payload mapping review. It then uses one exact selected route and the existing build-manifest, package, deployment-preview, confirmation, executor, rollback, verification, and candidate-evidence pipeline.

## Corrected runtime evidence

The earlier generic statement that all runtime facts are unknown is too broad.

The current repository contains exact pinned upstream route observations.

### Mono route observation

- adapter ID: `adapter.foa.mono`
- contract version: `1.0.0`
- evidence state: `HostLiveLoadValidated`
- framework version: `0.1.33`
- game version: `1.23.401`
- branch: `mono`
- runtime target: `Mono`
- Unity version: `6000.0.64f1`
- BepInEx version: `5.4.23.3`
- licence expression: `NOASSERTION`

### IL2CPP route observation

- adapter ID: `adapter.foa.il2cpp`
- contract version: `1.0.0`
- evidence state: `PackageInstallValidated`
- framework version: `0.1.36`
- game version: `1.23.401`
- branch: `il2cpp`
- runtime target: `IL2CPP`
- Unity version: `6000.0.64f1`
- BepInEx version: `6.0.0-be.735`
- licence expression: `NOASSERTION`

Durable source:

https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp#L99-L183

These are route observations tied to the pinned upstream system-port evidence. They do not prove the user's current lawful installation, select a deployment target, approve dependency redistribution, or authorize adapter build, deployment, execution, game API access, runtime mutation, or save access.

Both routes explicitly block `adapter.build`, `adapter.deploy`, `adapter.execute`, `game-api-access`, `runtime-mutation`, and `save-access`. The IL2CPP route additionally blocks `feature-tested-gameplay`.

The corrected distinction is:

```text
exact pinned route observations exist
≠
an active operator installation/profile is selected
≠
runtime implementation or execution is authorized
```

## Architecture option decision

| Option | Finding | Disposition |
| --- | --- | --- |
| Direct O3DE-native to Unity-native conversion | Fast prototype, but weak identity, loss, audit, migration, and reverse-mapping behavior | Reject as primary method |
| Canonical interchange package | Best determinism, auditability, reversibility, versioning, and gate separation | Accept as primary method |
| Shared domain documents plus format-specific payloads | Preserves domain semantics while allowing host-specific payloads | Accept within the canonical package method |
| Live editor bridge or IPC | Expands authority and weakens durable replay/audit boundaries | Defer until file-backed qualification is complete |

## Research-area conclusions

### Area 01 — Architecture and repository state

The ownership model is sufficient to continue research. No new generic identity, evidence, permission, catalog, or runtime system should be introduced by the porting work.

### Area 02 — Authoring interchange and assets

The package direction is accepted for further research. The schema, identity state transitions, canonical forward axis, matrix conventions, payload rules, transformation taxonomy, loss taxonomy, determinism classes, and migration rules remain unresolved.

### Area 03 — External-toolchain host

Discovery exists, but generic process supervision remains a separately reviewed later gate. Provider-specific launch code must not precede the generic supervisor.

### Area 04 — Blender provider

Blender remains first in qualification order. Exact version and add-on support are unqualified.

### Area 05 — Unity Editor interchange

The route remains editor-only. Exact editor version, package identity, package lock, project model, import hooks, headless statement, generated roots, GUID lifecycle, and reverse export remain unqualified.

### Area 06 — Runtime target profile

Exact route observations exist, but no active operator target is selected. A later bounded evidence capture must distinguish route evidence from the operator-provided installation and requested capability.

### Area 07 — FoA runtime adapters

Mono and IL2CPP route descriptors are present and separate. Executable adapter packages remain pending and all runtime authority remains absent.

### Area 08 — Security, privacy, legal, and licensing

Unity and Blender imports are code-execution-capable boundaries. Unknown licensing remains `NOASSERTION` and blocks redistribution decisions. Proprietary game files, private paths, credentials, extracted assets, assemblies, saves, and unredacted logs remain prohibited repository material.

### Area 09 — Qualification and experiments

Experiments remain plans until separately authorized. Successful import or process exit is never sufficient qualification evidence.

## Required qualification programme

Later experiment briefs must cover:

1. canonical manifest and path-safety fixtures;
2. stable identity, rename, duplicate, fork, merge, split, and one-to-many binding fixtures;
3. explicit coordinate, matrix, pivot, negative-scale, and hierarchy fixtures;
4. mesh, material, texture, skeleton, animation, collider, and unsupported-feature fixtures;
5. Blender export, reverse export, O3DE handoff, and round-trip fixtures;
6. Unity import, `.meta`/GUID retention, reverse export, project lock, cancellation, and cleanup fixtures;
7. malformed, hostile, stale-lock, partial-output, and atomic-publication negative fixtures;
8. separate Mono and IL2CPP mock-host build-planning fixtures;
9. package and deployment-preview fixtures with all execution authority false;
10. a later bounded no-op runtime-load evidence plan after exact target selection.

Each experiment must name exact tools and versions, platform, inputs, outputs, permitted roots, network policy, comparison method, negative cases, cleanup, and returned evidence.

## Gate mapping

No gate order changes are required.

- Gate 0 remains permanently inert.
- Generic process supervision precedes provider execution.
- Blender qualification precedes Unity execution.
- Unity Editor interchange precedes any game-facing runtime work.
- Runtime payload mapping re-enters at Gate 13.
- Existing Mono/IL2CPP route observations do not select an implementation route.
- Runtime build, package, deployment, load, verification, and capability work remain separately gated.

Controlling mapping:

https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Research/o3de-to-unity-conversion-and-runtime-bridge/gates/IMPLEMENTATION_GATE_MAPPING.md#L13-L88

## Unresolved blockers

- schema 1 field inventory and canonical serialization rules;
- identity lifecycle and native-binding state machine;
- canonical forward axis, vector convention, matrix storage, multiplication, and mapping direction;
- FBX byte versus semantic determinism;
- exact Blender version and extension set;
- exact Unity Editor version and editor-package lock;
- acceptable Unity project model and `.meta`/GUID retention strategy;
- source-artifact publication and Asset Processor rollback model;
- selected runtime content-loading mechanism;
- exact lawful operator installation/profile for a requested future capability;
- dependency licensing and redistribution for any executable runtime adapter;
- accepted experiment fixtures and evidence schemas.

## Next research question

The architecture and ownership question is sufficiently reconciled to advance to:

**What is the minimum versioned canonical interchange schema and identity model that preserves FOA-SDK domain semantics, stable identity, native bindings, transformations, provenance, losses, and round-trip evidence across O3DE, Blender, and Unity without granting native-host or runtime authority?**

Focused brief:

`areas/02-authoring-interchange-and-assets/CANONICAL_INTERCHANGE_SCHEMA_RESEARCH_QUESTION.md`

## Acceptance state

This research synthesis is acceptable as a research conclusion when:

- the baseline remains bound to `8fb3f0a729e4be4e513ba896ba52708a73d03eae`;
- later repository drift is reconciled rather than ignored;
- source and claim register additions remain durable and scoped;
- exact route observations are not confused with target selection or authority;
- the next schema research produces a field inventory, identity state machine, loss taxonomy, validation rules, fixture matrix, and migration consequences;
- no implementation code or executable authority is introduced by this research change.

## Permanent non-authority statement

This document does not authorize a schema, provider, process, Unity project mutation, runtime adapter, build, package, game inspection, deployment, launch, BepInEx/Harmony execution, runtime mutation, save access, catalog mutation, evidence promotion, signing, or publication.
