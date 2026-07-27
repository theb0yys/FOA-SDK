---
name: foa-change-impact-classifier
description: Use before any FOA-SDK edit, review, PR, or plan that can affect source, documentation, tests, workflows, contracts, schemas, assets, persistence, builds, Unity conversion, runtime adapters, installer behavior, releases, or protected external data. It classifies owners, surfaces, consumers, and blast radius before implementation.
---

# FOA-SDK Change Impact Classifier

Use after research authority and before implementation. This gate determines which owners, contracts, consumers, and validation surfaces are affected.

## First Actions

1. Normalise every target path.
2. Classify each target as one or more of:
   - source
   - contract or public API
   - schema or manifest
   - persistence or serialization
   - canonical interchange
   - UI or presentation
   - O3DE host integration
   - plug-in
   - external toolchain
   - Unity conversion
   - installer or package
   - runtime adapter
   - migration
   - test harness
   - documentation
   - process or governance
   - GitHub
   - build or release
   - protected external data
3. Map every target to its primary owner through `docs/systems/SYSTEM_INDEX.md`, local governance, implementation, and tests.
4. Identify interfaces, producers, consumers, readers, writers, serializers, importers, exporters, loaders, launchers, and external boundaries.
5. Produce a bounded blast-radius statement before editing.

## Research

Read:

- root process and governance
- system index
- owner-specific research and local rules
- existing implementation and tests
- compatibility and persistence authorities
- UI, build, installer, conversion, adapter, migration, and release gates when relevant

A path name or directory position alone is not ownership proof.

## Impact Questions

For each affected surface, answer:

- Which system owns the changed truth?
- Which public contracts, schemas, manifests, commands, configuration, or packages are exposed?
- Which systems or tools consume the surface?
- Is the consumer read-only, writable, generated, imported, or external?
- Could persisted workspaces, packs, evidence, catalog state, conversion results, installer state, or runtime adapters be affected?
- Does the change affect build targets, generated output, external destinations, permissions, or exact-install runtime evidence?
- Which narrower skills, tests, performance checks, compatibility gates, and evidence fields are required?

## Multi-Owner Changes

When multiple owners are affected:

1. Name each owner separately.
2. Separate owner-local changes from shared-contract changes.
3. Identify cross-owner chains and read/write direction.
4. Do not use a broad consistency change to bypass owner-scoped authority.
5. If the blast radius cannot be bounded, stop and report partial or blocked classification.

## Hard Stops

Stop when:

- a path or surface cannot be classified
- the primary owner is unknown
- consumers or affected public boundaries cannot be identified
- unrelated owners would be mixed without explicit scope
- protected data is implicated without permission
- a supposedly cosmetic change alters identity, schema, manifest, persistence, build, package, installer, adapter, runtime, or release behavior

## Validation

Report:

- affected systems and owners
- changed surfaces
- public contracts, schemas, manifests, persistence, configuration, build, runtime, UI, installer, and release impact
- producers, consumers, and cross-system chains
- blast radius
- required skills and gates
- required tests and performance proof
- artifact/deployment implications
- complete, partial, or blocked classification

## Runtime Proof

Impact analysis is static classification. It does not prove O3DE host behavior, Unity conversion, installer execution, adapter behavior, deployment, save compatibility, or Fall of Avalon runtime behavior. State `runtime sign-off not performed` when only static analysis ran.
