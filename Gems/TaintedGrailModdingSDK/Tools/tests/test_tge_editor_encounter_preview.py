#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Editor encounter preview input, session, plan and non-spawn boundaries."""

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tge_editor_connection as connection
from tge_sdk_client import SdkResponse
from tge_sdk_transport import TransportError


class EditorEncounterPreviewTests(unittest.TestCase):
    def setUp(self):
        env = patch.dict(os.environ, {"TGE_SDK_KEY": "ab" * 32})
        env.start()
        self.addCleanup(env.stop)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.path = Path(temporary.name) / "patrol with spaces.json"
        self.composition = {"contract": "foa-tge-encounter-composition/1", "name": "<b>Patrol</b>",
                            "entries": [{"template": "wyrdspirit", "count": 1}], "source": None}
        self.path.write_text(json.dumps(self.composition), encoding="utf-8")
        self.client = Mock()
        self.client.connect.return_value = SdkResponse("session", "succeeded", "identity", "", {
            "hostId": connection.FOA_HOST_ID, "hostVersion": "0.1.0"})
        self.values = {"planId": "a" * 32, "fingerprint": "b" * 64, "templates": "wyrdspirit",
                       "placement": "fixture|12,0,0", "expiresInSeconds": "30"}
        self.respond()

    def respond(self, status="succeeded", code="encounter_plan", values=None, session="session"):
        self.client.invoke.return_value = SdkResponse(session, status, code, "private response",
                                                     self.values if values is None else values)

    def check(self, session="session", **kwargs):
        return connection.check_encounter_preview(12345, "0.1.0", session, str(self.path),
            client_factory=lambda *a, **k: self.client, **kwargs)

    def test_preview_only_and_exact_file_digest(self):
        before = self.path.read_bytes()
        result = self.check()["encounter"]
        self.assertEqual(result["compositionSha256"], hashlib.sha256(before).hexdigest())
        self.assertEqual(result["name"], self.composition["name"])
        self.assertEqual(result["templates"], ["wyrdspirit"])
        self.client.create_request.assert_called_once_with("tge.foa.encounters", "0.1", "preview", {"templates": "wyrdspirit"})
        self.assertEqual(self.client.invoke.call_count, 1)
        self.assertEqual(self.path.read_bytes(), before)
        self.assertNotIn("ab" * 32, json.dumps(result))

    def test_invalid_inputs_never_connect_and_report_no_private_content(self):
        for content in ('{"name":1,"name":2}', " " * 65537, '"private contents"', '{}'):
            self.path.write_text(content)
            result = self.check()
            self.assertEqual(set(result), {"schema", "previewInputError"})
            self.assertNotIn("private", json.dumps(result))
        self.path.unlink()
        self.assertIn("previewInputError", self.check())
        self.client.connect.assert_not_called()

    def test_invalid_templates_counts_and_source_never_connect(self):
        for change in ({"entries": [{"template": "unknown", "count": 1}]},
                       {"entries": [{"template": "wyrdspirit", "count": 9}]}, {"source": {}},
                       {"entries": [{"template": "wyrdspirit", "count": True}]}):
            self.path.write_text(json.dumps(self.composition | change))
            self.assertIn("previewInputError", self.check())
        self.client.connect.assert_not_called()

    def test_file_edits_are_read_fresh(self):
        first = self.check()["encounter"]
        self.composition["name"] = "Changed patrol"
        self.path.write_text(json.dumps(self.composition))
        second = self.check()["encounter"]
        self.assertNotEqual(first["compositionSha256"], second["compositionSha256"])
        self.assertEqual(second["name"], "Changed patrol")

    def test_no_plan_for_missing_or_changed_session(self):
        for session in ("", "old"):
            with self.assertRaises(connection.ConnectionCheckError):
                self.check(session)
        self.client.invoke.assert_not_called()
        self.respond(session="new")
        with self.assertRaisesRegex(connection.ConnectionCheckError, "restarted"):
            self.check()

    def test_rejection_and_malformed_plans_fail_without_leaking(self):
        for change in ({"planId": "invalid"}, {"fingerprint": "invalid"}, {"templates": "outlaw-1h"},
                       {"expiresInSeconds": "31"}, {"placement": ""}, {"extra": "private"},
                       {"placement": "x" * 4097}):
            self.respond(values=self.values | change)
            with self.assertRaises(connection.ConnectionCheckError):
                self.check()
        self.respond("rejected", "private-code", {})
        with self.assertRaises(connection.ConnectionCheckError) as error:
            self.check()
        self.assertNotIn("private", str(error.exception))

    def test_deadline_before_and_after_plan(self):
        for times in ((0, 21), (0, 1, 21)):
            ticks = iter(times)
            with self.assertRaisesRegex(connection.ConnectionCheckError, "timed out"):
                self.check(clock=lambda: next(ticks))

    def test_cli_routes_composition_and_hides_transport_details(self):
        output = io.StringIO()
        with patch.object(connection, "check_encounter_preview", side_effect=TransportError("private")) as check, contextlib.redirect_stdout(output):
            result = connection.main(["--port", "12345", "--session", "session", "--operation", "encounter-preview", "--composition", str(self.path)])
        check.assert_called_once_with(12345, "0.1.0", "session", str(self.path))
        self.assertEqual(result, 1)
        self.assertNotIn("private", output.getvalue())


if __name__ == "__main__":
    unittest.main()
