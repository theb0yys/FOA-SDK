# Current Task

Goal: finish PR #260 CI verification and repair failures in the selected gates.
Classification: Routine validation-harness repair; the containing Pack Manager PR
remains Significant because of workspace admission and persistent draft recovery.
Primary owner for this repair: Editor acceptance; product owner: workspace-and-packs.

Scope: Item Viewer synthetic-fixture setup, test launch arguments, registered-pane
lookup and close/reopen polling, focused regression tests, and the cold-build CI
time budget. No C++, engine, workspace/pack/recovery format, dependency or public
API change is included.
Branch: `codex/pack-workspace-switch-protection`; prior validated head:
`1f91bf7bcc3448fa6f738c4f60c05c627f2f2b77`.

Confirmed repairs: enumerate literal fixture entries before copying; supply the
pinned Editor's required test-case name; distinguish registered docks from floating
containers with the same title; await both dock and floating-container deletion;
retain the reopened dock wrapper while querying its children.
Regression coverage executes the actual PowerShell setup and argument blocks and
checks dock selection with duplicate titles, hidden panes and deleted wrappers,
plus deferred container deletion and docked close conditions.

Validation is layered. Hosted static, CanonicalInterchange, CapabilityExecution and
agent-skill checks PASSED on `027f373be3d0e2bdb42e263b6dc37603114eef65`.
The final local source passes all 22 focused harness tests, 854 discovered Python
tests (845 passed, nine Windows symlink-privilege skips, zero failures), static
validators/fixtures and all 10 enabled pinned source-policy validators.

The actual final-source Item Viewer smoke PASSED all 17 checks on a private
Windows desktop, including Refresh Assets, product loading and selection after
close/reopen. It reused the unchanged pinned build below; no new local build is
claimed. Earlier failed attempts and native event traces remain external. The
trace established separate floating-container teardown and Python wrapper lifetime
issues in the smoke; no engine or product workaround was introduced.

Cold hosted runs 34662682095 and 34664083242 reached their 180-minute job limit
while still compiling the pinned Editor. The job now allows 300 minutes, retaining
parallelism 2 and the 600-second Editor smoke timeout. Fresh-head hosted CI must
still finish; terminal run results and exact changed-input hashes belong in the
external CI receipt, not a claim based on pending or superseded jobs.

Unchanged product baseline: prior pinned configure/build and all three compiled
registrations PASSED (581 cases passed, two symlink skips). Pack Manager docked
recovery, workspace switching and Editor exit passed 121 checks across 23 processes.
Those results remain bound to the prior head and unchanged compiled inputs.

Completion requires all applicable checks on the final PR head to finish and their
results to be reported accurately. PR approval and merge, workflow reruns/cancellation,
protected game data, runtime/deployment/release operations and further features remain
outside this task. No manual workflow transition is authorized or performed.
