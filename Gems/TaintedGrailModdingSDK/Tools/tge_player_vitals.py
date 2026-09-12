#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Read current and maximum player vitals through the extender-owned adapter."""

from __future__ import annotations

from dataclasses import dataclass
import math
import re
import struct

from tge_sdk_client import ProtocolError
from tge_sdk_transport import TgeSdkClient


_FIELDS = ("health", "healthMax", "stamina", "staminaMax", "mana", "manaMax")
_FLOAT = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?")


@dataclass(frozen=True)
class PlayerVitals:
    health: float
    health_max: float
    stamina: float
    stamina_max: float
    mana: float
    mana_max: float
    session_id: str


class PlayerVitalsUnavailable(RuntimeError):
    """The player or native stats are unavailable; no default values are supplied."""

    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def read_player_vitals(client: TgeSdkClient) -> PlayerVitals:
    """Take one fresh sample, without polling, retries or reconnection.

    Values preserve native ModifiedValue/UpperLimit semantics without clamping.
    Native stat getters can refresh their own caches. This is an observation
    request, not a guarantee that native getters are free of side effects.
    """
    request = client.create_request("tge.foa.player", "0.1", "vitals")
    result = client.invoke(request)
    if result.status == "rejected" and result.code in {"player_unavailable", "vitals_unavailable"}:
        if result.values:
            raise ProtocolError("Unavailable player response must not contain vital values")
        raise PlayerVitalsUnavailable(result.code)
    if not result.succeeded:
        raise ProtocolError(f"Player-vitals query did not succeed: {result.code}")
    if result.code != "player_vitals" or set(result.values) != set(_FIELDS):
        raise ProtocolError("Unexpected player-vitals result shape")
    values = []
    for field in _FIELDS:
        encoded = result.values[field]
        if not isinstance(encoded, str) or not _FLOAT.fullmatch(encoded):
            raise ProtocolError("Player-vitals response contains an invalid number")
        value = float(encoded)
        if not math.isfinite(value) or abs(value) > 3.4028235e38:
            raise ProtocolError("Player-vitals value is outside the finite Unity float range")
        nonzero = any(digit in "123456789" for digit in encoded.lower().split("e", 1)[0])
        if nonzero and struct.unpack("!f", struct.pack("!f", value))[0] == 0:
            raise ProtocolError("Player-vitals value underflows the Unity float range")
        values.append(value)
    return PlayerVitals(*values, session_id=result.session_id)
