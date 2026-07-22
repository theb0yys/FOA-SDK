# Semantic Hook Batch 005 — Source Repair Designs

These architecture contracts replace the adverse implementations identified in Batch 004. They do not modify upstream mods, load game assemblies, execute fixtures, or promote hooks.

## Exact typed economy adapters

Replace generic name-fallback reflection with profile-owned adapters:

- one adapter per exact profile and native type;
- exact declaring type, member, numeric type, permission, and assembly fingerprint;
- no arbitrary non-public lookup from caller strings;
- preflight reads and conversions before mutation;
- transaction state containing original values and idempotent rollback;
- commit only after every dependent write succeeds;
- reward mutation paired with rollback when native execution fails;
- vendor pricing bound to one exact `TradeUtils.Price` overload and argument contract;
- canonical item, currency, Hero, merchant, and location identities;
- regional rules cached per configuration identity.

The current reflection, quantity, reward, vendor, and regional-price helpers remain prohibited.

## Diagnostics writer repair

The replacement writer must:

- canonicalize the configured root and require descendant containment by component boundary;
- reject rooted session names, `.` and `..`, reserved names, empty normalized names, and overlong segments;
- reject or safely resolve symlink/junction escape;
- classify and redact every output field before serialization;
- enforce field, row, file, lane, and session byte limits;
- publish row data and summary state transactionally;
- persist counters only after durable publication;
- define retention, deletion, export, and crash recovery;
- neutralize spreadsheet formula prefixes in support CSV;
- reset session state during cleanup;
- emit nothing unless the operator explicitly enables the lane.

The current writer remains prohibited for capture and support sharing.

## Wolf mount staged transaction

Replace direct conversion with `Prepare → Validate → Commit → Rollback`.

Preparation captures explicit previous mount ownership including null, native actions and model elements, every field value, movement components and enabled states, borrowed mount-data identity, and all planned objects/components. Validation occurs before destructive changes. Commit cannot discard native actions without an exact inverse. Every stage supports injected failure and idempotent rollback. Deferred Unity destruction must reach quiescence before success. Scene unload, hero replacement, wolf discard, plug-in unload, and gate loss use the same rollback path.

The current wolf conversion remains prohibited.

## Avalon Companions API v2 design

```text
ApiVersion = 2
RegisterDialogueCommand(CommandRegistration) -> RegistrationResult
UnregisterDialogueCommands(ownerId, registrationToken) -> UnregistrationResult
QueryDialogueCommands(ownerId) -> read-only registrations
```

Requirements:

- immutable owner and command IDs;
- registration token on success;
- deterministic duplicate behavior and cross-owner isolation;
- explicit API/assembly version handshake;
- failed unregister retains retryable local state;
- stale token and reload behavior defined;
- no silent callback replacement;
- execution rechecks the runtime gate;
- cleanup succeeds only after confirmed external removal.

API v1 remains a source fact. The current bridge remains a candidate and is prohibited for runtime support claims.

## Acceptance boundary

A repaired implementation may be reconsidered only after source review, unchanged-or-stronger adverse fixtures, offline specification validation, separately governed exact-profile runtime evidence, and an explicit governance promotion decision. The offline runner can never provide runtime evidence or promotion authority.
