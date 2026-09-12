#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Client validation plus opt-in interoperability with the real managed adapter."""

import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tge_sdk_client import (
    MAX_FRAME_BYTES, ProtocolError, RESPONSE_CONTRACT, SdkRequest,
    decode_request, decode_response, encode_request,
)


def response(request, **changes):
    value = {
        "contract": RESPONSE_CONTRACT, "requestId": request.request_id,
        "correlationId": request.correlation_id, "sessionId": request.session_id or "new-session",
        "serviceId": request.service_id, "serviceVersion": request.service_version,
        "status": "succeeded", "code": "identity", "message": "", "values": {},
    }
    value.update(changes)
    return json.dumps(value)


class ClientTests(unittest.TestCase):
    def test_request_roundtrip_and_immutable_arguments(self):
        arguments = {"\u00e9": "\U0001f5e1\n\t\\\"", "a": "value"}
        request = SdkRequest("example.service", "1.0", "read", "session", arguments)
        arguments["a"] = "changed"
        decoded = decode_request(encode_request(request))
        self.assertEqual(dict(decoded.arguments), {"\u00e9": "\U0001f5e1\n\t\\\"", "a": "value"})
        self.assertEqual(decoded, request)
        with self.assertRaises(TypeError):
            request.arguments["a"] = "cannot change"

    def test_version_and_identity_validation(self):
        for version in ("01.0", "1", "1.0.0", "+1.0", "1.-1", "1.\u0660", "2147483648.0", " 1.0", 1):
            with self.subTest(version=version), self.assertRaises(ProtocolError):
                SdkRequest("service", version, "read")
        for identity in ("", " ", "x" * 257, "\ud800", None):
            with self.subTest(identity=identity), self.assertRaises(ProtocolError):
                SdkRequest(identity, "1.0", "read")

    def test_map_and_unicode_bounds(self):
        for values in ({str(i): "x" for i in range(65)}, {"x" * 129: "x"},
                       {"x": 1}, {"x": {"nested": "value"}}, {"x": "\U0001f5e1" * 2049}):
            with self.subTest(values=str(values)[:80]), self.assertRaises(ProtocolError):
                SdkRequest("service", "1.0", "read", arguments=values)
        with self.assertRaises(ProtocolError):
            encode_request(SdkRequest("service", "1.0", "read", arguments={str(i): "x" * 4096 for i in range(20)}))

    def test_unknown_and_duplicate_request_fields(self):
        frame = encode_request(SdkRequest.describe())
        for bad in (frame.replace('"arguments":{}', '"arguments":{},"extra":"x"'),
                    frame.replace('"arguments":{}', '"arguments":{},"operation":"describe"'),
                    frame.replace('"arguments":{}', '"arguments":{"a":"1","a":"2"}'),
                    frame.replace('"arguments":{}', '"arguments":{"a":null}'),
                    frame.replace('"arguments":{}', '"arguments":[]'),
                    frame.replace('request/1', 'request/2'), "[]", "null", "\ufeff" + frame):
            with self.subTest(bad=bad[:90]), self.assertRaises(ProtocolError):
                decode_request(bad)

    def test_response_is_bound_to_exact_request(self):
        request = SdkRequest.describe()
        for field in ("requestId", "correlationId", "serviceId", "serviceVersion", "contract"):
            with self.subTest(field=field), self.assertRaises(ProtocolError):
                decode_response(response(request, **{field: "different"}), request)

    def test_success_requires_current_session(self):
        request = SdkRequest("service", "1.0", "read", "session")
        for session in ("", "different", None):
            with self.subTest(session=session), self.assertRaises(ProtocolError):
                decode_response(response(request, sessionId=session), request)
        rejected = decode_response(response(request, sessionId="replacement", status="rejected", code="session_mismatch"), request)
        self.assertFalse(rejected.succeeded)
        self.assertEqual(request.session_id, "session")

    def test_only_handshake_can_establish_session(self):
        request = SdkRequest("service", "1.0", "read")
        with self.assertRaises(ProtocolError):
            decode_response(response(request), request)
        handshake = SdkRequest.describe()
        self.assertTrue(decode_response(response(handshake), handshake).succeeded)

    def test_malformed_response_rejection(self):
        request = SdkRequest.describe()
        for changes in ({"values": []}, {"values": {"x": None}}, {"status": "pending"},
                        {"status": []}, {"message": "x" * 4097}, {"code": ""}, {"extra": "field"}):
            with self.subTest(changes=str(changes)[:80]), self.assertRaises(ProtocolError):
                decode_response(response(request, **changes), request)
        with self.assertRaises(ProtocolError):
            decode_response(" " * (MAX_FRAME_BYTES + 1), request)
        with self.assertRaises(ProtocolError):
            decode_response(response(request).replace('"values": {}', '"values": {"x":"1","x":"2"}'), request)


@unittest.skipUnless(os.environ.get("TGE_SDK_FIXTURE_DLL"), "Set TGE_SDK_FIXTURE_DLL for compiled extender interoperability")
class ManagedAdapterInteropTests(unittest.TestCase):
    def setUp(self):
        dll = Path(os.environ["TGE_SDK_FIXTURE_DLL"]).resolve(strict=True)
        self.process = subprocess.Popen(
            ["dotnet", str(dll), "--stdio"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1,
        )
        self.lines = queue.Queue(maxsize=4)

        def read():
            while line := self.process.stdout.readline(MAX_FRAME_BYTES + 2):
                self.lines.put(line, timeout=5)

        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()

    def tearDown(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
            self.fail("Managed adapter fixture did not stop")
        finally:
            self.reader.join(timeout=5)
            self.process.stdout.close()
        errors = self.process.stderr.read()
        self.process.stderr.close()
        self.assertEqual(self.process.returncode, 0, errors)

    def send(self, request):
        self.process.stdin.write(encode_request(request) + "\n")
        self.process.stdin.flush()
        try:
            frame = self.lines.get(timeout=10)
        except queue.Empty:
            self.fail("No managed adapter response within 10 seconds")
        return decode_response(frame, request)

    def test_sdk_to_extender_service_and_correlated_response(self):
        identity = self.send(SdkRequest.describe())
        self.assertTrue(identity.succeeded)
        self.assertEqual(identity.values["hostId"], "tge.fixture.host")
        request = SdkRequest("fixture.echo", "1.0", "echo", identity.session_id, {"text": "\U0001f5e1 quoted \"value\"\n"})
        result = self.send(request)
        self.assertTrue(result.succeeded)
        self.assertEqual(result.values["text"], request.arguments["text"])
        self.assertEqual(result.values["count"], "1")
        self.assertEqual(self.send(request), result)
        conflicting = SdkRequest(request.service_id, request.service_version, request.operation,
                                 request.session_id, {"text": "changed"}, request.request_id, request.correlation_id)
        self.assertEqual(self.send(conflicting).code, "invocation_conflict")

    def test_service_discovery_and_exact_version_refusal(self):
        identity = self.send(SdkRequest.describe())
        listed = self.send(SdkRequest("tge.core.identity", "0.1", "services", identity.session_id))
        self.assertEqual(listed.values["total"], "2")
        self.assertIn("fixture.echo", listed.values.values())
        self.assertEqual(self.send(SdkRequest("fixture.echo", "1.1", "echo", identity.session_id)).code, "service_not_found")
        self.assertEqual(self.send(SdkRequest("fixture.echo", "1.0", "echo", "stale-session")).code, "session_mismatch")

    def test_control_character_frame_near_byte_limit(self):
        identity = self.send(SdkRequest.describe())
        request = SdkRequest("fixture.echo", "1.0", "echo", identity.session_id,
                             {str(i): "\n" * 4096 for i in range(7)})
        self.assertLess(len(encode_request(request).encode("utf-8")), MAX_FRAME_BYTES)
        result = self.send(request)
        self.assertTrue(result.succeeded)
        self.assertEqual(result.values["0"], "\n" * 4096)


if __name__ == "__main__":
    unittest.main()
