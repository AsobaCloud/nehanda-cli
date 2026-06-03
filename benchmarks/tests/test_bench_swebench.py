#!/usr/bin/env python3
"""unittest tests for bench_swebench.py helpers.

Tests _extract_patch, _build_prediction, and _write_predictions on synthetic inputs.
Does not require a model, Docker, or network.
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_swebench import _extract_patch, _build_prediction, _write_predictions


class TestExtractPatch(unittest.TestCase):
    def test_fenced_diff_block(self):
        response = (
            "Here's my fix:\n\n```diff\n"
            "diff --git a/foo.py b/foo.py\n"
            "--- a/foo.py\n"
            "+++ b/foo.py\n"
            "@@ -1,3 +1,4 @@\n"
            " def hello():\n"
            "+    return \"hi\"\n"
            "     pass\n"
            "```\n"
        )
        patch = _extract_patch(response)
        self.assertTrue(patch.startswith("diff --git"))
        self.assertIn("+    return \"hi\"", patch)
        self.assertTrue(patch.endswith("pass"))

    def test_fenced_diff_block_no_git_header(self):
        response = (
            "```diff\n"
            "This is not a proper diff\n"
            "no git headers here\n"
            "```\n"
        )
        patch = _extract_patch(response)
        self.assertEqual(patch, "")

    def test_raw_diff_git_header(self):
        response = (
            "To fix this issue apply:\n\n"
            "diff --git a/bar.py b/bar.py\n"
            "--- a/bar.py\n"
            "+++ b/bar.py\n"
            "@@ -0,0 +1,2 @@\n"
            "+x = 1\n"
            "+y = 2\n"
        )
        patch = _extract_patch(response)
        self.assertTrue(patch.startswith("diff --git"))
        self.assertIn("+x = 1", patch)
        self.assertIn("+y = 2", patch)

    def test_no_diff_returns_empty(self):
        response = (
            "I think you should refactor the code.\n"
            "diff --git is not in the response.\n"
        )
        patch = _extract_patch(response)
        self.assertEqual(patch, "")

    def test_empty_response(self):
        patch = _extract_patch("")
        self.assertEqual(patch, "")

    def test_only_git_marker_no_diff(self):
        response = "The diff --git marker appears but no diff content follows"
        patch = _extract_patch(response)
        self.assertEqual(patch, "")

    def test_multifile_diff(self):
        response = (
            "```diff\n"
            "diff --git a/file1.py b/file1.py\n"
            "--- a/file1.py\n"
            "+++ b/file1.py\n"
            "@@ -1 +1 @@\n"
            "-old\n"
            "+new\n"
            "diff --git a/file2.py b/file2.py\n"
            "--- a/file2.py\n"
            "+++ b/file2.py\n"
            "@@ -1 +1 @@\n"
            "-a\n"
            "+b\n"
            "```"
        )
        patch = _extract_patch(response)
        self.assertTrue(patch.startswith("diff --git"))
        self.assertIn("file1.py", patch)
        self.assertIn("file2.py", patch)
        self.assertIn("-old", patch)
        self.assertIn("+new", patch)


class TestBuildPrediction(unittest.TestCase):
    def test_prediction_keys_present(self):
        pred = _build_prediction("django__django-15426", "gpt-4", "diff --git ...")
        self.assertIn("instance_id", pred)
        self.assertIn("model_name_or_path", pred)
        self.assertIn("model_patch", pred)

    def test_prediction_values(self):
        pred = _build_prediction("pytest__pytest-101", "claude-3-sonnet", "patch content")
        self.assertEqual(pred["instance_id"], "pytest__pytest-101")
        self.assertEqual(pred["model_name_or_path"], "claude-3-sonnet")
        self.assertEqual(pred["model_patch"], "patch content")

    def test_empty_patch(self):
        pred = _build_prediction("repo-1", "model-x", "")
        self.assertEqual(pred["model_patch"], "")

    def test_prediction_not_mutated(self):
        # Build two predictions — they should be independent dicts
        p1 = _build_prediction("a", "m", "p1")
        p2 = _build_prediction("b", "m", "p2")
        self.assertIsNot(p1, p2)
        self.assertNotEqual(p1["instance_id"], p2["instance_id"])


class TestWritePredictions(unittest.TestCase):
    def test_roundtrip_single_record(self):
        preds = [{"instance_id": "x", "model_name_or_path": "y", "model_patch": "z"}]
        with tempfile.NamedTemporaryFile(suffix=".jsonl", delete=False) as fh:
            path = Path(fh.name)
        try:
            _write_predictions(path, preds)
            lines = path.read_text().strip().split("\n")
            self.assertEqual(len(lines), 1)
            rec = json.loads(lines[0])
            self.assertEqual(rec["instance_id"], "x")
        finally:
            path.unlink()

    def test_roundtrip_multiple_records(self):
        preds = [
            {"instance_id": "a", "model_name_or_path": "m1", "model_patch": "p1"},
            {"instance_id": "b", "model_name_or_path": "m2", "model_patch": "p2"},
            {"instance_id": "c", "model_name_or_path": "m3", "model_patch": ""},
        ]
        with tempfile.NamedTemporaryFile(suffix=".jsonl", delete=False) as fh:
            path = Path(fh.name)
        try:
            _write_predictions(path, preds)
            lines = path.read_text().strip().split("\n")
            self.assertEqual(len(lines), 3)
            for i, line in enumerate(lines):
                rec = json.loads(line)
                self.assertEqual(rec["instance_id"], preds[i]["instance_id"])
                self.assertEqual(rec["model_name_or_path"], preds[i]["model_name_or_path"])
                self.assertEqual(rec["model_patch"], preds[i]["model_patch"])
        finally:
            path.unlink()

    def test_creates_parent_dir(self):
        preds = [{"instance_id": "x", "model_name_or_path": "y", "model_patch": "z"}]
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "subdir" / "nested" / "out.jsonl"
            _write_predictions(path, preds)
            self.assertTrue(path.exists())
            rec = json.loads(path.read_text().splitlines()[0])
            self.assertEqual(rec["instance_id"], "x")


if __name__ == "__main__":
    unittest.main()