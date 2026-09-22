# Current Task

Status: canonical hot-path fix implemented; final local and hosted validation in progress.
Goal: make the maximum-metadata reopen test pass its unchanged 10-second budget
on the hosted Windows runner while retaining exact canonical and corruption checks.
Branch: codex/m3-metadata-reopen-performance.
Base: efa76f5f8523b8a30ec731a03fe2cda2e50bca2a.
Primary owner: capability-execution; Framework consumes M1 canonical values.
Classification: Routine contract-preserving performance fix, with the expanded
M3 persistence, operational, Editor and exact-source receipt validation required
by the owning design.

Scope: profile the repository/codec and canonical writer; change only the measured
hotspot, relevant regression/performance tests and owning scope documentation.
The canonical byte format, fingerprints, validation, admission, replay refusal,
store limits and performance threshold remain unchanged. Temporary diagnostic
instrumentation is removed before the final commit.

Acceptance: measured before/after costs at the same metadata cardinality;
malformed/tampered/round-trip regression tests; pinned configure/build, affected
compiled regressions and M3/M5 lifecycle checks; final local receipt and hosted
Windows performance proof; focused DCO-signed PR.

Out of scope: game/save writes, M6, campaign rendering/export, other Editor
features, dependency changes, branch rewrites and unrelated SDK-client edits.
The preceding integration merge is complete. No new merge or release transition
is inferred for this focused task.
