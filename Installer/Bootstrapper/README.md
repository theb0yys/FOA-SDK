# Installer bootstrapper

This directory owns bounded prerequisite detection, reviewed package acquisition, exact-chain verification, package-engine capability intake, and the transition into the suite wizard or a separately reviewed package engine.

## Verified execution handoff

`ExecutionHandoff/` accepts only a canonical Suite Wizard receipt. It re-verifies the embedded plan, view-model, acknowledgement confirmation, and receipt fingerprints before creating a canonical request for one lifecycle operation:

- install;
- repair;
- upgrade;
- rollback;
- uninstall.

The handoff records a stable logical target, caller identity and UTC time, resolver-owned package order and payload totals, and the exact capability names a future package engine would require. It grants no capability and contains no package-engine invocation, executable path, installation directory, private workspace path, environment value, or clock read.

Repair, upgrade, rollback, and uninstall require a stable prior-installation reference. Requested lifecycle support is derived from every selected package in the verified plan. Elevation may be listed only as a future required capability; elevation authority remains false.

## Package engine capability intake

`PackageEngine/` is the first execution-side package-engine boundary. It accepts a verified handoff only with a deterministic capability token bound to the exact `handoff_sha256` and exact required capability set.

The package-engine token is not a secret or bearer credential. It records issuer, subject, nonce, issue time, expiry time, exact granted capabilities, the embedded handoff, an all-false authority record, and `token_sha256`. Token issue time must be at or after the handoff request time, expiry must be after issue time, and token lifetime is capped at one hour.

The package-engine session consumes the verified handoff and token and records `session_state = capability-accepted-no-effects`, exact accepted capabilities, the embedded handoff and token, all-false effects, all-false authority, and `session_sha256`. Intake must occur within the token validity window.

This slice still performs no package copy, process launch, elevation, installation, repair, upgrade, rollback, uninstall, runtime execution, deployment, save mutation, signing, publication, catalog mutation, or evidence promotion. Copier, process launcher, elevation helper, and lifecycle executors remain later isolated units.

The bootstrapper must fail closed for unsupported hosts, missing prerequisites, source or hash drift, unsafe paths, dependency cycles, unreviewed redistribution, partial acquisition, unexpected elevation, stale suite definitions, stale receipts, unsupported lifecycle operations, mismatched logical targets, stale handoffs, expired tokens, capability mismatch, or capability escalation.

Network access, process execution, filesystem mutation, package-engine execution, and elevation are explicit separately reviewed capabilities. They are never inferred from package presence, a valid receipt, a valid handoff, or a valid package-engine token. The bootstrapper grants no FoA launch, deployment, runtime, save, signing, publication, catalog-mutation, or evidence-promotion authority.
