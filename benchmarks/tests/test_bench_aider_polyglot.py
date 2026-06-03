#!/usr/bin/env python3
"""unittest tests for bench_aider_polyglot.py helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_aider_polyglot import (
    _apply_edit,
    _apply_edits,
    _parse_edits,
)


class TestParseEdits(unittest.TestCase):
    def test_single_edit_block(self):
        response = (
            "src/main.py\n"
            "<<<<<<< SEARCH\n"
            "def foo():\n"
            "    return 1\n"
            "=======\n"
            "def foo():\n"
            "    return 42\n"
            ">>>>>>> REPLACE\n"
        )
        edits = _parse_edits(response)
        self.assertEqual(len(edits), 1)
        self.assertEqual(edits[0]["path"], "src/main.py")
        self.assertEqual(edits[0]["search"], "def foo():\n    return 1")
        self.assertEqual(edits[0]["replace"], "def foo():\n    return 42")

    def test_multiple_edit_blocks(self):
        response = (
            "foo.js\n"
            "<<<<<<< SEARCH\n"
            "const x = 1;\n"
            "=======\n"
            "const x = 2;\n"
            ">>>>>>> REPLACE\n"
            "bar.go\n"
            "<<<<<<< SEARCH\n"
            "var y = 1\n"
            "=======\n"
            "var y = 2\n"
            ">>>>>>> REPLACE\n"
        )
        edits = _parse_edits(response)
        self.assertEqual(len(edits), 2)
        self.assertEqual(edits[0]["path"], "foo.js")
        self.assertEqual(edits[1]["path"], "bar.go")

    def test_edit_block_with_leading_trailing_whitespace_in_path(self):
        response = (
            "  src/utils.rs  \n"
            "<<<<<<< SEARCH\n"
            "fn add(a: i32, b: i32) -> i32 { a + b }\n"
            "=======\n"
            "fn add(a: i32, b: i32) -> i32 { a - b }\n"
            ">>>>>>> REPLACE\n"
        )
        edits = _parse_edits(response)
        self.assertEqual(len(edits), 1)
        self.assertEqual(edits[0]["path"], "src/utils.rs")

    def test_no_edit_blocks_returns_empty_list(self):
        response = "Some text without edit markers."
        self.assertEqual(_parse_edits(response), [])

    def test_empty_response_returns_empty_list(self):
        self.assertEqual(_parse_edits(""), [])
        self.assertEqual(_parse_edits("   \n  \n"), [])

    def test_partial_marker_not_matched(self):
        """A << without the full SEARCH/REPLACE block should not match."""
        response = "<<<<<<< SEARCH\nnot a full block"
        self.assertEqual(_parse_edits(response), [])

    def test_multiline_search_and_replace(self):
        response = (
            "index.html\n"
            "<<<<<<< SEARCH\n"
            "<h1>Hello</h1>\n"
            "<p>World</p>\n"
            "=======\n"
            "<h1>Hi</h1>\n"
            "<p>Everyone</p>\n"
            ">>>>>>> REPLACE\n"
        )
        edits = _parse_edits(response)
        self.assertEqual(len(edits), 1)
        self.assertIn("<h1>Hello</h1>", edits[0]["search"])
        self.assertIn("<h1>Hi</h1>", edits[0]["replace"])

    def test_markers_in_search_text_are_not_recursive(self):
        """<<<<<<< inside search text should not break parsing."""
        search_content = "<<<<<<< SEARCH\nold\n=======\nnew\n>>>>>>> REPLACE"
        replace_content = "replaced"
        response = (
            "file.txt\n"
            f"<<<<<<< SEARCH\n{search_content}\n"
            f"=======\n{replace_content}\n"
            f">>>>>>> REPLACE\n"
        )
        edits = _parse_edits(response)
        self.assertEqual(len(edits), 1)
        self.assertIn("<<<<<<< SEARCH", edits[0]["search"])


class TestApplyEdit(unittest.TestCase):
    def test_exact_substring_replaced(self):
        content = "line1\nline2\nline3"
        result = _apply_edit(content, "line2", "LINE2")
        self.assertEqual(result, "line1\nLINE2\nline3")

    def test_first_occurrence_replaced(self):
        content = "aaa bbb aaa"
        result = _apply_edit(content, "aaa", "ccc")
        self.assertEqual(result, "ccc bbb aaa")

    def test_search_not_found_returns_none(self):
        content = "hello world"
        result = _apply_edit(content, "goodbye", "farewell")
        self.assertIsNone(result)

    def test_empty_search_replaces_at_start(self):
        content = "hello"
        result = _apply_edit(content, "", "START:")
        self.assertEqual(result, "START:hello")

    def test_empty_replace_removes_search(self):
        content = "<!-- comment -->\nreal code"
        result = _apply_edit(content, "<!-- comment -->\n", "")
        self.assertEqual(result, "real code")

    def test_multiline_search_and_replace(self):
        content = "a\nb\nc\nd"
        result = _apply_edit(content, "b\nc", "B\nC")
        self.assertEqual(result, "a\nB\nC\nd")

    def test_identical_search_replace_no_change(self):
        content = "unchanged"
        result = _apply_edit(content, "unchanged", "unchanged")
        self.assertEqual(result, "unchanged")


class TestApplyEdits(unittest.TestCase):
    def test_single_edit_to_single_file(self):
        files = {"main.py": "def foo():\n    return 1\n"}
        edits = [
            {
                "path": "main.py",
                "search": "def foo():\n    return 1\n",
                "replace": "def foo():\n    return 42\n",
            }
        ]
        result = _apply_edits(files, edits)
        self.assertIsNotNone(result)
        self.assertEqual(result["main.py"], "def foo():\n    return 42\n")

    def test_multiple_edits_multiple_files(self):
        files = {"a.txt": "aaa", "b.txt": "bbb"}
        edits = [
            {"path": "a.txt", "search": "aaa", "replace": "AAA"},
            {"path": "b.txt", "search": "bbb", "replace": "BBB"},
        ]
        result = _apply_edits(files, edits)
        self.assertIsNotNone(result)
        self.assertEqual(result["a.txt"], "AAA")
        self.assertEqual(result["b.txt"], "BBB")

    def test_second_edit_uses_updated_content(self):
        files = {"main.py": "line1\nline2\nline3"}
        edits = [
            {"path": "main.py", "search": "line1", "replace": "LINE1"},
            {"path": "main.py", "search": "LINE1", "replace": "FIRST"},
        ]
        result = _apply_edits(files, edits)
        self.assertIsNotNone(result)
        self.assertEqual(result["main.py"], "FIRST\nline2\nline3")

    def test_edits_preserves_unmodified_files(self):
        files = {"keep.py": "unchanged", "change.py": "original"}
        edits = [{"path": "change.py", "search": "original", "replace": "modified"}]
        result = _apply_edits(files, edits)
        self.assertIsNotNone(result)
        self.assertEqual(result["keep.py"], "unchanged")
        self.assertEqual(result["change.py"], "modified")

    def test_unknown_path_returns_none(self):
        files = {"main.py": "content"}
        edits = [{"path": "nonexistent.py", "search": "x", "replace": "y"}]
        self.assertIsNone(_apply_edits(files, edits))

    def test_failed_sub_edit_returns_none(self):
        files = {"main.py": "hello"}
        edits = [{"path": "main.py", "search": "goodbye", "replace": "hi"}]
        self.assertIsNone(_apply_edits(files, edits))

    def test_empty_edit_list_returns_copy_of_files(self):
        files = {"a.txt": "content", "b.txt": "more"}
        result = _apply_edits(files, [])
        self.assertIsNotNone(result)
        self.assertEqual(result, files)
        self.assertIsNot(result, files)  # must be a copy


if __name__ == "__main__":
    unittest.main()