import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from repair_primitives import (
    CommandRegistration,
    DiagnosticLimits,
    DiagnosticSession,
    DialogueRegistryV2,
    MappingTransaction,
    MountConversionTransaction,
    RepairError,
    RetryableCleanup,
    SyntheticMountState,
    TypedFieldAdapter,
    validate_session_segment,
)


class TypedTransactionTests(unittest.TestCase):
    def test_exact_identity_and_commit(self):
        record = {"quantity": 2}
        adapter = TypedFieldAdapter("profile", "item", "quantity", int, minimum=1, maximum=99)
        tx = MappingTransaction(adapter, record, "profile", "item", 5)
        tx.prepare()
        tx.commit()
        self.assertEqual(record["quantity"], 5)
        self.assertTrue(tx.committed)

    def test_failure_rolls_back_and_rollback_is_idempotent(self):
        record = {"quantity": 2}
        adapter = TypedFieldAdapter("profile", "item", "quantity", int, minimum=1)
        tx = MappingTransaction(adapter, record, "profile", "item", 5)
        tx.prepare()
        with self.assertRaises(RuntimeError):
            tx.commit(lambda: (_ for _ in ()).throw(RuntimeError("boom")))
        self.assertEqual(record["quantity"], 2)
        tx.rollback()
        self.assertEqual(record["quantity"], 2)

    def test_rejects_bool_and_wrong_identity(self):
        adapter = TypedFieldAdapter("profile", "item", "quantity", int)
        with self.assertRaises(RepairError):
            adapter.validate_value(True)
        with self.assertRaises(RepairError):
            adapter.read({"quantity": 1}, profile_id="other", type_id="item")


class DiagnosticTests(unittest.TestCase):
    def test_session_validation(self):
        for value in ("", ".", "..", "/tmp", "a/b", "CON", "bad\x00"):
            with self.subTest(value=value):
                with self.assertRaises(RepairError):
                    validate_session_segment(value)

    def test_redaction_formula_neutralization_and_limits(self):
        with tempfile.TemporaryDirectory() as tmp:
            session = DiagnosticSession(
                Path(tmp),
                "safe",
                DiagnosticLimits(field_bytes=32, row_bytes=128, session_bytes=256),
            )
            payload = session.encode_support_csv(
                {"display": "=2+2", "secret": "private", "long": "x" * 80},
                sensitive_fields={"secret"},
            )
            text = payload.decode()
            self.assertIn("'=2+2", text)
            self.assertIn("[REDACTED]", text)
            self.assertIn("[TRUNCATED]", text)

    def test_publish_is_atomic_on_summary_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            session = DiagnosticSession(Path(tmp), "safe")
            session.session_dir.mkdir(parents=True)
            row = session.session_dir / "rows.csv"
            summary = session.session_dir / "diagnostic-summary.json"
            row.write_bytes(b"old-row\n")
            summary.write_bytes(b"old-summary\n")
            original_replace = __import__("repair_primitives").os.replace

            def failing_replace(src, dst):
                if Path(dst).name == "diagnostic-summary.json":
                    raise OSError("injected")
                return original_replace(src, dst)

            with mock.patch("repair_primitives.os.replace", side_effect=failing_replace):
                with self.assertRaises(OSError):
                    session.publish("rows.csv", b"new-row\n", {"count": 1})
            self.assertEqual(row.read_bytes(), b"old-row\n")
            self.assertEqual(summary.read_bytes(), b"old-summary\n")


class MountTransactionTests(unittest.TestCase):
    def make_state(self):
        return SyntheticMountState(
            owned_mount=None,
            native_actions=("mount", "pet"),
            fields={"mount_kind": "none"},
            movement_enabled={"follow": True, "combat": True},
            created_objects=[],
        )

    def test_every_failure_point_restores_exact_snapshot(self):
        for point in ("create", "field", "movement", "ownership"):
            with self.subTest(point=point):
                state = self.make_state()
                before = json.dumps(state.__dict__, sort_keys=True)
                tx = MountConversionTransaction(state)
                tx.prepare()
                with self.assertRaises(RuntimeError):
                    tx.commit(wolf_id="wolf", fail_after=point)
                self.assertEqual(json.dumps(state.__dict__, sort_keys=True), before)
                tx.rollback()
                self.assertEqual(json.dumps(state.__dict__, sort_keys=True), before)

    def test_success_and_explicit_null_restoration(self):
        state = self.make_state()
        tx = MountConversionTransaction(state)
        tx.prepare()
        tx.commit(wolf_id="wolf")
        self.assertEqual(state.owned_mount, "wolf")
        tx.rollback()
        self.assertIsNone(state.owned_mount)
        self.assertEqual(state.native_actions, ("mount", "pet"))


class DialogueRegistryTests(unittest.TestCase):
    def test_owner_isolation_duplicate_and_retryable_cleanup(self):
        registry = DialogueRegistryV2()
        token_a = registry.register(CommandRegistration("a", "toggle", "Toggle"))
        token_b = registry.register(CommandRegistration("b", "toggle", "Toggle"))
        self.assertNotEqual(token_a, token_b)
        with self.assertRaises(RepairError):
            registry.register(CommandRegistration("a", "toggle", "Again"))
        with self.assertRaises(RepairError):
            registry.unregister("b", token_a)
        cleanup = RetryableCleanup(registry, "a", token_a)
        self.assertFalse(cleanup.attempt(fail=True))
        self.assertEqual(cleanup.token, token_a)
        self.assertEqual(registry.query("a"), (token_a,))
        self.assertTrue(cleanup.attempt())
        self.assertIsNone(cleanup.token)
        self.assertEqual(registry.query("a"), ())
        self.assertEqual(registry.query("b"), (token_b,))


if __name__ == "__main__":
    unittest.main()
