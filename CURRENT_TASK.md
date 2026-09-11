# Asset and Localisation Manager

Status: local implementation and validation complete.

The Manager provides project-owned PNG/JPEG images, translated text and assignments to items, actors and quests, with visible previews, save/reopen and actionable errors.

Classification: Significant. Primary owner content-pack-authoring; supporting owners catalog-and-identity, schemas-and-persistence, workspace-and-packs, artifact-ownership and ui-framework.

Catalog schema 7 adds images, localisation entries and presentation bindings. Schemas 1–6 remain validated load inputs; upgrades preserve a verified exact backup. Workspace/pack and runtime contracts and the O3DE pin are unchanged.

Validation: all static validators and four ten-check source-policy suites passed. Python discovered 831 tests (822 passed, nine explicit skips). Core, Framework, Editor, Catalog and Canonical targets built against the pinned O3DE host. Catalog: 474 passed, two explicit Windows symlink skips. Canonical: 39 passed. Actual Editor acceptance passed image/text previews, language fallback, duplicate keys, item/actor/quest assignments, consumer previews, missing-file errors, failed-save recovery, reopening, draft cancellation and small-window layout. Maximum measured UI timer gap: 0.157 seconds on the synthetic acceptance fixture.

Design and guide: docs/tainted-grail-sdk/ASSET_LOCALISATION_MANAGER_DESIGN.md and ASSET_LOCALISATION_MANAGER_GUIDE.md. Reproducible Editor fixture and smoke scripts are under Gems/TaintedGrailModdingSDK/Tools/editor_tests/.

Out of scope: native game extraction, mesh/audio conversion, translation import/export, packaging/deployment, game/save writes, release and other worktrees.

Delivery branch: codex/asset-localisation-manager, based on main 7720e036ebd8a946364bbd5b4ad3de6bfe1912ce. Submit through a focused PR for maintainer audit; no new merge or follow-on milestone is inferred.
