# Admission-bound execution handoff

`AdmissionBoundExecutionHandoff/` is the installer pre-engine binding gate between state-backed lifecycle admission and the existing inert execution handoff.

It consumes:

- one canonical execution handoff from `ExecutionHandoff/`;
- one canonical admission receipt from `LifecycleOperationAdmission/`.

It emits a deterministic binding document only when the handoff is backed by the exact admitted operation, target and prior state.

## Capability

```text
package-engine.bind-admitted-handoff
```

This capability appears only on the binding document. It does not create a package-engine session, capability token, copy receipt, process-launch result, elevation result, lifecycle result, state publication, runtime execution, save mutation, signing, network publication, catalogue mutation, evidence promotion, or product/game directory mutation.

## Binding behavior

The gate:

- revalidates the exact execution handoff through `ExecutionHandoff/`;
- revalidates the exact admission receipt through `LifecycleOperationAdmission/`;
- requires operation, target reference and prior installation reference to match the state-backed admission;
- requires the admission timestamp to precede the execution handoff request;
- requires binding within fifteen minutes of admission;
- embeds both verified source documents;
- records that no package-engine token or session was created.

## Boundary

This gate is a precondition bridge only. It does not grant package-engine execution authority by itself and does not perform filesystem reads or writes.

Later package-engine intake work can require this binding before capability-token issuance, but that integration is intentionally not introduced here.
