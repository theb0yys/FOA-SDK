---
name: foa-ui-asset-route-gates
description: Use for FOA-SDK UI, panes, Qt models, icons, textures, materials, meshes, prefabs, asset processing, visible copy, authoring presentation, or generated-resource cleanup.
---

# FOA-SDK UI and Asset Route Gates

## Research To Read

Read architecture, UI/asset owner docs, legal/content policy, source-policy and build docs, relevant `Research/`, current implementation/tests, and O3DE asset-processing requirements.

## Ownership

UI presents snapshots and forwards commands; it does not own domain truth. Assets must be project-owned or legally distributable, with provenance and exact references. External game assets and private source vaults remain protected.

## Front Gate

Archive before move/replace/regenerate; scan every file in a batch; reject filename-only proof; verify format, identity, provenance, licence, references, and generated-output location; one failed batch member makes the batch suspect until fully checked.

## Runtime Binding Rules

Use semantic assets, readable layouts, stable IDs, and bounded bindings. Do not expose internal IDs/paths as player-facing copy, stretch inappropriate source sheets, duplicate provider payload into control layers, or recompute domain truth during rendering.

## Validation

Keep UI tests distinct from owner tests. Run asset/source-policy validators, O3DE build/Asset Processor gates, and manual Editor acceptance when required. Generated output remains external.

## Hard Stops

Stop for protected/proprietary content, unresolved references, unknown provenance/licence, duplicated ownership, filename-only proof, generated output in source, or requested runtime sign-off without evidence.

## Runtime Proof

Static/Editor proof is not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless exact runtime evidence exists.
