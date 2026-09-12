#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Bounded Editor identity, player-position and vitals observations through TGE."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import time

from tge_sdk_client import ProtocolError
from tge_sdk_transport import FOA_HOST_ID, TgeSdkClient, TransportError, key_from_environment
from tge_player_position import PlayerPositionUnavailable, read_player_position
from tge_player_vitals import PlayerVitalsUnavailable, read_player_vitals
from tge_encounters import composition_slots, load_data, preview

MAX_SERVICES = 256
TOTAL_TIMEOUT = 20.0


class ConnectionCheckError(ValueError):
    """A safe, actionable message; never includes arbitrary remote response text."""


def count(value: str) -> int:
    if not isinstance(value, str) or not re.fullmatch(r"0|[1-9][0-9]{0,2}", value) or int(value) > MAX_SERVICES:
        raise ConnectionCheckError("The extender returned an invalid service count. Update the SDK and extender together.")
    return int(value)


def check_connection(port: int, version: str, previous_session: str = "", *,
                     host_id: str = FOA_HOST_ID, client_factory=TgeSdkClient, clock=time.monotonic) -> dict:
    key = key_from_environment()
    deadline = clock() + TOTAL_TIMEOUT
    client = client_factory(port, key, expected_host_id=host_id, expected_host_version=version, timeout=5.0)

    def budget():
        # Each call retains the transport's own total deadline, bounded by the
        # remaining budget for the complete paginated observation.
        remaining = deadline - clock()
        if remaining <= 0:
            raise ConnectionCheckError("The connection check timed out. Check the game and choose Connect again.")
        client._timeout = min(5.0, remaining)

    budget()
    identity = client.connect()
    if previous_session and previous_session != identity.session_id:
        raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
    total = count(identity.values.get("serviceCount"))
    if not total:
        raise ConnectionCheckError("The extender did not report its identity service.")
    services = []
    seen = set()
    while len(services) < total:
        offset = len(services)
        budget()
        result = client.invoke(client.create_request("tge.core.identity", "0.1", "services", {"offset": str(offset)}))
        if not result.succeeded:
            messages = {
                "session_mismatch": "The extender restarted. Choose Connect to use its new session.",
                "invocation_capacity": "The extender's request capacity is exhausted. Restart the game and reconnect.",
            }
            raise ConnectionCheckError(messages.get(result.code, "The extender rejected service discovery. Check its log and reconnect."))
        values = result.values
        size = min(16, total - offset)
        fields = {"total", "nextOffset"} | {f"{index}.{field}" for index in range(size) for field in ("id", "version", "owner")}
        next_offset = str(offset + size) if offset + size < total else ""
        if set(values) != fields or count(values["total"]) != total or values["nextOffset"] != next_offset:
            raise ConnectionCheckError("The extender's service list changed or is malformed. Refresh the connection.")
        for index in range(size):
            service = {field: values[f"{index}.{field}"] for field in ("id", "version", "owner")}
            pair = (service["id"], service["version"])
            if (pair in seen or any(not text.strip() or len(text.encode("utf-16-le")) > 512 for text in service.values())
                    or not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", service["version"])):
                raise ConnectionCheckError("The extender returned an invalid or duplicate service identity.")
            seen.add(pair)
            services.append(service)
    budget()
    if ("tge.core.identity", "0.1") not in seen:
        raise ConnectionCheckError("The extender did not report its identity service.")
    return {"schema": 1, "hostId": identity.values["hostId"], "hostVersion": identity.values["hostVersion"],
            "sessionId": identity.session_id, "services": services}


def check_player_position(port: int, version: str, previous_session: str, *,
                          host_id: str = FOA_HOST_ID, client_factory=TgeSdkClient, clock=time.monotonic) -> dict:
    """Read one position from the bound session, with no discovery, polling or retry."""
    if not previous_session:
        raise ConnectionCheckError("Connect to the game before reading the player position.")
    deadline = clock() + TOTAL_TIMEOUT
    client = client_factory(port, key_from_environment(), expected_host_id=host_id,
                            expected_host_version=version, timeout=5.0)
    identity = client.connect()
    if identity.session_id != previous_session:
        raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
    remaining = deadline - clock()
    if remaining <= 0:
        raise ConnectionCheckError("The player-position check timed out. Reconnect and try again.")
    client._timeout = min(5.0, remaining)
    try:
        position = read_player_position(client)
        if position.session_id != previous_session:
            raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
        player = {"available": True, "sceneName": position.scene_name,
                  "x": position.x, "y": position.y, "z": position.z}
    except PlayerPositionUnavailable as unavailable:
        player = {"available": False, "code": unavailable.code}
    except ProtocolError:
        raise ConnectionCheckError("The player-position service failed or returned invalid data. Reconnect and check the extender log.") from None
    if clock() >= deadline:
        raise ConnectionCheckError("The player-position check timed out. Reconnect and try again.")
    return {"schema": 1, "hostId": identity.values["hostId"], "hostVersion": identity.values["hostVersion"],
            "sessionId": identity.session_id, "player": player}


def check_player_vitals(port: int, version: str, previous_session: str, *,
                        host_id: str = FOA_HOST_ID, client_factory=TgeSdkClient, clock=time.monotonic) -> dict:
    """Observe native vitals in the bound session; no polling, retry or setters."""
    if not previous_session:
        raise ConnectionCheckError("Connect to the game before reading player vitals.")
    deadline = clock() + TOTAL_TIMEOUT
    client = client_factory(port, key_from_environment(), expected_host_id=host_id,
                            expected_host_version=version, timeout=5.0)
    identity = client.connect()
    if identity.session_id != previous_session:
        raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
    remaining = deadline - clock()
    if remaining <= 0:
        raise ConnectionCheckError("The player-vitals check timed out. Reconnect and try again.")
    client._timeout = min(5.0, remaining)
    try:
        vitals = read_player_vitals(client)
        if vitals.session_id != previous_session:
            raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
        observation = {"available": True, "health": vitals.health, "healthMax": vitals.health_max,
                       "stamina": vitals.stamina, "staminaMax": vitals.stamina_max,
                       "mana": vitals.mana, "manaMax": vitals.mana_max}
    except PlayerVitalsUnavailable as unavailable:
        observation = {"available": False, "code": unavailable.code}
    except ProtocolError:
        raise ConnectionCheckError("Player vitals could not be read. This extender may not support vitals, or returned invalid data. Check its version and log, then reconnect.") from None
    if clock() >= deadline:
        raise ConnectionCheckError("The player-vitals check timed out. Reconnect and try again.")
    return {"schema": 1, "hostId": identity.values["hostId"], "hostVersion": identity.values["hostVersion"],
            "sessionId": identity.session_id, "vitals": observation}


def check_encounter_preview(port: int, version: str, previous_session: str, composition_path: str, *,
                            host_id: str = FOA_HOST_ID, client_factory=TgeSdkClient, clock=time.monotonic) -> dict:
    """Validate one data file and request a temporary plan; never dispatch spawn."""
    if not previous_session:
        raise ConnectionCheckError("Connect to the game before previewing an encounter.")
    try:
        composition, raw = load_data(Path(composition_path))
        slots = composition_slots(composition)
    except (OSError, ValueError, ProtocolError):
        # Keep paths and arbitrary file contents out of diagnostics.
        return {"schema": 1, "previewInputError": "Choose a valid encounter composition JSON file (at most 64 KiB)."}
    deadline = clock() + TOTAL_TIMEOUT
    client = client_factory(port, key_from_environment(), expected_host_id=host_id,
                            expected_host_version=version, timeout=5.0)
    identity = client.connect()
    if identity.session_id != previous_session:
        raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
    remaining = deadline - clock()
    if remaining <= 0:
        raise ConnectionCheckError("The encounter preview timed out. Reconnect and try again.")
    client._timeout = min(5.0, remaining)
    try:
        plan = preview(client, slots)
    except ProtocolError:
        raise ConnectionCheckError("Encounter preview was rejected or invalid. Check that encounters are enabled and a player is loaded; multiple actors require a completed single-actor check. Reconnect to try again.") from None
    if plan.session_id != previous_session:
        raise ConnectionCheckError("The extender restarted. Choose Connect to use its new session.")
    if clock() >= deadline:
        raise ConnectionCheckError("The encounter preview timed out. Reconnect and try again.")
    if len(plan.placement.encode("utf-16-le")) > 8192:
        raise ConnectionCheckError("The encounter preview returned an oversized placement. Reconnect and check the extender log.")
    return {"schema": 1, "hostId": identity.values["hostId"], "hostVersion": identity.values["hostVersion"],
            "sessionId": identity.session_id, "encounter": {"name": composition["name"],
            "compositionSha256": hashlib.sha256(raw).hexdigest(), "templates": list(plan.templates),
            "placement": plan.placement, "planId": plan.plan_id, "fingerprint": plan.fingerprint}}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, type=int)
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--session", default="")
    parser.add_argument("--operation", choices=("connection", "position", "vitals", "encounter-preview"), default="connection")
    parser.add_argument("--composition", default="")
    args = parser.parse_args(argv)
    try:
        if args.operation == "encounter-preview":
            result = check_encounter_preview(args.port, args.version, args.session, args.composition)
        else:
            operation = {"connection": check_connection, "position": check_player_position, "vitals": check_player_vitals}[args.operation]
            result = operation(args.port, args.version, args.session)
    except ConnectionCheckError as exc:
        result = {"schema": 1, "error": str(exc)}
    except ProtocolError:
        result = {"schema": 1, "error": "Authentication or extender identity could not be verified. Check the 64-character key and expected extender version."}
    except TransportError:
        result = {"schema": 1, "error": "Cannot reach the extender. Check that the game is running, its SDK listener is enabled, and the port and key match."}
    print(json.dumps(result, ensure_ascii=True, separators=(",", ":")))
    return 1 if "error" in result else 0


if __name__ == "__main__":
    raise SystemExit(main())
