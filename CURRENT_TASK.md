# Current Task

Completed: protect unsaved Item and Recipe Editor drafts when switching workspaces.
Classification: Routine implementation and freshness repair within the accepted
Foundation workspace admission contract. Primary owner: ui-framework. Foundation
workspace-and-packs owns replacement; catalog-and-identity owns existing saves.

Branch: codex/item-recipe-workspace-protection. Includes the pane-close prerequisite
31892e877ab089099e00c427c148cae22612bf04 from PR #267 and integrates accepted main
9288678c3b52913916a6d740ba8654de55978838. Only the task document conflicted.

Save / Discard / Cancel covers every retained profile, ingredient, output and
acquisition draft. Cancel, failed Save or another handler's veto preserves the
old workspace and remaining drafts. Discard clears forms only after successful
replacement, including same-workspace reloads. Matching record IDs cannot carry
drafts across roots. Each form saves separately; earlier successes stay saved.

Foundation rebuilds a candidate sharing the current canonical root after admission
so newly saved catalog/source evidence is published. Failure preserves live state
and releases the guard for retry. Other roots need no extra candidate read. Native
reader cancellation and form reset follow committed replacement.

No public API, schema, engine, crash recovery, game, deployment or release change.
Authority: owner request, AGENTS.md, ENGINEERING_PROCESS.md, ARCHITECTURE.md,
FoundationNotificationBus.h, ITEM_RECIPE_EDITOR_GUIDE.md,
WORKSPACE_ATOMICITY_AND_SCHEMA.md and CI_AND_LOCAL_VALIDATION.md.

Final validation on the integrated build:
- PASSED: exact-pin configure and profile builds of Editor, SDK Editor module and
  Catalog tests on O3DE 68683f23fb747380d3efa2424bd5f30242e9c5a2. The known Windows
  generated-include path limit required a temporary build-only drive mapping;
  no engine source changed and the mapping was removed.
- PASSED: 497 compiled tests, including five new candidate/admission regressions.
  Two existing path-policy tests skipped for unavailable symlink privilege.
- PASSED: Status and Catalog Browser workspace suites, 37 checks each; 25 close
  regression checks covering all 45 editable fields and actual docked/floating
  close controls. All three Editor processes exited normally with matching module
  hashes. Maximum interactions: workspace 1.125s / 5s budget; close 0.609s / 3s.
- PASSED: L0 static validation, 855 Python tests; nine existing symlink-privilege
  skips. All ten source-policy validators enabled by the engine pin passed.
- PASSED: focused diff and protected-file audit; unrelated original-checkout work
  preserved. Synthetic fixtures, screenshots, logs and hashes remain outside source.
- NOT_APPLICABLE: game/runtime, installer, deployment and release operations.
  Runtime sign-off not performed. Human manual UI testing NOT_RUN; actual Editor
  UI acceptance was automated on an inactive private Windows desktop.

The guide and workspace contract document the behavior and acceptance commands.
Submit through a PR for maintainer review; no approval or merge is authorized.
