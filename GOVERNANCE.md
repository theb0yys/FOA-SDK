# Project Governance

## Purpose

This document defines how the Tainted Grail Modding Editor and SDK makes decisions, reviews changes, assigns responsibility, and protects the project's architecture and public trust.

The project is currently maintainer-led. Governance may evolve as the contributor base grows.

## Principles

Project decisions prioritise:

1. user safety and data integrity;
2. evidence-backed FoA knowledge;
3. clear ownership and exact identity;
4. maintainability and testability;
5. transparent public reasoning;
6. legal and licence compliance;
7. interoperability without collapsing architectural boundaries.

Working code alone is not sufficient when a change weakens safety, evidence, identity, compatibility, or maintainability.

## Roles

### Users

Users configure workspaces, create packs, import evidence, report defects, and provide workflow feedback.

### Contributors

Contributors submit issues, designs, code, documentation, tests, and legally distributable fixtures. Contributors follow the contribution, quality, conduct, and review policies.

### Reviewers

Reviewers evaluate correctness, architecture, safety, evidence, data formats, compatibility, tests, and documentation. Review comments must distinguish required changes from optional suggestions.

### Maintainers

Maintainers may:

- approve or reject designs and pull requests;
- define roadmap priority;
- manage releases, labels, milestones, and repository settings;
- enforce conduct, security, quality, and scope;
- resolve technical disagreements;
- revert unsafe or defective changes;
- appoint or remove reviewers and maintainers.

Current CODEOWNERS entries identify the default review owner for project-controlled paths.

## Decision model

### Routine changes

Small fixes, documentation corrections, tests, and internal refactors may proceed through normal pull-request review.

### Significant changes

Significant changes require a reviewed issue or design proposal before implementation. They include:

- new editor tools or domain services;
- new persistence formats or schema changes;
- identity, evidence, validation, risk, or permission changes;
- runtime-adapter contracts;
- deployment or save-impact behavior;
- new third-party dependencies;
- breaking API or document changes;
- security-sensitive changes.

### Final decision

Maintainers seek reasoned agreement, but unanimity is not required. When consensus is not reached, the responsible maintainer records the decision and its rationale.

## Architecture authority

The following boundaries are project invariants unless governance explicitly changes them:

- O3DE is the authoring host; FoA is a separate Unity runtime target.
- The editor and knowledge layer do not execute gameplay mutations.
- Native execution belongs to separate, validated adapters.
- Display names are not identities.
- Native references remain exact.
- Synthetic identities are pack-owned.
- Evidence, claims, reviewed records, validation, and permission remain separate.
- Missing proof fails closed.

A proposal to change an invariant requires a dedicated architecture decision record, threat and migration analysis, public review, and maintainer approval.

## Branch governance

The repository has two long-lived branches:

- `main` — reviewed integration state;
- `foa-development` — active development.

Direct development on `main` is prohibited. New work begins only after `foa-development` is synchronized to the latest accepted `main` state.

## Review and merge authority

A pull request requires:

- completed template;
- linked issue or design when required;
- contributor self-review;
- DCO-compliant commits;
- successful required checks;
- resolved review threads;
- documentation and migration updates;
- maintainer approval.

Maintainers may require additional reviewers for security, schema, persistence, adapter, or release changes.

No contributor is entitled to merge their own change. A maintainer may merge a change they authored only after the same documented checks and review expectations have been met; independent review is preferred whenever another qualified reviewer is available.

## Releases

Releases follow [RELEASE_PROCESS.md](docs/tainted-grail-sdk/RELEASE_PROCESS.md). Maintainers decide release readiness based on scope, CI, known defects, documentation, migration support, security, and compatibility—not calendar pressure alone.

## Security and emergency changes

Security fixes may be developed privately and merged with reduced public detail when disclosure would increase risk. The maintainer must still preserve review, testing, provenance, and post-release documentation to the extent safe.

A maintainer may revert or disable functionality immediately when it risks data loss, unsafe deployment, security compromise, or invalid runtime permission.

## Maintainer changes

Maintainer status is based on sustained, trustworthy contributions and demonstrated judgment across code quality, review, architecture, safety, and community conduct.

A maintainer may be removed for inactivity, repeated policy violations, loss of trust, undisclosed conflicts, abusive conduct, or actions that materially endanger the project or its users.

## Conflicts of interest

Reviewers and maintainers must disclose material conflicts, including financial interests, employer obligations, private mod distribution, or personal disputes that could affect a decision. Recusal is expected when impartial review is not practical.

## Amendments

Governance changes use the same significant-change process: public proposal, rationale, review, and maintainer approval. The effective change must be recorded in the pull request and changelog.
