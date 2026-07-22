import unittest
from pathlib import Path

from semantic_repair import (
    ApiHello,
    CommandRegistration,
    RepairError,
    negotiate_api_version,
)

FIXTURES = Path(__file__).parent / "fixtures" / "api_v2"


class ApiV2SerializationTests(unittest.TestCase):
    def test_hello_roundtrip_and_highest_common_version(self):
        local_bytes = (FIXTURES / "api-hello-local.json").read_bytes()
        remote_bytes = (FIXTURES / "api-hello-remote.json").read_bytes()
        local = ApiHello.from_bytes(local_bytes)
        remote = ApiHello.from_bytes(remote_bytes)
        self.assertEqual(local.to_bytes(), local_bytes)
        self.assertEqual(remote.to_bytes(), remote_bytes)
        self.assertEqual(negotiate_api_version(local, remote), 2)

    def test_no_version_overlap_fails_closed(self):
        local = ApiHello.from_bytes((FIXTURES / "api-hello-local.json").read_bytes())
        future = ApiHello.from_bytes((FIXTURES / "api-hello-no-overlap.json").read_bytes())
        with self.assertRaises(RepairError):
            negotiate_api_version(local, future)

    def test_registration_roundtrip_is_canonical(self):
        data = (FIXTURES / "command-registration.json").read_bytes()
        registration = CommandRegistration.from_bytes(data)
        self.assertEqual(registration.to_bytes(), data)
        self.assertEqual(registration.owner_id, "foa.synthetic.mounts")

    def test_unknown_fields_and_api_versions_are_rejected(self):
        bad = (
            b'{"api_version":3,"command_id":"x","kind":"command-registration",'
            b'"label":"X","owner_id":"o","schema_version":1}\n'
        )
        with self.assertRaises(RepairError):
            CommandRegistration.from_bytes(bad)


if __name__ == "__main__":
    unittest.main()
