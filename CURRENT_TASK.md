# M3 - Framework Orchestrator and Repositories

Status: accepted M3 implementation provided; acceptance is recorded in the exact-head validation receipt.
Goal: connect reviewed plans and live admission to M2, with durable execution,
artifact custody, target ownership and candidate evidence projection.
Classification: Significant design; Critical/Runtime implementation.
Primary owner: capability-execution in Framework.
Owner request: "Next: M3 — the Framework Orchestrator and Repositories."

In scope now: implementation of the accepted M3 design, exact source paths,
failure/recovery behavior and acceptance lanes.
Implementation scope: docs/tainted-grail-sdk/FRAMEWORK_EXECUTION_M3_DESIGN.md.
First execution profile: M2 Windows staging-only native batch phases.
Out of scope: M4 planner adaptation, M5 full deployment pipeline, game/Unity launch,
protected installations/saves, signing/publication and unrelated SDK-client work.

Acceptance: scope accepted before source implementation; then required exact-head
static/compiled/native/Editor gates actually pass. A design is not working-service
proof. Existing M1/M2 contracts and isolated engine remain unchanged.
Current branch: codex/framework-orchestrator-m3, based on validated M2 c41be264c7.
Handoff: exact-head static, compiled, native and Editor evidence accompanies the focused M3 pull request.
Maintainer review and merge remain separate actions. M4/M5 are not started.
Runtime sign-off not performed.
