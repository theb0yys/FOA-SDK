# Current Task

Goal: finish PR #260 CI verification and repair failures in the selected gates.
Classification: Routine validation-harness repair; the containing Pack Manager PR
remains Significant because of workspace admission and persistent draft recovery.
Primary owner for this repair: Editor acceptance; product owner: workspace-and-packs.

Scope: Item Viewer synthetic-fixture setup, test launch arguments, registered-pane
lookup and close/reopen polling, plus focused regression tests. No C++, engine,
workspace/pack/recovery format, dependency or public API change is included.
Branch: `codex/pack-workspace-switch-protection`; prior validated head:
`1f91bf7bcc3448fa6f738c4f60c05c627f2f2b77`.

Confirmed repairs: enumerate literal fixture entries before copying; supply the
pinned Editor's required test-case name; distinguish registered docks from floating
containers with the same title; reject deleted Qt wrappers and await dock deletion.
Regression coverage executes the actual PowerShell setup and argument blocks and
checks dock selection with duplicate titles, hidden panes and deleted wrappers.

Validation is layered. The prior head's hosted static, CanonicalInterchange,
CapabilityExecution and agent-skill checks PASSED. Local harness regression tests
PASSED. Final static/source-policy logs and the exact changed-input hashes are
retained outside source. Fresh-head hosted results are authoritative only once the
corresponding jobs finish; the final run IDs/results belong in the external CI receipt.

Local Item Viewer Editor evidence remains FAILED at close/reopen on a private
Windows desktop: refresh and product loading succeed, but the reopened pane is
removed during floating-container teardown. This is not a passing UI result.
The actual loaded SDK hash matches the previously validated build. Diagnostic
variants and native destruction stacks remain external; no engine workaround or
product behavior change is claimed by this harness repair.

Unchanged product baseline: prior pinned configure/build and all three compiled
registrations PASSED (581 cases passed, two symlink skips). Pack Manager docked
recovery, workspace switching and Editor exit passed 121 checks across 23 processes.
Those results remain bound to the prior head and unchanged compiled inputs.

Completion requires all applicable checks on the final PR head to finish and their
results to be reported accurately. PR approval and merge, workflow reruns/cancellation,
protected game data, runtime/deployment/release operations and further features remain
outside this task. No manual workflow transition is authorized or performed.
