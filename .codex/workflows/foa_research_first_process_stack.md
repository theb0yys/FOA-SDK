# FOA-SDK Research Escalation Workflow

This workflow is **conditional**. It is not the default path for ordinary repository implementation.

## Use this workflow when

Use research escalation when the current task depends on consequential information that is not established by the repository or directly inspected evidence, including:

- Fall of Avalon runtime behavior, native identities, saves, or installation state;
- proprietary/external formats or executable behavior;
- third-party compatibility, licence, or version facts;
- deployment, signing, publication, permission, or security claims that are not already proven;
- a material contradiction that must be resolved before the requested implementation can be correct;
- an explicit repository-owner request for research or Deep Research.

Do not invoke this workflow merely because a change touches code, tests, documentation, or an existing accepted architecture.

## Research path

1. State the exact unanswered claim.
2. Identify the evidence lane required to answer it.
3. Inspect repository-held evidence first.
4. Use the external research method explicitly requested or appropriate to the claim.
5. Preserve distinctions between research context, static/decompilation evidence, host execution, and live runtime proof.
6. Record unresolved contradictions or missing proof.
7. Return the verified conclusion to the owning design/current task before implementation relies on it.

## Deep Research

When the repository owner explicitly requests ChatGPT Deep Research, that method must actually be used. A brief, web search, ordinary synthesis, or repository inspection is not a substitute for a returned Deep Research report.

A returned research report is context until consequential claims are checked against the underlying evidence required by the claim.

## Capability execution

For capability execution, build/package/deploy/launch/verify, rollback, execution receipts, or artifact ownership, also use the accepted capability-execution architecture. Research escalation does not itself authorize execution.

## Proof discipline

Never collapse:

- static inspection into runtime proof;
- configure/build success into Editor interaction proof;
- adapter compilation into Fall of Avalon compatibility;
- research into permission or evidence promotion;
- a hash/receipt into independent authorization.

If required evidence remains unavailable, report the affected claim as `PARTIAL`, `BLOCKED`, or `NOT_RUN` rather than inventing a substitute.
