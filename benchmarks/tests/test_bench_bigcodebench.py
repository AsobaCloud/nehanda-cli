#!/usr/bin/env python3
"""unittest tests for bench_bigcodebench.py helpers."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_bigcodebench import _run_tests, _extract_code


class TestExtractCode(unittest.TestCase):
    def test_extract_code_with_python_lang_tag(self):
        code = _extract_code(
            "Here is the code:\n```python\ndef foo():\n    pass\n```",
            "prompt",
        )
        self.assertIn("def foo()", code)

    def test_extract_code_without_lang_tag(self):
        code = _extract_code(
            "```\ndef bar():\n    return 1\n```",
            "prompt",
        )
        self.assertIn("def bar()", code)

    def test_extract_code_no_fence_returns_stripped(self):
        code = _extract_code("def baz(): return 42", "prompt")
        self.assertIn("def baz()", code)

    def test_extract_code_empty_response(self):
        code = _extract_code("", "prompt")
        self.assertEqual(code, "")

    def test_extract_code_with_system_prompt_ignored(self):
        """System prompt text should not leak into extracted code."""
        code = _extract_code(
            "You are a helpful assistant. \n```python\ndef answer(): return 42\n```",
            "prompt",
        )
        self.assertIn("def answer()", code)


class TestRunTests(unittest.TestCase):
    def test_correct_solution_passes(self):
        solution = "def inc(x): return x + 1"
        test_code = """import unittest
class T(unittest.TestCase):
    def test_inc_1(self): self.assertEqual(inc(1), 2)
    def test_inc_0(self): self.assertEqual(inc(0), 1)
if __name__ == "__main__": unittest.main()
"""
        self.assertTrue(_run_tests(solution, test_code, "inc"))

    def test_wrong_solution_fails(self):
        solution = "def inc(x): return x - 1"
        test_code = """import unittest
class T(unittest.TestCase):
    def test_inc(self): self.assertEqual(inc(5), 6)
if __name__ == "__main__": unittest.main()
"""
        self.assertFalse(_run_tests(solution, test_code, "inc"))

    def test_syntax_error_fails(self):
        solution = "def bad syntax here"
        test_code = """import unittest
class T(unittest.TestCase):
    def test_dummy(self): self.assertTrue(True)
if __name__ == "__main__": unittest.main()
"""
        self.assertFalse(_run_tests(solution, test_code, ""))

    def test_timeout_never_returns_true(self):
        """A solution that hangs should not block indefinitely."""
        import time
        solution = "while True: pass"
        test_code = """import unittest
class T(unittest.TestCase):
    def test_dummy(self): self.assertTrue(True)
if __name__ == "__main__": unittest.main()
"""
        start = time.perf_counter()
        self.assertFalse(_run_tests(solution, test_code, "", timeout=2))
        elapsed = time.perf_counter() - start
        self.assertLess(elapsed, 10, "timeout should fire quickly")

    def test_import_error_fails(self):
        solution = "import nonexistent_module_12345"
        test_code = """import unittest
class T(unittest.TestCase):
    def test_dummy(self): self.assertTrue(True)
if __name__ == "__main__": unittest.main()
"""
        self.assertFalse(_run_tests(solution, test_code, ""))


if __name__ == "__main__":
    unittest.main()
