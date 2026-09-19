# Kandra Pipeline

Status: derivative reference. Authority remains with the owning FOA-SDK sources listed in [SOURCE_MAP.md](SOURCE_MAP.md).

## 1. Why Kandra exists

Kandra prevents five common failures:

1. research being mistaken for implementation authority;
2. one evidence lane being substituted for another;
3. generated reports losing provenance or being silently rewritten;
4. a documented "next task" being executed without owner authority;
5. agents promoting their own conclusions into accepted project truth.

The pipeline is deliberately restartable. Every stage should leave enough durable state for another reviewer to continue without relying on chat memory.

## 2. Entry decision: normal engineering or Kandra escalation

Start with the exact owner request.

Use normal engineering when the repository already establishes the consequential facts and the work stays inside accepted architecture.

Escalate into Kandra when any of these apply:

- Fall of Avalon runtime behavior, native identity, save/install state, or proprietary format behavior is unresolved;
- a third-party compatibility, version, licence, security, deployment, signing, publication, or permission fact is unproven;
- protected external data may be involved;
- controlling evidence is materially contradictory or insufficient;
- the owner explicitly requests research or ChatGPT Deep Research.

Do not add research ceremony merely because a task touches code, tests, documentation, or an existing subsystem.

## 3. Stage K0 - scope and authority triage

Record:

- exact user request and intended outcome;
- target repository and branch;
- requested repository transition;
- owner system and affected public surface;
- change classification: Routine, Significant, or Critical/Runtime;
- in-scope and out-of-scope behavior;
- consequential claims that are already proven;
- exact unanswered claim, if any;
- protected-data boundary;
- evidence lane required to answer the claim.

Exit K0 only when it is clear whether repository evidence is sufficient or research is required.

If repository evidence is sufficient, return to the normal engineering process.

## 4. Stage K1 - governed research question

When research is required, formulate the narrowest question that can unblock the owning gate.

A good research question names:

- the blocked decision;
- why it matters;
- what the repository already proves;
- what remains unknown;
- which source/evidence lane can answer it;
- protected files or operations to avoid;
- the consuming FOA-SDK gate or design;
- the required form of returned evidence.

When authority is missing, unclear, stale, or contradictory, use the repository Deep Research Brief format before implementation.

## 5. Stage K2 - execute the required research method

Method selection is binding.

If the owner requests ChatGPT Deep Research, actual ChatGPT Deep Research must be executed. A brief, ordinary web search, repository inspection, manual synthesis, static decompilation, or a different research tool is not a substitute.

Likewise:

- public research does not substitute for decompilation/static evidence;
- decompilation/static evidence does not substitute for live runtime proof;
- runtime observation does not substitute for persistence/save proof;
- repository evidence does not prove the state of an installed game.

If the required method cannot execute, the affected stage is `BLOCKED`. Do not manufacture a replacement report.

## 6. Stage K3 - preserve the returned input

Preserve the original returned report unchanged when it becomes repository research input.

Record:

- source/origin;
- intake date;
- exact path;
- fingerprint when the topic process requires one;
- version/revision or retrieval date where available;
- inaccessible, private, opaque, or conversation-local citations;
- evidence lane and known limitations.

The preserved input is not cleaned in place.

A cleaned derivative must remain labelled as a derivative and must point back to the preserved original.

## 7. RH0-RH5 review ladder

The owner-level Kandra review ladder is:

### RH0 - intake

Purpose: prove intake integrity and preserve provenance.

Required:

- original input preserved;
- origin/date/path recorded;
- citation durability problems identified;
- protected-data boundary recorded;
- research method/evidence lane identified;
- no implementation authority implied.

Exit: intake is reproducible and the original can still be audited.

### RH1 - claim evaluation

Purpose: convert a report into individually reviewable claims.

Required:

- stable claim IDs;
- direct durable sources where available;
- exact version/revision/date context;
- scoped statement of what each source supports;
- facts, inferences, proposals, unknowns, contradictions, and superseded claims kept distinct;
- source register and claim register updated as appropriate;
- consequential claims checked against underlying evidence rather than accepted because the report states them.

Exit: each material claim has an evidence state and limitation.

### RH2 - domain/repository review

Purpose: reconcile claims with the system that would consume them.

Required:

- compare with current `README.md`, roadmap, accepted architecture/design, schemas, code contracts, and owner documents;
- identify owner and forbidden domain;
- classify each relevant claim as aligned, bounded refinement, contradicted, stale, or outside the current boundary;
- identify implementation, compatibility, migration, legal, security, and runtime consequences;
- keep unresolved contradictions visible.

Exit: the domain owner can see exactly what the research would change and what remains unproven.

### RH3 - independent review

Purpose: obtain a genuinely separate review of the claim set and authority mapping.

Required:

- reviewer works from preserved inputs, registers, and controlling sources;
- reviewer checks source quality, contradictions, missing proof, scope, and lane separation;
- reviewer does not rely on the first reviewer's conclusion as proof;
- review findings are recorded, including disagreements and unresolved items.

Exit: material conclusions have survived independent scrutiny or are explicitly marked unresolved.

### RH4 - validation review

Purpose: determine whether the evidence required for the consequential claim actually exists.

Required:

- map claims to the correct evidence layer;
- select applicable L0-L4 validation;
- keep public research, static/decompilation, repository/static, compiled host, Editor/UI, operational runtime, persistence, deployment, signing, and release evidence separate;
- verify exact source/ref/artifact binding where relevant;
- record unavailable or inapplicable evidence honestly;
- reject zero-test, stale-head, pending, skipped, wrong-commit, or self-declared evidence as a pass.

Exit: evidence strength and remaining gaps are explicit.

### RH5 - human promotion

Purpose: allow a human promotion owner to decide whether reviewed research becomes normative project authority.

Required:

- material RH0-RH4 findings available;
- unresolved unknowns and adverse evidence remain visible;
- exact owner and destination named;
- schema/version/migration/security/legal/test/documentation/rollback consequences stated where applicable;
- promotion is a human decision.

Agents cannot act as final promotion owner.

Exit: either:
- promoted into an accepted normative design/decision/gate;
- rejected;
- returned for more evidence;
- left unpromoted.

No promotion means no new implementation authority.

## 8. Repository R0-R5 research gates

FOA-SDK also contains topic-specific research gates named R0-R5. They are related to Kandra but are not identical to RH0-RH5 and must not be equated by number.

The existing conversion/runtime-bridge research gate model is:

| Gate | Meaning |
| --- | --- |
| R0 | Intake integrity |
| R1 | Durable sourcing |
| R2 | Repository reconciliation |
| R3 | Authority-lane classification |
| R4 | Experiment readiness |
| R5 | Promotion candidate |

Important distinctions:

- R5 creates a promotion candidate, not an accepted decision.
- R4 means an experiment is reviewable, not that it may run.
- RH3 is independent review; repository R3 is authority-lane classification.
- RH4 is validation/evidence review; repository R4 is experiment readiness.
- RH5 requires a human promotion decision; repository R5 only permits proposing a candidate.

## 9. After promotion

Promotion is not implementation.

A promoted conclusion must be consumed by the appropriate normative owner: architecture, design, schema, gate, durable decision, or current task.

Only then may implementation proceed if the current owner request authorises it.

Implementation follows the normal engineering process:

```text
define
-> inspect
-> implement smallest coherent change
-> applicable validation
-> review
-> focused PR
-> maintainer integration decision
```

For capability execution, preserve the accepted spine when applicable:

```text
Build -> Package -> Deploy -> Launch -> Verify
```

Do not infer deployment, launch, save, signing, publication, or release authority from research.

## 10. Where Kandra feeds the mod-development lifecycle

The repository's mod-development process uses this broader lifecycle:

```text
research
-> design
-> scaffold
-> implement
-> self-review
-> independent review
-> static and compiled validation
-> controlled runtime validation
-> package review
-> release decision
-> support and compatibility maintenance
```

Kandra governs the research/evidence/authority side of that lifecycle. It does not replace design, implementation, packaging, release, or support ownership.

Practical handoff:

- Kandra research resolves consequential unknowns and produces reviewed promotion candidates.
- Human promotion moves accepted conclusions into the normative owner.
- Design converts promoted authority into an implementable contract.
- Implementation stays small, reversible, and inside the authorised boundary.
- Validation selects the evidence layer required by the changed surface.
- Runtime validation is controlled and separate from static/compiled proof.
- Packaging includes only reviewed redistributable material and binds output to exact inputs.
- Release remains a separate human/maintainer decision.
- Support changes compatibility records only from verified reports and reproducible profiles.

Missing runtime, packaging, or release evidence remains pending; it is never backfilled by earlier research.

## 11. Hard-stop conditions

Stop the affected operation when:

- required research method cannot execute;
- no controlling source authorises the requested behavior;
- consequential authority is missing, contradictory, stale, or unproven;
- implementation would invent native identity, runtime profile, permission, deployment behavior, or migration policy;
- required protected material cannot be lawfully or safely inspected;
- required evidence lane is unavailable;
- the only support is a proposal, candidate, generated report, model memory, or chat memory;
- the current task does not authorise the next repository transition.

Use `BLOCKED`, `PARTIAL`, or `NOT_RUN` as appropriate. Do not replace the blocked operation with unrequested work.
