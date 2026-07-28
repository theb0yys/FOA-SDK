---
name: foa-ui-asset-route-gates
description: Use for FOA-SDK UI, panes, Qt models, icons, textures, materials, meshes, prefabs, asset processing, visible copy, authoring presentation, or generated-resource cleanup. It preserves UI/domain separation, legal provenance, provider ownership, per-file batch proof, and O3DE asset-processing boundaries.
---

# FOA-SDK UI and Asset Route Gates

Use whenever the task changes a visible surface, UI binding, asset provider, source asset, generated resource, or O3DE asset-processing route.

## Research To Read

Read:

- architecture and system index
- UI and asset owner documents
- legal and content policy
- source-policy and external-build documentation
- task-relevant `Research/`
- current UI models, services, components, assets, and tests
- O3DE Asset Processor and Editor requirements
- plug-in and provider boundaries when optional UI or asset systems are involved

## Ownership

- UI presents snapshots and forwards commands; it does not own or recompute domain truth.
- Domain services own validation, persistence, identity, and state transitions.
- Assets must be project-owned or legally distributable, with recorded provenance and licence state.
- Exact asset IDs, type IDs, references, and provider ownership remain stable.
- External game assets, extracted commercial content, private source vaults, and unknown-provenance material remain protected.
- Generated resources belong under reviewed output locations and do not become source authority.

## Required Front Gate

Before a move, replacement, import, regeneration, or cleanup:

1. Identify the owner and intended runtime or Editor route.
2. Snapshot or archive the affected project-owned source when the operation is destructive or regenerating.
3. Classify the complete batch by file type and provenance.
4. Scan every relevant file; filename-only proof is forbidden.
5. Verify format, dimensions, identity, references, provenance, licence, and output location.
6. Verify that generated outputs belong to the correct provider and do not duplicate ownership.
7. If one file in a conversion or import batch fails, treat the complete batch as suspect until each file is checked.
8. Promote higher-level prefabs or UI bindings only after their lower-level source and generated dependencies pass.

## UI Rules

- Keep UI-route tests distinct from owner-core tests.
- Bind to stable snapshots and commands rather than internal mutable state.
- Use semantic assets and readable surfaces rather than random decoration.
- Do not expose internal IDs, paths, provider names, or diagnostics as user-facing copy unless explicitly designed.
- Do not stretch or repurpose source sheets in ways that violate their intended semantic role.
- Do not duplicate an asset payload into a control-only or runtime-only layer.
- Do not perform unbounded scans, heavy parsing, file IO, reflection, or domain recomputation during rendering or repeated binding.
- UI failure must degrade safely and produce actionable diagnostics without mutating domain truth.

## Asset Rules

For each asset type, validate the applicable surface:

- textures: format, dimensions, colour-space intent, naming, ownership, source and generated paths
- materials: shader or material type, texture slots, exact references, provider ownership
- meshes: object and resource identity, material slots, collision or LOD structure where applicable
- prefabs: component and resource references, provider availability, deterministic source
- icons and UI art: semantic role, resolution, accessibility, readable scaling, licence and provenance
- generated metadata: deterministic output, external location, exact source relationship, no stale references

## Cleanup Rules

- Do not broadly delete generated or duplicate-looking files without owner and generation authority.
- Confirm which file is active, which process generates it, and which references consume it.
- Keep one authoritative provider route where the architecture requires one.
- Do not remove compatibility aliases or generated resources until rebuilt outputs and references prove they are no longer required.

## Hard Stops

Stop when:

- protected or proprietary content is implicated
- provenance or licence is unknown
- references cannot be resolved
- filename-only evidence is offered
- ownership is duplicated or unclear
- generated output is proposed inside source control contrary to policy
- UI would seize domain authority
- an asset batch contains a failed member and the remaining files are not checked
- runtime sign-off is requested without exact-install evidence

## Validation

Run the applicable proof:

- UI model and route tests
- owner-core tests separately
- asset, provenance, schema, and source-policy validators
- deterministic generation checks
- O3DE configure/build and Asset Processor gates
- manual Editor acceptance and screenshots when required
- package and provider-layout assertions

Report archives, source and output paths, references checked, batch failures, generated-output location, and any manual or runtime gate not performed.

## Runtime Proof

Static inspection, O3DE builds, Asset Processor results, and Editor presentation are not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless lawful exact-install runtime evidence exists.
