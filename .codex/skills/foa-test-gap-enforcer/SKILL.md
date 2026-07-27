---
name: foa-test-gap-enforcer
description: Use for every FOA-SDK code, UI, O3DE host, plug-in, toolchain, Unity conversion, installer, runtime-adapter, migration, harness, test, or bug-fix change to prevent generic false-green validation.
---

# FOA-SDK Test Gap Enforcer

## First Actions

1. Run or reproduce `Get-AgentTestPlan.ps1`.
2. Identify owner systems, changed surfaces, immediate commands, O3DE rows, static assertions, manual Editor/Unity/installer rows, runtime rows, and non-runnable governed rows.
3. Compare required proof with existing tests.
4. Mark every gap add-now, blocked, partial, or not applicable with authority.

## Research

Read the test workflow, owner research/contracts, existing tests, and relevant host, conversion, installer, adapter, and release gates.

## Hard Stops

Stop or report partial/blocked if owner is unknown, proof from one surface substitutes for another, a required lane is missing and not added, or build success is presented as Editor/Unity/installer/runtime proof.

## Validation

Report required/existing/missing tests, commands/results, manual and non-runnable rows, and gap status.

## Runtime Proof

Local tests do not prove Fall of Avalon runtime behaviour. Without an exact-install lane, state `runtime sign-off not performed`.
