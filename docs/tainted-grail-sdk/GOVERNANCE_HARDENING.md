# Governance Reliability Baseline

## Purpose

This document records the corrective hardening applied after professional review of the first catalog-governance implementation. It is an architectural contract for future governance, validation, permission, and domain-authoring work.

## Persisted documents and typed domain logic

The schema-1 catalog remains string-compatible so existing `catalog.tgcatalog.json` documents do not require a breaking migration merely to adopt compiler-safe logic.

Strings are accepted only at persistence and user-interface boundaries. Before a governance transition executes, values are parsed into strong internal types:

- `CatalogSubjectKind`;
- `GovernanceAxis`;
- `ResearchStage`;
- `ConfidenceLevel`;
- `OperationalRisk`;
- `ValidationState`;
- `StalenessState`;
- `PermissionDecision`.

Unknown values fail at the boundary. The engine does not compare ad hoc string literals throughout transition logic.

Legacy schema values such as `unknown` maturity and `unrated` confidence remain explicitly representable. Compatibility is deliberate rather than accidental.

## One governed subject state

Records and relationships are adapted into one `GovernedSubjectState` before transition rules run.

The shared state contains:

- subject kind and stable ID;
- maturity;
- confidence;
- operational risk;
- validation state;
- staleness;
- allowed and forbidden usages;
- missing references;
- conflicts;
- supersession;
- update time.

Every governance axis is implemented once against that shared state. Record and relationship adapters only read and write the common fields. New axes must not introduce separate record and relationship transition branches.

## Intrinsic service atomicity

`CatalogGovernanceService` accepts a `const CatalogDatabase&`. It never mutates a caller-owned catalog.

For each decision it:

1. validates the request, exact profile, evidence, and typed values;
2. copies the source catalog into a private candidate;
3. applies the state transition to the candidate;
4. appends the validation or governance history event to that same candidate;
5. returns the complete candidate only after every step succeeds.

If history insertion fails, the caller receives failure and its original catalog remains unchanged. Audit history and current state cannot diverge through this service.

## Persistence transaction boundary

`CatalogTransactionService` converts a candidate database into a complete catalog document and invokes the configured save callback.

The service returns a publishable `CatalogCommitResult` only after the save succeeds. `FoundationService` updates its live catalog and file path only from that successful result.

A failed write therefore leaves:

- the published in-memory catalog unchanged;
- the previous catalog path unchanged;
- the failed candidate unpublished.

Domain services must not publish state before persistence succeeds.

## Deterministic audit IDs

New governance and validation IDs use deterministic sequence-based identifiers within the catalog document. Duplicate IDs fail closed.

A duplicate audit ID must never be repaired by overwriting the existing event. The complete candidate is rejected and the source catalog remains unchanged.

## Required negative coverage

The catalog/governance test target must retain coverage for:

- unknown evidence IDs;
- evidence whose subject does not match the governed record;
- evidence bound to another game profile;
- duplicate governance-event IDs;
- duplicate validation-event IDs;
- failed persistence callbacks;
- partial-update rollback;
- corrupted documents with duplicate history;
- documents containing unsupported typed-state strings;
- typed parser rejection of misspellings;
- legacy typed-value compatibility;
- identical governance rules for records and relationships.

Happy-path tests alone are not sufficient for changes to audit, validation, permission, staleness, or supersession logic.

## Review rule

A pull request changing governed state may not merge unless reviewers can establish all of the following:

- internal transition logic uses typed values;
- record and relationship behavior uses the shared state path;
- the service cannot mutate caller-owned state on failure;
- history and current state are committed together;
- persistence completes before publication;
- failure and rollback tests cover the changed path;
- durable string representations and compatibility behavior are documented.

## Product boundary

These guarantees protect editor-side knowledge and governance integrity. They do not execute FoA actions and do not remove the need for separate adapter capability checks, persistence, cleanup, and rollback.
