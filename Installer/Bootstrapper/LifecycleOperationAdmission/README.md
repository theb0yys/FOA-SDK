# Lifecycle operation admission

`LifecycleOperationAdmission/` is the state-backed installer operation admission gate.

It consumes one exact validated lifecycle eligibility document from `InstallationStateRegistry/` and emits a short-lived admission grant plus admission receipt for exactly one lifecycle operation:

```text
install
repair
upgrade
rollback
uninstall
```

## Capability

```text
package-engine.admit-lifecycle-operation
```

The capability is represented only on the admission grant and admission receipt. It does not create a lifecycle handoff or package-engine session by itself.

## Admission behavior

The admission gate:

- accepts only canonical eligibility documents with `eligible = true`;
- rejects blocked eligibility decisions;
- binds the exact `eligibility_sha256`, `snapshot_sha256`, operation, target reference, current state reference and recommended prior installation reference;
- uses issuer, nonce and a bounded UTC validity window;
- emits a canonical receipt for the admitted operation.

## Boundary

This gate does not read or write files, copy payloads, launch processes, request elevation, publish state, create lifecycle handoffs, create package-engine sessions, mutate saves, sign artifacts, publish releases, mutate catalogues, promote evidence, or touch product/game directories.

Admission receipts are state-backed preconditions. They are not installer execution receipts and do not grant execution authority by themselves.
