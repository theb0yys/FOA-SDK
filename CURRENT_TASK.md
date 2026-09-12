# Current Task

Goal: protect unsaved Item and Recipe Editor drafts when closing the pane.
Classification: Routine Editor/UI behavior inside the existing authoring boundary.
Primary owner: ui-framework (ItemRecipeEditorWidget); catalog-and-identity and
Foundation remain the owners of existing save validation and persistence.

Scope: Save / Discard / Cancel for item profiles, recipe profiles, ingredient and
output forms, and acquisition fields, including retained drafts for other selected
definitions. Cancel, Escape, and failed saves keep the pane open and preserve
remaining drafts. Each existing save command remains a separate transaction;
earlier successful saves are retained if a later draft fails. No format, public
contract, engine, game, workspace admission, or crash-recovery change is included.

Branch: codex/item-recipe-close-protection, based on main 9264f11d95.
Authority: owner request, AGENTS.md, ENGINEERING_PROCESS.md, ARCHITECTURE.md,
ITEM_RECIPE_EDITOR_GUIDE.md and CI_AND_LOCAL_VALIDATION.md.

Validation: focused compiled catalog/economy tests, pinned Editor build and actual
native docked/floating pane close tests with synthetic external fixtures, plus L0
static and pinned source-policy checks. Required cases cover clean and reverted
forms, Cancel/Escape, Discard, multi-definition Save, manual saves, invalid data,
persistence failure, partial success and retry. Validation PASSED: pinned Editor configure/build; the focused compiled Catalog
registration (492 cases passed, two symlink-privilege skips); static validation
(854 discovered Python tests: 845 passed, nine symlink-privilege skips, zero
failures); all 10 enabled pinned source-policy validators; and 25 actual Editor
acceptance checks across docked/floating panes with 45 editable fields exercised
in each mode and verified clean process exit. The receipt binds the final source
inputs to the loaded module SHA-256 and external test artifacts. Host tests cover
all four definitions, partial saves, acquisition evidence, real catalog locking,
and retry. No FoA runtime or deployment result is claimed.
Generated fixtures, logs and evidence remain outside source. No protected game
inputs, runtime, deployment, release, workflow or maintainer merge action is needed.
