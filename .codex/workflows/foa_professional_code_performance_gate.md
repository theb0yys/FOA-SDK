# FOA-SDK Professional Code and Performance Gate

Use this workflow when a change has a material performance risk. It is not a universal pre-edit requirement for every source or UI change.

## Rule P0: Professional, owner-scoped implementation

Avoid unnecessary broad refactors, speculative architecture, hidden state repair, stale-truth caches, and shortcut implementation.

## Rule P1: Classify performance applicability

A performance review is applicable when the change can materially affect latency, frame/UI responsiveness, startup/build time, memory, large-data scaling, file/process throughput, or repeated hot-path work.

For low-risk changes with no credible performance effect, record performance as `NOT_APPLICABLE` rather than creating a ceremonial benchmark.

## Rule P2: Hot-path audit

When applicable, inspect editor ticks, event handlers, buses, UI binding/rendering, asset scans, catalog operations, serialization, file interchange, external-process supervision, conversion, packaging, installer operations, adapter hooks, reflection, logging, and large collections.

Do not add unbounded scans, IO or logging in loops, repeated allocation-heavy transforms, repeated reflection, accidental quadratic work, or synchronous blocking on interactive paths without a justified bound.

## Rule P3: Deterministic guard for high risk

High-risk changes require a representative deterministic guard with:

- baseline or justified expected cost;
- threshold;
- measured result;
- command and build configuration;
- machine/editor/toolchain/runtime context;
- representative data cardinality.

Missing required measurement is `PARTIAL` or `BLOCKED`, not a pass.

## Rule P4: Optional planning helper

Use `.codex/scripts/Get-AgentPerformancePlan.ps1` when risk or measurement ownership is unclear. Its output is guidance and does not make performance testing applicable to an unrelated change.

## Rule P5: Evidence boundaries

Configure/build success is not measured performance proof. UI, catalog, serialization, asset processing, conversion, external tools, installer, and runtime adapters require evidence appropriate to their own surface.

## Rule P6: Handoff

Report applicability, risk, hot-path findings, benchmark/guard command and result when required, and any missing or `NOT_APPLICABLE` lane.
