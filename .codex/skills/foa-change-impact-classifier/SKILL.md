---
name: foa-change-impact-classifier
description: Use before any FOA-SDK edit, review, PR, or plan that can affect source, docs, tests, workflows, contracts, schemas, assets, persistence, builds, runtime adapters, installer, releases, or protected external data.
---

# FOA-SDK Change Impact Classifier

## First Actions

1. Normalise target paths.
2. Classify each as source, contract, schema, manifest, persistence, interchange, UI, O3DE host, plug-in, toolchain, Unity conversion, installer, runtime adapter, migration, harness, docs, process, GitHub, build, release, or protected data.
3. Map targets to owner systems through `SYSTEM_INDEX.md` and local structure.
4. Identify interfaces, producers, consumers, readers, writers, loaders, and external boundaries.
5. Produce a blast-radius statement before edits.

## Research

Read root process, system index, owner docs, implementation/tests, and nearby compatibility, persistence, UI, build, runtime, installer, and release authorities.

## Hard Stops

Stop when a path, owner, consumer, or affected public/persistence/build/runtime surface cannot be classified, or unrelated owners would be mixed without explicit scope.

## Validation

Report systems, surfaces, contracts/schemas, persistence/config/build/runtime/UI impact, consumers and chains, required skills/gates/tests/performance/evidence/artifact gates, and complete/partial/blocked classification.

## Runtime Proof

Impact analysis is not runtime proof. State `runtime sign-off not performed` when only static analysis ran.
