#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Real socket interoperability with the production extender listener and Pump."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import secrets
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tge_sdk_client import MAX_FRAME_BYTES, ProtocolError, SdkRequest, encode_request
from tge_sdk_transport import (
    HELLO, REQUEST_DOMAIN, RESPONSE_DOMAIN, TgeSdkClient, TransportError, key_from_environment,
)


def read_exact(stream, length):
    result = bytearray()
    while len(result) < length:
        chunk = stream.recv(length - len(result))
        if not chunk:
            raise ConnectionError("Incomplete fixture frame")
        result.extend(chunk)
    return bytes(result)


class TransportClientTests(unittest.TestCase):
    def test_configuration_and_unconnected_calls_fail_closed(self):
        valid = dict(port=12345, key=b"k" * 32, expected_host_id="fixture", expected_host_version="1")
        for changes in ({"port": 0}, {"port": True}, {"port": 65536}, {"key": b"k"},
                        {"timeout": 0}, {"timeout": float("nan")}, {"timeout": float("inf")},
                        {"timeout": True}, {"expected_host_id": ""}, {"expected_host_version": ""}):
            with self.subTest(changes=changes), self.assertRaises(ProtocolError):
                TgeSdkClient(**(valid | changes))
        client = TgeSdkClient(**valid)
        with self.assertRaises(ProtocolError):
            client.create_request("test", "1.0", "read")
        with self.assertRaises(ProtocolError):
            client.invoke(SdkRequest.describe())
        for key in ("", "g" * 64, "a" * 63, " " + "a" * 63):
            with patch.dict(os.environ, {"TGE_SDK_KEY": key}), self.assertRaises(ProtocolError):
                key_from_environment()
        with patch.dict(os.environ, {"TGE_SDK_KEY": "AB" * 32}):
            self.assertEqual(key_from_environment(), b"\xab" * 32)

    def test_client_rejects_bad_hello_response_length_and_authentication(self):
        for mode in ("hello", "length", "mac", "nonce"):
            with self.subTest(mode=mode), socket.socket() as listener:
                listener.bind(("127.0.0.1", 0))
                listener.listen(1)
                listener.settimeout(3)
                key = secrets.token_bytes(32)

                def forged_server():
                    with listener.accept()[0] as stream:
                        stream.settimeout(3)
                        server_nonce = secrets.token_bytes(32)
                        stream.sendall((b"UNKNOWN!" if mode == "hello" else HELLO) + server_nonce)
                        if mode == "hello":
                            return
                        prefix = read_exact(stream, 36)
                        read_exact(stream, struct.unpack("!I", prefix[32:])[0] + 32)
                        if mode == "length":
                            stream.sendall(struct.pack("!I", MAX_FRAME_BYTES + 1))
                            return
                        body = b"{}"
                        size = struct.pack("!I", len(body))
                        nonce = b"x" * 32 if mode == "nonce" else prefix[:32]
                        tag = hmac.digest(key, RESPONSE_DOMAIN + server_nonce + nonce + size + body, hashlib.sha256)
                        if mode == "mac":
                            tag = bytes([tag[0] ^ 1]) + tag[1:]
                        stream.sendall(size + body + tag)

                with ThreadPoolExecutor(max_workers=1) as pool:
                    worker = pool.submit(forged_server)
                    client = TgeSdkClient(listener.getsockname()[1], key, expected_host_id="fixture", expected_host_version="1")
                    with self.assertRaises(ProtocolError) as error:
                        client.connect()
                    if mode in ("mac", "nonce"):
                        self.assertIn("authentication", str(error.exception))
                    self.assertEqual(client.session_id, "")
                    worker.result(timeout=4)


@unittest.skipUnless(os.environ.get("TGE_SDK_FIXTURE_DLL"), "Set TGE_SDK_FIXTURE_DLL for production listener interoperability")
class ManagedTransportInteropTests(unittest.TestCase):
    def setUp(self):
        self.key = secrets.token_bytes(32)
        self.env = os.environ | {"TGE_SDK_KEY": self.key.hex()}
        dll = Path(os.environ["TGE_SDK_FIXTURE_DLL"]).resolve(strict=True)
        self.process = subprocess.Popen(["dotnet", str(dll), "--tcp"], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                        encoding="utf-8", bufsize=1, env=self.env)
        self.lines = queue.Queue(maxsize=8)

        def read():
            while line := self.process.stdout.readline(4096):
                self.lines.put(line, timeout=5)

        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.port = self.next_line()["port"]
        self.client = self.new_client()

    def tearDown(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
            self.fail("Production transport fixture failed to shut down")
        finally:
            self.reader.join(timeout=3)
            self.process.stdout.close()
        errors = self.process.stderr.read()
        self.process.stderr.close()
        self.assertEqual(self.process.returncode, 0, errors)

    def next_line(self):
        try:
            return json.loads(self.lines.get(timeout=5))
        except queue.Empty:
            self.fail("No fixture control response within five seconds")

    def command(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        return self.next_line()

    def wait_stats(self, predicate):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            result = self.command("stats")
            if predicate(result):
                return result
            time.sleep(0.01)
        self.fail(f"Fixture did not reach expected state: {result}")

    def new_client(self, **changes):
        options = dict(port=self.port, key=self.key, expected_host_id="tge.fixture.host",
                       expected_host_version="1.0.0", timeout=3)
        return TgeSdkClient(**(options | changes))

    def open_raw(self):
        stream = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        self.addCleanup(stream.close)
        hello = read_exact(stream, 40)
        self.assertEqual(hello[:8], HELLO)
        return stream, hello[8:]

    def signed_request(self, nonce, body):
        prefix = secrets.token_bytes(32) + struct.pack("!I", len(body)) + body
        return prefix + hmac.digest(self.key, REQUEST_DOMAIN + nonce + prefix, hashlib.sha256)

    def expect_closed(self, stream):
        try:
            self.assertEqual(stream.recv(1), b"")
        except (ConnectionResetError, ConnectionAbortedError):
            pass

    def echo(self, text="value"):
        return self.client.create_request("fixture.echo", "1.0", "echo", {"text": text})

    def test_identity_discovery_unicode_replay_and_collision(self):
        identity = self.client.connect()
        self.assertEqual(identity.values["hostId"], "tge.fixture.host")
        result = self.client.invoke(self.client.create_request("tge.core.identity", "0.1", "services"))
        self.assertIn("fixture.echo", result.values.values())
        request = self.echo("\U0001f5e1 quoted \"value\"\n")
        start = time.monotonic()
        result = self.client.invoke(request)
        print(f"MEASURED: Python-to-managed loopback echo {(time.monotonic() - start) * 1000:.3f} ms")
        self.assertTrue(result.succeeded)
        self.assertEqual(result.values["text"], request.arguments["text"])
        self.assertEqual(result.values["count"], "1")
        self.assertEqual(self.client.invoke(request), result)
        changed = SdkRequest(request.service_id, request.service_version, request.operation, request.session_id,
                             {"text": "changed"}, request.request_id, request.correlation_id)
        self.assertEqual(self.client.invoke(changed).code, "invocation_conflict")

    def test_wrong_key_and_wrong_host_never_bind(self):
        with self.assertRaises(TransportError):
            self.new_client(key=secrets.token_bytes(32)).connect()
        for changes in ({"expected_host_id": "different"}, {"expected_host_version": "different"}):
            client = self.new_client(**changes)
            with self.assertRaises(ProtocolError):
                client.connect()
            self.assertEqual(client.session_id, "")
        self.client.connect()
        self.assertEqual(self.command("stats")["count"], 0)
        self.assertEqual(self.client.invoke(self.echo()).values["count"], "1")

    def test_restart_requires_explicit_reconnection(self):
        self.client.connect()
        request = self.echo()
        old_session = self.client.session_id
        self.assertTrue(self.client.invoke(request).succeeded)
        self.command("restart")
        self.assertEqual(self.client.invoke(request).code, "session_mismatch")
        self.assertEqual(self.client.session_id, old_session)
        self.assertEqual(self.command("stats")["count"], 0)
        self.client.connect()
        self.assertNotEqual(self.client.session_id, old_session)
        with self.assertRaises(ProtocolError):
            self.client.invoke(request)
        self.assertEqual(self.client.invoke(self.echo()).values["count"], "1")

    def test_tampered_body_and_replayed_transport_frame_are_rejected(self):
        self.client.connect()
        body = encode_request(self.echo()).encode()
        stream, nonce = self.open_raw()
        frame = self.signed_request(nonce, body)
        tampered = frame[:36] + bytes([frame[36] ^ 1]) + frame[37:]
        stream.sendall(tampered)
        self.expect_closed(stream)
        stream, new_nonce = self.open_raw()
        self.assertNotEqual(nonce, new_nonce)
        stream.sendall(frame)
        self.expect_closed(stream)
        self.assertEqual(self.command("stats")["count"], 0)

    def test_invalid_lengths_and_authenticated_invalid_utf8(self):
        for length in (0, MAX_FRAME_BYTES + 1, 0xFFFFFFFF):
            stream, _ = self.open_raw()
            stream.sendall(secrets.token_bytes(32) + struct.pack("!I", length))
            self.expect_closed(stream)
        stream, nonce = self.open_raw()
        stream.sendall(self.signed_request(nonce, b"\xff"))
        self.expect_closed(stream)
        self.assertEqual(self.command("stats")["count"], 0)

    def test_fragmented_valid_frame_and_authenticated_json_rejection(self):
        stream, nonce = self.open_raw()
        frame = self.signed_request(nonce, b"{}")
        for offset in range(0, len(frame), 7):
            stream.sendall(frame[offset:offset + 7])
        size = read_exact(stream, 4)
        body = read_exact(stream, struct.unpack("!I", size)[0])
        tag = read_exact(stream, 32)
        expected = hmac.digest(self.key, RESPONSE_DOMAIN + nonce + frame[:32] + size + body, hashlib.sha256)
        self.assertTrue(hmac.compare_digest(tag, expected))
        self.assertEqual(json.loads(body)["code"], "invalid_request")
        self.client.connect()
        request = self.client.create_request("fixture.echo", "1.0", "echo", {str(i): "\n" * 4096 for i in range(7)})
        self.assertTrue(self.client.invoke(request).succeeded)

    def test_expired_queue_is_removed_without_late_execution(self):
        self.client.connect()
        self.command("pause")
        request = self.echo()
        with ThreadPoolExecutor(max_workers=1) as pool:
            pending = pool.submit(self.client.invoke, request)
            self.wait_stats(lambda state: state["pending"] == 1)
            with self.assertRaises(TransportError):
                pending.result(timeout=4)
        state = self.wait_stats(lambda state: state["pending"] == 0 and state["active"] == 0)
        self.assertEqual(state["count"], 0)
        self.command("resume")
        self.assertEqual(self.client.invoke(request).values["count"], "1")

    def test_admission_limit_releases_disconnected_clients(self):
        streams = [self.open_raw()[0] for _ in range(4)]
        self.assertEqual(self.command("stats")["active"], 4)
        with socket.create_connection(("127.0.0.1", self.port), timeout=3) as rejected:
            self.expect_closed(rejected)
        for stream in streams:
            stream.close()
        self.wait_stats(lambda state: state["active"] == 0)
        self.assertTrue(self.client.connect().succeeded)

    def test_four_authenticated_requests_queue_and_dispatch_on_host(self):
        self.client.connect()
        self.command("pause")
        requests = [self.echo(str(i)) for i in range(4)]
        with ThreadPoolExecutor(max_workers=4) as pool:
            pending = [pool.submit(self.client.invoke, request) for request in requests]
            state = self.wait_stats(lambda state: state["pending"] == 4)
            self.assertEqual(state["count"], 0)
            self.command("resume")
            results = [item.result(timeout=3) for item in pending]
        self.assertEqual({result.values["count"] for result in results}, {"1", "2", "3", "4"})
        self.assertTrue(all(result.succeeded for result in results))

    def test_timeout_during_handler_does_not_reexecute_on_retry(self):
        self.client.connect()
        request = self.client.create_request("fixture.echo", "1.0", "slow")
        with self.assertRaises(TransportError):
            self.client.invoke(request)
        self.wait_stats(lambda state: state["count"] == 1)
        self.assertEqual(self.client.invoke(request).values["count"], "1")
        self.assertEqual(self.command("stats")["count"], 1)

    def test_restart_cancels_queue_before_binding_new_session(self):
        self.client.connect()
        self.command("pause")
        request = self.echo()
        with ThreadPoolExecutor(max_workers=1) as pool:
            pending = pool.submit(self.client.invoke, request)
            self.wait_stats(lambda state: state["pending"] == 1)
            self.command("restart")
            with self.assertRaises(TransportError):
                pending.result(timeout=3)
        self.assertEqual(self.command("stats")["count"], 0)
        self.command("resume")
        self.client.connect()
        self.assertEqual(self.client.invoke(self.echo()).values["count"], "1")

    def test_partial_frame_has_total_deadline(self):
        stream, _ = self.open_raw()
        start = time.monotonic()
        for _ in range(3):
            stream.sendall(b"x")
            time.sleep(0.2)
        self.expect_closed(stream)
        self.assertLess(time.monotonic() - start, 1.7)
        self.wait_stats(lambda state: state["active"] == 0)
        self.assertTrue(self.client.connect().succeeded)

    def test_shutdown_cancels_queued_request_and_closes_listener(self):
        self.client.connect()
        self.command("pause")
        request = self.echo()
        with ThreadPoolExecutor(max_workers=1) as pool:
            pending = pool.submit(self.client.invoke, request)
            self.wait_stats(lambda state: state["pending"] == 1)
            self.process.stdin.write("stop\n")
            self.process.stdin.flush()
            self.process.wait(timeout=3)
            with self.assertRaises(TransportError):
                pending.result(timeout=3)
        with self.assertRaises(TransportError):
            self.client.invoke(request)

    def test_lost_response_retry_preserves_invocation_identity(self):
        self.client.connect()
        request = self.echo()
        stream, nonce = self.open_raw()
        stream.sendall(self.signed_request(nonce, encode_request(request).encode()))
        self.wait_stats(lambda state: state["count"] == 1)
        stream.close()  # Deliberately discard the first completed result.
        self.assertEqual(self.client.invoke(request).values["count"], "1")
        self.assertEqual(self.command("stats")["count"], 1)

    def test_cli_discovers_registered_services(self):
        tool = Path(__file__).resolve().parents[1] / "tge_sdk_transport.py"
        result = subprocess.run([sys.executable, str(tool), "--port", str(self.port),
                                 "--expected-host-id", "tge.fixture.host", "--expected-host-version", "1.0.0"],
                                env=self.env, capture_output=True, text=True, encoding="utf-8", timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "succeeded")
        self.assertIn("fixture.echo", output["values"].values())
        self.assertNotIn(self.key.hex(), result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
