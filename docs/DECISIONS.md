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


## Agent Process Parity Validator

- **Decision:** The FOA-SDK root Codex pack must carry the same process-validation layer as the Waning Realm source model: restartable state handoff, skill index, research-first stack, preflight helpers, test/performance/artifact gates, evidence templates, behavioural evals, pull-request template checks, CODEOWNERS checks, and forbidden source-context detection.
- **Rationale:** Process parity is not proven by file presence alone. The structural validator must catch missing integration wording, stale source-context terms, missing GitHub handoff surfaces, broken helper output, and incomplete skill eval coverage.
- **Scope:** `CURRENT_TASK.md`, `DECISIONS.md`, `.codex/`, and GitHub handoff surfaces named by the active task. This does not change product source, runtime behavior, public data formats, deployment authority, or release authority.
- **Validation:** `.codex/skills/tests/Validate-AgentSkills.ps1` is the primary structural gate. Validators must still detect forbidden source-project terms without storing those terms literally in `.codex` files where self-scanning would fail.
- **Status:** Accepted by current process-port task direction.

## Unity And Tainted Grail Domain Skill Coverage

- **Decision:** The FOA-SDK root Codex pack requires explicit domain skills for Unity authoring and Tainted Grail modding. Unity Editor package, metadata, GUID, fixture, and test-project work must not collapse into runtime bridge proof. Tainted Grail handbook, hook, profile, package, source-port, Mono, and IL2CPP work must not bypass exact evidence, route separation, source-licence, protected-data, or no-runtime-authority checks.
- **Rationale:** The existing Unity bridge gate protects conversion, external-process, adapter, deployment, and runtime boundaries, but it does not fully describe Unity authoring/test-project or Tainted Grail modding handbook/source-port/profile escape paths.
- **Scope:** `.codex/skills/foa-unity-authoring-gates/`, `.codex/skills/foa-tainted-grail-modding-gates/`, `.codex/skills/README.md`, `.codex/README.md`, `.codex/scripts/Get-AgentSkillPlan.ps1`, and skill validators. This does not authorize Unity execution, FoA runtime behavior, game-project access, deployment, save mutation, signing, publication, catalog mutation, or evidence promotion.
- **Validation:** `Get-AgentSkillPlan.ps1` must select these domain skills from request text or target paths, and `Validate-AgentSkills.ps1` must require their metadata and eval coverage.
- **Status:** Accepted by current process-hardening task direction.
