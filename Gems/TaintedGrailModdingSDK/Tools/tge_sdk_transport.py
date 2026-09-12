#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Authenticated local connection to the adapter hosted by Tainted Grail Extender.

The key is supplied through TGE_SDK_KEY; it is never a command-line argument.
No automatic retry, deployment, process launch or session replacement is performed.
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import math
import os
import re
import secrets
import socket
import struct
import sys
import time
from typing import Mapping

from tge_sdk_client import MAX_FRAME_BYTES, ProtocolError, SdkRequest, SdkResponse, decode_response, encode_request

HELLO = b"TGESDK1\0"
REQUEST_DOMAIN = b"TGE-SDK-REQUEST/1\0"
RESPONSE_DOMAIN = b"TGE-SDK-RESPONSE/1\0"
FOA_HOST_ID = "kane.tgfoa.tainted-grail-extender"


class TransportError(ConnectionError):
    """Connection failed; if a request was sent its execution outcome is unknown."""


def key_from_environment() -> bytes:
    value = os.environ.get("TGE_SDK_KEY", "")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", value):
        raise ProtocolError("TGE_SDK_KEY must contain 64 hexadecimal characters")
    return bytes.fromhex(value)


def _remaining(deadline: float) -> float:
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise TransportError("SDK connection deadline expired; execution outcome may be unknown")
    return remaining


def _read(stream: socket.socket, count: int, deadline: float) -> bytes:
    result = bytearray()
    while len(result) < count:
        stream.settimeout(_remaining(deadline))
        part = stream.recv(count - len(result))
        if not part:
            raise TransportError("SDK connection closed before a complete response; execution outcome may be unknown")
        result.extend(part)
    return bytes(result)


class TgeSdkClient:
    """Explicit host/session binding with a fresh authenticated connection per call.

    Share a cryptographically random 32-byte key with the selected host process.
    Traffic stays on IPv4 loopback and is authenticated, not encrypted.
    """

    def __init__(self, port: int, key: bytes, *, expected_host_id: str,
                 expected_host_version: str, timeout: float = 5.0):
        if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
            raise ProtocolError("SDK port must be an integer between 1 and 65535")
        if not isinstance(key, bytes) or len(key) != 32:
            raise ProtocolError("A 32-byte SDK key is required")
        if not isinstance(expected_host_id, str) or not expected_host_id.strip():
            raise ProtocolError("An exact expected host identity is required")
        if not isinstance(expected_host_version, str) or not expected_host_version.strip():
            raise ProtocolError("An exact expected host version is required")
        if isinstance(timeout, bool) or not isinstance(timeout, (float, int)) or not math.isfinite(timeout) or not 0 < timeout <= 30:
            raise ProtocolError("SDK timeout must be finite and between zero and 30 seconds")
        self._port = port
        self._key = key
        self._host_id = expected_host_id
        self._host_version = expected_host_version
        self._timeout = timeout
        self._session = ""

    @property
    def session_id(self) -> str:
        return self._session

    def connect(self) -> SdkResponse:
        """Explicitly verify this host and establish its current session."""
        self._session = ""
        identity = self._exchange(SdkRequest.describe())
        if not identity.succeeded or identity.values.get("hostId") != self._host_id or identity.values.get("hostVersion") != self._host_version:
            raise ProtocolError("Extender identity does not match the selected host and version")
        self._session = identity.session_id
        return identity

    def create_request(self, service_id: str, service_version: str, operation: str,
                       arguments: Mapping[str, str] | None = None) -> SdkRequest:
        if not self._session:
            raise ProtocolError("Connect to the selected extender before creating a service request")
        return SdkRequest(service_id, service_version, operation, self._session, arguments or {})

    def invoke(self, request: SdkRequest) -> SdkResponse:
        """Send once. Retain this exact request if its outcome needs reconciliation."""
        if not self._session or request.session_id != self._session:
            raise ProtocolError("Request does not belong to the connected extender session")
        return self._exchange(request)

    def _exchange(self, request: SdkRequest) -> SdkResponse:
        body = encode_request(request).encode("utf-8")
        deadline = time.monotonic() + self._timeout
        try:
            with socket.create_connection(("127.0.0.1", self._port), timeout=_remaining(deadline)) as stream:
                stream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                hello = _read(stream, len(HELLO) + 32, deadline)
                if hello[:len(HELLO)] != HELLO:
                    raise ProtocolError("Unsupported extender transport protocol")
                server_nonce = hello[len(HELLO):]
                client_nonce = secrets.token_bytes(32)
                prefix = client_nonce + struct.pack("!I", len(body)) + body
                tag = hmac.digest(self._key, REQUEST_DOMAIN + server_nonce + prefix, hashlib.sha256)
                stream.settimeout(_remaining(deadline))
                stream.sendall(prefix + tag)
                size = _read(stream, 4, deadline)
                length = struct.unpack("!I", size)[0]
                if not 1 <= length <= MAX_FRAME_BYTES:
                    raise ProtocolError("Invalid extender transport response length")
                response = _read(stream, length, deadline)
                tag = _read(stream, 32, deadline)
                expected = hmac.digest(self._key, RESPONSE_DOMAIN + server_nonce + client_nonce + size + response, hashlib.sha256)
                if not hmac.compare_digest(tag, expected):
                    raise ProtocolError("Extender response authentication failed")
                return decode_response(response.decode("utf-8"), request)
        except UnicodeError as exc:
            raise ProtocolError("Invalid UTF-8 extender response") from exc
        except OSError as exc:
            raise TransportError("SDK connection failed; execution outcome may be unknown. No retry was attempted.") from exc


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, type=int)
    parser.add_argument("--expected-host-id", default=FOA_HOST_ID)
    parser.add_argument("--expected-host-version", default="0.1.0")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--service-id", default="tge.core.identity")
    parser.add_argument("--service-version", default="0.1")
    parser.add_argument("--operation", default="services")
    parser.add_argument("--argument", action="append", default=[], metavar="KEY=VALUE")
    args = parser.parse_args()
    try:
        values = {}
        for item in args.argument:
            key, separator, value = item.partition("=")
            if not separator or key in values:
                raise ProtocolError("Arguments require unique KEY=VALUE pairs")
            values[key] = value
        client = TgeSdkClient(args.port, key_from_environment(), expected_host_id=args.expected_host_id,
                              expected_host_version=args.expected_host_version, timeout=args.timeout)
        client.connect()
        request = client.create_request(args.service_id, args.service_version, args.operation, values)
        print(json.dumps({"requestId": request.request_id, "correlationId": request.correlation_id,
                          "sessionId": request.session_id}), file=sys.stderr)
        result = client.invoke(request)
        print(json.dumps({"status": result.status, "code": result.code, "message": result.message,
                          "sessionId": result.session_id, "values": dict(result.values)}, ensure_ascii=False))
        return 0 if result.succeeded else 1
    except (ProtocolError, TransportError) as exc:
        parser.exit(2, str(exc) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
