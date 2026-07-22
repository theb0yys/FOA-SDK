# Package engine capability intake

`PackageEngine/` is the first execution-side boundary after the deterministic receipt-to-execution handoff.

It does **not** copy files, start processes, request elevation, install, repair, upgrade, rollback, uninstall, launch FoA, execute runtime adapters, deploy content, mutate saves, sign artifacts, upload releases, mutate catalogues, or promote evidence.

This unit owns only two deterministic artifacts:

```text
PackageEngine/
├── README.md
├── token.schema.json
├── session.schema.json
└── Source/
    └── package_engine.py
```

## Capability token

A package-engine capability token is a canonical JSON document bound to one exact `handoff_sha256`.

It contains:

- `token_scope = package-engine-capability-token`;
- `audience = foa-sdk.package-engine`;
- the complete embedded handoff;
- the handoff operation and logical target;
- the exact required capability names from the handoff;
- a grant set that must match those required capabilities exactly;
- caller-supplied issuer, subject, nonce, issue time and expiry time;
- `token_sha256` over every preceding field.

The token is not a bearer secret. It serializes no credential, executable path, private filesystem path, installation directory, environment value, process argument vector, elevation command, or OS handle. It is review evidence that the next package-engine stage may accept the named capability set for one exact handoff.

Token chronology fails closed:

```text
handoff.requested_at_utc <= token.issued_at_utc < token.expires_at_utc
```

Token lifetime is capped at one hour.

## Package-engine session

A package-engine session consumes a verified handoff plus a verified token and emits a canonical session record.

The session records:

- `session_scope = package-engine-capability-session`;
- `session_state = capability-accepted-no-effects`;
- exact handoff, token, receipt, plan, view-model and confirmation hashes;
- operation, logical target and prior-installation reference;
- resolver-owned package order and payload summary;
- accepted capabilities;
- accepted actor and UTC time;
- the complete embedded handoff and token;
- an exact all-false effect record;
- an exact all-false authority record;
- `session_sha256` over every preceding field.

Session chronology fails closed:

```text
token.issued_at_utc <= session.accepted_at_utc <= token.expires_at_utc
```

## Persistence

The CLI provides only:

```text
package_engine.py token
package_engine.py session
package_engine.py verify-token
package_engine.py verify-session
```

Published token and session files use canonical UTF-8 JSON, create-once publication, symbolic-link rejection, byte verification and idempotency for byte-identical existing files.

## Boundary

This package engine slice establishes a capability-checked execution path **up to intake only**.

The following remain later isolated units:

1. package payload copier;
2. process launcher;
3. elevation helper;
4. lifecycle executors for install, repair, upgrade, rollback and uninstall;
5. runtime adapter launch or live-load execution;
6. installation-result receipts.

Each later unit must consume the exact `session_sha256`, receive its own explicit capability, and prove that external workspaces and user-authored content are preserved.
