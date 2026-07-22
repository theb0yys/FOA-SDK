import json
import unittest

from semantic_repair import (
    MatrixAxis,
    MountConversionTransaction,
    SyntheticMountState,
    matrix_cases,
    run_matrix,
)


class FailureMatrixTests(unittest.TestCase):
    def test_cartesian_cases_are_deterministic(self):
        cases = matrix_cases(
            MatrixAxis("failure", ("create", "field")),
            MatrixAxis("owned", (None, "horse")),
        )
        self.assertEqual(
            cases,
            (
                {"failure": "create", "owned": None},
                {"failure": "create", "owned": "horse"},
                {"failure": "field", "owned": None},
                {"failure": "field", "owned": "horse"},
            ),
        )

    def test_mount_failure_matrix_restores_all_prestate_variants(self):
        cases = matrix_cases(
            MatrixAxis("failure", ("create", "field", "movement", "ownership")),
            MatrixAxis("owned", (None, "horse")),
        )

        def execute(case):
            state = SyntheticMountState(
                owned_mount=case["owned"],
                native_actions=("mount", "pet"),
                fields={"mount_kind": "original"},
                movement_enabled={"follow": True, "combat": False},
                created_objects=["existing"],
            )
            before = json.dumps(state.__dict__, sort_keys=True)
            tx = MountConversionTransaction(state)
            tx.prepare()
            try:
                tx.commit(wolf_id="wolf", fail_after=case["failure"])
            finally:
                self.assertEqual(json.dumps(state.__dict__, sort_keys=True), before)

        results = run_matrix(cases, execute)
        self.assertEqual(len(results), 8)
        self.assertTrue(all(result.status == "failed" for result in results))
        self.assertTrue(all(result.error_type == "RuntimeError" for result in results))


if __name__ == "__main__":
    unittest.main()
