# Mod Package Builder and Export

Status: implemented and locally validated; exact-commit receipt and PR record the final handoff.
Goal: preview and export a selected local mod and its declared workspace dependencies; verify and reopen the portable package into a new workspace with definitions, images, translated text and assignments intact.
Classification: Critical/Runtime for the archive input and filesystem boundary; no game runtime operation.
Primary owner: capability-execution. Supporting owners: content-pack-authoring, packaging-preview, artifact-ownership, workspace-and-packs, schemas-and-persistence and ui-framework.

In scope: bounded local package planning/provider, explicit export and import, deterministic inventory and checksums, dependency/reference and image validation, clean-workspace reconstruction, Editor workflow, negative/round-trip/performance tests and documentation.
Out of scope: native game payload export, native override ownership inference, arbitrary file packaging, Unity/native builds, deployment, game/save writes, signing, publication and existing-workspace merge.

Acceptance: compiled round trips cover all supported authoring collections; actual Editor preview/export/reopen preserves images/text/assignments. Corrupt, future, missing, stale, unsafe and failure paths are rejected. Existing outputs are preserved. Background responsiveness and cancellation/close recovery were measured.
Branch: codex/mod-package-export, based on Asset Manager commit 252998a3f8fe1c6ad89b4d34070167fb27402f1f (PR #259 prerequisite).

Validation performed:
- PASSED: static validation with pinned-host source policy.
- PASSED: pinned-host configure and SDK/consumer builds.
- PASSED: catalog compiled suite, 485 discovered, 483 passed and two existing Windows symlink-creation skips; canonical interchange, 39 passed.
- PASSED: actual Editor asset prerequisite and seven package workflow checks, including export/import/open, image preview, existing-target preservation, corrupt-file recovery, cancellation and busy-window close/reopen.
- NOT_APPLICABLE: installer, deployment, Fall of Avalon runtime, signing and release. This package contains editable authoring data.

The exact-head receipt, private logs/screenshots and final protected-file audit accompany the PR handoff. Maintainer approval and merge remain separate.
