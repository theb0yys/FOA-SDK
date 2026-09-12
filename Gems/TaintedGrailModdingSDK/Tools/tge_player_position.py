#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Read the extender's latest native player coordinates and active Unity scene."""

from __future__ import annotations

from dataclasses import dataclass
import math
import re
import struct

from tge_sdk_client import ProtocolError
from tge_sdk_transport import TgeSdkClient


@dataclass(frozen=True)
class PlayerPosition:
    scene_name: str
    x: float
    y: float
    z: float
    session_id: str


class PlayerPositionUnavailable(RuntimeError):
    """No position was returned; code identifies the unavailable native input."""

    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def read_player_position(client: TgeSdkClient) -> PlayerPosition:
    """Take one fresh sample. Never retry, poll, reconnect or reuse an invocation ID.

    scene_name is the raw active Unity scene, not a logical game location.
    Coordinates are the most recently published native movement position.
    """
    request = client.create_request("tge.foa.player", "0.1", "position")
    result = client.invoke(request)
    if result.status == "rejected" and result.code in {
        "player_unavailable", "scene_unavailable", "position_unavailable",
    }:
        if result.values:
            raise ProtocolError("Unavailable player response must not contain position values")
        raise PlayerPositionUnavailable(result.code)
    if not result.succeeded:
        raise ProtocolError(f"Player-position query did not succeed: {result.code}")
    if result.code != "player_position" or set(result.values) != {"sceneName", "x", "y", "z"}:
        raise ProtocolError("Unexpected player-position result shape")
    scene = result.values["sceneName"]
    if not isinstance(scene, str) or not scene.strip():
        raise ProtocolError("Player-position response requires an active scene name")
    coordinates = []
    for axis in ("x", "y", "z"):
        value = result.values[axis]
        if not isinstance(value, str) or not re.fullmatch(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", value):
            raise ProtocolError("Player-position response contains an invalid coordinate")
        coordinate = float(value)
        if not math.isfinite(coordinate) or abs(coordinate) > 3.4028235e38:
            raise ProtocolError("Player-position coordinate is outside the finite Unity float range")
        nonzero = any(digit in "123456789" for digit in value.lower().split("e", 1)[0])
        if nonzero and struct.unpack("!f", struct.pack("!f", coordinate))[0] == 0:
            raise ProtocolError("Player-position coordinate underflows the Unity float range")
        coordinates.append(coordinate)
    return PlayerPosition(scene, *coordinates, session_id=result.session_id)
