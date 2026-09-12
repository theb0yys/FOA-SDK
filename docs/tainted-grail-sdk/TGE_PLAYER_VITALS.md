# Player vitals through Tainted Grail Extender

The extender's `tge.foa.player@0.1` service supports `vitals` with empty arguments.
It returns current and maximum health, stamina and mana. The implementation lives
inside the extender's BepInEx host and uses its existing authenticated adapter.

## SDK Editor

Open **FOA-SDK Home → Connect to Game**, connect to the extender, and choose
**Get Player Vitals**. The pane displays current / maximum health, stamina and
mana plus the local read time. **Refresh Vitals** requests a fresh observation.
The button requires the exact player service, while an older host that lacks
the `vitals` operation returns an error and requires reconnecting.

Values are displayed without clamping; zero maxima, negative values and current
values above their maximum remain visible. HUD rounding can differ. A read in
progress clears previous vitals. Missing player/stats show an unavailable
message without default values. Failures, connection refresh, disconnect and
pane close clear both player observations.

Position and vitals retain separate read times and thirty-second freshness
timers. Reading one does not refresh or overwrite the other. There is no
automatic network polling, refill, setter, persistence or game launch.
The existing Framework connection service owns the snapshots and supervises
one asynchronous worker at a time. The widget only forwards requests and
presents results. The worker uses the unchanged typed vitals consumer and
the established session, deadline, output and environment-key rules.

Editor validation on 2026-09-12: `PASSED` for 71 Python tests with no skips,
18 compiled connection-service tests, exact-pin Windows Profile configure/build,
and 20 actual Editor UI checks using an authenticated synthetic listener.
Checks cover refreshed stats, preserved native ranges, independent position/vitals
freshness, unavailable data, older-host rejection, cancellation and pane lifecycle.
The vitals layout was visually inspected. Maximum observed UI event gaps were
12 ms in compiled checks and 47 ms in the Editor, below the 500 ms test budget.
Static and scoped source-policy checks passed. Actual game readings, HUD
comparisons and native stat changes remain `NOT_RUN`; runtime acceptance is
`PARTIAL`. Build logs, screenshots and machine-readable evidence stay outside
the source checkout.

## Python consumer

After establishing an SDK connection:

```python
from tge_player_vitals import read_player_vitals, PlayerVitalsUnavailable

try:
    vitals = read_player_vitals(client)
    print(vitals.health, vitals.health_max)
    print(vitals.stamina, vitals.stamina_max)
    print(vitals.mana, vitals.mana_max)
except PlayerVitalsUnavailable as unavailable:
    print(unavailable.code)
```

`PlayerVitals` is immutable and also carries `session_id`. Each call creates a
fresh invocation; no polling, retries, reconnects or persistence are added.
Reusing a raw request ID through the generic client replays its earlier result.
The existing per-session invocation/history capacity still applies.

The generic command line can also query it:

```shell
python Gems/TaintedGrailModdingSDK/Tools/tge_sdk_transport.py --port <port> --service-id tge.foa.player --service-version 0.1 --operation vitals
```

Success is `succeeded/player_vitals` with exactly `health`, `healthMax`, `stamina`,
`staminaMax`, `mana`, `manaMax`, encoded as invariant round-trip float strings.
Current values use native `ModifiedValue`; maxima use native `UpperLimit`.
Finite native values are preserved without clamping or rounding, including zero,
negative values or values above their maximum. HUD display rounding may differ.

The host checks player readiness and missing/discarded native stat containers.
`player_unavailable` and `vitals_unavailable` are rejected results without values,
raised as `PlayerVitalsUnavailable`. Nonfinite stats never produce a partial
snapshot. Malformed responses, provider failures, stale sessions, capacity
rejections and an older host's `unknown_operation` are errors, not zero vitals.
Discovery identifies the service, not whether that host supports every operation.

The six native reads happen sequentially on the game thread. They are not an
atomic snapshot. Native getters can refresh caches, recalculate tweaks and emit
native stat events; the BaseValue fallback can normalize a value. This is an
observation operation with no added setter, refill, patch or save command, not a
promise that the game's getters have zero side effects.

The companion runtime repository owns the contract in
`mods/tainted-grail-extender/docs/foa-sdk-player-vitals-v1.md`. Core, Contracts,
the `position` response, wire envelope and persisted SDK formats stay unchanged.

Validation uses production service source linked against synthetic native types.
Set `TGE_VITALS_FIXTURE_DLL` to the built `TGE.PlayerVitals.Fixtures.dll`, then run:

```shell
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p test_tge_player_vitals.py -v
```

Without that variable the socket case is explicitly skipped. Synthetic stat
spending/recovery proves interoperability, not in-game behavior. Native host
compilation and exact installed-game acceptance are separate evidence lanes.
Runtime sign-off not performed; no game installation or save is changed by
implementing this helper.

On 2026-09-12 the native Release host built with zero warnings/errors, 368
synthetic vitals checks and 302 existing position/shutdown checks passed, and all
15 SDK player tests passed with no skips (nine vitals and six position). Both
socket cases exercised the production listener. Independent source review passed.
In-game vitals and native stat changes remain `NOT_RUN`; acceptance is `PARTIAL`.
