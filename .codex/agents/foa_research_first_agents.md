# FOA-SDK Specialist Agent Roles

Use these roles when a Significant or Critical/Runtime change benefits from parallel review, or when a focused research escalation needs specialist analysis.

They are optional coordination roles. They do not replace the repository owner, `AGENTS.md`, `ENGINEERING_PROCESS.md`, the owning architecture, or maintainer review.

- **Research authority agent:** resolves a specific external-fact or evidence-authority question.
- **Impact classifier agent:** maps affected paths, owners, consumers, and blast radius.
- **Compatibility agent:** reviews public contracts, schemas, persistence, interchange, configuration, dependencies, packages, and migration.
- **Test-gap agent:** maps required evidence to existing tests and identifies missing lanes.
- **Performance-budget agent:** evaluates material hot-path, scale, latency, memory, or build-time risk.
- **Evidence-pack agent:** audits a structured evidence pack for a complex/high-risk change.
- **PR/release agent:** reviews the focused repository or release handoff.

The primary agent remains responsible for scope, implementation decisions, truthful validation reporting, and stopping at the transition authorized by the repository owner.
