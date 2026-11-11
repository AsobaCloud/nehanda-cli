# AGENTS.md: Core Mandates for AI Engineering

This document is the absolute authority on workflow and safety for AI agents. Violations are zero-tolerance.

## 1. MANDATORY RULES (MANDATORY)

1.  **NO CODE WITHOUT GITHUB ISSUE**: Every change must reference an existing issue in Project #4. Format: `fix: description (#123)`.
2.  **EXPLORE BEFORE CODING**: Use `grep` and `glob` to find existing patterns. Read `docs/MAP.md` to identify required context.
3.  **PLAN THEN CONFIRM**: Present a detailed plan (files, functions, logic). **STOP and WAIT** for explicit approval before writing any code.
4.  **NO DUPLICATE FUNCTIONS**: Always search for and modify existing functions instead of creating duplicates.
5.  **SAFETY CHECKERS MUST PASS**:
    *   `node lib/scripts/js-safety-checker.cjs .`
    *   `bash lib/scripts/shell-safety-checker.sh .`
    *   `python3 lib/scripts/python-safety-checker.py .`
6.  **VALIDATION IS BEHAVIOR-FIRST**: Verify at least 70% of plausible behavioral paths. Availability (ping) checks are NOT validation. Every plan MUST include a **Behavioral Specification** (Given-When-Then) for all functional changes.
7.  **TDD IS THE STANDARD**: Follow the Red-Green-Refactor cycle. Create or update a behavioral test *before* implementing the logic fix or feature. Use the `_behavioral.py` or similar suffix for behavioral test files.
8.  **ZERO ROOT CLUTTER**: The project root is a protected zone. Agents MUST NOT create scripts, JSON data, or CSV exports in the root. All diagnostic scripts, temporary analyses, and tool outputs MUST be stored in `/tmp/` and NEVER committed.

## 2. THE MANDATORY WORKFLOW

The runtime uses eleven phases, including idle. Follow the same order when working on this repository:

1. **Explore** (`explore`): Read `docs/MAP.md`, locate the actual owners and contracts, and inspect targeted code. Use Read/Glob/Grep. Preserve observations; stop repeated no-progress retrieval.
2. **Diagnose** (`diagnose`): Document observed behavior, expected behavior and the evidenced cause before proposing a fix.
3. **Create issue** (`create_issue`): Reference an existing issue or create the required issue record. The harness validates its local artifact before entering test creation.
4. **Create tests** (`create_tests`): Write behavioral Given/When/Then acceptance tests and execute them. Confirm genuine assertion RED; runner/import/syntax failures and model claims do not qualify.
5. **Plan and confirm** (`planning`): List every affected file/function and concrete logic. Categorize behaviors CREATED/UPDATED/REMOVED and map scenarios to test methods. Present the plan after RED and **wait for explicit approval** before production changes. Missing input capability and refusal never authorize work.
6. **Implement** (`implement`): Apply the minimal approved changes. Test creation owns test files; production implementation owns production changes.
7. **Verify GREEN** (`verify`): Execute the original unchanged regression against current source. Changed tests, source changes during/after execution, unrelated successes and stale runs do not verify the fix.
8. **Safety** (`safety`): Run all three named checkers and measure changed-production branch coverage. Require the current latest checker outcomes and qualifying coverage of at least 70%; a zero exit with no relevant files examined is not meaningful validation.
9. **Confirm fix** (`confirm_fix`): Review the acceptance results with the operator and require affirmative confirmation before completion/commit authorization.
10. **Complete** (`done`): Commit only when authorized, preserving the issue reference. Report actual Git failures. Update Project #4 status when authorized and accessible. `CHANGELOG.md` is maintained automatically on merge to main.

The remaining phase, `idle`, is the initial state and the post-completion reset. The engine records `reset_ack` and clears transient exploration state while retaining durable Jev observations. On failed verification or safety, return through the legal retry edges and revalidate changed inputs.

Evidence and tool ownership are runtime gates, not optional prompt suggestions. Stored legacy `test` phases migrate to `create_tests` without prior implementation or `verify` with prior implementation. Re-execute historical results that lack current execution provenance.

## 3. TECHNICAL INVARIANTS
*   **Lambda Names**: Must use `ona-{service}-{stage}` (via `get_lambda_name()` in `config/environment.sh`).
*   **UI Pattern**: Must use Controller/Renderer architecture (Subscriber/Timer vs. DOM/Mutation).
*   **Error Handling**: Must use standardized response formats from `ARCHITECTURAL_FIRST_PRINCIPLES.md`.
*   **NO STATEMENTS WITHOUT EVIDENCE**: Do not make statements of fact about the system without citing BOTH a specific line of documentation AND a specific function or piece of code. Every factual claim must have dual citations.
*   **SDLC Phase Engine**: Use the canonical 11-phase sequence: idle → explore → diagnose → create_issue → create_tests → planning → implement → verify → safety → confirm_fix → done.
