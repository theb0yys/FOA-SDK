# Lifecycle execution coordinator

`LifecycleCoordinator/` is the installer operation-level coordination gate for:

```text
install
repair
upgrade
rollback
uninstall
```

It consumes one exact verified package-engine session and issues one short-lived lifecycle grant for the matching `package-engine.execute.<operation>` capability.

## Evidence model

The coordinator does not copy payloads, launch processes, request elevation, run runtime adapters, mutate saves, sign artifacts, publish releases, mutate catalogues, or promote evidence.

Instead, it consumes already-verified lower-level evidence:

- package payload copy receipt from `PackageCopier/`;
- process-launch result from `ProcessLauncher/`;
- elevation result from `ElevationHelper/` when the lifecycle grant explicitly allows it;
- elevated completion observation from `ElevatedCompletionReceipt/` when the lifecycle grant explicitly allows elevated execution evidence.

Install, repair, and upgrade require a verified copy receipt because those operations depend on staged payload evidence. Rollback and uninstall do not require a copy receipt, but every operation requires exactly one execution evidence lane: direct launch result, raw elevation result, or elevated completion observation.

## Elevated operations

A raw elevation result proves only that the operating system accepted an explicit elevation request. It does not prove process completion, installer success, or product state mutation. If only the raw elevation result is supplied, the coordinator records:

```text
elevation-requested-pending-completion-receipt
```

When an authenticated elevated completion observation is supplied, the coordinator finalizes the lifecycle result from the observed elevated helper outcome:

```text
completed
blocked-nonzero-return
blocked-timeout
blocked-output-limit
```

The lifecycle result records both that elevation was requested and whether elevated completion was observed. It still does not claim product or game directory mutation; installation-state publication remains a separate gate.

## Result boundary

A lifecycle result may mark a non-elevated operation as `completed` only when the exact process-launch result reports `return_code = 0`, `timed_out = false`, and `output_limit_exceeded = false`.

A lifecycle result may mark an elevated operation as `completed` only when the exact elevated completion observation reports `completed = true` and is bound to the same package-engine session and operation.

The result always records the following forbidden side effects as false:

- product or game directory mutation;
- runtime execution;
- save mutation;
- signing;
- publication;
- catalogue mutation;
- evidence promotion.

This gate coordinates lifecycle evidence. Installation-state publication remains a later isolated gate.

## Security hardening

Lifecycle grants and results are authenticated, one-shot records and every consumed copy, launch, elevation, or elevated completion observation is reverified against the same session trust anchor.
