# Player position through Tainted Grail Extender

The extender owns `tge.foa.player@0.1`. Its `position` operation accepts no arguments and reads the game's latest published player world coordinates plus the raw active Unity scene name. An active Unity scene is not a logical location or a guarantee that spawning is safe.

## SDK Editor

Open **FOA-SDK Home → Connect to Game**, establish the connection, then choose
**Get Player Position**. The pane shows X/Y/Z world coordinates, the raw active
Unity scene and the local time of the read. **Refresh Position** takes another
snapshot. The button is enabled only when discovery advertises the exact
`tge.foa.player@0.1` service owned by `tge.foa.player`.

Each query verifies the connected extender session before calling the service.
A restarted game requires reconnecting. A query in progress, an unavailable
player/scene/position, a failed query, connection refresh, disconnect or pane
close clears old coordinates. Successful coordinates are marked stale after
30 seconds; there is no automatic polling. Scene text is displayed literally.
Coordinates and scene names remain transient and are not saved or logged.

The existing Framework `GameConnectionService` owns this observation and process
supervision; the widget displays its snapshot. The packaged Python worker adds
an explicit `position` operation using the existing typed helper. The TGE wire
contract, native reader, workspace schemas and runtime permissions are unchanged.
The operation uses the existing bounded asynchronous worker, environment-only
key transfer, cancellation and session rules. It never launches or configures
the game and never changes player state.

### Editor acceptance, 2026-09-12

- PASSED: 54 scoped Python codec, transport, worker and player-helper tests,
  including production managed-service interoperability; no skips.
- PASSED: exact-pin O3DE configure and affected Framework, test and Editor
  compilation using existing pinned engine dependencies.
- PASSED: all 13 compiled connection-service tests. Maximum UI heartbeat gap:
  32 ms, below 500 ms; deadline, freshness and cancellation guards passed.
- PASSED: 13 live Editor checks with a synthetic authenticated listener,
  including changed coordinates, unavailable player, absent service, restart,
  cancellation and close/reopen. Maximum UI event gap: 78 ms, below 500 ms.
  The coordinate/scene/timestamp layout was visually inspected.
- PASSED: scoped static checks and ten enabled source-policy validators.
  Unrelated encounter Python work was excluded from the source-policy scan.
- NOT_RUN: this pane reading a loaded player in the actual game. Synthetic
  movement and earlier runtime receipts do not establish that result.

Private test logs, screenshots and source/artifact hashes remain outside the
checkout. No game installation, configuration, launch, save or state mutation
was performed for Editor acceptance. Runtime sign-off was not performed.

## Python consumer

After establishing the existing authenticated SDK connection:

```python
from tge_player_position import read_player_position, PlayerPositionUnavailable

try:
    position = read_player_position(client)
    print(position.scene_name, position.x, position.y, position.z)
except PlayerPositionUnavailable as unavailable:
    print(unavailable.code)
```

The returned snapshot is immutable and contains `scene_name`, `x`, `y`, `z` and `session_id`. Each call creates a fresh invocation. No automatic polling, retry, reconnection or position persistence occurs. Retrying the same raw request through the generic client returns its earlier cached response by design.

The existing generic CLI can invoke the service:

```shell
python Gems/TaintedGrailModdingSDK/Tools/tge_sdk_transport.py --port <port> --service-id tge.foa.player --service-version 0.1 --operation position
```

Successful wire values are exactly `sceneName`, `x`, `y`, `z`, with invariant round-trip float strings. `player_unavailable`, `scene_unavailable` and `position_unavailable` are explicit rejected results with no coordinates. The typed helper raises `PlayerPositionUnavailable` for these states and rejects malformed, nonfinite or underflowed values. Unsupported operations, arguments, service versions and sessions remain errors.

The runtime implementation is in the companion repository under `mods/tainted-grail-extender/src/TGE.FoAHost.BepInEx/Sdk/TgePlayerPositionService.cs`; its `docs/foa-sdk-player-position-v1.md` owns the service contract. TGE Core and public contracts remain game-neutral. The host reads local game assemblies; no game assembly is distributed with the service.

Validation uses a dedicated managed fixture that links the production service against explicitly synthetic Hero/Scene types, followed by native compilation and separate installed-game checks. Set `TGE_PLAYER_FIXTURE_DLL` to the built `TGE.PlayerPosition.Fixtures.dll` to execute the real-socket consumer case:

```shell
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p test_tge_player_position.py -v
```

Without that fixture path, socket interoperability is explicitly skipped. Synthetic movement does not prove in-game movement. The original handshake receipt describes the earlier host DLL; current service validation belongs to the player-position evidence in the runtime repository.

Executed on 2026-09-12: the native host compiled with zero warnings/errors; 297 synthetic player checks and all six SDK tests passed, including socket interoperability with no skips. In the actual Mono game build 24270691, service discovery and the startup `player_unavailable` query passed. The game exited normally and all temporary test files were removed. Loaded-player coordinates, walking and loading/menu transitions remain `NOT_RUN`; the shutdown callback marker remains unverified. Overall live feature acceptance is `PARTIAL`.
