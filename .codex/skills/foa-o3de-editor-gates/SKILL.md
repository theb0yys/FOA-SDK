---
name: foa-o3de-editor-gates
description: Use for tasks touching the pinned O3DE host, TaintedGrailModdingEditor, product Gems, editor components, panes, buses, assets, Asset Processor, build configuration, source policy, or host acceptance. It preserves the external-engine boundary and requires exact-pin host proof without treating Editor validation as Fall of Avalon runtime authority.
---

# FOA-SDK O3DE Editor Gates

Use for any change that affects the O3DE authoring host or the product-owned Editor project and Gems.

## Research To Read

Read:

- `o3de.lock.json`
- external O3DE dependency documentation
- architecture and development guide
- open-and-test Editor guide
- CI and local-validation guide
- relevant Gem and project README files
- component, bus, pane, asset, and source-policy documents
- current tests and acceptance evidence

## Ownership

- FOA-SDK owns its product Gems, Editor project, components, panes, buses, schemas, services, and tests.
- The separately pinned O3DE checkout owns engine source and stock host functionality.
- Generated build, cache, Asset Processor, diagnostic, screenshot, and test output belongs under the reviewed external build root.
- Product work must not silently copy, fork, patch, or treat external O3DE source as FOA-SDK-owned source.

## Required Workflow

1. Confirm the exact repository, branch, and target paths.
2. Verify the selected O3DE checkout matches `o3de.lock.json`.
3. Read the local owner documents and existing tests.
4. Map components, buses, panes, services, assets, and public contracts affected.
5. Keep the implementation inside the product-owned project or Gems.
6. Run the applicable prerequisites and source-policy gates.
7. Configure against the exact pinned engine and external build directory.
8. Build the required targets.
9. Execute compiled tests with zero matching tests treated as an error.
10. Run Asset Processor or Editor-host acceptance where required.
11. Keep generated output outside both source checkouts.
12. Report host proof separately from Unity, installer, adapter, deployment, and runtime proof.

## Host Boundary Rules

- O3DE is an authoring host, not the Fall of Avalon runtime.
- Editor components may create and validate neutral authoring data and governed work orders.
- Editor code must not mutate the game installation, saves, runtime state, or external Unity project without an authorised provider and reviewed handoff.
- Optional plug-ins register through ExtensionAPI rather than acquiring mutable Foundation internals.
- Display names are not identities; exact type IDs, asset IDs, and native references remain exact.
- Missing engine identity, host capability, or external provider proof fails closed.

## Build and Source Policy

- Use the exact pinned engine commit; do not substitute a branch tip.
- Use the external build root; do not commit generated build products.
- Keep direct includes, Gem dependencies, component reflection, serialization versions, and bus ownership explicit.
- Do not add stock O3DE files to the product repository.
- Engine changes require their own reviewed lock update and host evidence.

## Hard Stops

Stop when:

- the selected O3DE checkout does not match the lock
- product code is proposed inside external engine source
- generated output is proposed as source truth
- required component, bus, service, or pane ownership is unclear
- host proof is inferred from static inspection or an unrelated build
- zero tests are accepted as a test pass
- runtime mutation or deployment authority crosses into the Editor layer
- Unity-native output is authored or claimed from O3DE without the reviewed conversion layer

## Validation

Use the applicable FOA-SDK commands, including `developer_preview.py` and `run_local_validation.py`, with the exact pinned engine and external build root. Record:

- engine identity and commit
- prerequisite result
- configure command and result
- build targets and result
- compiled tests and discovered test count
- source-policy result
- Asset Processor or Editor acceptance result
- generated-output location
- skipped or blocked host gates

Host-heavy Editor acceptance remains a distinct manual gate.

## Runtime Proof

O3DE configure, build, compiled tests, Asset Processor, and Editor acceptance are not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless lawful exact-install runtime evidence exists.
