# Project Governance

## Purpose

This document defines decision authority for the Tainted Grail Modding Editor and SDK. Day-to-day engineering workflow is defined by [ENGINEERING_PROCESS.md](docs/tainted-grail-sdk/ENGINEERING_PROCESS.md), and validation requirements are defined by [CI_AND_LOCAL_VALIDATION.md](docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md).

The project is maintainer-led.

## Principles

Project decisions prioritize:

1. user safety and data integrity;
2. evidence-backed Fall of Avalon knowledge;
3. clear ownership and exact identity;
4. maintainability and testability;
5. legal and licence compliance;
6. interoperability without collapsing architecture boundaries;
7. engineering progress with rigor proportional to risk.

Working code is necessary but is not sufficient when a change weakens safety, identity, compatibility, evidence, or maintainability.

## Roles

### Users

Users configure workspaces, author content, import evidence, report defects, and provide workflow feedback.

### Contributors

Contributors may submit code, documentation, tests, designs, and legally distributable fixtures. They follow `CONTRIBUTING.md` and the engineering process.

### Reviewers

Reviewers evaluate correctness, scope, architecture, compatibility, risk, tests, and documentation. Review comments should distinguish blockers from optional suggestions.

### Maintainers

Maintainers may:

- approve or reject designs and pull requests;
- set roadmap priority;
- resolve technical disagreements;
- manage releases and repository administration;
- revert unsafe or defective changes;
- appoint reviewers or maintainers.

`CODEOWNERS` identifies default review ownership where configured.

## Change authority

FOA-SDK uses three engineering classifications.

### Routine

Existing-architecture implementation, bug fixes, tests, internal refactors, build repairs, and ordinary documentation normally proceed directly to implementation and focused review.

### Significant

New public APIs, subsystems, persistence/schema changes, dependencies, architecture boundaries, or substantial build behavior require a short reviewed design or durable decision before implementation.

### Critical/Runtime

Deployment, process execution, save mutation, runtime adapters, signing, publication, permissions, security-sensitive operations, or Fall of Avalon runtime claims require explicit design/threat boundaries and the exact proof appropriate to those operations.

The full classification rules are in `ENGINEERING_PROCESS.md`.

## Architecture invariants

The following remain project invariants unless explicitly changed through a Significant or Critical architecture decision:

- O3DE is the authoring host; Fall of Avalon is a separate Unity runtime target.
- FOA-SDK is the product repository; the pinned O3DE source checkout is external.
- Editor and knowledge-layer code does not silently execute gameplay mutations.
- Native execution belongs to separately reviewed execution/runtime boundaries.
- Display names are not identities.
- Native references remain exact.
- Synthetic identities are product/pack-owned.
- Evidence, claims, reviewed records, validation, permission, execution outcome, and promotion remain distinct.
- Missing proof cannot be represented as successful proof.

## Branch and review model

`main` is the reviewed integration branch.

Work is performed on focused non-`main` branches and enters `main` through pull requests. `foa-development` may be used by maintainers as a convenience branch, but it is not a required base or a second source of truth.

Direct development on `main` is prohibited unless the repository owner explicitly authorizes an exception for the current task.

## Merge authority

A pull request may merge when:

- its scope and classification are clear;
- the validation required for that classification and changed surface has passed;
- DCO requirements are satisfied;
- blocking review findings are resolved;
- relevant documentation and migration/rollback notes are current;
- a maintainer approves the change.

The repository does not require irrelevant host, UI, runtime, installer, or release evidence for a change that cannot affect those surfaces.

Pending, skipped, absent, stale-head, wrong-commit, or zero-test results are not passes.

## Security and emergency changes

Security or active data-loss fixes may use a narrower private review path when disclosure itself would increase risk. The mitigation must remain scoped, preserve provenance, and receive the strongest practical review and validation.

## Conflicts of interest

Reviewers and maintainers disclose material conflicts and recuse when impartial review is not practical.

## Amendments

Governance changes require an explicit repository-owner or maintainer decision, a focused pull request, and review appropriate to the impact. Process changes should update the single owning document rather than duplicating the same rule across multiple files.
