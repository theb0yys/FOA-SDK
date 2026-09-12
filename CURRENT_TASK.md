# Current Task

Goal: protect unsaved Item and Recipe Editor drafts when switching workspaces.
Classification: Routine implementation and freshness repair inside the existing
Foundation workspace admission contract. Primary owner: ui-framework. Foundation
workspace-and-packs owns replacement and catalog-and-identity owns existing saves.

Branch: codex/item-recipe-workspace-protection, based on verified pane-close
commit 31892e877ab089099e00c427c148cae22612bf04 (PR #267, still open at intake).

The pane reuses Save / Discard / Cancel for every retained profile, ingredient,
output and acquisition draft. Cancel, failed Save or another handler's veto keeps
the old workspace and remaining drafts. Discard clears forms only after successful
replacement, including reloads of the same workspace. No draft crosses roots.
Foundation must rebuild a candidate that shares the current root after admission,
because a Save handler may have updated its catalog and source evidence. A failed
rebuild preserves live state and permits retry; unrelated roots need no extra read.

No public contract, schema, engine, recovery, game, deployment or release change.
Each save remains a separate transaction; prior successful saves are retained if a
later form fails. Native reader cancellation follows committed replacement.
Authority: owner request, AGENTS.md, ENGINEERING_PROCESS.md, ARCHITECTURE.md,
FoundationNotificationBus.h, ITEM_RECIPE_EDITOR_GUIDE.md and CI_AND_LOCAL_VALIDATION.md.

Validation required: focused compiled candidate/admission regression tests, pinned
Editor build, actual workspace pickers in Status and Catalog Browser, multi-record
Save/Discard/Cancel, invalid input/file, failed write/retry, same-root reload,
shared-ID isolation, multiple admission handlers, and pane-close regression.
Use existing five-second workspace interaction and three-second close fixture
budgets. Tests use synthetic external roots and an inactive private Windows desktop.
Validation before main integration:
- PASSED: exact-pin SDK Editor module build and focused Catalog compiled lane;
  497 tests passed, two existing symlink-privilege tests skipped.
- PASSED: Status and Catalog Browser workspace suites, 37 checks each, actual
  dialogs and clean native exit on the matching built SDK module.
- PASSED: L0 static validation (845 Python tests passed, nine symlink-privilege
  skips) and all ten source-policy validators enabled by the pinned engine.
- PASSED: pane-close regression, 25 checks and clean native exit.
- Final integration validation is in progress.

Main advanced to 9288678c3b52913916a6d740ba8654de55978838 during verification.
Integrate that accepted baseline without changing its ExternalToolchain work,
then rebuild and verify the resulting feature branch before PR delivery.
