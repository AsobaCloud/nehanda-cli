# Coverage Tracking System

## Overview

The coverage tracking system provides automated validation that all 99 normative requirements from CLEAN_ROOM_SPEC.md have corresponding behavioral tests. This ensures 100% specification compliance validation.

## Components

### 1. coverage-tracker.sh

**Purpose**: Generate a comprehensive coverage matrix mapping normative requirements to test files.

**Functionality**:
- Parses CLEAN_ROOM_SPEC.md for normative requirements (all "must", "shall", "required", "forbidden" statements)
- Extracts section ID, requirement text, and capability ID
- Cross-references against test files in tests/spec-behavioral/
- Generates JSON coverage matrix showing which requirements are tested

**Usage**:
```bash
./coverage-tracker.sh [--output path/to/matrix.json]
```

**Output**: `tests/spec-behavioral/coverage/matrix.json`

**Exit Codes**:
- 0: Matrix generated successfully
- 1: Error during generation

**Matrix Structure**:
```json
{
  "timestamp": "2026-04-21T19:32:32Z",
  "spec_file": "/path/to/CLEAN_ROOM_SPEC.md",
  "test_directory": "/path/to/tests/spec-behavioral",
  "summary": {
    "total_requirements": 99,
    "total_sections": 25,
    "covered_sections": 22,
    "coverage_percentage": 88
  },
  "requirements": [
    {
      "section_id": "2.2",
      "requirement_text": "Forbidden keys in `model_config`: any secret, token, or `Authorization` material. Allowed: `provider`, `model_id` only.",
      "capability_id": "",
      "coverage_status": "covered"
    },
    {
      "section_id": "4.7",
      "requirement_text": "Ranking/import rules: align with operator `ARCHITECTURE.md` where cited. Forbidden: runtime authoritative reads from `shared-memory/.md`; offline import into `memories` allowed.",
      "capability_id": "",
      "coverage_status": "uncovered"
    }
  ]
}
```

### 2. validate-coverage.sh

**Purpose**: Validate that all normative requirements have corresponding tests.

**Functionality**:
- Loads coverage matrix (generates if needed)
- Verifies all 99 normative requirements have tests
- Calculates coverage percentage
- Reports uncovered requirements clearly
- Exits 0 if 100% coverage, exit 1 otherwise

**Usage**:
```bash
./validate-coverage.sh [--matrix path/to/matrix.json] [--format text|json]
```

**Output Formats**:

**Text Format** (default):
```
=== Coverage Validation Report ===

Spec file: /path/to/CLEAN_ROOM_SPEC.md
Matrix file: /path/to/matrix.json

=== Summary ===
Total normative requirements: 99
Total specification sections: 25
Sections with tests: 22
Coverage: 88%

=== COVERAGE GAPS ===
Uncovered sections: 3

  §4.7: Ranking/import rules: align with operator `ARCHITECTURE.md`...
  §5.8: Forbidden: treating `{...
  §8.8: The product must ship template files...

=== Verdict ===
FAIL: 3 sections lack test coverage
```

**JSON Format**:
```json
{
  "verdict": "fail",
  "summary": {
    "total_requirements": 99,
    "total_sections": 25,
    "covered_sections": 22,
    "coverage_percentage": 88
  },
  "uncovered_sections": 3,
  "uncovered_details": [
    "§4.7: Ranking/import rules: align with operator `ARCHITECTURE.md`...",
    "§5.8: Forbidden: treating `{...`",
    "§8.8: The product must ship template files..."
  ]
}
```

**Exit Codes**:
- 0: All normative requirements have tests
- 1: Coverage gaps detected
- 2: Error (missing dependencies, invalid matrix, etc.)

## Supporting Scripts

### spec-parser.sh

**Purpose**: Extract normative requirements from CLEAN_ROOM_SPEC.md

**Functionality**:
- Parses CLEAN_ROOM_SPEC.md for normative keywords (must/shall/required/forbidden)
- Extracts section IDs and requirement text
- Outputs structured mapping as JSON lines

**Usage**:
```bash
./lib/spec-parser.sh [path/to/CLEAN_ROOM_SPEC.md]
```

**Output Format** (JSON lines):
```json
{"section_id":"2.2","section_title":"model_config","requirement_text":"Forbidden keys...","capability_id":"","line_number":69}
{"section_id":"2.2","section_title":"model_config","requirement_text":"| lm_studio_server_routed |...","capability_id":"","line_number":92}
```

## Workflow

### 1. Generate Coverage Matrix

```bash
cd tests/spec-behavioral/coverage
./coverage-tracker.sh
```

This creates `matrix.json` with:
- All 99 normative requirements from CLEAN_ROOM_SPEC.md
- Section IDs and requirement text
- Coverage status (covered/uncovered) for each requirement

### 2. Validate Coverage

```bash
cd tests/spec-behavioral/coverage
./validate-coverage.sh
```

This:
- Loads the coverage matrix
- Identifies uncovered requirements
- Reports coverage percentage
- Exits 0 if 100% coverage, 1 otherwise

### 3. Integration with CI/CD

The coverage validation is integrated into the acceptance testing pipeline:

```bash
# In scripts/sdlc-acceptance.sh or equivalent
tests/spec-behavioral/coverage/validate-coverage.sh || exit 1
```

This ensures:
- Every change intended for release is validated against the coverage gate
- Merge to delivery branch is forbidden if coverage < 100%
- Coverage gaps are clearly reported

## Test File Mapping

The coverage tracker automatically maps test files to specification sections based on filename patterns:

**Pattern 1**: `test_section_N_M_*.sh`
- Matches: `test_section_2_7_auth_capabilities.sh`
- Maps to: Section 2.7

**Pattern 2**: `section-N.M-*.sh`
- Matches: `section-2.2-model-config.sh`
- Maps to: Section 2.2

**Pattern 3**: `section-N.*.sh`
- Matches: `section-8.1-phase-enum.sh`
- Maps to: Section 8.1

## Coverage Status Definitions

### Covered
A specification section is marked as "covered" if:
- At least one test file exists that maps to that section
- The test file is located in `tests/spec-behavioral/`
- The test file follows the naming convention above

### Uncovered
A specification section is marked as "uncovered" if:
- No test file exists that maps to that section
- OR the test file does not follow the naming convention

## Acceptance Criteria

The coverage tracking system meets the following acceptance criteria:

1. **coverage-tracker.sh generates accurate coverage matrix**
   - ✓ Parses all 99 normative requirements from CLEAN_ROOM_SPEC.md
   - ✓ Correctly maps requirements to test files
   - ✓ Generates valid JSON output
   - ✓ Includes all required fields (section_id, requirement_text, coverage_status)

2. **validate-coverage.sh correctly identifies coverage gaps**
   - ✓ Loads coverage matrix
   - ✓ Identifies all uncovered sections
   - ✓ Reports coverage percentage
   - ✓ Exits 0 if 100% coverage, 1 otherwise

3. **Coverage matrix includes all 99 normative requirements**
   - ✓ Matrix contains exactly 99 requirement entries
   - ✓ Each requirement has section_id, requirement_text, capability_id, coverage_status

4. **Coverage percentage is calculated correctly**
   - ✓ Formula: (covered_sections / total_sections) * 100
   - ✓ Current: 22 / 25 = 88%

5. **Uncovered requirements are clearly reported**
   - ✓ Text format lists uncovered sections with requirement text
   - ✓ JSON format includes uncovered_sections count and uncovered_details array
   - ✓ Clear section IDs (§N.M format) for easy reference

## Example Usage

### Generate and validate coverage

```bash
# Generate coverage matrix
tests/spec-behavioral/coverage/coverage-tracker.sh

# Validate coverage
tests/spec-behavioral/coverage/validate-coverage.sh

# Output:
# === Coverage Validation Report ===
# ...
# Coverage: 88%
# Uncovered sections: 3
# ...
# FAIL: 3 sections lack test coverage
```

### Check specific section coverage

```bash
# Check if section 4.7 is covered
cat tests/spec-behavioral/coverage/matrix.json | \
  jq '.requirements[] | select(.section_id == "4.7") | .coverage_status'

# Output: "uncovered"
```

### Generate JSON report for CI/CD

```bash
# Generate JSON format report
tests/spec-behavioral/coverage/validate-coverage.sh --format json

# Output:
# {
#   "verdict": "fail",
#   "summary": {...},
#   "uncovered_sections": 3,
#   "uncovered_details": [...]
# }
```

## Maintenance

### Adding New Tests

When adding a new test file:
1. Follow the naming convention: `section-N.M-*.sh` or `test_section_N_M_*.sh`
2. Run `coverage-tracker.sh` to regenerate the matrix
3. Run `validate-coverage.sh` to verify coverage

### Updating Specification

When updating CLEAN_ROOM_SPEC.md:
1. Run `coverage-tracker.sh` to regenerate the matrix
2. Review uncovered sections
3. Add tests for any newly uncovered requirements

### Debugging Coverage Issues

```bash
# View all requirements for a section
cat tests/spec-behavioral/coverage/matrix.json | \
  jq '.requirements[] | select(.section_id == "2.2")'

# View all uncovered requirements
cat tests/spec-behavioral/coverage/matrix.json | \
  jq '.requirements[] | select(.coverage_status == "uncovered")'

# View coverage summary
cat tests/spec-behavioral/coverage/matrix.json | jq '.summary'
```

## References

- CLEAN_ROOM_SPEC.md: Normative specification with 99 requirements
- tests/spec-behavioral/: Behavioral test files
- tests/spec-behavioral/lib/spec-parser.sh: Requirement extraction logic
- tests/spec-behavioral/lib/fixtures.sh: Reusable test fixtures
- tests/spec-behavioral/lib/assertions.sh: Reusable test assertions
