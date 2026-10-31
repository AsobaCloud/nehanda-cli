# Property-Based Tests

This directory contains property-based tests using [fast-check](https://github.com/dubzzz/fast-check) to validate universal correctness properties of the ona codebase.

## Overview

Property-based testing validates that certain properties hold true for all possible inputs, not just specific test cases. This provides stronger guarantees about code correctness.

## Test Files

### `settings-persistence-idempotence.mjs`

**Validates Requirements:** 4.1, 4.2, 4.4

**Properties Tested:**

1. **Multiple Startups Idempotence**: Multiple startups without changes return identical settings
   - Generates arbitrary settings objects
   - Simulates runtime changes via `updateEffectiveSettings()`
   - Verifies that multiple calls to `bootstrapSettings()` return identical results
   - Runs 100 test cases with random settings

2. **Runtime Changes Preservation**: Runtime changes (via /model) are preserved across restarts
   - Generates two arbitrary settings objects (initial and runtime change)
   - Simulates initial bootstrap, runtime change, and restart
   - Verifies that the runtime change is preserved after restart
   - Runs 100 test cases with random settings

3. **Multiple Changes Stability**: Settings remain stable after multiple runtime changes and restarts
   - Generates a sequence of 1-5 arbitrary settings changes
   - Simulates multiple runtime changes followed by a restart
   - Verifies that the last runtime change is preserved
   - Runs 50 test cases with random change sequences

## Running Tests

### Run all property tests:
```bash
./tests/property/run-settings-persistence-tests.sh
```

### Run a specific test directly:
```bash
node tests/property/settings-persistence-idempotence.mjs
```

## Test Output

Successful test runs will show:
```
✓ Property 1 passed: Multiple startups are idempotent
✓ Property 2 passed: Runtime changes are preserved across restarts
✓ Property 3 passed: Settings remain stable after multiple changes and restarts
✓ All property tests passed!
```

Failed tests will show detailed error messages with the counterexample that caused the failure.

## Dependencies

- `fast-check`: Property-based testing library
- `better-sqlite3`: SQLite database for testing
- Node.js 22+

## Integration with CI/CD

These tests can be integrated into CI/CD pipelines by adding them to the test suite:

```bash
# In your CI script
npm install
./tests/property/run-settings-persistence-tests.sh
```

## Writing New Property Tests

To add new property tests:

1. Create a new `.mjs` file in this directory
2. Import `fast-check` and necessary modules
3. Define arbitrary generators for your data types
4. Write properties using `fc.property()`
5. Assert properties using `fc.assert()`
6. Create a bash wrapper script for integration

Example structure:
```javascript
import fc from 'fast-check'

const myArbitrary = fc.record({
  field: fc.string()
})

const property = fc.property(myArbitrary, (data) => {
  // Test your property
  return true // or false
})

fc.assert(property, { numRuns: 100 })
```

## References

- [fast-check documentation](https://github.com/dubzzz/fast-check/tree/main/documentation)
- [Property-Based Testing Guide](https://github.com/dubzzz/fast-check/blob/main/documentation/1-Guides/PropertyBasedTesting.md)
- Spec: `.kiro/specs/flexible-ollama-model-support/design.md`
