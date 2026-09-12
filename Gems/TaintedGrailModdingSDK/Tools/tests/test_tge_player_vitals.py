#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Vitals consumer contracts and real sockets with explicitly synthetic stats."""

from dataclasses import FrozenInstanceError
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
from tge_player_vitals import PlayerVitalsUnavailable, read_player_vitals
from tge_player_position import read_player_position
from tge_sdk_client import ProtocolError, SdkResponse
from tge_sdk_transport import TgeSdkClient


GOOD = {"health": "75.25", "healthMax": "100.5", "stamina": "50.5",
        "staminaMax": "80.75", "mana": "10.75", "manaMax": "40.25"}


class PlayerVitalsTests(unittest.TestCase):
    def client(self, *, status="succeeded", code="player_vitals", values=None):
        client = Mock()
        client.invoke.return_value = SdkResponse("session", status, code, "", GOOD if values is None else values)
        return client

    def test_exact_request_and_frozen_snapshot(self):
        client = self.client()
        result = read_player_vitals(client)
        self.assertEqual((result.health, result.health_max, result.stamina, result.stamina_max,
                          result.mana, result.mana_max, result.session_id),
                         (75.25, 100.5, 50.5, 80.75, 10.75, 40.25, "session"))
        client.create_request.assert_called_once_with("tge.foa.player", "0.1", "vitals")
        client.invoke.assert_called_once_with(client.create_request.return_value)
        with self.assertRaises(FrozenInstanceError):
            result.health = 0

    def test_each_call_creates_a_request_without_retry(self):
        client = self.client()
        first, second = object(), object()
        client.create_request.side_effect = [first, second]
        read_player_vitals(client)
        read_player_vitals(client)
        self.assertEqual([call.args[0] for call in client.invoke.call_args_list], [first, second])

    def test_unavailable_is_explicit(self):
        for code in ("player_unavailable", "vitals_unavailable"):
            with self.subTest(code=code), self.assertRaises(PlayerVitalsUnavailable) as error:
                read_player_vitals(self.client(status="rejected", code=code, values={}))
            self.assertEqual(error.exception.code, code)
            with self.assertRaises(ProtocolError):
                read_player_vitals(self.client(status="rejected", code=code))

    def test_failures_and_older_host_are_errors(self):
        for status, code in (("failed", "service_failed"), ("rejected", "unknown_operation"),
                             ("rejected", "service_not_found"), ("rejected", "session_mismatch"),
                             ("rejected", "invocation_capacity"), ("failed", "vitals_unavailable")):
            with self.subTest(code=code), self.assertRaises(ProtocolError):
                read_player_vitals(self.client(status=status, code=code, values={}))

    def test_exact_shape_and_success_code(self):
        invalid = [{}, GOOD | {"extra": "1"}]
        invalid.extend({key: value for key, value in GOOD.items() if key != missing} for missing in GOOD)
        for values in invalid:
            with self.subTest(values=values), self.assertRaises(ProtocolError):
                read_player_vitals(self.client(values=values))
        with self.assertRaises(ProtocolError):
            read_player_vitals(self.client(code="player_position"))

    def test_invalid_numbers_rejected_for_every_field(self):
        for field in GOOD:
            for value in ("NaN", "Infinity", "-Infinity", "1e999", "3.5e38", "-3.5e38", "1,25",
                          " 1", "1 ", "1_0", "", "01", "+1", "\u0661", "1e-999", "-1e-999",
                          "1e-46", "-1e-46", "1\n", None, 10, True):
                with self.subTest(field=field, value=value), self.assertRaises(ProtocolError):
                    read_player_vitals(self.client(values=GOOD | {field: value}))

    def test_native_ranges_are_not_clamped(self):
        values = {"health": "3.4028235E+38", "healthMax": "0", "stamina": "-1E-45",
                  "staminaMax": "-1", "mana": "1E-45", "manaMax": "-0.0E-999"}
        result = read_player_vitals(self.client(values=values))
        self.assertGreater(result.health, result.health_max)
        self.assertLess(result.stamina, 0)
        self.assertEqual(result.stamina_max, -1)
        self.assertGreater(result.mana, 0)
        self.assertEqual(result.mana_max, 0)

    def test_transport_failure_is_not_retried(self):
        client = self.client()
        client.invoke.side_effect = TimeoutError("test timeout")
        with self.assertRaises(TimeoutError):
            read_player_vitals(client)
        self.assertEqual(client.invoke.call_count, 1)


@unittest.skipUnless(os.environ.get("TGE_VITALS_FIXTURE_DLL"), "Set TGE_VITALS_FIXTURE_DLL for synthetic native socket proof")
class PlayerVitalsSocketTests(unittest.TestCase):
    def setUp(self):
        key = secrets.token_bytes(32)
        self.process = subprocess.Popen(["dotnet", os.environ["TGE_VITALS_FIXTURE_DLL"], "--tcp"],
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
        self.client = TgeSdkClient(port, key, expected_host_id="tge.vitals.fixture", expected_host_version="1.0.0")
        self.client.connect()

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
            self.fail("Synthetic vitals fixture failed to close")
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

    def test_discovery_spend_recovery_replay_and_unavailable(self):
        discovery = self.client.invoke(self.client.create_request("tge.core.identity", "0.1", "services"))
        self.assertIn("tge.foa.player", discovery.values.values())
        with self.assertRaises(PlayerVitalsUnavailable):
            read_player_vitals(self.client)
        self.command("player")
        first = read_player_vitals(self.client)
        held = self.client.create_request("tge.foa.player", "0.1", "vitals")
        old = self.client.invoke(held)
        self.command("spend")
        spent = read_player_vitals(self.client)
        self.assertEqual((spent.health, spent.stamina, spent.mana), (60, 12, 3))
        self.assertEqual((spent.health_max, spent.stamina_max, spent.mana_max),
                         (first.health_max, first.stamina_max, first.mana_max))
        self.assertEqual(self.client.invoke(held), old)
        self.assertEqual(first.session_id, spent.session_id)
        self.command("recover")
        self.assertEqual(read_player_vitals(self.client), first)
        self.assertEqual(read_player_position(self.client).x, 1.25)
        self.command("nonfinite")
        with self.assertRaises(PlayerVitalsUnavailable) as error:
            read_player_vitals(self.client)
        self.assertEqual(error.exception.code, "vitals_unavailable")
        self.command("menu")
        with self.assertRaises(PlayerVitalsUnavailable) as error:
            read_player_vitals(self.client)
        self.assertEqual(error.exception.code, "player_unavailable")


if __name__ == "__main__":
    unittest.main()
