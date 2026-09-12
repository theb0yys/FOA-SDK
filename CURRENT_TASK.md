# Current Task

Status: PASSED for PR #260 conflict resolution and targeted acceptance. Expanded
coverage is PARTIAL only for the Windows symlink-privilege skips listed below.

Goal: resolve PR #260 against main and validate the combined Pack Manager workspace
protection, draft recovery and docked close behavior.

Classification: Routine reconciliation inside accepted contracts. The containing PR
is Significant because it adds workspace admission and a separate recovery format.
Primary owner: workspace-and-packs; consumer: Editor acceptance.

In scope: merge main at `66549462b37d4ede7f6e6295717b0cdce73f7807` into
`codex/pack-workspace-switch-protection`, preserving PR #262 already integrated at
`39df0a1d021c98bcb09af450d1af97047c3910bd`. The only textual conflict was this
active-task record. Foundation declarations, source inventories and validators
combined cleanly. Main's asset/localisation and M1 contracts remain intact.

Out of scope: new features, contract redesign, external engine changes, game data,
installer/deployment/runtime/release operations, PR approval or merge into main.

Acceptance: PASSED static validators, fixtures, 10 enabled pinned source-policy
checks, pinned configure, SDK and test builds, complete Editor consumer build and
AssetProcessorBatch build. All three compiled CTest registrations passed: 581 cases
passed and two symlink cases skipped. The 37 focused workspace/persistence/recovery
cases all passed. Python: 839 passed, nine symlink cases skipped, zero failures.
Editor: docked recovery 12 processes/50 checks; workspace switching two/44; exit
nine/27. Final runs had four intentional checkpoint-confirmed terminations, 19 clean
exits and no unintended crashes or forced cleanup.

Evidence: external logs, per-process receipts, screenshots and source/module hashes.
UI runs used private desktops, synthetic local fixtures, a prepared cache and external
launch wrappers isolating the Asset Processor connection. An initial pre-initialization
shader-load crash is retained as FAILED; the complete host rebuild and isolated
connection preceded the passing rerun. A unique cause was not independently proven.
No new product behavior or engine source change was needed to resolve the conflict.

Current branch: `codex/pack-workspace-switch-protection`.

Next action: maintainer review of the updated PR #260. Approval and merge into main
remain separate; no further development task is started by this reconciliation.
