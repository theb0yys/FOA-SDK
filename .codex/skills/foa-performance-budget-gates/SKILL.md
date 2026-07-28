---
name: foa-performance-budget-gates
description: Use for any FOA-SDK path affecting Editor events, UI, catalog or asset scans, persistence, canonical interchange, external processes, Unity conversion, packaging, installer behavior, runtime adapters, builds, or release performance. It requires bounded hot paths, deterministic guards, and measured evidence rather than compile-based claims.
---

# FOA-SDK Performance Budget Gates

Use before implementing or approving any performance-relevant code change.

## First Actions

1. Run or reproduce `.codex/scripts/Get-AgentPerformancePlan.ps1`.
2. Identify affected surfaces and performance risk.
3. Identify hot paths, call frequency, data cardinality, allocation, IO, reflection, process launch, polling, hashing, compression, serialization, and blocking risk.
4. Find the existing benchmark, guard, soak, or harness lane.
5. If no budget exists, identify a researched temporary threshold or report the lane missing.

## Research

Read:

- `.codex/workflows/foa_professional_code_performance_gate.md`
- owner research and implementation
- existing benchmarks, guards, soak tests, and evidence
- O3DE host, Asset Processor, Editor, conversion, installer, adapter, and release requirements where relevant

## Risk Classification

Typical high-risk surfaces include:

- Editor tick, notification, event-bus, or render callbacks
- UI binding and repeated view-model updates
- whole-catalog, whole-workspace, asset, filesystem, or installation scans
- hashing, compression, package inventory, and manifest generation
- persistence and canonical-interchange serialization
- external-process discovery, launch, polling, and log capture
- Unity import or conversion batches
- installer staging and payload verification
- runtime-adapter reflection, binding, or exact-install discovery
- test harnesses that enumerate large governed matrices

## Forbidden Shortcuts

Do not add:

- heavy invasive unneeded code
- unbounded scans in repeated paths
- repeated allocation-heavy transforms where bounded iteration is available
- reflection, parsing, process discovery, IO, hashing, or logging inside hot loops without measurement
- synchronous blocking on UI or Editor-critical threads
- chatty diagnostics in repeated callbacks
- hidden repair or opportunistic migration
- stale-truth caches that improve speed by weakening correctness
- a compile-success claim as performance evidence

## Required Guard

For medium or high risk, require a deterministic guard that records:

- baseline or researched expected cost
- threshold or budget
- measured result
- exact command
- configuration
- machine or runtime context
- input cardinality and fixture shape
- warm or cold state when relevant

The guard must fail when the budget is exceeded. A timing anecdote is not a deterministic guard.

## Surface-Specific Proof

Use the relevant lane:

- UI or Editor responsiveness
- catalog, asset, or workspace scan complexity
- persistence and serialization throughput
- conversion-batch cardinality and process overhead
- package and installer staging cost
- adapter discovery and binding overhead
- build-time or test-harness scaling
- exact-install runtime performance only when lawful runtime evidence is required

## Hard Stops

Stop or report blocked when:

- high-risk code has no deterministic guard
- baseline, threshold, result, command, configuration, context, or cardinality cannot be recorded
- a hot path introduces unbounded work or repeated IO, reflection, logging, process discovery, hashing, parsing, or allocation without measured authority
- a cache changes authority, identity, freshness, or fail-closed semantics
- compilation or static review is used as performance proof

## Validation

Report:

- performance risk and affected surfaces
- hot paths and cardinality
- forbidden shortcuts audited
- guard or benchmark lane
- baseline or expected cost
- threshold
- measured result
- command, configuration, and context
- missing measurements or lanes
- complete, partial, or blocked status

## Runtime Proof

Local measurements and O3DE host benchmarks are not exact Fall of Avalon runtime proof. State `runtime sign-off not performed` unless the lawful exact-install runtime performance lane ran.
