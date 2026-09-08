# Current Task

## Status and goal

Actor and Troop Editor completion: IMPLEMENTED AND VALIDATED. The owner requested supported game actor loading, local actor/troop creation and editing, member addition/change/removal, available portraits and real Editor save/reopen/error acceptance. Spawn and Encounter Editor remains a later task.

Branch: `codex/actors-troops-completion`, based on `c9be3e262bfb3f7f15f8d768fe28766e0fb430e8`. The prerequisite Items and Recipes PR #249 is merged. Deliver this focused DCO commit through a new PR for maintainer audit; no approval or merge is inferred.

## Classification and ownership

Critical/Runtime for the bounded external reader, with Significant authoring commands and UI. Primary owner: `content-pack-authoring`. Supporting owners: `unity-provider`, `catalog-and-identity`, `schemas-and-persistence`, and `workspace-and-packs`.

The pinned isolated provider publishes version-1 actor observations. Foundation validates exact profile, source hashes, evidence and protected paths before durable catalog publication. Core validates actor/troop definitions and atomic membership changes. Editor widgets present these services and portable local portraits.

## Completed scope

- Load supported installed NPC templates, preserve exact native identity and authored values on refresh, and report unsupported entries without guessed enum meanings.
- Create pack-owned actors and valid troops with a first leader; edit typed profiles and add/change/remove members with automatically generated link identity and local authoring intent.
- Keep troop upserts additive unless explicit owned member IDs are removed; validate the complete composition before durable publication.
- Preview bounded workspace PNG/JPEG portraits, clear stale or invalid images, and preserve portable references on reopen.
- Protect dirty drafts, provide cancellation and actionable save errors, fit long record identities within the pane, and populate collapsed review tables when expanded.

## Compatibility and boundaries

Catalog schema 2, workspace schema 1 and persisted stable IDs remain unchanged. The troop command's default-empty explicit removal collection preserves existing additive callers. UnityPy remains pinned at 1.24.2; the actor worker selects its supplied pure-Python type-tree fallback to avoid a native decoder shutdown failure observed on NPC managed references.

The supported NPC component supplies no portrait, model or localisation binding. Native enum meanings and equipped character appearance remain unresolved; local image preview does not reconstruct Unity prefabs. Game files, saves, engine sources and proprietary material are read-only. Private observations, screenshots and generated output stay outside the checkout. Spawning, encounters, deployment, game execution and release are outside this task.

## Executed acceptance

- PASSED: 827 Python tests discovered, 818 passed and nine explicit platform/privilege skips; applicable static and fixture validators passed.
- PASSED: all ten enabled source-policy validators against O3DE pin `68683f23fb747380d3efa2424bd5f30242e9c5a2`.
- PASSED: Windows Profile configure, Core, Framework, Catalog.Tests and Editor builds.
- PASSED: Catalog compiled suite (429 passed, two symlink-privilege skips) and Canonical Interchange suite (39 passed).
- PASSED: real provider read 885 supported NPC templates. Nine level-zero templates and 64 non-NPC entries remain explicitly unsupported.
- PASSED: eight live Editor checks with 3,914 existing items and 356 recipes cover initial native intake, deferred review-table expansion, local creation, exact portrait pixels, member addition/change/removal, invalid-save rollback, dirty drafts, cancellation, saved-workspace reopening and pane width.
- PASSED: first mixed-catalog intake took 3.046 seconds; maximum UI timer gap was 1.938 seconds, below the three-second limit. The initial failing large-catalog result was corrected by deferring collapsed review tables; full save validation remains mandatory.
- NOT_APPLICABLE: FoA runtime, spawning, deployment, save mutation and release sign-off. Runtime sign-off was not performed.

Private logs, source observations, screenshots and machine-readable evidence remain outside the repository. Maintainer review is the next repository transition. No later feature is started by this task.
