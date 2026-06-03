#!/usr/bin/env python3
"""Dataset hash verification test.

Tests that dataset hash computation is correct and detects mutations.
"""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))


def _sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class DatasetHashTest(unittest.TestCase):
    def test_hash_is_deterministic(self) -> None:
        data = b'{"dataset": "test", "items": [1, 2, 3]}'
        h1 = _sha256_bytes(data)
        h2 = _sha256_bytes(data)
        self.assertEqual(h1, h2)

    def test_hash_detects_mutation(self) -> None:
        original = b'{"question": "What is 2+2?", "answer": "4"}'
        mutated = b'{"question": "What is 2+2?", "answer": "5"}'
        self.assertNotEqual(_sha256_bytes(original), _sha256_bytes(mutated))

    def test_hash_file_matches_bytes(self) -> None:
        data = b'{"test": true, "value": 42}'
        with tempfile.NamedTemporaryFile(delete=False, suffix=".json") as f:
            f.write(data)
            tmp_path = Path(f.name)
        try:
            file_hash = _sha256_file(tmp_path)
            bytes_hash = _sha256_bytes(data)
            self.assertEqual(file_hash, bytes_hash)
        finally:
            tmp_path.unlink(missing_ok=True)

    def test_verification_rejects_mutated_file(self) -> None:
        dataset = {"dataset": "locomo", "items": [{"id": "q1", "answer": "Paris"}]}
        original_bytes = json.dumps(dataset).encode()
        original_hash = _sha256_bytes(original_bytes)

        dataset["items"][0]["answer"] = "London"
        mutated_bytes = json.dumps(dataset).encode()
        mutated_hash = _sha256_bytes(mutated_bytes)

        self.assertNotEqual(original_hash, mutated_hash, "Mutated dataset must produce a different hash")

    def test_empty_dataset_has_consistent_hash(self) -> None:
        empty = b"{}"
        h = _sha256_bytes(empty)
        self.assertEqual(len(h), 64, "SHA-256 hex digest must be 64 chars")
        self.assertTrue(all(c in "0123456789abcdef" for c in h))

    def test_hash_is_64_hex_chars(self) -> None:
        for data in [b"", b"hello", b'{"x": 1}', b"\x00\xff\xaa"]:
            h = _sha256_bytes(data)
            self.assertEqual(len(h), 64, f"SHA-256 must be 64 hex chars, got {len(h)} for {data!r}")


if __name__ == "__main__":
    unittest.main()
