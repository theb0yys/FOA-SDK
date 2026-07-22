# Package engine capability intake

`PackageEngine/` is the first execution-side boundary after the deterministic receipt-to-execution handoff has been bound to state-backed lifecycle admission.

It does **not** copy files, start processes, request elevation, install, repair, upgrade, rollback, uninstall, launch FoA, execute runtime adapters, deploy content, mutate saves, sign artifacts, upload releases, mutate catalogues, or promote evidence.

This unit owns only two authenticated artifacts:

```text
PackageEngine/
├── README.md
├── token.schema.json
├── session.schema.json
└── Source/
    └── package_engine.py
```

## Required precondition

Token issuance requires one exact canonical document from `AdmissionBoundExecutionHandoff/`. A raw `ExecutionHandoff/` is not sufficient.

The binding must already prove:

- one inert execution handoff was validated;
- one lifecycle admission receipt was validated;
- operation, target reference and prior-installation reference match;
- admission preceded the handoff request;
- the binding was created inside the bounded admission window.

## Capability token

A package-engine capability token is a canonical authenticated JSON document bound to one exact admission-bound handoff, inert handoff, reviewed operation plan, authority proof, and trust-key identity.

The handoff-required capabilities must be a subset of the authenticated operation plan's complete capability allow-list. The token grants the signed plan capability set; a caller cannot append, replace, or manufacture capabilities because the operation plan and authority proof are revalidated and HMAC-bound.

Token chronology fails closed:

```text
binding.bound_at_utc <= token.issued_at_utc < token.expires_at_utc
```

Token lifetime is capped at one hour and must not outlive the authority proof.

## Package-engine session

A package-engine session consumes a verified admission-bound handoff plus a verified token and emits a canonical authenticated session record. Session chronology fails closed:

```text
token.issued_at_utc <= session.accepted_at_utc <= token.expires_at_utc
```

Caller-supplied session reference, actor, timestamp, admission binding, token, chronology, capability plan, and trust-key state are validated before the token's one-shot claim is consumed. A correctable preflight error therefore does not burn the token.

The session embeds the exact authenticated chain needed by downstream gates and retains all-false effect and authority records. It records capability intake only.

## File intake and persistence

Token, session, admission-bound-handoff, operation-plan, and authority-proof file reads are bounded strict UTF-8 JSON operations. Token and session loaders reject:

- symbolic-link files or symbolic-link path components;
- documents larger than the configured byte limit;
- excessive JSON nesting or node counts;
- invalid UTF-8 or JSON;
- noncanonical token/session bytes;
- stale, altered, incorrectly signed, or mismatched records.

Published token and session files use full-write loops, fsync, deterministic temporary paths, atomic no-replace publication, exact readback verification, and byte-identical idempotency. A different existing file is never overwritten.

## Boundary

This package-engine slice establishes a capability-checked execution path **up to intake only**. Payload copying, process launch, elevation, lifecycle coordination, installation-state publication, runtime adapter execution, and installation-result receipts remain separate gates. Each downstream unit consumes the exact session fingerprint, verifies its own explicit capability and grant, and records its own bounded effect evidence.
