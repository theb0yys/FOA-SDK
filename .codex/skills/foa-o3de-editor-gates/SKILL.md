---
name: foa-o3de-editor-gates
description: Use for tasks touching the pinned O3DE host, TaintedGrailModdingEditor, product Gems, editor components, panes, buses, assets, Asset Processor, build configuration, source policy, or host acceptance.
---

# FOA-SDK O3DE Editor Gates

## Research To Read

Read `o3de.lock.json`, external dependency documentation, architecture, development guide, open/test guide, CI/local validation guide, relevant Gem/project docs, and current tests.

## Ownership

FOA-SDK owns product Gems and Editor project. The external pinned O3DE checkout owns engine source. Do not copy or modify stock O3DE source as part of product work unless separately and explicitly authorised.

## Required Workflow

Research; evidence map; owner assignment; scoped implementation; exact-pin prerequisite/configure/build; compiled tests; static/source-policy checks; Editor acceptance when required; generated-output cleanup outside source; honest handoff.

## Hard Stops

Stop if the selected O3DE checkout does not match the lock; product code is proposed inside external engine source; generated output becomes source truth; host proof is inferred from static inspection; or runtime authority crosses into the editor.

## Validation

Use `developer_preview.py` and `run_local_validation.py` with the exact pinned engine and external build root. Host-heavy Editor acceptance remains a distinct manual gate.

## Runtime Proof

O3DE Editor validation is not Fall of Avalon runtime proof. State `runtime sign-off not performed` unless exact runtime evidence exists.
