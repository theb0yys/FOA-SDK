# Aqua - Kandra Operating Handbook

Status: derivative operating reference

Purpose: consolidate the repository owner's Kandra research, evidence, review, and promotion workflow into one restartable place for FOA-SDK work.

## Authority boundary

This folder is not a new source of normative authority. It does not replace or override:

- `AGENTS.md`;
- `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md`;
- `docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md`;
- `Research/README.md`;
- the architecture, design, schema, gate, or owner document for the affected subsystem;
- maintainer review and explicit human promotion.

When this folder conflicts with an authoritative repository source, the authoritative source wins and this handbook must be corrected.

"Kandra" is the owner's shorthand for the governed path that keeps research, claims, implementation authority, evidence, validation, and human promotion separate.

## Core model

Kandra is conditional. Routine engineering inside accepted architecture follows the normal engineering process:

```text
scope
-> inspect the owning surface
-> implement
-> focused validation
-> review
```

Use the Kandra research path when consequential facts are unresolved, evidence authority is unclear, protected external material is involved, sources materially contradict each other, or the owner explicitly requests research or ChatGPT Deep Research.

The research path is:

```text
exact request
-> authority and evidence triage
-> governed research question/brief when needed
-> requested research method actually executed
-> original report preserved
-> RH0 intake
-> RH1 claim evaluation
-> RH2 domain/repository review
-> RH3 independent review
-> RH4 validation/evidence review
-> RH5 human promotion
-> normative design/current task consumes promoted result
-> implementation only when authorised
-> applicable L0-L4 validation
-> focused pull request
-> maintainer integration decision
```

A returned ChatGPT Deep Research report is E1 research context. It is not runtime proof, implementation authority, a compatibility declaration, or promotion.

## Documents in Aqua

- [KANDRA_PIPELINE.md](KANDRA_PIPELINE.md) - full end-to-end pipeline, stage entry/exit conditions, R0-R5 and RH0-RH5 distinction.
- [GOLDEN_RULES.md](GOLDEN_RULES.md) - non-negotiable operating rules.
- [EVIDENCE_AND_STATUS.md](EVIDENCE_AND_STATUS.md) - evidence lanes, L0-L4 validation layers, claim states, and exact status vocabulary.
- [DEEP_RESEARCH.md](DEEP_RESEARCH.md) - how ChatGPT Deep Research enters, returns, is preserved, and is reviewed.
- [OPERATING_CHECKLIST.md](OPERATING_CHECKLIST.md) - what must be done before, during, and after Kandra work.
- [TEMPLATES.md](TEMPLATES.md) - restartable brief, intake, claim, review, promotion, and handoff templates.
- [SOURCE_MAP.md](SOURCE_MAP.md) - controlling FOA-SDK sources and where each rule comes from.
- [GLOSSARY.md](GLOSSARY.md) - common Kandra and FOA-SDK terms.

## One-line rule

Truth takes precedence over completion: never promote a claim, test, repository transition, or runtime state beyond what was directly performed and verified.
