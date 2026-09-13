# M4 Framework Planner Adaptation

The owner requested M4 after the Framework implementation in M3. This is the
bounded implementation of the Existing Planner Adaptation batch in
[the execution contract](CAPABILITY_EXECUTION_CONTRACT.md#existing-service-disposition).
The current instruction authorizes this preview integration; it does not
authorize M5 or game deployment.

## Ownership and scope

Classification: Significant. Primary owner: capability-execution in Framework.
The existing Core work-order, build-manifest, package-preview, deployment-preview,
deployment-work-order and post-deployment-report services continue to own their
results, canonical bytes, validation and refusal reasons. The Framework service
delegates to these services. It neither writes their registries nor adds authority
to their V1 flags. All existing serialized versions and canonicalizers remain
unchanged. No persistence migration or new dependency is introduced.

The implementation lives in:
- Code/Source/ExecutionPlanning/FrameworkPlannerService.h and .cpp;
- Code/Source/FoundationPlannerService.cpp and the FoundationService accessor;
- the existing Framework source manifest;
- existing preview-pane callers and their headers;
- focused planner tests and existing owner fixture tests, with their test manifests;
- the M3 native fixture test for the Framework preview callback boundary;
- a static planner ownership/consumer guard and its tests/local validation wiring;
- exact M1 consumer and Catalog test allowlist additions in their existing validators;
- an Editor acceptance script and focused test fixture when needed for pane proof;
- CURRENT_TASK.md, this design, CHANGELOG.md and the execution/validation guides.

All Code and Tools paths above are under Gems/TaintedGrailModdingSDK.
No M1 Core contracts, M2 contracts/supervisor, M3 persistence, provider selection,
policy, execution lifecycle or installation operations change.

## API and flow

Foundation owns a stateless FrameworkPlannerService. Existing panes call it for
domain work orders, build manifests, package previews, deployment previews,
operator work orders and post-deployment assessment. Returned typed values retain
the complete owner result, including refusals and blockers. Assessment consumes
supplied observations only; it does not perform verification or promotion.

An additive in-memory PlannerSnapshot binds owner canonical source bytes to an
M1 request through reserved, fingerprinted request options. It is a preview input,
not an execution plan, receipt, stored artifact or authorization.

BindBuild validates the request, exact pack and host profile and the owner's
ready manifest. BindPackage requires that exact manifest and preserves its
snapshot. BindDeployment requires that exact package preview and exact supplied
operator work order for the resulting target preview. Each stage returns a new
immutable snapshot with new request identity bytes; previous snapshots stay intact.
No missing stage, provider, path, digest, profile or evidence is invented.

A provider preview callback can read a source only with the exact final request
and matching source phase. Wrong workspace, pack, profile, options, source bytes,
stage, unsupported version, owner refusal or bounds fail closed. All source
canonical bytes are retained unchanged in the private in-memory snapshot. Each
source exposes an M1 PhaseExtensionReferenceV1 containing only a source contract ID
and the SHA-256 of those bytes. Its fingerprint is bound in the request option.
Legacy diagnostics contain semicolons which M1 deliberately rejects as opaque
execution payloads. The reference preserves that frozen screening and keeps
legacy text out of serialized M1 values; no encoding, sanitizing or reinterpretation
of owner bytes occurs. The reference is not proof of execution or permission.
No stored receipt or legacy Ready/Allowed field supplies live admission.

M3 still supports only its accepted staging phases. A bound deployment preview
cannot make DEPLOY executable. Providers and the complete synthetic pipeline are
M5 work. Legacy singleton registries and other consumer migration remain for their
later batches.

## Failure and compatibility

Owner refusals remain ordinary inspectable typed results. Snapshot binding fails
with an actionable diagnostic and returns no partially updated snapshot.
Canonical/fingerprint mismatch requires a new preview, never silent repair.
No writes, scans, clocks, filesystem probes, process launch, executor calls,
registration, evidence promotion or installation access occur in the adapter.
Package and deployment comparisons include canonical fields, inventory digests,
backup/rollback descriptions and operator confirmation scope/expiry.

Snapshot creation and source reads are explicit bounded worker operations. They
are not performed by pane rendering. M1 byte/string/collection limits apply to
embedded sources and request options. Domain planning keeps its existing bounded
owner behavior; adapter work is linear in the canonical bytes it handles.

## Acceptance

- L0: focused source/consumer purity guard, owner/V1 policy, diff checks.
- L1/L2: exact-pin build, nonzero Catalog/planner and M1/M3 regressions.
- Compare ready and refused owner outputs with Framework outputs; preserve all V1
  false flags, sequence order and canonical bytes.
- Bind the full build/package/deployment/work-order preview chain; reject stale
  source, wrong profile/pack/workspace, version/flag tampering, expired
  confirmations, wrong source phase and oversized values.
- Exercise an actual M3 preview callback with the bound request; a changed request
  cannot consume the earlier snapshot, and M3 deployment remains unsupported.
- Measure representative wrapper versus direct owner cost and maximum source
  binding. Guard 100 ordinary bindings within 5 seconds in Profile and reject
  values over M1 source limits. Record machine/configuration/cardinality.
- L3: existing affected panes open, refresh and close in the pinned Editor; no new
  pane, button or execution behavior is claimed.
- L4 installation/game, saves, release and deployment execution: NOT_APPLICABLE.
  Runtime sign-off not performed.

Generated build logs, receipts and fixture output remain outside source. Report
failed/skipped hosted rows separately. The focused PR remains unmerged for
maintainer review.

## Recorded local validation — 12 September 2026

Implementation acea9f1 was validated against pinned O3DE
68683f23fb747380d3efa2424bd5f30242e9c5a2 on Windows, MSVC Profile. Final documentation
updates do not alter the compiled source. Private logs, source hashes, screenshots
and the machine-readable evidence pack stay outside the checkout.

- PASSED: configure and ordered affected product/test/Editor builds. The existing
  pinned engine dependencies were reused; this is not a fresh whole-engine build.
- PASSED: static validation, 956 Python cases with 33 explicit skips, and four
  pinned source-policy selections of ten tests each.
- PASSED: Catalog (539 passed, two symlink-privilege skips), Framework contracts
  (13), Framework native operations (20), and M1 contracts (50). All 21 new M4
  compiled cases passed; no M4 case was skipped.
- PASSED: all six existing panes opened, closed and reopened with their read-only
  tables in the empty disposable project. Screenshots were inspected and the
  Editor exited normally after aboutToQuit. This proves lifecycle, not populated
  data presentation or game behavior.
- PASSED: 100 representative direct previews took 8.694 ms, the Framework route
  took 8.532 ms, and 100 source bindings took 21.964 ms. The exact 1 MiB source
  bound/read in 10.583 ms; one byte over the limit was rejected.
- NOT_APPLICABLE: game deployment, saves, runtime, release, signing and installer
  operations. Runtime sign-off not performed.

The inherited M3 hosted metadata-reopen performance check failed at roughly
49.6 seconds against a ten-second budget. The unchanged production path passed
locally at 5.551 and 5.509 seconds. M3's installer smoke itself passed; its hosted
job lost runner communication during evidence upload. These hosted failures are
not reclassified as passes by this local M4 validation.
