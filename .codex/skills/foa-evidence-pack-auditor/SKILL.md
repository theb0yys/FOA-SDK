---
name: foa-evidence-pack-auditor
description: Use for every FOA-SDK change, review, validation, handoff, pull request, release, or blocked task. It requires machine-readable evidence for authority, impact, protected files, compatibility, tests, performance, artifacts, external writes, runtime proof, skipped gates, and final validation status.
---

# FOA-SDK Evidence Pack Auditor

Use before any completion, validation, PR, release, or blocked handoff claim.

## First Actions

1. Open `.codex/checklists/evidence_pack_template.json`.
2. Fill or mentally map every section before handoff.
3. Require direct evidence for every claim: command output, test name, artifact path, hash, timestamp, screenshot, log, approval, or explicit blocked reason.
4. Separate static, local runnable, O3DE host, manual Editor/Unity/installer, exact-install runtime, and non-runnable governed evidence.
5. Check that skipped, failed, partial, and blocked gates include reasons.

## Research

Read:

- root process and governance
- research-first workflow
- deep review checklist
- system test matrix
- compatibility and performance gates
- artifact/deployment gate
- owner-specific skills and release requirements
- PR template and CODEOWNERS when GitHub handoff is involved

## Required Evidence Sections

The evidence pack must cover:

- request and scope
- activated skills
- controlling research and supporting context
- contradictions, stale sources, and missing authority
- Deep Research Brief status
- systems, surfaces, consumers, and blast radius
- protected files touched, avoided, and permission status
- compatibility and migration status
- tests by executability class
- performance budget and measured result
- build, conversion, packaging, installer, and adapter artifacts
- external destinations, authority, backups, hashes, and timestamps
- GitHub branch, commit, PR, CODEOWNERS, and release status
- runtime proof status
- final complete, partial, blocked, or not-run verdict
- next researched stop/process

## Evidence Classes

Do not collapse these classes:

1. Static review: documents, source, schemas, manifests, and package shape.
2. Local runnable evidence: validators, unit tests, malformed-input tests, round trips, and deterministic guards.
3. O3DE host evidence: prerequisites, configure, builds, compiled tests, Asset Processor, or Editor-host checks.
4. Manual product evidence: Editor interaction, UI screenshots, Unity conversion, installer execution, repair, upgrade, rollback, or uninstall.
5. Exact-install runtime evidence: lawful Fall of Avalon profile and adapter execution.
6. Non-runnable governed rows: required evidence not available in the current environment.

A stronger-looking class does not automatically satisfy a different required class.

## Unsupported Claims

Reject claims such as:

- “validation passed” without exact commands and results
- “tests passed” without naming the relevant owner and lanes
- “build passed” as proof of Editor, Unity, installer, adapter, or runtime behavior
- “deployed” without authority, source and destination paths, backup, hash, and timestamp
- “runtime signed off” without exact profile, adapter, operation, expected and observed result, diagnostics, and pass/fail
- “compatible” without producers, consumers, versions, migration or rejection behavior, and proof
- “complete” while applicable gates are missing, skipped, failed, or blocked

## Hard Stops

Stop or report blocked when:

- controlling research cannot be named
- unclear authority lacks a Deep Research Brief
- validation is presented without direct evidence
- skipped gates lack reasons
- runtime proof lacks exact-install evidence
- external writes or deployment are claimed without explicit authority and verification
- evidence belongs to a different commit, configuration, target profile, or artifact
- the final verdict does not match the evidence

## Validation

Audit the completed evidence pack for:

- internal consistency
- exact command and result pairing
- evidence ownership and commit identity
- compatibility with the changed scope
- explicit failures and skipped gates
- runtime-proof separation
- final complete, partial, blocked, or not-run status

## Runtime Proof

Runtime proof requires the exact Fall of Avalon version/profile, Mono or IL2CPP adapter path, operation performed, expected and observed result, diagnostics or logs, and explicit pass/fail. Otherwise state `runtime sign-off not performed`.
