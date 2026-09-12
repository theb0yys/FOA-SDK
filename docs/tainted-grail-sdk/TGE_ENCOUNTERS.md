# TGE encounter commands

Status: source implementation; exact-install gameplay acceptance NOT_RUN.
Owner: Tainted Grail Extender owns native actors and cleanup. FOA-SDK owns the
request client and the explicit saved-composition export. Classification:
Critical/Runtime. The current owner requested one NPC, observed appearance,
exact-owned removal, then saved compositions.

The SDK uses the existing authenticated loopback transport and adds
`tge.foa.encounters@0.1`. No existing V1 work order or execution flag is made
executable. There is no new Editor runtime button in this slice.

## First actor

The TGE host must be explicitly enabled for both `SdkTransport/Enabled` and
`SdkEncounters/Enabled`, using a shared ephemeral `TGE_SDK_KEY`. The latter defaults
to false. The native binding requires the inspected Mono TG.Main hash
`749aabbfbec121bb69bda0ae226223154406d2c990df3312ad12365d513fa982`.
Only `wyrdspirit` and `outlaw-1h` are candidate spawn bindings. Their ordinary native
AI remains active. MarkedNotSaved is applied before World.Add, but save/reload
compatibility has not been established.

Save this data-only composition outside the source checkout as `one-npc.json`:

```json
{
  "contract": "foa-tge-encounter-composition/1",
  "name": "One Wyrdspirit",
  "entries": [{"template": "wyrdspirit", "count": 1}],
  "source": null
}
```

With the SDK Tools directory as the working directory, run the following using the
available Python interpreter. Replace PORT with the port from this host session.

```text
python tge_encounters.py --port PORT preview --composition one-npc.json
python tge_encounters.py --port PORT spawn --composition one-npc.json --request-output one-npc-request.json
python tge_encounters.py --port PORT status --session SESSION --encounter-id ENCOUNTER
python tge_encounters.py --port PORT remove --session SESSION --encounter-id ENCOUNTER
```

Spawn explicitly previews and executes one fingerprint-bound plan. The request
file is created exclusively before dispatch, contains no shared key, and is never
overwritten. Use the session/encounter IDs returned by spawn. Query status again
after remove: a returned discard request is not confirmed removal.

A lost reply is ambiguous. Reconcile the exact saved invocation instead of
starting another spawn:

```text
python tge_encounters.py --port PORT reconcile --request one-npc-request.json
```

Plans expire after 30 seconds. One active encounter and 32 retained attempts are
allowed per host lifetime. Up to eight actors are supported, with at most one
native spawn and one discard per tick; a failing spawn may request its cleanup
in the same tick. Multi-actor previews are rejected until one
actor has been observed ready and its removal confirmed in the same host lifetime.

State meanings:

| State | Meaning |
| --- | --- |
| pending | Accepted/initializing; appearance has not been established |
| appeared | All requested living NPCs and loaded active visuals were observed |
| removing | Exact-owned cleanup was requested; confirmation is pending |
| removed | Successful operation, with every tracked actor's removal confirmed |
| failed | Operation failed, with known owned cleanup subsequently confirmed |
| cleanup_failed | Cleanup failed or ownership/removal remains unconfirmed |

Every actor has an opaque handle plus an observational native ID. Removal accepts
only the encounter handle; it cannot target arbitrary native IDs or existing
world actors. Historical errors are retained even if later observations confirm
cleanup. Unknown ownership stays quarantined.

Initialization has a 15-second deadline and encounters a 120-second lifetime.
World changes request cleanup. Shutdown logs a bounded summary of unconfirmed
cleanup; it does not fabricate a successful result. If removal happens before
any visual is observed, cleanup remains unconfirmed unless subsequent native
observations establish the required destruction. Native observations do not
replace the user's visual in-game check.

## Saved Editor compositions

The Spawn and Encounter Editor's catalog schema stays at version 3. The exporter
reads it without modifying it, and requires explicit actor-record-to-native-template
mapping because imported NPC-template IDs are not LocationTemplate spawn IDs.

Supported export is composition membership/counts: fixed counts, manual activation,
no authored placement, one active instance, and nonunique/nonessential/nonpersistent
actors with valid typed profiles. Troops, random count ranges, triggers, authored
stats and placement materialization are outside this slice. Unsupported policies
are rejected rather than silently executed. The explicit mapping selects the
actual native NPC for each authored actor entry; it is not evidence that custom
stats have been applied.

Create a bindings JSON object mapping each referenced saved actor ID exactly once:

```json
{"custom.actor.EXAMPLE": "wyrdspirit"}
```

```text
python tge_encounters.py export --catalog catalog.tgcatalog.json --encounter-id custom.encounter.EXAMPLE --bindings bindings.json --output patrol.tgeencounter.json
```

The exported file includes the source catalog digest, encounter ID and explicit
bindings. Review the printed mapping, then use the same preview/spawn/status/remove
commands. The output path must be new. Runtime capability checks remain authoritative
at preview/execution; an exported file does not qualify native content or authorize
deployment.

## Validation and operational boundary

Required local lanes are focused Python contract/export tests, managed production
controller tests, synthetic production-native-binding tests, two independent
runtime reviews and the native Mono host build with deployment disabled.
O3DE rebuild/UI tests are NOT_APPLICABLE to these Python-only SDK additions.

The native lifecycle tests do not prove visible in-game appearance, scene teardown,
native frame time, installed loader compatibility or save behavior. Those require
a separately observed exact-install run. The protected-files policy requires
explicit current-task permission for exact installation paths before deployment.
No game installation, save, release, source branch or shared work order is changed
by these client commands.

Related contracts: [SDK adapter](TGE_SDK_ADAPTER.md),
[capability execution](CAPABILITY_EXECUTION_CONTRACT.md),
[protected files](../protected-files-policy.md).

Executed source validation: PASSED, 57 SDK Python tests (including 11 encounter
tests, zero skips), 77 managed controller checks, 68 synthetic production-binding
checks, and the final native build with zero warnings/errors. Two independent
runtime reviews are APPROVED. Installed-game appearance/removal remains NOT_RUN.
