---
name: test-driven-development
description: Use before changing behavior or fixing a bug when a focused test can prove the change.
source: bundled
triggers:
  tool: [Write, Edit, MultiEdit]
  path_pattern: ["_test.", "test_"]
---

# Test Driven Development

Use this when behavior is changing and the system can express the desired result as a test.

Work in this order:

1. Write or update the smallest test that proves the desired behavior.
2. Run it and confirm it fails for the expected reason.
3. Make the smallest implementation change that can pass the test.
4. Run the focused test again.
5. Refactor only after the test is green, then rerun the relevant test.

Do not treat a compile error, unrelated fixture failure, or unrun test as a valid red phase. The red phase must show the behavior gap you intend to close.
