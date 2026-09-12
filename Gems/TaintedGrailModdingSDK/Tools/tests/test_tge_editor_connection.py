#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Read-only Editor observations: bounded paging, session binding and real transport."""

import contextlib
import io
import json
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tge_editor_connection as connection
from tge_sdk_client import ProtocolError, SdkResponse
from tge_sdk_transport import TransportError


class FakeClient:
    def __init__(self, total=33):
        self.total = total
        self.calls = []
        self.session = "current-session"
        self.alter = lambda values: values
        self.code = "services"

    def connect(self):
        return SdkResponse(self.session, "succeeded", "identity", "", {
            "serviceCount": str(self.total), "hostId": connection.FOA_HOST_ID, "hostVersion": "0.1.0"})

    def create_request(self, service, version, operation, arguments):
        self.calls.append((service, version, operation, arguments))
        return arguments

    def invoke(self, request):
        offset = int(request["offset"])
        end = min(offset + 16, self.total)
        values = {"total": str(self.total), "nextOffset": str(end) if end < self.total else ""}
        for index in range(end - offset):
            values.update({f"{index}.id": "tge.core.identity" if offset + index == 0 else f"fixture.service{offset + index}",
                           f"{index}.version": "0.1", f"{index}.owner": "fixture"})
        return SdkResponse(self.session, "succeeded" if self.code == "services" else "rejected", self.code,
                           "untrusted text", self.alter(values))


class ConnectionTests(unittest.TestCase):
    def setUp(self):
        self.env = patch.dict(os.environ, {"TGE_SDK_KEY": "ab" * 32})
        self.env.start()
        self.addCleanup(self.env.stop)
        self.client = FakeClient()

    def check(self, **kwargs):
        return connection.check_connection(12345, "0.1.0", client_factory=lambda *a, **k: self.client, **kwargs)

    def test_all_pages_are_read_only_and_complete(self):
        result = self.check()
        self.assertEqual(len(result["services"]), 33)
        self.assertEqual([row[3]["offset"] for row in self.client.calls], ["0", "16", "32"])
        self.assertTrue(all(row[:3] == ("tge.core.identity", "0.1", "services") for row in self.client.calls))
        self.assertNotIn("ab" * 32, json.dumps(result))

    def test_maximum_services_has_exactly_sixteen_pages(self):
        self.client.total = 256
        self.assertEqual(len(self.check()["services"]), 256)
        self.assertEqual(len(self.client.calls), 16)

    def test_changed_session_never_discovers_services(self):
        with self.assertRaisesRegex(connection.ConnectionCheckError, "restarted"):
            self.check(previous_session="old-session")
        self.assertEqual(self.client.calls, [])

    def test_same_session_can_be_refreshed(self):
        self.assertEqual(self.check(previous_session=self.client.session)["sessionId"], self.client.session)

    def test_malformed_and_changed_page_fails_atomically(self):
        changes = ({"nextOffset": "0"}, {"nextOffset": "016"}, {"total": "32"}, {"total": "257"},
                   {"0.version": "01.0"}, {"0.owner": ""}, {"extra": "bad"}, {"1.id": "tge.core.identity"})
        for change in changes:
            with self.subTest(change=change):
                self.client.alter = lambda values: values | change
                with self.assertRaises(connection.ConnectionCheckError):
                    self.check()

    def test_missing_identity_and_missing_page_field_are_rejected(self):
        for field in ("0.id", "nextOffset", "total"):
            def remove(values):
                del values[field]
                return values
            self.client.alter = remove
            with self.assertRaises(connection.ConnectionCheckError):
                self.check()
        self.client.total = 1
        self.client.alter = lambda values: values | {"0.id": "fixture.other"}
        with self.assertRaisesRegex(connection.ConnectionCheckError, "identity service"):
            self.check()

    def test_invalid_counts_do_not_start_paging(self):
        for total in (0, 257, -1, "01", None, "9" * 100):
            self.client.total = total
            with self.subTest(total=total), self.assertRaises(connection.ConnectionCheckError):
                self.check()
        self.assertEqual(self.client.calls, [])

    def test_deadline_stops_before_next_page(self):
        ticks = iter((0, 0, 1, 21))
        with self.assertRaisesRegex(connection.ConnectionCheckError, "timed out"):
            self.check(clock=lambda: next(ticks))
        self.assertEqual(len(self.client.calls), 1)

    def test_rejections_have_safe_actionable_text(self):
        for code in ("session_mismatch", "invocation_capacity", "unexpected"):
            self.client.code = code
            with self.assertRaises(connection.ConnectionCheckError) as error:
                self.check()
            self.assertNotIn("untrusted text", str(error.exception))

    def test_cli_failure_never_prints_exception_secrets(self):
        for error in (ProtocolError("private key"), TransportError("private path")):
            out = io.StringIO()
            with patch.object(connection, "check_connection", side_effect=error), contextlib.redirect_stdout(out):
                self.assertEqual(connection.main(["--port", "12345"]), 1)
            result = json.loads(out.getvalue())
            self.assertIn("error", result)
            self.assertNotIn("private", out.getvalue())


@unittest.skipUnless(os.environ.get("TGE_SDK_FIXTURE_DLL"), "Set TGE_SDK_FIXTURE_DLL for production listener interoperability")
class EditorManagedInteropTests(unittest.TestCase):
    def setUp(self):
        from test_tge_sdk_transport import ManagedTransportInteropTests
        self.host = ManagedTransportInteropTests()
        self.host.setUp()
        self.addCleanup(self.host.tearDown)
        self.env = patch.dict(os.environ, self.host.env)
        self.env.start()
        self.addCleanup(self.env.stop)

    def check(self, session=""):
        return connection.check_connection(self.host.port, "1.0.0", session, host_id="tge.fixture.host")

    def test_production_listener_discovery_refresh_and_restart(self):
        first = self.check()
        self.assertIn("tge.core.identity", [row["id"] for row in first["services"]])
        self.assertEqual(self.check(first["sessionId"]), first)
        self.host.command("restart")
        with self.assertRaisesRegex(connection.ConnectionCheckError, "restarted"):
            self.check(first["sessionId"])
        self.assertNotEqual(self.check()["sessionId"], first["sessionId"])

    def test_wrong_version_and_key_are_rejected(self):
        with self.assertRaises(ProtocolError):
            connection.check_connection(self.host.port, "2.0.0", host_id="tge.fixture.host")
        with patch.dict(os.environ, {"TGE_SDK_KEY": "cd" * 32}), self.assertRaises((TransportError, ProtocolError)):
            self.check()


class EditorPlayerTests(unittest.TestCase):
    def setUp(self):
        env = patch.dict(os.environ, {"TGE_SDK_KEY": "ab" * 32})
        env.start()
        self.addCleanup(env.stop)
        self.client = Mock()
        self.client.connect.return_value = SdkResponse("session", "succeeded", "identity", "", {
            "hostId": connection.FOA_HOST_ID, "hostVersion": "0.1.0"})
        self.values = {"sceneName": "fixture-\u03b1", "x": "1.25", "y": "-2.5", "z": "3.75"}
        self.response()

    def response(self, status="succeeded", code="player_position", values=None):
        self.client.invoke.return_value = SdkResponse("session", status, code, "private remote message",
            self.values if values is None else values)

    def check(self, session="session", **kwargs):
        return connection.check_player_position(12345, "0.1.0", session,
            client_factory=lambda *a, **k: self.client, **kwargs)

    def test_position_is_fresh_and_only_calls_read_operation(self):
        first = self.check()
        self.response(values=self.values | {"x": "10.5"})
        second = self.check()
        self.assertEqual((first["player"]["x"], second["player"]["x"]), (1.25, 10.5))
        self.assertEqual(first["player"]["sceneName"], "fixture-\u03b1")
        self.assertEqual(self.client.create_request.call_count, 2)
        self.client.create_request.assert_called_with("tge.foa.player", "0.1", "position")
        self.assertNotIn("ab" * 32, json.dumps(first))

    def test_missing_or_changed_session_does_not_query_position(self):
        for session in ("", "old-session"):
            with self.assertRaises(connection.ConnectionCheckError):
                self.check(session)
        self.client.invoke.assert_not_called()

    def test_unavailable_results_have_no_coordinates(self):
        for code in ("player_unavailable", "scene_unavailable", "position_unavailable"):
            self.response("rejected", code, {})
            self.assertEqual(self.check()["player"], {"available": False, "code": code})

    def test_malformed_or_failed_result_is_not_an_unavailable_player(self):
        for values in ({}, self.values | {"x": "NaN"}, self.values | {"sceneName": ""}):
            self.response(values=values)
            with self.assertRaises(connection.ConnectionCheckError):
                self.check()
        self.response("failed", "private-code", {})
        with self.assertRaises(connection.ConnectionCheckError) as error:
            self.check()
        self.assertNotIn("private", str(error.exception))

    def test_changed_result_session_is_rejected(self):
        self.client.invoke.return_value = SdkResponse("changed", "succeeded", "player_position", "", self.values)
        with self.assertRaisesRegex(connection.ConnectionCheckError, "restarted"):
            self.check()

    def test_deadline_before_and_after_position_query(self):
        for times in ((0, 21), (0, 1, 21)):
            ticks = iter(times)
            with self.assertRaisesRegex(connection.ConnectionCheckError, "timed out"):
                self.check(clock=lambda: next(ticks))

    def test_cli_routes_position_and_suppresses_private_errors(self):
        out = io.StringIO()
        with patch.object(connection, "check_player_position", side_effect=ProtocolError("private")), contextlib.redirect_stdout(out):
            self.assertEqual(connection.main(["--port", "12345", "--session", "session", "--operation", "position"]), 1)
        self.assertNotIn("private", out.getvalue())


@unittest.skipUnless(os.environ.get("TGE_PLAYER_FIXTURE_DLL"), "Set TGE_PLAYER_FIXTURE_DLL for production player interoperability")
class EditorPlayerManagedTests(unittest.TestCase):
    def test_worker_reads_production_service_movement_and_menu(self):
        from test_tge_player_position import PlayerSocketTests
        host = PlayerSocketTests()
        host.setUp()
        self.addCleanup(host.doCleanups)
        session = host.client.connect().session_id
        def check():
            with patch.dict(os.environ, {"TGE_SDK_KEY": host.client._key.hex()}):
                return connection.check_player_position(host.client._port, "1.0.0", session, host_id="tge.player.fixture")
        self.assertEqual(check()["player"], {"available": False, "code": "player_unavailable"})
        host.command("player")
        self.assertEqual(check()["player"]["x"], 1.25)
        host.command("move")
        self.assertEqual(check()["player"]["x"], 10.5)
        host.command("menu")
        self.assertFalse(check()["player"]["available"])


class EditorVitalsTests(unittest.TestCase):
    def setUp(self):
        env = patch.dict(os.environ, {"TGE_SDK_KEY": "ab" * 32})
        env.start()
        self.addCleanup(env.stop)
        self.client = Mock()
        self.client.connect.return_value = SdkResponse("session", "succeeded", "identity", "", {
            "hostId": connection.FOA_HOST_ID, "hostVersion": "0.1.0"})
        self.values = {"health": "75.25", "healthMax": "100.5", "stamina": "50.5",
                       "staminaMax": "80.75", "mana": "10.75", "manaMax": "40.25"}
        self.response()

    def response(self, status="succeeded", code="player_vitals", values=None, session="session"):
        self.client.invoke.return_value = SdkResponse(session, status, code, "private message",
            self.values if values is None else values)

    def check(self, session="session", **kwargs):
        return connection.check_player_vitals(12345, "0.1.0", session,
            client_factory=lambda *a, **k: self.client, **kwargs)

    def test_fresh_native_values_are_preserved_without_clamping(self):
        first = self.check()["vitals"]
        self.assertEqual(first, {"available": True} | {key: float(value) for key, value in self.values.items()})
        self.response(values=self.values | {"health": "120", "stamina": "-1", "manaMax": "0"})
        second = self.check()["vitals"]
        self.assertEqual((second["health"], second["stamina"], second["manaMax"]), (120, -1, 0))
        self.client.create_request.assert_called_with("tge.foa.player", "0.1", "vitals")
        self.assertEqual(self.client.create_request.call_count, 2)
        self.assertNotIn("ab" * 32, json.dumps(first))

    def test_missing_or_changed_session_does_not_read_vitals(self):
        for session in ("", "old-session"):
            with self.assertRaises(connection.ConnectionCheckError):
                self.check(session)
        self.client.invoke.assert_not_called()

    def test_unavailable_does_not_supply_zero_or_partial_stats(self):
        for code in ("player_unavailable", "vitals_unavailable"):
            self.response("rejected", code, {})
            self.assertEqual(self.check()["vitals"], {"available": False, "code": code})

    def test_malformed_and_unsupported_operation_are_errors(self):
        for values in ({}, self.values | {"mana": "NaN"}, self.values | {"healthMax": "1e-999"}):
            self.response(values=values)
            with self.assertRaises(connection.ConnectionCheckError):
                self.check()
        self.response("rejected", "unknown_operation", {})
        with self.assertRaisesRegex(connection.ConnectionCheckError, "may not support vitals") as error:
            self.check()
        self.assertNotIn("private", str(error.exception))

    def test_changed_result_session_is_rejected(self):
        self.response(session="changed")
        with self.assertRaisesRegex(connection.ConnectionCheckError, "restarted"):
            self.check()

    def test_deadline_before_and_after_vitals(self):
        for times in ((0, 21), (0, 1, 21)):
            ticks = iter(times)
            with self.assertRaisesRegex(connection.ConnectionCheckError, "timed out"):
                self.check(clock=lambda: next(ticks))

    def test_cli_routes_vitals_without_private_error_text(self):
        out = io.StringIO()
        with patch.object(connection, "check_player_vitals", side_effect=TransportError("private")), contextlib.redirect_stdout(out):
            self.assertEqual(connection.main(["--port", "12345", "--session", "session", "--operation", "vitals"]), 1)
        self.assertNotIn("private", out.getvalue())


@unittest.skipUnless(os.environ.get("TGE_VITALS_FIXTURE_DLL"), "Set TGE_VITALS_FIXTURE_DLL for production vitals interoperability")
class EditorVitalsManagedTests(unittest.TestCase):
    def test_worker_observes_production_vitals_spend_recovery_and_menu(self):
        from test_tge_player_vitals import PlayerVitalsSocketTests
        host = PlayerVitalsSocketTests()
        host.setUp()
        self.addCleanup(host.doCleanups)
        session = host.client.connect().session_id
        def check():
            with patch.dict(os.environ, {"TGE_SDK_KEY": host.client._key.hex()}):
                return connection.check_player_vitals(host.client._port, "1.0.0", session, host_id="tge.vitals.fixture")
        self.assertFalse(check()["vitals"]["available"])
        host.command("player")
        first = check()["vitals"]
        host.command("spend")
        self.assertEqual(check()["vitals"]["stamina"], 12)
        host.command("recover")
        self.assertEqual(check()["vitals"], first)
        host.command("nonfinite")
        self.assertEqual(check()["vitals"], {"available": False, "code": "vitals_unavailable"})
        host.command("menu")
        self.assertEqual(check()["vitals"], {"available": False, "code": "player_unavailable"})


if __name__ == "__main__":
    unittest.main()
