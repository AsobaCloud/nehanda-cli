#!/usr/bin/env python3
"""Tests for benchmarks/check_provisioning.py provisioning audit."""

from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks import check_provisioning as cp
from benchmarks.check_provisioning import audit, load_catalog_datasets, load_manifest


class RealManifestTest(unittest.TestCase):
    """The shipped manifest must cover every shipped catalog dataset."""

    def test_every_catalog_dataset_is_documented(self) -> None:
        catalog = load_catalog_datasets(cp.DEFAULT_CATALOG)
        manifest, meta = load_manifest(cp.DEFAULT_MANIFEST)
        report = audit(catalog, manifest, meta, Path("data"))
        self.assertEqual(report["missing"], [], f"undocumented datasets: {report['missing']}")
        self.assertEqual(report["orphan"], [], f"orphan manifest entries: {report['orphan']}")

    def test_every_manifest_entry_has_a_method(self) -> None:
        manifest, _ = load_manifest(cp.DEFAULT_MANIFEST)
        valid = {"auto", "auto_hf", "manual", "bundled"}
        for ds_id, entry in manifest.items():
            self.assertIn(entry.get("method"), valid, f"{ds_id} has invalid method {entry.get('method')!r}")
            self.assertTrue(entry.get("files"), f"{ds_id} has no files declared")


class AuditStatusTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.data = Path(self._tmp.name)
        self.meta: dict = {}

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _write(self, rel: str, content: bytes) -> str:
        path = self.data / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return hashlib.sha256(content).hexdigest()

    def test_missing_and_orphan(self) -> None:
        catalog = {"a": {"pillar": "memory", "hash": ""}, "b": {"pillar": "memory", "hash": ""}}
        manifest = {"a": {"method": "manual", "files": ["a/x"]}, "c": {"method": "manual", "files": ["c/x"]}}
        report = audit(catalog, manifest, self.meta, self.data)
        self.assertEqual(report["missing"], ["b"])
        self.assertEqual(report["orphan"], ["c"])

    def test_not_provisioned_when_absent(self) -> None:
        catalog = {"a": {"pillar": "memory", "hash": ""}}
        manifest = {"a": {"method": "auto", "files": ["a/data.json"]}}
        report = audit(catalog, manifest, self.meta, self.data)
        self.assertEqual(report["datasets"]["a"]["status"], "not_provisioned")

    def test_provisioned_unpinned_reports_hash(self) -> None:
        h = self._write("a/data.json", b'{"k":1}')
        catalog = {"a": {"pillar": "memory", "hash": ""}}
        manifest = {"a": {"method": "auto", "files": ["a/data.json"]}}
        report = audit(catalog, manifest, self.meta, self.data)
        rec = report["datasets"]["a"]
        self.assertEqual(rec["status"], "provisioned_unpinned")
        self.assertEqual(rec["computed_hash"], h)

    def test_provisioned_verified_matches_catalog_hash(self) -> None:
        h = self._write("a/data.json", b'{"k":1}')
        catalog = {"a": {"pillar": "memory", "hash": h}}
        manifest = {"a": {"method": "auto", "files": ["a/data.json"]}}
        report = audit(catalog, manifest, self.meta, self.data)
        self.assertEqual(report["datasets"]["a"]["status"], "provisioned_verified")

    def test_hash_mismatch_detected(self) -> None:
        self._write("a/data.json", b'{"k":1}')
        catalog = {"a": {"pillar": "memory", "hash": "deadbeef"}}
        manifest = {"a": {"method": "auto", "files": ["a/data.json"]}}
        report = audit(catalog, manifest, self.meta, self.data)
        self.assertEqual(report["datasets"]["a"]["status"], "hash_mismatch")

    def test_multifile_hash_is_order_independent(self) -> None:
        self._write("a/one.json", b"one")
        self._write("a/two.json", b"two")
        catalog = {"a": {"pillar": "memory", "hash": ""}}
        # declared order swapped should NOT change the aggregate hash
        m1 = {"a": {"method": "auto", "files": ["a/one.json", "a/two.json"]}}
        m2 = {"a": {"method": "auto", "files": ["a/two.json", "a/one.json"]}}
        h1 = audit(catalog, m1, self.meta, self.data)["datasets"]["a"]["computed_hash"]
        h2 = audit(catalog, m2, self.meta, self.data)["datasets"]["a"]["computed_hash"]
        self.assertEqual(h1, h2)

    def test_directory_hash_is_deterministic(self) -> None:
        self._write("d/sub/x.txt", b"x")
        self._write("d/y.txt", b"y")
        h1 = cp._hash_path(self.data / "d")
        h2 = cp._hash_path(self.data / "d")
        self.assertEqual(h1, h2)
        self.assertEqual(len(h1), 64)


if __name__ == "__main__":
    unittest.main()
