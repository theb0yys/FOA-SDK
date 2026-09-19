# Kandra Source and Authority Map

Status: derivative source map.

This file shows where Kandra's rules are grounded. It is an index, not a higher authority.

## Core controlling repository sources

### `AGENTS.md`

Controls:

- authority order;
- pre-write inspection;
- research escalation triggers;
- non-`main` branch/PR delivery;
- transitions requiring explicit owner authority;
- validation truth;
- protected information;
- completion semantics.

Use it first for agent execution.

### `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`

Controls:

- Routine / Significant / Critical-Runtime classification;
- standard engineering cycle;
- when research escalation is required;
- definition of done;
- evidence-layer separation.

Key point: research is conditional. Routine work inside accepted architecture does not require the whole research stack.

### `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`

Controls:

- L0-L4 evidence layers;
- changed-surface validation matrix;
- hosted/read-only CI meaning;
- exact-head evidence;
- status reporting;
- prohibition on substituting one proof layer for another.

### `Research/README.md`

Controls research archive behavior:

- research is context, not implementation authority;
- preserve supplied inputs;
- replace chat-local citations with durable source-register entries before relying on claims;
- bind repository observations to exact paths/revisions;
- bind external claims to direct sources/version/date where available;
- separate fact/inference/proposal/contradiction/unknown;
- record experiments as plans until separately authorised;
- route implementation candidates back through normative design/review/testing/delivery.

### `.codex/workflows/foa_research_first_process_stack.md`

Controls the conditional research escalation path:

- exact unanswered claim;
- required evidence lane;
- repository evidence first;
- requested research method;
- proof-discipline boundaries;
- returned conclusion must return to owning design/current task;
- Deep Research cannot be replaced by web search or ordinary synthesis when explicitly requested.

### `.codex/skills/foa-sdk-research-sentinel/SKILL.md`

Controls research triage:

- when escalation is necessary;
- when it is not;
- how to choose evidence lane;
- what to do when Deep Research is explicitly requested;
- fail-closed behavior when evidence is unavailable.

### `.codex/skills/foa-sdk-research-authority/SKILL.md`

Controls research authority review:

- controlling vs supporting/stale/contradictory sources;
- owner/public contract/gate mapping;
- hard-stop conditions;
- Deep Research brief requirements;
- implementation-authority handoff.

### `.codex/checklists/deep_research_brief_template.md`

Repository template for a governed Deep Research brief.

### `.codex/checklists/deep_review.md`

Optional review checklist for Significant/Critical work and non-trivial compatibility/evidence/security/operational review.

### `docs/protected-files-policy.md`

Controls external game data, saves, installations, credentials, proprietary material, and other protected inputs.

Read it before any task that may touch those surfaces.

### `docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md`

Controls focused review and integration into `main`.

## Concrete research-gate example

`Research/o3de-to-unity-conversion-and-runtime-bridge/gates/RESEARCH_GATES.md` defines a topic-specific R0-R5 model:

- R0 intake integrity;
- R1 durable sourcing;
- R2 repository reconciliation;
- R3 authority-lane classification;
- R4 experiment readiness;
- R5 promotion candidate.

The file explicitly says those gates govern research quality only and do not grant implementation, installation, execution, inspection, deployment, runtime, save, licensing, or publication authority.

## Concrete intake/source/claim example

`Research/world-authoring-terrain-heightmap/` demonstrates the current durable pattern:

- preserved reports under `inputs/`;
- cleaned derivatives under `intakes/`;
- `SOURCE_REGISTER.md`;
- `CLAIM_REGISTER.md`;
- research briefs under `briefs/`;
- explicit unknown/contradicted state;
- no-guess evidence boundary;
- returned Deep Research reports treated as E1 context;
- separate static/decompilation evidence;
- explicit promotion boundary.

Useful examples:

- `Research/world-authoring-terrain-heightmap/README.md`
- `Research/world-authoring-terrain-heightmap/intakes/DR_TH_001_CLEAN_INTAKE.md`
- `Research/world-authoring-terrain-heightmap/SOURCE_REGISTER.md`
- `Research/world-authoring-terrain-heightmap/CLAIM_REGISTER.md`

## Owner-level Kandra convention

The repository owner's Kandra review ladder is:

```text
RH0 intake
RH1 claim evaluation
RH2 domain review
RH3 independent review
RH4 validation review
RH5 human promotion
```

This owner-level RH ladder complements repository-specific R0-R5 research gates. The two numbering systems must not be treated as synonyms.

The owner-level Deep Research path is:

```text
governed brief
-> actual Deep Research execution
-> returned report
-> preservation/intake
-> claim review
-> required evidence/validation
-> human promotion
```

A returned report remains E1 context until its consequential claims gain authority through appropriate evidence and review.

## Authority order for Kandra use

When applying this handbook:

1. current explicit owner instruction;
2. legal/licence/security/protected-file restrictions;
3. `AGENTS.md`;
4. engineering process;
5. current task context;
6. accepted durable decisions;
7. owning architecture/design/schema/folder policy;
8. validation policy;
9. this Aqua derivative handbook;
10. historical reports and chat context.

If Aqua ever differs from a higher source, fix Aqua rather than using it as a tie-breaker.
