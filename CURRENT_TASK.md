# M5 — Isolated Synthetic Spine

Status: implementation and applicable local validation PASSED; exact-commit receipt
and pull-request handoff pending.
Goal: supervised Build -> Package -> Deploy -> Launch -> Verify -> Rollback in a
new disposable target, with failure, cancellation and recovery evidence.
Owner: capability-execution. Classification: Critical/Runtime.
Scope: docs/tainted-grail-sdk/FRAMEWORK_SYNTHETIC_M5_DESIGN.md.
Branch: codex/framework-synthetic-m5, based on M4 028a58f220.

Validation on 13 September 2026:
- PASSED: static validation, 960 Python cases with 33 explicit skips; additional
  10 tooling tests and four exact-pin source-policy selections.
- PASSED: affected native provider, Framework and Editor Profile builds.
- PASSED: four compiled selections, 641 passed and two explicit Catalog symlink
  skips. All 19 M5 native cases passed, including failure, cancellation, timeout,
  drift, revoked admission, recovery and full observation storage.
- PASSED: disposable exact-pin Editor success, cancel, hard crash and fresh
  confirmation recovery; six artifacts on success, restored target and joined
  workers on normal shutdown. These are programmatic Editor lifecycle checks.
- NOT_RUN: manual visual acceptance; no UI controls or pane behavior changed.
- NOT_APPLICABLE: Fall of Avalon runtime, general deployment, installer or release
  sign-off. Only repository-owned synthetic data was deployed and restored.

The owner has requested M6. Keep its terrain/provider work on a separate branch
following this prerequisite; do not treat M5 observations as terrain runtime proof.
