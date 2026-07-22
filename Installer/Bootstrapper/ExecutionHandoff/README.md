# Verified receipt-to-package-engine handoff

`Installer/Bootstrapper/ExecutionHandoff/` owns the deterministic boundary between a verified Suite Wizard receipt and a future package engine.

This unit is deliberately **non-executing**. It records a requested lifecycle operation and the exact capability names that a future package engine would need, but it grants no capability, invokes no engine, copies no payload, performs no elevation, and mutates no installation.

## Contract

A handoff is a self-contained canonical JSON document containing:

- the exact verified Suite Wizard receipt and `receipt_sha256`;
- the bound `plan_sha256`, `view_model_sha256`, and `confirmation_sha256`;
- one requested operation: `install`, `repair`, `upgrade`, `rollback`, or `uninstall`;
- a stable logical target reference, never a filesystem path;
- a stable prior-installation reference for every operation except first install;
- resolver-owned package order and payload summary;
- caller-supplied identity and ISO-8601 UTC request time;
- the exact future capability names required by the requested operation;
- an empty `granted_capabilities` array;
- an all-false authority record;
- `handoff_sha256` over every preceding field.

Creation re-verifies the complete embedded receipt before producing bytes. Repair, upgrade, and uninstall require every selected package to declare the corresponding lifecycle support. Rollback requires every selected package to declare rollback protection. Elevation, when required by the verified plan, appears only as `package-engine.elevation` in `required_capabilities`; elevation authority remains false.

No capability token, executable path, installation directory, private workspace path, environment value, or clock reading is serialized.

## Create

```powershell
python Installer/Bootstrapper/ExecutionHandoff/Source/receipt_execution_handoff.py create `
  --receipt foa-build/evidence/review.foa-receipt.json `
  --operation install `
  --target-reference installation.foa-sdk.default `
  --requested-by "Installer operator" `
  --requested-at-utc 2026-07-22T12:00:00Z `
  --output foa-build/evidence/install.foa-execution-handoff.json
```

Repair, upgrade, rollback, and uninstall also require:

```text
--prior-installation-reference installation.foa-sdk.previous
```

The destination parent must already exist and contain no symbolic-link component. Handoff files must end in `.foa-execution-handoff.json`. Publication is canonical, create-once, atomic, non-overwriting, and idempotent for byte-identical content.

## Verify

```powershell
python Installer/Bootstrapper/ExecutionHandoff/Source/receipt_execution_handoff.py verify `
  --handoff foa-build/evidence/install.foa-execution-handoff.json `
  --receipt foa-build/evidence/review.foa-receipt.json
```

Supplying `--receipt` proves the handoff is bound to the exact current receipt rather than merely carrying an internally valid historical receipt.

## Capability boundary

A valid handoff is not permission to execute. A future package engine must be separately reviewed and must:

1. reverify the canonical handoff and embedded receipt;
2. receive an explicit capability through a separate trusted channel;
3. prove that capability exactly covers every `required_capabilities` entry;
4. bind execution to the exact `handoff_sha256`;
5. return a separately verified result and rollback evidence.

This unit contains no downloader, package copier, installer engine, repair/upgrade/rollback/uninstall implementation, process launcher, elevation helper, network client, deployment service, signer, uploader, or runtime authority.
