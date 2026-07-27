# AGENTS.md: Core Mandates for AI Engineering

This document is the absolute authority on workflow and safety for AI agents. Violations are zero-tolerance.

## 1. THE SEVEN COMMANDMENTS (MANDATORY)

1.  **NO CODE WITHOUT GITHUB ISSUE**: Every change must reference an existing issue in Project #4. Format: `fix: description (#123)`.
2.  **EXPLORE BEFORE CODING**: Use `grep` and `glob` to find existing patterns. Read `docs/MAP.md` to identify required context.
3.  **PLAN THEN CONFIRM**: Present a detailed plan (files, functions, logic). **STOP and WAIT** for explicit approval before writing any code.
4.  **NO DUPLICATE FUNCTIONS**: Always search for and modify existing functions instead of creating duplicates.
5.  **SAFETY CHECKERS MUST PASS**:
    *   `node ui/js-safety-checker.js`
    *   `./scripts/shell-safety-checker.sh`
    *   `python3 scripts/python-safety-checker.py`
6.  **VALIDATION IS BEHAVIOR-FIRST**: Verify at least 70% of plausible behavioral paths. Availability (ping) checks are NOT validation. Every plan MUST include a **Behavioral Specification** (Given-When-Then) for all functional changes.
7.  **TDD IS THE STANDARD**: Follow the Red-Green-Refactor cycle. Create or update a behavioral test *before* implementing the logic fix or feature. Use the `_behavioral.py` or similar suffix for behavioral test files.
8.  **ZERO ROOT CLUTTER**: The project root is a protected zone. Agents MUST NOT create scripts, JSON data, or CSV exports in the root. All diagnostic scripts, temporary analyses, and tool outputs MUST be stored in `/tmp/` and NEVER committed.

## 2. THE MANDATORY WORKFLOW (E-P-C-V-D)

### Phase 1: EXPLORE
*   **Search**: Find similar implementations.
*   **Registry**: Consult `docs/MAP.md` for required context (READMEs, Schemas, Architecture).
*   **Principles**: Review `docs/ARCHITECTURAL_FIRST_PRINCIPLES.md` for CORS, error handling, and response patterns.

### Phase 2: PLAN
*   List every file to be modified.
*   List every function to be added/changed/deleted.
*   Draft the **Behavioral Specification**:
    *   Categorize behaviors as **CREATED**, **UPDATED**, or **REMOVED**.
    *   Define each behavior using **Given-When-Then** (Gherkin) scenarios.
    *   Example: "HP-1: Given an authenticated user, When they request a forecast, Then return 200 with forecast data."
*   Draft the **Behavioral Coverage Matrix** (Mapping scenarios to specific test files/methods).

### Phase 3: CONFIRM (Stop Point)
*   Present the plan, including the full Behavioral Specification.
*   **WAIT** for explicit approval ("Proceed", "Yes", "Approved").

### Phase 4: CODE & VALIDATE
*   Implement minimal surgical changes.
*   Run all Safety Checkers.
*   Verify behavioral paths from the Matrix.

### Phase 5: DEPLOY
*   **Changelog**: `CHANGELOG.md` is automatically updated on merge to `main` via `scripts/update-changelog-staging.py`.
*   **Commit**: Use issue reference: `fix: description (#123)`.
*   **Project**: Update issue status in Project #4.

## 3. TECHNICAL INVARIANTS
*   **Lambda Names**: Must use `ona-{service}-{stage}` (via `get_lambda_name()` in `config/environment.sh`).
*   **UI Pattern**: Must use Controller/Renderer architecture (Subscriber/Timer vs. DOM/Mutation).
*   **Error Handling**: Must use standardized response formats from `ARCHITECTURAL_FIRST_PRINCIPLES.md`.
*   **NO STATEMENTS WITHOUT EVIDENCE**: Do not make statements of fact about the system without citing BOTH a specific line of documentation AND a specific function or piece of code. Every factual claim must have dual citations.
