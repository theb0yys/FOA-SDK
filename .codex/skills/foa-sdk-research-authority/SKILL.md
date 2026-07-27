---
name: foa-sdk-research-authority
description: Use this skill when FOA-SDK work depends on exact controlling research, architecture, ownership, migration, runtime-profile, or process authority. It separates controlling sources from context, stops on contradiction or missing proof, and requires a Deep Research Brief instead of implementation by assumption.
---

# FOA-SDK Research Authority

Use this skill after the research sentinel and before implementation whenever the task changes code, data, contracts, schemas, configuration, build behavior, conversion, installer behavior, runtime adapters, deployment, or public process.

## First Actions

1. State the request and intended outcome precisely.
2. Map the controlling research path from request to owner, gate, validation, and next process.
3. Search the exact research, architecture, policy, ADR, schema, owner, implementation, and test files that could authorise the work.
4. Classify every source as controlling authority, supporting context, stale, contradictory, or irrelevant.
5. Record stop conditions before planning edits.

## Research

1. Read the root governance and the smallest domain research set that can authorise the request.
2. Map the request to the exact owner system, public surface, persistence or interchange boundary, and required gate.
3. Prefer the most specific reviewed repository source. Recency matters only when the newer source clearly supersedes the older one.
4. A filename match, implementation convenience, generated output, passing test, display name, chat memory, or model memory is not authority.
5. Read existing implementation and tests only to understand current state; they do not override controlling research.
6. Record the next researched stop/process. Do not invent a convenient next action.

## Authority Questions

The review must answer:

- Which exact file authorises the behavior or change?
- Which system owns the truth?
- Which systems may consume it, and are those reads or writes?
- Which public contract, schema, manifest, canonical handoff, installer, or adapter boundary applies?
- What does the owner explicitly forbid?
- Which compatibility, test, performance, artifact, manual, or runtime gates are required?
- Does the source authorise implementation now, or only further research, design, or evidence collection?

## Contradictions

When sources disagree:

1. Name the contradiction precisely.
2. Identify whether one source explicitly supersedes another.
3. Do not invent a tie-breaker.
4. If no exact authority resolves the conflict, stop implementation.
5. Produce a Deep Research Brief containing the conflicting paths, relevant sections or symbols, missing proof, and the FOA-SDK gate waiting for the answer.

## Hard Stop

Stop when:

- no controlling source authorises the requested owner or behavior
- authority is missing, unclear, stale, contradictory, or unproven
- implementation would invent architecture, native identity, runtime profile, permission, deployment behavior, or migration policy
- research describes a candidate, proposal, or future state but not current implementation authority
- a protected source would need modification without explicit current-task permission

A hard stop is a blocked research-authority result. It must not be rewritten as successful implementation.

## Deep Research Brief

Use `.codex/checklists/deep_research_brief_template.md`. Include:

- blocked decision or implementation question
- known repository facts
- all uncertain areas
- exact missing proof
- repository-relative paths and GitHub URLs when available
- highlighted sections, symbols, lines, TODOs, contradictions, or absent evidence
- protected paths to avoid
- required research-agent output
- consuming FOA-SDK gate
- next action only if authority is proven

## Validation

Static research review proves only document authority. It does not prove O3DE configure/build, Editor behavior, Unity conversion, installer behavior, runtime-adapter execution, deployment, save compatibility, or Fall of Avalon runtime behavior.

Report `runtime sign-off not performed` unless exact-install runtime evidence was actually captured.

## Handoff

Report:

- controlling authority
- supporting context
- contradictions or stale sources
- owner and forbidden domain
- implementation authority: authorised, partial, or blocked
- required downstream gates
- Deep Research Brief path when blocked
- next researched stop/process
