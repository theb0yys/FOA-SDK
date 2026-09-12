#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Consumer validation and real-socket tests with explicitly synthetic game inputs."""
import json
import os
from pathlib import Path
import queue
import secrets
import subprocess
import sys
import threading
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tge_player_position import PlayerPositionUnavailable, read_player_position
from tge_sdk_client import ProtocolError, SdkResponse
from tge_sdk_transport import TgeSdkClient


class PlayerPositionTests(unittest.TestCase):
    def client(self, *, status="succeeded", code="player_position", values=None):
        client = Mock()
        client.invoke.return_value = SdkResponse("session", status, code, "", values if values is not None else {
            "sceneName": "fixture-scene-\u03b1", "x": "1.25", "y": "-2.5", "z": "3.75",
        })
        return client

    def test_typed_snapshot_and_exact_request(self):
        client = self.client()
        result = read_player_position(client)
        self.assertEqual((result.scene_name, result.x, result.y, result.z, result.session_id),
                         ("fixture-scene-\u03b1", 1.25, -2.5, 3.75, "session"))
        client.create_request.assert_called_once_with("tge.foa.player", "0.1", "position")
        client.invoke.assert_called_once_with(client.create_request.return_value)

    def test_unavailable_has_no_default_coordinates(self):
        for code in ("player_unavailable", "scene_unavailable", "position_unavailable"):
            with self.subTest(code=code), self.assertRaises(PlayerPositionUnavailable) as error:
                read_player_position(self.client(status="rejected", code=code, values={}))
            self.assertEqual(error.exception.code, code)

    def test_failures_are_not_unavailability(self):
        for status, code in (("failed", "service_failed"), ("rejected", "unknown_service"),
                             ("rejected", "session_mismatch"), ("rejected", "capacity_exceeded")):
            with self.subTest(code=code), self.assertRaises(ProtocolError):
                read_player_position(self.client(status=status, code=code, values={}))

    def test_malformed_results_rejected(self):
        good = dict(self.client().invoke.return_value.values)
        bad_maps = [{}, good | {"extra": "x"}, good | {"sceneName": " "}]
        for axis in ("x", "y", "z"):
            for value in ("NaN", "Infinity", "-Infinity", "1e999", "3.5e38", "1,25", " 1", "1_0", "", "01", "\u0661",
                          "1e-999", "-1e-999", "1e-46", "-1e-46"):
                bad_maps.append(good | {axis: value})
        for values in bad_maps:
            with self.subTest(values=values), self.assertRaises(ProtocolError):
                read_player_position(self.client(values=values))
        with self.assertRaises(ProtocolError):
            read_player_position(self.client(code="unexpected"))
        with self.assertRaises(ProtocolError):
            read_player_position(self.client(status="rejected", code="player_unavailable"))

    def test_scientific_notation_and_zero(self):
        result = read_player_position(self.client(values={
            "sceneName": "Scene", "x": "3.4028235E+38", "y": "-1.401298E-45", "z": "0",
        }))
        self.assertGreater(result.x, 3e38)
        self.assertLess(result.y, 0)
        self.assertEqual(result.z, 0)
        result = read_player_position(self.client(values={
            "sceneName": "Scene", "x": "1E-45", "y": "-1E-45", "z": "-0.0E-999",
        }))
        self.assertGreater(result.x, 0)
        self.assertLess(result.y, 0)
        self.assertEqual(result.z, 0)


@unittest.skipUnless(os.environ.get("TGE_PLAYER_FIXTURE_DLL"), "Set TGE_PLAYER_FIXTURE_DLL for synthetic native socket proof")
class PlayerSocketTests(unittest.TestCase):
    def setUp(self):
        key = secrets.token_bytes(32)
        self.process = subprocess.Popen(["dotnet", os.environ["TGE_PLAYER_FIXTURE_DLL"], "--tcp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            encoding="utf-8", env=os.environ | {"TGE_SDK_KEY": key.hex()})
        self.lines = queue.Queue(maxsize=8)
        def read():
            for line in self.process.stdout:
                self.lines.put(line, timeout=5)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.addCleanup(self.close)
        port = json.loads(self.lines.get(timeout=10))["port"]
        self.client = TgeSdkClient(port, key, expected_host_id="tge.player.fixture", expected_host_version="1.0.0")
        self.client.connect()

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
            self.fail("Synthetic player fixture failed to close")
        finally:
            self.reader.join(timeout=3)
            self.process.stdout.close()
            errors = self.process.stderr.read()
            self.process.stderr.close()
        self.assertEqual(self.process.returncode, 0, errors)

    def command(self, value):
        self.process.stdin.write(value + "\n")
        self.process.stdin.flush()
        self.assertEqual(json.loads(self.lines.get(timeout=5)), {})

    def test_discovery_movement_replay_and_return_to_menu(self):
        discovery = self.client.invoke(self.client.create_request("tge.core.identity", "0.1", "services"))
        self.assertIn("tge.foa.player", discovery.values.values())
        with self.assertRaises(PlayerPositionUnavailable):
            read_player_position(self.client)
        self.command("player")
        first = read_player_position(self.client)
        held = self.client.create_request("tge.foa.player", "0.1", "position")
        old = self.client.invoke(held)
        self.command("move")
        second = read_player_position(self.client)
        self.assertEqual((first.x, second.x), (1.25, 10.5))
        self.assertEqual(first.scene_name, "fixture-scene-\u03b1")
        self.assertEqual(first.session_id, second.session_id)
        self.assertEqual(self.client.invoke(held), old)
        self.command("menu")
        with self.assertRaises(PlayerPositionUnavailable):
            read_player_position(self.client)


if __name__ == "__main__":
    unittest.main()
