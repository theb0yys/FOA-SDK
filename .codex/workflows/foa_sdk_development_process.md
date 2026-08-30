# FOA-SDK Agent Development Adapter

The public engineering workflow is `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`. This file exists only to map that workflow to agent execution.

## 1. Scope and classify

- Read the repository owner's current request.
- Read `AGENTS.md` and `CURRENT_TASK.md`.
- Identify the owning system and exact files needed.
- Classify the change as Routine, Significant, or Critical/Runtime.

Do not automatically turn a Routine change into a research, design, performance, or release exercise.

## 2. Inspect the owner surface

Read the architecture/design/local README that directly controls the files being changed. Inspect existing implementation and focused tests.

Use the research escalation workflow only when the task actually depends on unresolved consequential external facts.

## 3. Implement

- use a focused non-`main` branch;
- make the smallest coherent change that satisfies the request;
- preserve architecture boundaries;
- do not perform unrelated cleanup or later milestones.

## 4. Validate

Select validation from `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`.

Run and report only applicable layers. A narrower layer never substitutes for a broader layer, and an unrelated broader layer is not required just for ceremony.

## 5. Handoff

Report:

```text
Status:
Summary:
Files changed:
Validation:
Protected files:
Known limitations / not run:
Repository transition:
```

Use exact states (`PASSED`, `FAILED`, `PARTIAL`, `BLOCKED`, `NOT_RUN`, `NOT_APPLICABLE`) and do not claim acceptance, merge, runtime behavior, or proof that did not occur.
