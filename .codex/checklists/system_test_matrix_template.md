# System Test Matrix Template

Use this optional template when a change needs a written test plan. Do not commit a filled matrix unless requested or required by the owning design.

Select evidence by changed surface and risk. Do not populate unrelated host, UI, runtime, installer, or release rows merely because the template contains them.

## Change Scope

- Request:
- Classification:
- Target paths:
- Changed surface:
- Owner system:
- Shared contracts/routes affected:
- Out of scope:
- Blast radius:

## Owned Behavior

- Governing contract/design:
- Owned truth:
- Forbidden domain:
- Publishers and consumers:
- Lifecycle/persistence obligations:
- Failure/degraded-dependency obligations:
- Existing focused tests:
- Consequence if the behavior regresses:

## Applicable Evidence Matrix

| Evidence layer | Applicable? | Command or procedure | Expected proof | Result |
| --- | --- | --- | --- | --- |
| L0 repository/static |  |  |  |  |
| L1 focused unit/contract |  |  |  |  |
| L2 configure/build/compiled host |  |  |  |  |
| L3 Editor/UI/manual host |  |  |  |  |
| L4 installer/deployment/runtime/signing/release |  |  |  |  |

## Focused Test Plan

- Positive behavior:
- Negative/malformed input:
- Boundary and ownership:
- Persistence/migration/round trip:
- Lifecycle/cleanup:
- Degraded dependencies:
- Determinism/idempotency:
- Existing lanes:
- Missing lanes to add now:
- Required but blocked lanes:
- Explicitly `NOT_APPLICABLE` lanes:

## Compatibility Guard — If Applicable

- Contract/schema/persistence/interchange/configuration/dependency/package impact:
- Producers/consumers/loaders:
- Compatibility status:
- Migration or rejection policy:
- Static assertions:
- Runtime loader proof, when the claim requires it:

## Performance Guard — If Applicable

- Material risk and hot path:
- Representative cardinality:
- Baseline and threshold:
- Guard command:
- Configuration/context:
- Measured result:
- Missing measurements:

## Artifacts and Operational Proof — If Applicable

- Artifacts affected:
- Build/generation result:
- External destination:
- Backup/rollback or recovery:
- Exact operational procedure:
- Runtime/installer/release result:
- Protected-data boundary:

## Results

- Exact source head:
- Commands/procedures actually run:
- `PASSED`:
- `FAILED`:
- `PARTIAL`:
- `BLOCKED`:
- `NOT_RUN`:
- `NOT_APPLICABLE`:
- Remaining risk:
