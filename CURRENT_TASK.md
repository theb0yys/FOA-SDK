# Quest inspection and local authoring

Status: implemented and locally validated; awaiting maintainer review.
Branch: `codex/quest-state-authoring`; base: `cc2d2162bc0cf0b0f2e32925fe4ca4df6639822e` (world PR #255).
Classification: Significant. Primary owner: content-pack-authoring. Supporting: catalog-and-identity, schemas-and-persistence, validation-and-evidence, ui-framework.

Scope: searchable saved/imported quest inspection, typed phase/objective/transition/condition/action/outcome/role editing, visual progression graph, state-key declarations, exact catalog references, transactional save/reopen, malformed-input and live Editor acceptance.
Design: [Quest authoring](docs/tainted-grail-sdk/QUEST_AUTHORING_DESIGN.md).
Usage: [Quest inspection and authoring guide](docs/tainted-grail-sdk/QUEST_AUTHORING_GUIDE.md).

The current explicit owner instruction authorizes local UI and authoring beyond historical contract-only quest gates. QuestDefinition V1 and QuestBindingManifest V1 retain their inert semantics and disabled authority flags. No native quest intake, runtime execution, save editing, deployment, engine changes or unrelated worktree changes.

Catalog reader 1–6 / writer 6 adds quest profiles, with the existing exact pre-migration backup and explicit older-version rejection. Older Editors require the pre-migration backup; later changes are not backported.

## Validation

- PASSED: static validation, 827 tests discovered with nine explicit skips, and all applicable source-policy surfaces. Final rerun retained in the private evidence pack.
- PASSED: pinned O3DE configure and SDK Core.Static, Framework.Static, Editor, Catalog.Tests and CanonicalInterchange.Tests builds. Engine pin `68683f23fb747380d3efa2424bd5f30242e9c5a2`; profile configuration. Unchanged engine artifacts were reused; this was not a clean engine rebuild.
- PASSED: compiled Catalog, 461 discovered / 459 passed / two Windows symlink privilege skips; compiled CanonicalInterchange, 39 passed. Seven new quest test groups cover creation/edit/reopen, typed state and malformed graphs, exact references, inspection/adoption, ownership/conflicts/write failure, schema-5 recovery and invalid catalog rejection.
- PASSED: six actual Editor acceptance groups through `quests_live_smoke.py`, including all ten element forms, state declarations, actor/item/location links, search, rendered graph, failed-save/dirty-draft protection, valid/invalid imported inspection, explicit adoption, reopen/edit/remove and 860×640 controls.
- PASSED: migration retained all 5,174 pre-existing canonical records and prior domain collections in a copied QA catalog and preserved an exact schema-5 backup. The normal workspace catalog remained unchanged.
- PASSED: actual screenshots visually reviewed for phase labels, directed transition, edited state row and visible save/edit controls.
- PASSED: maximum measured UI gaps 1.532 seconds for full quest save and 2.203 seconds after reopening, below the three-second budget.
- NOT_APPLICABLE: Unity conversion, installer, game runtime, deployment and release. Runtime sign-off not performed.

Private QA artifacts, copied catalog/source/evidence data, screenshots, failed iterations and machine-readable evidence remain outside source control. Initial creation/ID defects and two test expectation defects were corrected before passing compiled tests. The isolated QA project's missing shader includes and modal-confirmation test driver were corrected before the final Editor pass. Maintainer review and merge remain separate.
