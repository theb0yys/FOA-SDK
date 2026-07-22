import tempfile
import unittest
from pathlib import Path

from semantic_repair.abandonment import (
    LockAbandonmentReview,
    build_lock_abandonment_review,
    write_lock_abandonment_review,
)
from semantic_repair.errors import LockOwnershipError
from semantic_repair.ownership import ExclusiveResourceLock


class LockAbandonmentReviewTests(unittest.TestCase):
    def test_review_is_canonical_and_does_not_remove_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lock = ExclusiveResourceLock(root / ".lock", "resource")
            lease = lock.acquire("writer-a")
            review = build_lock_abandonment_review(
                lock,
                "reviewer",
                "owner process is unavailable; manual decision required",
            )
            path = write_lock_abandonment_review(review, root / "review.json")
            self.assertEqual(LockAbandonmentReview.from_bytes(path.read_bytes()), review)
            self.assertEqual(lock.current_identity(), lease.identity)
            self.assertTrue(lease.active)
            self.assertEqual(review.to_bytes(), review.to_bytes())

    def test_review_requires_an_active_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            lock = ExclusiveResourceLock(Path(tmp) / ".lock", "resource")
            with self.assertRaises(LockOwnershipError):
                build_lock_abandonment_review(lock, "reviewer", "no lock exists")


if __name__ == "__main__":
    unittest.main()
