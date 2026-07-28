# Visual Game-Content Browser and Preview Pipeline Gate

Status: blocking design gate.

This document records the visual game-content browser and preview pipeline as a required prerequisite before any item, recipe, actor, troop, placement, or visual browser workflow is described as function-complete. Current bridge and economy tools may be described as staged, bounded, evidence-producing, review-ready, or non-executable. They must not be described as complete end-user modding functions until this gate has been satisfied with implementation evidence.

## Controlling rule

No item, recipe, actor, troop, placement, or visual-browser workflow may be described as function-complete until FOA-SDK can perform the following chain with evidence and without violating runtime boundaries:

```text
FoA native asset reference
→ version-bound discovery record
→ local preview artefact
→ generated O3DE preview product
→ typed authoring binding
```

The chain is an editor-preview chain only. Preview success does not grant runtime permission, deployment authority, catalog promotion, save access, signing, publication, or adapter execution.

## Required separation

The visual pipeline uses layered identities. A native game asset reference, a discovered asset record, a local thumbnail, a neutral preview conversion result, an O3DE preview product, and a typed authoring binding are separate records with separate authority. No layer silently overwrites another.

Required identity layers:

| Layer | Authority |
| --- | --- |
| FoA native asset reference | Native discovery identity for the exact game profile. |
| Version-bound discovery record | Searchable local evidence bound to workspace, profile, branch, runtime target, source fingerprint, and discovery tool. |
| Local preview artefact | Generated local-only thumbnail or payload outside repository and engine source trees. |
| Neutral preview conversion result | Evidence envelope that records emitted payloads, losses, warnings, and fingerprints. |
| Generated O3DE preview product | Editor rendering product only; never authoritative runtime content. |
| Typed authoring binding | User selection inside item, recipe, actor, troop, or placement authoring state. |

Generated outputs remain outside repository and engine source trees. No proprietary game payloads may be committed.

## Alpha gate

The Alpha gate is the smallest acceptable implementation path. It must include all of the following before any affected workflow is function-complete:

1. Read-only asset discovery and indexing.
2. Native icon and thumbnail extraction for a bounded, evidence-backed set.
3. Unity-to-neutral preview handoff design and at least one deterministic static-preview fixture.
4. Neutral-to-O3DE preview conversion into a controlled generated-output root.
5. Asset browser pane that shows exact identity, provenance, validation state, stale state, preview status, and blockers.
6. 3D preview viewport for a bounded static preview cohort, with explicit fidelity state.
7. Visual selectors in Item and Recipe Editor that bind selected native/preview records without replacing typed identity semantics.

Actor equipment and appearance preview, troop composition preview, and drag-and-drop world placement are not Alpha-complete requirements. They are later gates unless separately approved by exact local evidence.

## Ten-stage sequence

The full visual pipeline remains ordered as follows:

1. Read-only asset discovery and indexing.
2. Native icon and thumbnail extraction.
3. Unity-to-neutral preview handoff.
4. Neutral-to-O3DE preview conversion.
5. Asset browser pane.
6. 3D preview viewport.
7. Visual selectors in Item and Recipe Editor.
8. Actor equipment and appearance preview.
9. Troop composition preview.
10. Drag-and-drop world placement.

Each stage must be profile-bound, runtime-target-bound, fingerprint-bound, and tool-version-bound. Cache reuse across game profiles, branches, runtime targets, tool versions, preview schemas, or source fingerprints is prohibited unless a reviewed compatibility rule explicitly permits it.

## Non-authority boundary

This gate does not approve any of the following:

- runtime-assisted capture for Alpha;
- recursive scanning of arbitrary disks;
- unbounded installation inspection;
- Unity project discovery from the proprietary game;
- Unity, FoA, BepInEx, Harmony, or game API calls from the O3DE editor;
- game launch;
- save inspection or mutation;
- deployment;
- signing;
- release publication;
- automatic evidence promotion;
- automatic catalog mutation;
- redistributed extracted or derived game content.

No runtime-assisted capture is approved for Alpha. Local discovery must start with bounded file evidence, explicit extracted-data inputs, and generated local-only preview artefacts.

## Required implementation artefacts

A visual browser or preview PR must provide:

- one exact source/evidence format for discovered native assets;
- one deterministic preview-index document;
- one local generated-output root contract;
- one preview artefact manifest with SHA-256 fingerprints;
- stale/invalidation rules for game version, branch, runtime target, source hash, extractor version, preview schema, and O3DE import settings;
- explicit fidelity states: `exact`, `approximate`, `partial`, `placeholder`, `unsupported`, and `blocked`;
- clear user-visible blockers for missing evidence, unsafe paths, unsupported formats, licensing uncertainty, and stale previews;
- negative tests for private paths, traversal, absolute paths, source fingerprint drift, duplicate native references, stale generated products, and false runtime authority.

## Completion wording

Allowed wording before this gate is satisfied:

- staged;
- candidate;
- review-ready;
- evidence-producing;
- non-executable;
- preview-gated;
- not function-complete.

Forbidden wording before this gate is satisfied:

- function-complete;
- complete modding function;
- end-to-end item creation;
- fully functional item workflow;
- full visual browser;
- production-ready preview pipeline.

Existing bridge tools, managed identifier export, local diagnostic collection, source/evidence intake, and economy candidate promotion remain upstream prerequisites. They are not complete user-facing visual workflows without this gate.

## First implementation slice after this gate

The first implementation slice after this gate should be read-only asset discovery and preview indexing. It should not attempt actor composition, troop staging, world placement, runtime capture, or deployment. Done means an exact workspace profile can produce a local, searchable, evidence-bound asset index with safe stale-state and blocker reporting.
