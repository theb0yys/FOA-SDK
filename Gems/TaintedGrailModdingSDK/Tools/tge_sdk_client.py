#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""FOA-SDK producer/consumer for the extender-owned SDK adapter protocol.

This module encodes requests and verifies responses. The selected transport owns
delivery, authentication and host-thread marshalling; this client does not launch
processes, install packages or treat legacy preview work orders as commands.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from types import MappingProxyType
from typing import Mapping
from uuid import uuid4

REQUEST_CONTRACT = "foa-sdk-tge-request/1"
RESPONSE_CONTRACT = "foa-sdk-tge-response/1"
MAX_FRAME_BYTES = 65_536
REQUEST_FIELDS = {
    "contract", "requestId", "correlationId", "sessionId", "serviceId",
    "serviceVersion", "operation", "arguments",
}
RESPONSE_FIELDS = {
    "contract", "requestId", "correlationId", "sessionId", "serviceId",
    "serviceVersion", "status", "code", "message", "values",
}


class ProtocolError(ValueError):
    """Malformed, unsupported or uncorrelated adapter data."""


def _text(value: str, name: str, maximum: int = 4096, *, empty: bool = False) -> str:
    if not isinstance(value, str) or (not empty and not value.strip()):
        raise ProtocolError(f"Invalid {name}")
    try:
        if len(value.encode("utf-16-le")) // 2 > maximum:
            raise ProtocolError(f"Oversized {name}")
    except UnicodeError as exc:
        raise ProtocolError(f"Invalid Unicode in {name}") from exc
    return value


def _version(value: str) -> str:
    _text(value, "serviceVersion", 256)
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", value):
        raise ProtocolError("Service version must be canonical major.minor")
    if any(int(part) > 2_147_483_647 for part in value.split(".")):
        raise ProtocolError("Service version exceeds the extender integer range")
    return value


def _map(value: Mapping[str, str]) -> Mapping[str, str]:
    if not isinstance(value, Mapping) or len(value) > 64:
        raise ProtocolError("Invalid or oversized value map")
    return MappingProxyType({
        _text(key, "map key", 128): _text(item, "map value", empty=True)
        for key, item in value.items()
    })


def _frame(value: str) -> None:
    try:
        if not isinstance(value, str) or len(value) > MAX_FRAME_BYTES or len(value.encode("utf-8")) > MAX_FRAME_BYTES:
            raise ProtocolError("Invalid or oversized SDK frame")
    except UnicodeError as exc:
        raise ProtocolError("Invalid Unicode in SDK frame") from exc


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProtocolError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _load(frame: str) -> dict:
    _frame(frame)
    try:
        result = json.loads(frame, object_pairs_hook=_pairs)
    except (ValueError, RecursionError) as exc:
        raise ProtocolError("Malformed SDK JSON") from exc
    if not isinstance(result, dict):
        raise ProtocolError("SDK frame must be an object")
    return result


@dataclass(frozen=True)
class SdkRequest:
    service_id: str
    service_version: str
    operation: str
    session_id: str = ""
    arguments: Mapping[str, str] = field(default_factory=dict)
    request_id: str = field(default_factory=lambda: uuid4().hex)
    correlation_id: str = field(default_factory=lambda: uuid4().hex)

    def __post_init__(self):
        for name in ("service_id", "operation", "request_id", "correlation_id"):
            _text(getattr(self, name), name, 256)
        _text(self.session_id, "session_id", 256, empty=True)
        _version(self.service_version)
        object.__setattr__(self, "arguments", _map(self.arguments))

    @classmethod
    def describe(cls) -> SdkRequest:
        """Ask the extender for its current session before service invocation."""
        return cls("tge.core.identity", "0.1", "describe")


@dataclass(frozen=True)
class SdkResponse:
    session_id: str
    status: str
    code: str
    message: str
    values: Mapping[str, str]

    @property
    def succeeded(self) -> bool:
        return self.status == "succeeded"


def encode_request(request: SdkRequest) -> str:
    # UTF-16 ordinal order agrees with the managed adapter's canonical map writer.
    arguments = dict(sorted(request.arguments.items(), key=lambda pair: pair[0].encode("utf-16-be")))
    result = json.dumps({
        "contract": REQUEST_CONTRACT,
        "requestId": request.request_id,
        "correlationId": request.correlation_id,
        "sessionId": request.session_id,
        "serviceId": request.service_id,
        "serviceVersion": request.service_version,
        "operation": request.operation,
        "arguments": arguments,
    }, ensure_ascii=False, separators=(",", ":"))
    _frame(result)
    return result


def decode_request(frame: str) -> SdkRequest:
    value = _load(frame)
    if set(value) != REQUEST_FIELDS or value["contract"] != REQUEST_CONTRACT:
        raise ProtocolError("Unsupported SDK request fields or contract")
    return SdkRequest(
        value["serviceId"], value["serviceVersion"], value["operation"],
        value["sessionId"], value["arguments"], value["requestId"], value["correlationId"],
    )


def decode_response(frame: str, request: SdkRequest) -> SdkResponse:
    value = _load(frame)
    if set(value) != RESPONSE_FIELDS or value["contract"] != RESPONSE_CONTRACT:
        raise ProtocolError("Unsupported SDK response fields or contract")
    for name, expected in {
        "requestId": request.request_id, "correlationId": request.correlation_id,
        "serviceId": request.service_id, "serviceVersion": request.service_version,
    }.items():
        if value[name] != expected:
            raise ProtocolError(f"SDK response does not match request {name}")
    session = _text(value["sessionId"], "sessionId", 256, empty=True)
    status = value["status"]
    if status not in ("succeeded", "rejected", "failed"):
        raise ProtocolError("Unknown SDK response status")
    # A stale-session rejection may report the replacement session, but cannot
    # silently switch an SDK caller to it or present an old invocation as success.
    if status == "succeeded" and (not session or (request.session_id and session != request.session_id)):
        raise ProtocolError("Successful response has a missing or different session")
    if status == "succeeded" and not request.session_id and (
        request.service_id, request.service_version, request.operation, dict(request.arguments)
    ) != ("tge.core.identity", "0.1", "describe", {}):
        raise ProtocolError("Only the identity handshake may establish a session")
    return SdkResponse(session, status, _text(value["code"], "code", 256),
                       _text(value["message"], "message", empty=True), _map(value["values"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--service-id", default="tge.core.identity")
    parser.add_argument("--service-version", default="0.1")
    parser.add_argument("--operation", default="describe")
    parser.add_argument("--session-id", default="")
    parser.add_argument("--argument", action="append", default=[], metavar="KEY=VALUE")
    args = parser.parse_args()
    try:
        values = {}
        for item in args.argument:
            key, separator, value = item.partition("=")
            if not separator or key in values:
                raise ProtocolError("Arguments require unique KEY=VALUE pairs")
            values[key] = value
        print(encode_request(SdkRequest(args.service_id, args.service_version, args.operation, args.session_id, values)))
    except ProtocolError as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
