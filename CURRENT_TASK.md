# Current Task

## Status and goal

`Items and Recipes completion` - IMPLEMENTED AND VALIDATED, authorized by the owner after the item viewer and category correction in PR #248. The local authoring service loads installed item and recipe definitions, resolves exact ingredient and output links, and creates, edits, saves and reopens pack-owned definitions. Maintainer PR review is the remaining repository transition.

## Classification and ownership

Significant authoring/persistence work with Critical/Runtime validation for the external reader and compiled Editor integration. Primary owner: `content-pack-authoring`; `catalog-and-identity` owns canonical identities, `schemas-and-persistence` owns durable publication, `workspace-and-packs` owns pack/workspace context, and `unity-provider` supplies read-only observations.

The producer is the existing bounded Unity provider. Foundation validates and persists reviewed intake; Core validates typed records and joins; the Item and Recipe Editor presents those services. Existing catalog schema 2 and workspace schema 1 remain unchanged. New provider output is versioned independently.

## Bounded scope

- Read installed item/recipe component fields through the pinned isolated reader.
- Resolve Addressables GUIDs to exact container paths, preserving abstract item templates and unresolved/ambiguous references explicitly.
- Import source evidence and canonical economy records without runtime grants; refresh preserves existing authored profiles and unrelated catalog records.
- Add pack-owned item/recipe creation through Foundation and complete join selection, editing, removal, search and automatic link identity generation.
- Load saved workspace context on pane open and preserve drafts across unrelated Foundation notifications.
- Update the user guide and prove the real compiled Editor workflow.

## Boundaries and compatibility

Game files, saves, engine sources and proprietary assets are read-only. Generated metadata, screenshots and logs stay outside the checkout. No runtime adapter, deployment, release, inventory or save behavior is claimed. Unknown station, unlock, stack-limit and runtime semantics remain unknown. Existing persisted catalogs and stable IDs are preserved; no schema migration is introduced.

## Acceptance and evidence

- Real installation supplies item and recipe choices with exact resolved joins.
- New local definitions and ingredient/output changes survive save and reopen.
- Invalid/profile-mismatched/escaping input and failed writes do not publish a partial catalog; repeated import does not overwrite authored data.
- Responsive bounded reader, cancellation and real Editor selection/edit/reopen checks; applicable static, source-policy and both mandatory compiled suites.
- Focused DCO commit and PR handoff; maintainer retains approval/merge authority.

## Branch

`codex/items-recipes-completion`, based on category fix `dfe5684f7e`.

## Executed acceptance

- PASSED: 819 Python tests discovered, 810 passed and nine explicit skips; static and fixture validators passed.
- PASSED: all ten enabled pinned O3DE source-policy validators and the Windows Profile Framework, Catalog test and Editor targets.
- PASSED: mandatory Catalog suite (424 passed, two symlink-privilege skips) and Canonical Interchange suite (39 passed).
- PASSED: compiled Editor imported into an empty workspace: 3,914 native items and 356 supported recipes; created a saved mod and custom definitions; edited quantities; preserved drafts; removed/re-added links; reopened saved values; cancelled without replacing the catalog. First import took 4.562 seconds with a maximum UI timer gap of 2.688 seconds (limit below three seconds). Dropdown models are published in batches, and the existing transaction validates once before persisting evidence and catalog.
- PASSED: malformed/profile-mismatched/changed-source intake, failed catalog writes, exact link evidence, wrong-recipe removal and preservation of authored values on a fresh reader run have compiled regression coverage.
- Four crafting-bundle entries lack supported recipe fields and remain explicitly unsupported. Game runtime, deployment, save mutation and release sign-off are NOT_APPLICABLE to this authoring completion.

Private source observations, screenshots, build logs and the evidence pack remain outside the repository. The focused PR is for maintainer audit; no approval or merge is inferred.
