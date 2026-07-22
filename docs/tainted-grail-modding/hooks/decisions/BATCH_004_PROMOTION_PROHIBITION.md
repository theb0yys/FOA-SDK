# Batch 004 Promotion and Prohibition Decisions

```yaml
decision_set_id: tg.decision.hooks.batch-004
status: reviewed-source-decision
profile_id: foa-1.23.401-mono-bepinex5-tf-0.1.33
profile_evidence_state: HostLiveLoadValidated
fixture_execution_state: specification-only
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
hook_promotions: 0
current_source_prohibitions: 7
permanent_prohibitions: 0
```

## Decision vocabulary

- `promote` — the record has the exact evidence required by the hook catalogue promotion rules.
- `remain-candidate` — source identity improved, but promotion evidence is incomplete and no deterministic defect alone requires prohibition.
- `prohibited-current-source` — the reviewed implementation has adverse or missing properties that prevent supported runtime guidance and hook promotion. Reopening requires a new reviewed implementation and the named fixture evidence.
- `permanently-prohibited` — the goal itself is forbidden regardless of implementation. Batch 004 makes no permanent prohibition.

A host-load profile is not target, behavior, cleanup, or compatibility evidence. A source signature is not a compiled assembly fingerprint. A fixture specification is not a fixture result.

## Decisions

| Surface | Decision | Basis | Reopening gate |
|---|---|---|---|
| `EconomyReflection` generic non-public lookup | `prohibited-current-source` | no exact member allowlist or cache; throwing reads abort fallback; arbitrary getters and `ToString` can execute; output fields are not classified | exact assembly fingerprints, allowlisted member map, deterministic fallback/exception behavior, bounded allocation, redaction |
| `QuantityLive` reflective quantity write | `prohibited-current-source` | item write occurs before optional owner write-back; no transaction or rollback; broad numeric/member guessing | typed exact-profile adapter, prevalidated conversion, atomic commit/inverse, failure-injection and idempotence fixtures |
| `RewardWealthLive` story-step write | `prohibited-current-source` | substring currency identity; pre-original mutation; no rollback when native execution fails or is skipped | exact currency/member contract, transactional prefix/finalizer design, story/save/ordering fixtures |
| vendor and regional price mutation | `prohibited-current-source` | unresolved overload/arguments; exact-name Hero test; heuristic item/merchant identity; first-match substring rules; repeated parsing and hot-path output | exact overload fingerprint, canonical identities, cached parsed rules, rounding/ordering/performance and combined-mod fixtures |
| `EconomyDiagnosticWriter` capture/support output | `prohibited-current-source` | prefix-only root containment, unsafe dot segments, non-atomic append/summary/count, no byte cap/redaction/retention/safe spreadsheet export | canonical descendant check, dot/root/length rejection, atomic publication, row+byte bounds, field policy, retention and cleanup fixtures |
| wolf native mount conversion and seat controller | `prohibited-current-source` | discarded actions are not reconstructed; prior-null ownership is not explicitly restored; private-field/object graph mutation is not transactional; movement suspension is heuristic | staged transaction, complete snapshot/inverse, fail injection at every step, scene/unload/save and combined-mod fixtures |
| Avalon Companions bridge runtime registration/cleanup | `prohibited-current-source` | exact source API exists, but built assembly/load/owner-isolation behavior is unverified; unregister failure discards retry state | fingerprinted `AvalonCompanions.dll`, API v1 resolution, duplicate/owner/load-order tests, retryable cleanup and stale-callback proof |

## Source facts accepted

Two facts are accepted at source scope:

1. the pinned diagnostic writer's exact row schema, row-count cap, path-resolution sequence, file names, and write ordering;
2. `AvalonCompanions` source version `0.2.17`, assembly version `0.2.17.0`, API version `1`, and the two public static API signatures.

These facts do not promote runtime hooks. They do not authorize capture, mutation, registration, deployment, or publication.

## Fixture mapping

| Decision | Fixture manifest |
|---|---|
| reflection, quantity, reward, vendor | [`batch-004-economy-profile.json`](../fixtures/batch-004-economy-profile.json) |
| diagnostic writer | [`batch-004-diagnostic-writer.json`](../fixtures/batch-004-diagnostic-writer.json) |
| wolf mount rollback | [`batch-004-wolf-mount-rollback.json`](../fixtures/batch-004-wolf-mount-rollback.json) |
| Avalon Companions API | [`batch-004-avalon-companions-api.json`](../fixtures/batch-004-avalon-companions-api.json) |

## Promotion ceiling

No hook may advance on the basis of this batch. Future evidence must be returned as candidate evidence, reviewed independently, and reconciled against the exact profile, exact source or build identity, fixture implementation digest, observations, adverse outcomes, cleanup, and combined-mod state.
