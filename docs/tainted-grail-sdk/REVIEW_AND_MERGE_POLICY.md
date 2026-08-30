# Review and Merge Policy

## Purpose

Review proves that a change is correctly scoped, appropriately validated, and safe to integrate. Review depth follows the change classification in `ENGINEERING_PROCESS.md`.

Command-level validation requirements live in `CI_AND_LOCAL_VALIDATION.md`.

## Routine changes

Routine changes require:

- focused scope;
- self-review of the complete diff;
- DCO-signed commits;
- automatic/read-only repository validation applicable to the changed paths;
- focused tests for changed behavior;
- documentation updates when public behavior or instructions changed;
- maintainer review before merge.

Routine changes do **not** require an O3DE host build, exact-head receipt, Windows UI evidence, runtime proof, installer evidence, or release evidence unless the changed surface actually requires that layer.

## Significant changes

Significant changes require all applicable Routine requirements plus:

- a short reviewed design or durable decision before implementation;
- compatibility and migration/rejection behavior when public contracts or durable data change;
- configure/build/compiled evidence when the changed surface participates in those layers;
- expanded negative/malformed/failure coverage appropriate to the contract;
- explicit dependency/licence review for new dependencies.

## Critical/Runtime changes

Critical/Runtime changes require all applicable Significant requirements plus:

- explicit threat, permission, and operational boundaries;
- rollback/recovery design where mutation can occur;
- exact-head operational evidence from every affected execution layer;
- runtime/deployment/installer/signing/release evidence only for operations actually affected;
- an explicit maintainer merge decision.

Static inspection or compilation cannot satisfy live operational evidence.

## Pull-request requirements

Every pull request should state:

- one change classification;
- summary;
- scope and out-of-scope behavior;
- design/architecture impact where applicable;
- compatibility/data/rollback impact where applicable;
- exact validation that actually ran;
- security/protected-data impact;
- documentation changes.

The PR template is a communication aid. Read-only CI validates the repository and reviewed range; it does not infer that every possible validation layer applies to every PR.

## Required merge conditions

A pull request may merge when:

- classification and scope are correct;
- all **applicable** validation for the changed surface passed;
- DCO requirements are met;
- blocking review findings are resolved;
- relevant documentation and migration/rollback notes are current;
- no unresolved security, legal, data-loss, or architecture concern remains;
- a maintainer approves.

Pending, queued, skipped, absent, stale-head, wrong-event, wrong-commit, or zero-test results are not passes.

## Exact-head evidence

Exact-head receipts are required for Critical/Runtime changes and for Significant changes whose owning design explicitly requires them. They are optional evidence for Routine work.

A receipt records executed evidence. It is not a signature, maintainer approval, or runtime proof beyond the commands it contains.

## Review focus by risk

### Routine

Review correctness, scope, focused regression coverage, and documentation.

### Significant

Also review contract boundaries, compatibility/migration, failure behavior, dependencies, and build/test integration.

### Critical/Runtime

Also review threat model, permissions, adversarial input, rollback/recovery, protected data, exact operational evidence, and failure containment.

## Prohibited merge behavior

Do not:

- merge failing required checks;
- describe static-only, queued, skipped, absent, stale, or zero-test evidence as a pass;
- delete tests or weaken validation solely to obtain green status;
- merge unresolved security/legal/data-loss issues;
- infer runtime compatibility or permission from research, source inspection, or compilation alone.
