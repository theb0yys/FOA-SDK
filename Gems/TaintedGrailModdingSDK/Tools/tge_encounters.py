#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Explicit TGE encounter operations and data-only saved compositions.

No game install/launch, catalog write, native identity inference or automatic retry.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from dataclasses import dataclass

from tge_sdk_client import ProtocolError, decode_request, encode_request
from tge_sdk_transport import TgeSdkClient

SERVICE = "tge.foa.encounters"
TEMPLATES = frozenset({"wyrdspirit", "outlaw-1h"})
COMPOSITION = "foa-tge-encounter-composition/1"


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProtocolError("Duplicate JSON field")
        result[key] = value
    return result


def load_data(path: Path, maximum: int = 65536):
    with path.open("rb") as stream:
        raw = stream.read(maximum + 1)
    if len(raw) > maximum:
        raise ProtocolError("Input exceeds its size limit")
    try:
        value = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=_pairs)
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise ProtocolError("Invalid JSON input") from exc
    return value, raw


def _count(value, maximum=8):
    if type(value) is not int or not 1 <= value <= maximum:
        raise ProtocolError("Count must be an integer from 1 to " + str(maximum))
    return value


def composition_slots(value) -> tuple[str, ...]:
    if not isinstance(value, dict) or set(value) != {"contract", "name", "entries", "source"}:
        raise ProtocolError("Unexpected composition fields")
    if value["contract"] != COMPOSITION:
        raise ProtocolError("Unsupported composition contract")
    if not isinstance(value["name"], str) or not value["name"].strip() or len(value["name"]) > 128:
        raise ProtocolError("A bounded composition name is required")
    entries = value["entries"]
    if not isinstance(entries, list) or not 1 <= len(entries) <= 8:
        raise ProtocolError("Composition needs 1 to 8 entries")
    slots = []
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"template", "count"}:
            raise ProtocolError("Unexpected composition entry")
        if not isinstance(entry["template"], str) or entry["template"] not in TEMPLATES:
            raise ProtocolError("Template has no supported native spawn binding")
        slots.extend([entry["template"]] * _count(entry["count"]))
        if len(slots) > 8:
            raise ProtocolError("Composition exceeds eight actors")
    source = value["source"]
    if source is not None:
        if not isinstance(source, dict) or set(source) != {"catalogSha256", "encounterId", "actorBindings"}:
            raise ProtocolError("Unexpected composition source fields")
        if not isinstance(source["catalogSha256"], str) or not re.fullmatch("[0-9a-f]{64}", source["catalogSha256"]):
            raise ProtocolError("Invalid catalog digest")
        if not isinstance(source["encounterId"], str) or not 1 <= len(source["encounterId"]) <= 256:
            raise ProtocolError("Invalid source encounter ID")
        bindings = source["actorBindings"]
        if not isinstance(bindings, dict) or not 1 <= len(bindings) <= 8:
            raise ProtocolError("Invalid explicit actor bindings")
        for actor_id, template in bindings.items():
            if not isinstance(actor_id, str) or not 1 <= len(actor_id) <= 256 or not isinstance(template, str) or template not in TEMPLATES:
                raise ProtocolError("Invalid explicit actor binding")
    return tuple(slots)


def export_editor_composition(catalog_path: Path, encounter_id: str, bindings: dict) -> dict:
    """Read schema-3 fixed manual actor counts, with an explicit native binding per actor.

    This exports composition membership only. It does not implement authored stats,
    triggers, placement, quests, troop expansion or catalog runtime qualification.
    """
    catalog, raw = load_data(catalog_path, 32 * 1024 * 1024)
    if not isinstance(catalog, dict) or type(catalog.get("SchemaVersion")) is not int or catalog["SchemaVersion"] != 3:
        raise ProtocolError("Encounter export requires catalog schema 3")
    if not isinstance(bindings, dict) or not 1 <= len(bindings) <= 8:
        raise ProtocolError("Provide explicit actor-record to runtime-template bindings")

    def records(key):
        rows = catalog.get(key, [])
        if not isinstance(rows, list) or len(rows) > 20000:
            raise ProtocolError("Invalid catalog collection: " + key)
        result = {}
        for row in rows:
            if not isinstance(row, dict) or not isinstance(row.get("RecordId"), str) or not row["RecordId"]:
                raise ProtocolError("Invalid catalog record")
            if row["RecordId"] in result:
                raise ProtocolError("Duplicate catalog record ID")
            result[row["RecordId"]] = row
        return result

    encounters = records("EncounterDefinitions")
    if encounter_id not in encounters:
        raise ProtocolError("Saved encounter was not found")
    encounter = encounters[encounter_id]
    if (encounter.get("ActivationMode", "manual") != "manual" or encounter.get("Conditions", []) != []
            or encounter.get("PlacementRecordId", "") != "" or encounter.get("PlacementSubjectRef", "") != ""
            or encounter.get("UniqueEncounter", False) is not False
            or type(encounter.get("MaximumActiveInstances", 1)) is not int
            or encounter.get("MaximumActiveInstances", 1) != 1):
        raise ProtocolError("Only manual, unplaced, nonunique single-instance encounters can be exported")
    catalog_records = records("Records")
    actor_profiles = records("ActorProfiles")
    encounter_record = catalog_records.get(encounter_id, {})
    if encounter_record.get("RecordKind") != "encounter" or encounter_record.get("Domain") != "population":
        raise ProtocolError("Saved encounter requires its population catalog record")
    entries = encounter.get("Entries", [])
    if not isinstance(entries, list) or not 1 <= len(entries) <= 8:
        raise ProtocolError("Saved encounter needs 1 to 8 entries")
    result_entries, used, entry_ids = [], set(), set()
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("EntryId"), str) or not entry["EntryId"]:
            raise ProtocolError("Invalid encounter entry")
        if entry["EntryId"] in entry_ids:
            raise ProtocolError("Duplicate encounter entry ID")
        entry_ids.add(entry["EntryId"])
        actor_id = entry.get("TargetRecordId")
        if not isinstance(actor_id, str):
            raise ProtocolError("Invalid actor target")
        record = catalog_records.get(actor_id, {})
        if record.get("RecordKind") != "actor" or record.get("Domain") != "population":
            raise ProtocolError("Only saved actor entries are supported; troop expansion is unavailable")
        if actor_id in used:
            raise ProtocolError("Duplicate actor target in saved encounter")
        if actor_id not in actor_profiles:
            raise ProtocolError("Saved actor requires its typed actor profile")
        profile = actor_profiles[actor_id]
        if any(profile.get(flag, False) is not False for flag in ("UniqueActor", "EssentialActor", "PersistentActor")):
            raise ProtocolError("Unique, essential or persistent actor policy cannot be exported")
        minimum = _count(entry.get("MinimumCount", 1))
        maximum = _count(entry.get("MaximumCount", 1))
        if minimum != maximum:
            raise ProtocolError("Select fixed encounter counts before export")
        template = bindings.get(actor_id)
        if not isinstance(template, str) or template not in TEMPLATES:
            raise ProtocolError("Every actor needs an explicit supported native-template binding")
        used.add(actor_id)
        result_entries.append({"template": template, "count": minimum})
    if used != set(bindings):
        raise ProtocolError("Bindings must exactly match the saved encounter's actor targets")
    value = {"contract": COMPOSITION, "name": catalog_records.get(encounter_id, {}).get("DisplayName", encounter_id),
             "entries": result_entries,
             "source": {"catalogSha256": hashlib.sha256(raw).hexdigest(), "encounterId": encounter_id, "actorBindings": bindings}}
    slots = composition_slots(value)
    population = encounter.get("PopulationLimit", 1000)
    if type(population) is not int or population < len(slots):
        raise ProtocolError("Saved population limit is below the composition count")
    return value


@dataclass(frozen=True)
class EncounterPlan:
    session_id: str
    plan_id: str
    fingerprint: str
    templates: tuple[str, ...]
    placement: str


def _identity(value, name):
    if not isinstance(value, str) or not re.fullmatch("[0-9a-f]{32}", value):
        raise ProtocolError("Invalid " + name)
    return value


def preview(client: TgeSdkClient, slots: tuple[str, ...]) -> EncounterPlan:
    composition_slots({"contract": COMPOSITION, "name": "Preview",
                       "entries": [{"template": slot, "count": 1} for slot in slots], "source": None})
    result = client.invoke(client.create_request(SERVICE, "0.1", "preview", {"templates": ",".join(slots)}))
    if not result.succeeded:
        raise ProtocolError("Encounter preview rejected: " + result.code)
    fields = {"planId", "fingerprint", "templates", "placement", "expiresInSeconds"}
    if result.code != "encounter_plan" or set(result.values) != fields:
        raise ProtocolError("Unexpected encounter plan")
    values = result.values
    _identity(values["planId"], "plan ID")
    if (not re.fullmatch("[0-9a-f]{64}", values["fingerprint"]) or values["templates"] != ",".join(slots)
            or values["expiresInSeconds"] != "30" or not values["placement"]):
        raise ProtocolError("Encounter plan does not match the requested composition")
    return EncounterPlan(result.session_id, values["planId"], values["fingerprint"], slots, values["placement"])


def spawn_request(client: TgeSdkClient, plan: EncounterPlan):
    if client.session_id != plan.session_id:
        raise ProtocolError("Preview belongs to a different game session")
    _identity(plan.plan_id, "plan ID")
    if not re.fullmatch("[0-9a-f]{64}", plan.fingerprint):
        raise ProtocolError("Invalid plan fingerprint")
    return client.create_request(SERVICE, "0.1", "spawn",
                                 {"planId": plan.plan_id, "fingerprint": plan.fingerprint})


def state(result) -> dict:
    if not result.succeeded:
        raise ProtocolError("Encounter operation did not succeed: " + result.code)
    values = dict(result.values)
    fixed = {"encounterId", "state", "plannedCount", "trackedCount", "appeared", "cleanupComplete", "failure", "cleanupFailure"}
    if result.code != "encounter_state" or not fixed <= set(values):
        raise ProtocolError("Unexpected encounter state")
    _identity(values["encounterId"], "encounter ID")
    if not re.fullmatch("[1-8]", values["plannedCount"]) or not re.fullmatch("[0-8]", values["trackedCount"]):
        raise ProtocolError("Invalid encounter counts")
    planned, tracked = int(values["plannedCount"]), int(values["trackedCount"])
    if tracked > planned or values["appeared"] not in ("true", "false") or values["cleanupComplete"] not in ("true", "false"):
        raise ProtocolError("Inconsistent encounter state")
    expected = fixed | {f"actor.{i}.{field}" for i in range(tracked) for field in ("id", "nativeId", "state")}
    if set(values) != expected or values["state"] not in {"pending", "appeared", "removing", "removed", "failed", "cleanup_failed"}:
        raise ProtocolError("Unexpected actor state fields")
    handles = set()
    for i in range(tracked):
        handle = _identity(values[f"actor.{i}.id"], "actor handle")
        if handle in handles or values[f"actor.{i}.state"] not in {"pending", "ready", "removed"}:
            raise ProtocolError("Inconsistent tracked actor")
        handles.add(handle)
        if values[f"actor.{i}.state"] == "ready" and not values[f"actor.{i}.nativeId"]:
            raise ProtocolError("Ready actor requires observed native identity")
    if values["state"] == "appeared" and (tracked != planned or values["appeared"] != "true"
            or any(values[f"actor.{i}.state"] != "ready" for i in range(tracked))):
        raise ProtocolError("Appeared requires every actor observed ready")
    complete = values["cleanupComplete"] == "true"
    if complete != (values["state"] in {"removed", "failed"}) or (complete and
            any(values[f"actor.{i}.state"] != "removed" for i in range(tracked))):
        raise ProtocolError("Removal is not confirmed")
    if values["state"] == "removed" and values["failure"]:
        raise ProtocolError("Failed execution cannot be reported as removed success")
    return {"sessionId": result.session_id, **values}


def query(client, encounter_id, remove=False):
    _identity(encounter_id, "encounter ID")
    return state(client.invoke(client.create_request(SERVICE, "0.1", "remove" if remove else "status",
                                                    {"encounterId": encounter_id})))


def write_new(path: Path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int)
    sub = parser.add_subparsers(dest="command", required=True)
    export = sub.add_parser("export")
    export.add_argument("--catalog", type=Path, required=True)
    export.add_argument("--encounter-id", required=True)
    export.add_argument("--bindings", type=Path, required=True)
    export.add_argument("--output", type=Path, required=True)
    for name in ("preview", "spawn"):
        p = sub.add_parser(name)
        p.add_argument("--composition", type=Path, required=True)
        if name == "spawn":
            p.add_argument("--request-output", type=Path, required=True,
                           help="Exclusive new file written before dispatch; preserves exact request for reconciliation")
    for name in ("status", "remove"):
        p = sub.add_parser(name)
        p.add_argument("--session", required=True)
        p.add_argument("--encounter-id", required=True)
    retry = sub.add_parser("reconcile")
    retry.add_argument("--request", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "export":
            bindings, _ = load_data(args.bindings)
            value = export_editor_composition(args.catalog, args.encounter_id, bindings)
            write_new(args.output, value)
            print(json.dumps(value, indent=2))
            return
        key = os.environ.get("TGE_SDK_KEY", "")
        if not re.fullmatch("[0-9a-fA-F]{64}", key):
            raise ProtocolError("TGE_SDK_KEY must contain the shared 32-byte key in hex")
        client = TgeSdkClient(args.port, bytes.fromhex(key),
                             expected_host_id="kane.tgfoa.tainted-grail-extender", expected_host_version="0.1.0")
        client.connect()
        if args.command in ("preview", "spawn"):
            composition, _ = load_data(args.composition)
            plan = preview(client, composition_slots(composition))
            print(json.dumps({"sessionId": plan.session_id, "templates": plan.templates, "placement": plan.placement,
                              "planId": plan.plan_id, "fingerprint": plan.fingerprint}), flush=True)
            if args.command == "spawn":
                request = spawn_request(client, plan)
                # Failure after this point is ambiguous: reconcile this exact request; never reissue spawn.
                write_new(args.request_output, json.loads(encode_request(request)))
                print(json.dumps(state(client.invoke(request))), flush=True)
        elif args.command == "reconcile":
            _, raw = load_data(args.request)
            request = decode_request(raw.decode("utf-8-sig"))
            if request.service_id != SERVICE or request.service_version != "0.1" or request.operation != "spawn":
                raise ProtocolError("Only an exact saved encounter spawn request can be reconciled")
            print(json.dumps(state(client.invoke(request))), flush=True)
        else:
            if args.session != client.session_id:
                raise ProtocolError("Encounter belongs to a different game session")
            print(json.dumps(query(client, args.encounter_id, args.command == "remove")), flush=True)
    except (ProtocolError, OSError, TypeError) as exc:
        parser.exit(2, f"Encounter command failed: {exc}\n")


if __name__ == "__main__":
    main()
