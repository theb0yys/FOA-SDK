# FOA-SDK Test Gates

Use this workflow when test ownership, evidence separation, or missing coverage needs explicit analysis. The authoritative applicability matrix is `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`.

## Rule T0: Match proof to the changed surface

One generic command cannot prove mixed surfaces. Select only the evidence layers the change can affect, and keep repository/static, unit/contract, compiled host, Editor/UI, and operational/runtime evidence distinct.

## Rule T1: Ground tests in owned behavior

Tests should trace to the owning contract, publisher/consumer boundary, lifecycle and persistence obligations, malformed/degraded input, and failure behavior.

## Rule T2: Focused coverage first

Run the affected system's focused tests. Broader packs apply when a shared contract, foundation spine, schema/migration boundary, build graph, release surface, or harness authority actually changes.

Missing targeted tests must be added with the change or reported as `PARTIAL` or `BLOCKED` when they are required for the claim.

## Rule T3: Optional planning helper

Use `.codex/scripts/Get-AgentTestPlan.ps1` when the affected systems or available commands are unclear. Its output is guidance and does not override the engineering-process classification or validation matrix.

## Rule T4: Runtime evidence

Runtime-required claims need lawful evidence from the exact profile, adapter, operation, expected result, observed result, diagnostics, and outcome. Otherwise state `runtime sign-off not performed`.

## Rule T5: Handoff

Report the commands that actually ran, their result, required but missing lanes, and any `NOT_RUN` or `NOT_APPLICABLE` evidence layer. Do not require unrelated proof merely because a helper lists it.
