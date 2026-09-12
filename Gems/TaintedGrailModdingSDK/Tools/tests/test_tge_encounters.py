#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import json
from pathlib import Path
import tempfile
import unittest

from tge_encounters import (COMPOSITION, EncounterPlan, composition_slots, export_editor_composition,
                            load_data, preview, query, spawn_request, state, write_new)
from tge_sdk_client import ProtocolError, SdkRequest, SdkResponse


def composition():
    return {"contract": COMPOSITION, "name": "Small patrol",
            "entries": [{"template": "wyrdspirit", "count": 1}], "source": None}


def response(**changes):
    values = {"encounterId": "a" * 32, "state": "appeared", "plannedCount": "1", "trackedCount": "1",
              "appeared": "true", "cleanupComplete": "false", "failure": "", "cleanupFailure": "",
              "actor.0.id": "b" * 32, "actor.0.nativeId": "RuntimeLocation:fixture:1", "actor.0.state": "ready"}
    values.update(changes)
    return SdkResponse("session", "succeeded", "encounter_state", "", values)


class Client:
    session_id = "session"
    def __init__(self, result):
        self.result, self.calls = result, []
    def create_request(self, service, version, operation, arguments=None):
        return SdkRequest(service, version, operation, self.session_id, arguments or {})
    def invoke(self, request):
        self.calls.append(request)
        return self.result


class EncounterTests(unittest.TestCase):
    def test_composition_bounds_and_types(self):
        value = composition()
        self.assertEqual(composition_slots(value), ("wyrdspirit",))
        value["entries"] = [{"template": "outlaw-1h", "count": 8}]
        self.assertEqual(len(composition_slots(value)), 8)
        for count in (0, 9, True, 1.0, "1", None, -1):
            with self.subTest(count=count), self.assertRaises(ProtocolError):
                value["entries"][0]["count"] = count
                composition_slots(value)
        for key, bad in (("entries", []), ("entries", None), ("name", ""), ("source", {}),
                         ("contract", "future/2")):
            with self.subTest(key=key, bad=bad), self.assertRaises(ProtocolError):
                value = composition(); value[key] = bad; composition_slots(value)
        value = composition(); value["entries"][0]["template"] = "quest-king"
        with self.assertRaises(ProtocolError): composition_slots(value)

    def test_duplicate_json_and_size(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "composition.json"
            path.write_text('{"name":"a","name":"b"}')
            with self.assertRaises(ProtocolError): load_data(path)
            path.write_text(" " * 17)
            with self.assertRaises(ProtocolError): load_data(path, 16)
            path.write_bytes(b"\xef\xbb\xbf{}")
            self.assertEqual(load_data(path)[0], {})

    def test_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "request.json"
            write_new(path, {"operation": "spawn"})
            with self.assertRaises(FileExistsError): write_new(path, {})
            self.assertEqual(load_data(path)[0]["operation"], "spawn")

    def test_preview_and_explicit_spawn_request(self):
        result = SdkResponse("session", "succeeded", "encounter_plan", "",
            {"planId": "a" * 32, "fingerprint": "b" * 64, "templates": "wyrdspirit",
             "placement": "fixture|12,0,0", "expiresInSeconds": "30"})
        client = Client(result)
        plan = preview(client, ("wyrdspirit",))
        self.assertEqual([r.operation for r in client.calls], ["preview"])
        request = spawn_request(client, plan)
        self.assertEqual(request.arguments, {"planId": plan.plan_id, "fingerprint": plan.fingerprint})
        self.assertEqual(len(client.calls), 1)  # Building a mutation request does not execute it.
        client.session_id = "new-session"
        with self.assertRaises(ProtocolError): spawn_request(client, plan)

    def test_preview_mismatch_rejected(self):
        for change in ({"templates": "outlaw-1h"}, {"expiresInSeconds": "60"}, {"fingerprint": "x" * 64}):
            values = {"planId": "a" * 32, "fingerprint": "b" * 64, "templates": "wyrdspirit",
                      "placement": "fixture", "expiresInSeconds": "30"}
            values.update(change)
            with self.assertRaises(ProtocolError):
                preview(Client(SdkResponse("session", "succeeded", "encounter_plan", "", values)), ("wyrdspirit",))

    def test_state_requires_actual_observations(self):
        self.assertEqual(state(response())["state"], "appeared")
        for change in ({"trackedCount": "0"}, {"appeared": "false"}, {"actor.0.state": "pending"},
                       {"actor.0.nativeId": ""}, {"plannedCount": "2"}, {"extra": "unexpected"}):
            with self.subTest(change=change), self.assertRaises(ProtocolError):
                state(response(**change))
        with self.assertRaises(ProtocolError):
            state(response(state="removed", cleanupComplete="true"))
        self.assertEqual(state(response(state="removed", cleanupComplete="true", **{"actor.0.state": "removed"}))["state"], "removed")

    def test_late_cleanup_keeps_failure(self):
        value = state(response(state="failed", cleanupComplete="true",
                               failure="discard_failed", cleanupFailure="discard_failed",
                               **{"actor.0.state": "removed"}))
        self.assertEqual(value["cleanupComplete"], "true")
        self.assertEqual(value["failure"], "discard_failed")

    def test_exact_handle_remove(self):
        client = Client(response(state="removing"))
        query(client, "a" * 32, remove=True)
        self.assertEqual(client.calls[0].arguments, {"encounterId": "a" * 32})
        with self.assertRaises(ProtocolError): query(client, "RuntimeLocation:unowned:1", remove=True)

    def catalog(self):
        return {"SchemaVersion": 3, "Records": [
            {"RecordId": "encounter.one", "RecordKind": "encounter", "Domain": "population", "DisplayName": "Saved patrol"},
            {"RecordId": "actor.one", "RecordKind": "actor", "Domain": "population"}],
            "ActorProfiles": [{"RecordId": "actor.one", "UniqueActor": False}],
            "EncounterDefinitions": [{"RecordId": "encounter.one", "Entries": [
                {"EntryId": "entry.one", "TargetRecordId": "actor.one", "MinimumCount": 2, "MaximumCount": 2}]}]}

    def export(self, catalog, bindings=None):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalog.json"
            raw = json.dumps(catalog).encode()
            path.write_bytes(raw)
            result = export_editor_composition(path, "encounter.one", bindings or {"actor.one": "wyrdspirit"})
            self.assertEqual(path.read_bytes(), raw)
            return result

    def test_saved_editor_composition_mapping(self):
        result = self.export(self.catalog())
        self.assertEqual(composition_slots(result), ("wyrdspirit", "wyrdspirit"))
        self.assertEqual(result["source"]["actorBindings"], {"actor.one": "wyrdspirit"})
        self.assertEqual(len(result["source"]["catalogSha256"]), 64)
        self.assertEqual(result["name"], "Saved patrol")

    def test_export_rejects_unsupported_authoring_semantics(self):
        for key, value in (("ActivationMode", "all_conditions"), ("Conditions", ["night"]),
                           ("PlacementSubjectRef", "courtyard"), ("PlacementRecordId", "place"),
                           ("UniqueEncounter", True), ("MaximumActiveInstances", 2), ("PopulationLimit", 1)):
            catalog = self.catalog(); catalog["EncounterDefinitions"][0][key] = value
            with self.subTest(key=key), self.assertRaises(ProtocolError): self.export(catalog)
        catalog = self.catalog(); catalog["ActorProfiles"][0]["PersistentActor"] = True
        with self.assertRaises(ProtocolError): self.export(catalog)
        catalog = self.catalog(); catalog["Records"][1]["RecordKind"] = "troop"
        with self.assertRaises(ProtocolError): self.export(catalog)

    def test_export_rejects_missing_ambiguous_and_random_counts(self):
        for change in ("random", "duplicate", "duplicate_actor", "future", "oversized",
                       "missing_profile", "missing_encounter_record", "duplicate_target"):
            catalog = self.catalog()
            if change == "random": catalog["EncounterDefinitions"][0]["Entries"][0]["MaximumCount"] = 3
            if change == "duplicate": catalog["EncounterDefinitions"][0]["Entries"] *= 2
            if change == "duplicate_actor": catalog["Records"].append(copy.deepcopy(catalog["Records"][1]))
            if change == "future": catalog["SchemaVersion"] = 4
            if change == "oversized": catalog["EncounterDefinitions"][0]["Entries"][0]["MinimumCount"] = 999
            if change == "missing_profile": catalog["ActorProfiles"] = []
            if change == "missing_encounter_record": catalog["Records"].pop(0)
            if change == "duplicate_target":
                entry = copy.deepcopy(catalog["EncounterDefinitions"][0]["Entries"][0])
                entry["EntryId"] = "entry.two"
                catalog["EncounterDefinitions"][0]["Entries"].append(entry)
            with self.subTest(change=change), self.assertRaises(ProtocolError): self.export(catalog)
        for bindings in ({"missing": "wyrdspirit"}, {"actor.one": "other"}, {"actor.one": "wyrdspirit", "extra": "outlaw-1h"}):
            with self.assertRaises(ProtocolError): self.export(self.catalog(), bindings)


if __name__ == "__main__":
    unittest.main()
