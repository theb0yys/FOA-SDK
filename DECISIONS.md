# Durable Decisions

Record accepted FOA-SDK architecture and process decisions here. Active work belongs in `CURRENT_TASK.md`; durable decisions belong here.

## Repository State Is Authority

- **Decision:** Repository files, governing documents, current task state, durable decisions, recent diffs, and relevant code comments are authoritative. Conversation history is background only unless the repository owner explicitly overrides it for the current task.
- **Rationale:** This prevents context drift and preserves auditable continuation state.
- **Scope:** All repository work.
- **Status:** Accepted.

## Research Authority Is Required

- **Decision:** Implementation stops when exact controlling research, ownership, compatibility, validation, or next-process authority is missing, unclear, contradictory, outdated, or unproven.
- **Rationale:** FOA-SDK must not invent game facts, runtime assumptions, native identities, permissions, or architecture.
- **Scope:** Code, tests, documentation, process, packaging, adapters, and release work.
- **Status:** Accepted.

## Context-Only Process Port

- **Decision:** The Waning Realm agent operating model is ported to FOA-SDK without behavioural redesign. Only project-context substitutions are permitted.
- **Rationale:** The requested outcome is process parity, not a new workflow.
- **Scope:** Root agent policy integration and `.codex/` process assets.
- **Status:** Accepted.
