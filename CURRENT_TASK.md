# Current Task

Status: Core terrain input and Framework BUILD preview implemented and locally
validated. M6 remains PARTIAL. The owner requested continuation
through Core/Framework integration, production qualification and game deployment.
Branch: codex/heightmap-core-m6, based on the native qualification PR #284 head
0f06e3be8a78c851282f71a170f2f8782dc4741e. PR #284 has not been assumed merged.

This Critical/Runtime increment follows the accepted
[M6 design](docs/tainted-grail-sdk/HEIGHTMAP_VERTICAL_SLICE_M6_DESIGN.md) and the
[Core handoff contract](docs/tainted-grail-sdk/HEIGHTMAP_CORE_HANDOFF.md).
World Authoring validates the exact workspace revision and bounded source samples;
Framework binds captured input to an immutable BUILD request; Unity owns native
terrain, metadata and bundles. Canonical V1, M1 contracts and M2 isolation limits
remain unchanged. The ordinary dirty SDK-client checkout is not modified.

Acceptance for this increment: compiled import/preparation/Framework tests,
actual Unity consumption of the Framework-exported SDK fixture, full height and
collision readbacks, fresh-process reopen, negative packet/request cases,
independent audit and the required static/source-policy lanes. Test outputs and
private execution evidence stay outside the repository. All authority flags stay
false. No game or save writes are part of this increment.

Remaining M6: a qualified production process/dependency/IPC boundary, terrain
execution providers and Editor reachability, packaging and exact owned-target
deployment, game loading/observations/unload, and verified rollback. The existing
M2 profile cannot represent the observed Unity dependency set; ordinary bounded
native test execution is not LPAC qualification. There is no production process
fallback. Final game execution requires the concrete reviewed artifact, target,
side effects and recovery plan. Runtime sign-off not performed.

Validation on 13 September 2026: pinned O3DE configure and affected targets PASSED.
The full compiled Catalog suite ran 592 tests: 590 PASSED and two existing Windows
symlink tests SKIPPED. All 15 new terrain/Framework tests and the campaign-worker
failure/cancellation test passed. The static lane ran 1446 tests with 34 explicit
skips, ten tooling tests, and four sets of ten source-policy checks. Its initial
wrong runner path was corrected; that invocation is retained as NOT_RUN.

Actual Unity 6000.0.64f1 consumed the Framework-exported packet and passed native
creation, repeated bundle builds, all five 1089-sample readbacks, twenty collider
probes and fresh-process reopen. The fixed-source regression control also passed.
Maximum height error was 0.122 mm. All 27 independent auditor tests and both final
native evidence audits passed. No product UI or game-runtime pass is claimed.
