# M4 — Existing Planner Adaptation

Status: implemented and locally validated; prepared for maintainer PR review.
Goal: expose existing build, package, deployment, work-order and assessment owners
through one Framework preview service with exact source binding.
Classification: Significant; additive host API and existing pane routing.
Primary owner: capability-execution (Framework).
In scope: docs/tainted-grail-sdk/FRAMEWORK_PLANNER_M4_DESIGN.md.
Out of scope: new providers/execution profiles, game operations, M5, legacy
registry retirement, schema migration, releases and unrelated SDK-client work.
Branch: codex/framework-planner-m4; integrated main 8fa4601abc after M3 PR #272 merged.
Validated implementation: acea9f1; subsequent changes record documentation only.

PASSED: pinned O3DE configure and affected product/test/Editor builds; static
validation (956 Python cases, 33 explicit skips) and four source-policy selections.
PASSED: four compiled suites, 622 passed and two existing symlink-privilege skips.
All 21 M4-specific compiled tests passed, including the real Framework callback.
PASSED: all six panes opened, closed and reopened in an empty disposable Editor
workspace; normal process exit. Ready/refused payloads were proved by compiled
synthetic fixtures, not by empty-pane screenshots.
PASSED: bounded preview/source binding performance; exact 1 MiB source in 10.583 ms.

M3's inherited hosted metadata-reopen budget failure remains separate from local
acceptance (49.6 seconds hosted versus 5.55 seconds locally, 10-second budget).
Its installer smoke passed but hosted evidence upload lost runner communication.
No hosted M4 result is claimed here. Runtime sign-off not performed.
Next action: maintainer review of the focused M4 PR. M5 needs a new owner instruction.
