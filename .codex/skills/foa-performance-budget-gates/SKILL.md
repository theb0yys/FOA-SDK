---
name: foa-performance-budget-gates
description: Use for any FOA-SDK path affecting editor events, UI, catalog or asset scans, persistence, interchange, external processes, Unity conversion, packaging, installer, runtime adapters, builds, or release performance.
---

# FOA-SDK Performance Budget Gates

## First Actions

1. Run or reproduce `Get-AgentPerformancePlan.ps1`.
2. Identify hot paths, call frequency, data cardinality, allocation, IO, reflection, process, and blocking risk.
3. Find existing benchmark, guard, soak, or harness lane.
4. If no budget exists, define a researched temporary threshold or report the lane missing.

## Research

Read the performance workflow, owner research, existing tests/benchmarks, and prior evidence.

## Hard Stops

Stop or report blocked if high-risk code lacks a deterministic guard; baseline, threshold, result, command, configuration, or context cannot be recorded; hot paths add unbounded scans, repeated allocation, reflection, IO, logging, process discovery, or blocking without measured authority; or compilation replaces measurement.

## Validation

Report budget, baseline, result, command, configuration/context, cardinality, and missing lanes.

## Runtime Proof

Local measurements are not exact Fall of Avalon runtime proof. State `runtime sign-off not performed` unless that lane ran.
