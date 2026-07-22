# Installer bootstrapper

This directory owns bounded prerequisite detection, reviewed package acquisition, exact-chain verification, and the transition into the suite wizard or a separately reviewed package engine.

## Verified execution handoff

`ExecutionHandoff/` accepts only a canonical Suite Wizard receipt. It re-verifies the embedded plan, view-model, acknowledgement confirmation, and receipt fingerprints before creating a canonical request for one lifecycle operation:

- install;
- repair;
- upgrade;
- rollback;
- uninstall.

The handoff records a stable logical target, caller identity and UTC time, resolver-owned package order and payload totals, and the exact capability names a future package engine would require. It grants no capability and contains no package-engine invocation, executable path, installation directory, private workspace path, environment value, or clock read.

Repair, upgrade, rollback, and uninstall require a stable prior-installation reference. Requested lifecycle support is derived from every selected package in the verified plan. Elevation may be listed only as a future required capability; elevation authority remains false.

The bootstrapper must fail closed for unsupported hosts, missing prerequisites, source or hash drift, unsafe paths, dependency cycles, unreviewed redistribution, partial acquisition, unexpected elevation, stale suite definitions, stale receipts, unsupported lifecycle operations, mismatched logical targets, or capability escalation.

Network access, process execution, filesystem mutation, package-engine execution, and elevation are explicit separately reviewed capabilities. They are never inferred from package presence, a valid receipt, or a valid handoff. The bootstrapper grants no FoA launch, deployment, runtime, save, signing, publication, catalog-mutation, or evidence-promotion authority.
