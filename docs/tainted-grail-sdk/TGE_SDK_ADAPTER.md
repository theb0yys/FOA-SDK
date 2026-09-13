# SDK connection to Tainted Grail Extender

The runtime adapter belongs to **Tainted Grail Extender (TGE)**. The SDK authors requests and consumes correlated results; TGE owns registration, lifecycle and dispatch to runtime services.

The implementation lives in the companion `Tainted-Grail-The-Fall-of-Avalon-mods` repository under `mods/tainted-grail-extender/src/TGE.Core/Sdk/`. Its `docs/foa-sdk-adapter-v1.md` is the wire-contract reference. Public service values and interfaces live in `TGE.Contracts/Sdk/`. The existing BepInEx host exposes `HandleSdkRequest(string)` on its host thread.

The additive [player-position service](TGE_PLAYER_POSITION.md) now exposes `tge.foa.player@0.1` through this adapter. Its native reader belongs to the extender's BepInEx host; the SDK consumes its position and active-scene snapshot.

The [inventory and equipment service](TGE_INVENTORY.md) adds `tge.foa.inventory@0.1`, also owned by the extender. Its Python consumer reads eight equipment slots and inventory pages with item identities, names and quantities.

The SDK codec is [tge_sdk_client.py](../../Gems/TaintedGrailModdingSDK/Tools/tge_sdk_client.py). The live local client is [tge_sdk_transport.py](../../Gems/TaintedGrailModdingSDK/Tools/tge_sdk_transport.py). Both use only the Python standard library.

## Implemented boundary

```text
SDK client -> authenticated loopback connection -> TGE host-thread queue
  -> TGE JSON adapter -> registered extender service -> authenticated SDK response
```

The adapter provides versioned request/result envelopes, exact service/version selection, a session handshake, transactional extension registration, bounded replay protection and explicit failure responses. The built-in `tge.core.identity@0.1` service describes the current host and lists the services actually registered in it. Game-facing services must be implemented and registered by their runtime owners.

The client encodes `foa-sdk-tge-request/1` and validates `foa-sdk-tge-response/1`, including request, correlation, service, version and successful-response session binding. It rejects duplicate keys, unknown fields, unsupported versions, malformed Unicode and oversized frames.

The adapter never interprets the SDK's existing inert V1 adapter plans as executable requests. Workspace, catalog, pack and old adapter schemas are unchanged. A successful response is an observation of one handler invocation, not deployment permission or evidence promotion.

## Client use

### Connect from the Editor

Open **Tools → Tainted Grail SDK → FOA-SDK Home → Connect to Game**.
Start the game separately with the extender's
SDK listener configured as described below. Enter its logged local port, exact
extender version, and matching 64-character hexadecimal key, then choose
**Connect**. An empty key field uses `TGE_SDK_KEY` inherited by the Editor.

The pane displays the last verified extender version, session, check time and
all registered services (up to 256). **Refresh** checks that same session again;
a restarted extender requires **Connect** again. **Disconnect**, cancellation,
errors and hiding or closing the pane discard the connection and service snapshot.
Successful results become visibly stale after 30 seconds. There is no automatic
polling: the extender has a bounded invocation history, so unattended polling
would eventually exhaust a session. Service rows are read-only.

When the player service is available, **Get Player Position** reads world
coordinates and the active Unity scene. **Refresh Position** takes a new
snapshot; see the [player-position guide](TGE_PLAYER_POSITION.md) for unavailable
states and freshness behavior.

**Get Player Vitals** shows current / maximum health, stamina and mana, with a
separate refresh and freshness timer. See the [vitals guide](TGE_PLAYER_VITALS.md).

The key field clears when connecting. The key remains in process memory only
for that connection and is supplied to the worker through its environment,
never command-line arguments, workspace files, preferences or logs. Disconnect
clears the service's retained key; it does not remove an environment variable
supplied by the user when launching the Editor.

### Editor implementation boundary

This owner-requested change is Critical/Runtime for the authenticated external
connection, with Significant Editor integration. Primary owner:
`runtime-adapter-contracts`; the Editor presents transient observations, and TGE
continues to own runtime dispatch. `GameConnectionService` supervises the
packaged Python `tge_editor_connection.py` worker using the existing O3DE Python
runtime. It invokes `tge.core.identity@0.1` `describe` and `services`, and the
explicit `tge.foa.player@0.1` `position` and `vitals` observation operations.
Existing inert work orders, workspace/catalog schemas and runtime permissions
are unchanged. No new third-party dependency is introduced.

The worker uses the existing authenticated transport and validates complete,
bounded pagination before publishing a snapshot. It checks the prior session
on refresh, rejects malformed lists and never silently reconnects. Each network
call has at most a five-second deadline, the observation has a twenty-second
budget, and the Editor kills an unfinished worker after twenty-one seconds.
Output is capped at 2 MiB. Network IO and JSON discovery run outside the UI
thread; cancellation never waits for worker completion. The installed SDK
includes the worker, codec, transport and position/vitals helpers under `scripts/foa-sdk`.

Required acceptance: Python malformed/paging/deadline tests and production
managed-listener interoperability; compiled service tests for state, process
failure, cancellation, stale observations and responsiveness; exact-pin Editor
build and live pane acceptance. A synthetic listener proves the Editor route,
not the installed game. Actual game acceptance needs the configured running
extender; this feature does not install, configure or launch it. A successful
host handshake does not attest the installed game profile or grant gameplay,
deployment, save, release or evidence-promotion authority.

### Listener setup

The host's optional listener uses IPv4 loopback. At host startup, set BepInEx configuration section `SdkTransport` to `Enabled = true`; `Port = 0` selects an available port reported in the host log. A fixed port may also be configured. Set `TGE_SDK_KEY` in both the selected host's process environment and the SDK process environment to the same cryptographically random 32-byte key encoded as 64 hexadecimal characters. Generate a fresh key using `secrets.token_hex(32)` and supply it through your local process-launch environment; do not commit or print its value. Missing or malformed keys leave the optional listener disabled. Configuration changes take effect on host restart.

The subsequent owner-authorized live probe temporarily installed the reviewed extender DLLs and config, supplied a per-launch key, and restored the initial absence of those test files afterward. Loopback traffic is authenticated, not encrypted. Key holders can invoke registered services; game-facing providers still need their own operation policy.

Once that host is running, use the actual logged port and expected host identity/version:

```python
from tge_sdk_transport import TgeSdkClient, key_from_environment

client = TgeSdkClient(port, key_from_environment(),
                      expected_host_id="kane.tgfoa.tainted-grail-extender",
                      expected_host_version="0.1.0")
identity = client.connect()

query = client.create_request("tge.core.identity", "0.1", "services")
services = client.invoke(query)
```

Service discovery returns up to 16 entries per page. When `values["nextOffset"]` is nonempty, pass it as the next request's `arguments={"offset": next_offset}`. Use a new invocation ID for each new operation. Retain the exact request for an intentional retry: a transport timeout after dispatch may have an unknown execution outcome. The adapter returns the cached result for the same request ID and unchanged contents. The client never retries automatically and never silently changes sessions after a host restart.

To connect and list registered services from the command line (replace `<port>` with the actual port):

```shell
python Gems/TaintedGrailModdingSDK/Tools/tge_sdk_transport.py --port <port>
```

`--service-id`, `--service-version`, `--operation` and repeated `--argument KEY=VALUE` select an implemented service. The CLI writes request/correlation/session IDs to stderr and the result to stdout. Exit codes: 0 success, 1 explicit service rejection/failure, 2 client or connection failure. The key is read only from the environment.

To print a handshake request for inspection:

```shell
python Gems/TaintedGrailModdingSDK/Tools/tge_sdk_client.py
```

## Executed validation

### Editor connection acceptance, 2026-09-12

- PASSED: 40 Python codec, transport and Editor observation tests, including
  production managed-listener interoperability.
- PASSED: exact-pin prerequisites/configure and changed Framework/Editor/test
  targets, using the existing engine dependency libraries. The full dependency
  rebuild encountered Windows Controlled Folder Access blocking Qt's generator.
- PASSED: 439 compiled Catalog tests discovered, 437 passed and two explicit
  privilege skips; all eight connection-service cases passed.
- PASSED: seven live Editor checks against a synthetic authenticated listener,
  including Home navigation, masked key, two-page discovery, refresh, restart,
  wrong key, cancellation and close/reopen. The observed maximum UI timer gap
  was 47 ms against a 500 ms threshold. Closing/hiding clears retained state;
  scrolling preserves access to details in short dock areas.
- PASSED: applicable static checks and ten enabled source-policy validators
  for this scope. Two unrelated in-progress player-position files were excluded
  from source-policy acceptance and remain reported by the full Gem scan.
- PARTIAL: the disposable asset preparation had unrelated generated material
  failures; startup assets sufficed for UI acceptance. This is not full renderer
  or Asset Processor acceptance.
- NOT_RUN: this new pane connected to the installed game. The earlier live
  client handshake below is historical evidence, not a new Editor/game result.

Private logs, screenshots and source/artifact hashes stay outside the checkout.
Run `editor_game_connection_smoke.py` through a dedicated Editor `--runpython`
session with `FOA_CONNECTION_EVIDENCE` set to an external directory. The script
starts a synthetic loopback listener, tests the pane and exits that test Editor.

### Earlier adapter and transport acceptance

The adapter and transport implementation passed:

- 1,165 managed checks, including actual service invocation, immutable inputs, exact versions, malformed frames, rollback of failed registration, replay, aggregate replay limits, transport lifecycle, thread affinity and stop/restart.
- 11 Python client tests, including three cases against the compiled managed adapter: Unicode and replay, service discovery/version rejection, and a near-limit escaped frame.
- 17 Python transport tests, including 15 against the production C# listener/Pump: authentication and host binding, malformed and fragmented frames, concurrent dispatch, expiry, saturation, restart, shutdown, lost results and retries after handler timeout.
- 16 existing TGE manifest/package-lifecycle cases.
- TGE Contracts, Core and native BepInEx host compilation, with deployment disabled.
- Focused independent source reviews. The initial adapter escaping mismatch was fixed and covered by an interoperability regression; both transport review passes reported no actionable findings.

Run the managed fixture from the runtime repository:

```shell
dotnet run --project mods/tainted-grail-extender/tests/TGE.SdkAdapter.Fixtures/TGE.SdkAdapter.Fixtures.csproj -c Release -p:DeployOnBuild=false
```

Then point `TGE_SDK_FIXTURE_DLL` at that project's compiled `bin/Release/net10.0/TGE.SdkAdapter.Fixtures.dll` and run from FOA-SDK:

```shell
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p test_tge_sdk_client.py -v
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p test_tge_sdk_transport.py -v
```

Without that environment variable, the three adapter and fifteen transport interoperability cases are explicitly skipped. The fixture's `--stdio` and `--tcp` modes run a managed test host identified as `tge.fixture.host`, version `1.0.0`; neither is the running game. The TCP suite creates a fresh test key per fixture process and uses real OS sockets and the production listener.

The listener admits at most four connections and four pending requests, limits each JSON frame to 65,536 UTF-8 bytes and uses a total five-second server deadline. `Plugin.Update` pumps at most four requests, checking a 2 ms budget between handlers. It does not preempt a running handler. Expired pending work is removed even while pumping is paused. Shutdown closes sockets and cancels queued requests without waiting on network workers.

## Remaining operational work

The connection pane also supports [Preview Encounter](TGE_ENCOUNTER_PREVIEW.md)
for existing exported compositions. This requests a temporary host placement plan
and displays its actors, file digest and expiry without dispatching spawn/remove.
Its Editor acceptance and actual installed-game acceptance are separate lanes.

The owner-authorized installed-game check PASSED on Steam Mono build `24270691`, UnityPlayer `6000.0.64f1`, BepInEx `5.4.23.5`. The SDK authenticated to the extender in the actual game process, and discovery returned `tge.core.identity@0.1` owned by `tge.core`. Replay, stale-session, unknown-service and wrong-key controls passed, followed by another healthy discovery request. Windows verified that the listening port belonged to that game process.

The first direct launch exited before the loader started and Steam created a delayed replacement process. That failed attempt is preserved. The corrected launch used Valve's documented temporary `steam_appid.txt` development hint with app ID `1466060`, so the process carrying the SDK key could continue. The proof DLLs, config and app-ID hint were all removed after the test.

Process exit code zero, closed listener and file rollback are verified. No fresh extender shutdown marker was captured, so shutdown-callback evidence and the overall lifecycle receipt remain **PARTIAL**. This does not establish that the callback failed. Editor UI acceptance is recorded above; gameplay services and full lifecycle acceptance remain separate work. Host identity alone does not attest the game profile; this test also recorded the exact installed artifact hashes.

The runtime repository's `mods/tainted-grail-extender/docs/foa-sdk-live-handshake.md` documents the procedure; `docs/validation/foa-sdk-live-handshake-2026-09-12-attempt2.json` records the actual connection results and lifecycle limitation. Its `tools/run_sdk_live_probe.py` provides read-only preflight by default and a bounded `--execute` mode, pinned to the inspected build and loader. The adapter and transport evidence remain historical checkpoints. The generic runtime route compiler still lacks extender catalogue/performance coverage and is recorded BLOCKED, with the task's equivalent owner route documented separately.

No save-load or gameplay SDK command was issued. Full runtime sign-off was not performed.
