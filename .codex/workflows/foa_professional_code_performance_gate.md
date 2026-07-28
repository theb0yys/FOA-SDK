# FOA-SDK Professional Code and Performance Gate

## Rule P0: No Shortcut Code

Only professional, owner-scoped code is allowed. No heavy invasive unnecessary code, broad refactor, speculative architecture, unresearched dependency, hidden state repair, stale-truth cache, or shortcut implementation.

## Rule P1: Run the Performance Preflight

For source, UI, O3DE system/component, asset-processing, interchange, conversion, installer, packaging, runtime-adapter, migration, or harness changes, run `.codex/scripts/Get-AgentPerformancePlan.ps1` before editing.

Record performance risk, changed surfaces, data cardinality, forbidden shortcuts, required checks, and evidence requirements.

## Rule P2: Hot-Path Audit

Audit editor ticks, event handlers, buses, UI binding/render, asset scans, catalog operations, serialization, file interchange, external-process supervision, conversion, packaging, installer operations, adapter hooks, reflection, logging, and large collections.

Do not add unbounded scans, IO or logging in loops, repeated allocation-heavy transforms, repeated reflection, accidental quadratic work, or synchronous blocking on interactive paths without measured authority.

## Rule P3: Deterministic Performance Guard

High-risk changes require a deterministic guard with:

- baseline or researched expected cost;
- threshold;
- measured result;
- command;
- build configuration;
- machine, editor, toolchain, or runtime context;
- representative data cardinality.

No lane or measurement means partial or blocked validation.

## Rule P4: Surface-Specific Performance Proof

Do not substitute configure/build success for measured proof. UI, catalog, serialization, asset processing, conversion, external tools, installer, and runtime adapters require proof appropriate to their own surface.

## Rule P5: Handoff Evidence

Report risk, hot-path findings, complexity before/after, budget, measurement, command, context, missing lanes, and whether exact Fall of Avalon runtime performance proof was performed.
